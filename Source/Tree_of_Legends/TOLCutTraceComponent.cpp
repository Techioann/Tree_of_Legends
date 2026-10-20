#include "TOLCutTraceComponent.h"
#include "TOLCutFragment.h"
#include "TOLCutMeshData.h"
#include "TOLTreeActivationSubsystem.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "Math/RotationMatrix.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
	// Rendering duplicates vertices at material and UV seams; identify shared positions.
	// Rendering can duplicate vertices at UV seams, so connect triangles by position.
	FIntVector WeldKey(const FVector& Position)
	{
		constexpr double PositionsPerUnit = 100.0;
		return FIntVector(
			FMath::RoundToInt(Position.X * PositionsPerUnit),
			FMath::RoundToInt(Position.Y * PositionsPerUnit),
			FMath::RoundToInt(Position.Z * PositionsPerUnit));
	}

	bool GetTriangle(const FProcMeshSection& Section, int32 TriangleIndex,
		FVector& A, FVector& B, FVector& C)
	{
		const int32 FirstIndex = TriangleIndex * 3;
		if (TriangleIndex < 0 || FirstIndex + 2 >= Section.ProcIndexBuffer.Num())
		{
			return false;
		}

		const uint32 IndexA = Section.ProcIndexBuffer[FirstIndex];
		const uint32 IndexB = Section.ProcIndexBuffer[FirstIndex + 1];
		const uint32 IndexC = Section.ProcIndexBuffer[FirstIndex + 2];
		if (IndexA >= static_cast<uint32>(Section.ProcVertexBuffer.Num()) ||
			IndexB >= static_cast<uint32>(Section.ProcVertexBuffer.Num()) ||
			IndexC >= static_cast<uint32>(Section.ProcVertexBuffer.Num()))
		{
			return false;
		}

		A = FVector(Section.ProcVertexBuffer[IndexA].Position);
		B = FVector(Section.ProcVertexBuffer[IndexB].Position);
		C = FVector(Section.ProcVertexBuffer[IndexC].Position);
		return true;
	}

	bool FindHitTriangle(UProceduralMeshComponent* Mesh, int32 FaceIndex,
		const FVector& LocalHitPoint, int32& OutSectionIndex, int32& OutTriangleIndex)
	{
		int32 CollisionFaceStart = 0;
		double BestDistanceSquared = TNumericLimits<double>::Max();
		int32 BestSectionIndex = INDEX_NONE;
		int32 BestTriangleIndex = INDEX_NONE;

		for (int32 SectionIndex = 0; SectionIndex < Mesh->GetNumSections(); ++SectionIndex)
		{
			const FProcMeshSection* Section = Mesh->GetProcMeshSection(SectionIndex);
			if (!Section || !Section->bEnableCollision)
			{
				continue;
			}

			const int32 TriangleCount = Section->ProcIndexBuffer.Num() / 3;
			for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
			{
				FVector A, B, C;
				if (!GetTriangle(*Section, TriangleIndex, A, B, C))
				{
					continue;
				}

				const FVector Closest = FMath::ClosestPointOnTriangleToPoint(LocalHitPoint, A, B, C);
				const double DistanceSquared = FVector::DistSquared(LocalHitPoint, Closest);
				if (DistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = DistanceSquared;
					BestSectionIndex = SectionIndex;
					BestTriangleIndex = TriangleIndex;
				}

				// The collision face index disambiguates overlapping segment surfaces at forks.
				if (FaceIndex == CollisionFaceStart + TriangleIndex && DistanceSquared <= 25.0)
				{
					OutSectionIndex = SectionIndex;
					OutTriangleIndex = TriangleIndex;
					return true;
				}
			}
			CollisionFaceStart += TriangleCount;
		}

		// Some trace providers do not supply FaceIndex. Locate the hit surface instead.
		if (BestSectionIndex != INDEX_NONE && BestDistanceSquared <= 25.0)
		{
			OutSectionIndex = BestSectionIndex;
			OutTriangleIndex = BestTriangleIndex;
			return true;
		}
		return false;
	}

	struct FMeshEdge
	{
		FVector A, B;
		TArray<FVector> Normals;
	};

	UTOLCutMeshData* GetCutData(UProceduralMeshComponent* Mesh)
	{
		if (!Mesh || !Mesh->GetOwner()) return nullptr;
		UTOLCutMeshData* Data = Mesh->GetOwner()->FindComponentByClass<UTOLCutMeshData>();
		if (!Data)
		{
			Data = NewObject<UTOLCutMeshData>(Mesh->GetOwner());
			Mesh->GetOwner()->AddInstanceComponent(Data);
			Data->RegisterComponent();
		}
		return Data->EnsureSegments(Mesh) ? Data : nullptr;
	}
	bool FindLocalAxis(UProceduralMeshComponent* Mesh, const FVector& Point,
		const FVector& PreferredDirection, FVector& Axis, int32* OutIndex = nullptr)
	{
		const UTOLCutMeshData* Data=GetCutData(Mesh);
		if (!Data) return false;
		double Best=TNumericLimits<double>::Max();
		for (int32 Index=0; Index<Data->Segments.Num(); ++Index)
		{
			const FTOLCutSegment& Segment=Data->Segments[Index];
			const FVector Candidate=(Segment.End-Segment.Start).GetSafeNormal();
			if (Candidate.IsNearlyZero()) continue;
			const double Distance=FVector::DistSquared(Point,
				FMath::ClosestPointOnSegment(Point,Segment.Start,Segment.End));
			if (Distance < Best) { Best=Distance; Axis=Candidate; if (OutIndex) *OutIndex=Index; }
		}
		if (Best == TNumericLimits<double>::Max()) return false;
		if (FVector::DotProduct(Axis,PreferredDirection)<0.0) Axis=-Axis;
		return true;
	}

	FProcMeshSection CopyTriangles(const FProcMeshSection& Source, const TArray<int32>& Triangles)
	{
		FProcMeshSection Result;
		Result.bEnableCollision = Source.bEnableCollision;
		Result.bSectionVisible = Source.bSectionVisible;
		TMap<int32, int32> VertexMap;
		for (const int32 Triangle : Triangles)
		{
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 SourceIndex = Source.ProcIndexBuffer[Triangle * 3 + Corner];
				int32* DestIndex = VertexMap.Find(SourceIndex);
				if (!DestIndex)
				{
					const int32 NewIndex = Result.ProcVertexBuffer.Add(Source.ProcVertexBuffer[SourceIndex]);
					Result.SectionLocalBox += Source.ProcVertexBuffer[SourceIndex].Position;
					DestIndex = &VertexMap.Add(SourceIndex, NewIndex);
				}
				Result.ProcIndexBuffer.Add(*DestIndex);
			}
		}
		return Result;
	}

	void AppendSection(FProcMeshSection& Dest, const FProcMeshSection& Source)
	{
		const int32 Offset = Dest.ProcVertexBuffer.Num();
		for (const FProcMeshVertex& Vertex : Source.ProcVertexBuffer)
		{
			Dest.ProcVertexBuffer.Add(Vertex);
			Dest.SectionLocalBox += Vertex.Position;
		}
		for (const uint32 Index : Source.ProcIndexBuffer)
		{
			Dest.ProcIndexBuffer.Add(Offset + Index);
		}
	}

	void ConnectedGeometry(UProceduralMeshComponent* Mesh, int32 HitSection, int32 HitTriangle,
		TArray<FProcMeshSection>& Selected, TArray<FProcMeshSection>& Remainder)
	{
		struct FTriangleRef { int32 Section, Triangle; };
		TArray<FTriangleRef> Triangles;
		TMap<FIntVector,TArray<int32>> Adjacency;
		int32 Seed=INDEX_NONE;
		for (int32 S=0; S<Mesh->GetNumSections(); ++S)
		{
			const FProcMeshSection* Section=Mesh->GetProcMeshSection(S);
			if (!Section) continue;
			for (int32 T=0; T<Section->ProcIndexBuffer.Num()/3; ++T)
			{
				FVector A,B,C;
				if (!GetTriangle(*Section,T,A,B,C)) continue;
				const int32 Index=Triangles.Add({S,T});
				if (S==HitSection && T==HitTriangle) Seed=Index;
				for (const FVector& P : {A,B,C}) Adjacency.FindOrAdd(WeldKey(P)).Add(Index);
			}
		}
		TSet<int32> Visited;
		TArray<int32> Queue;
		if (Seed!=INDEX_NONE) { Queue.Add(Seed); Visited.Add(Seed); }
		for (int32 I=0; I<Queue.Num(); ++I)
		{
			const FTriangleRef Ref=Triangles[Queue[I]];
			FVector A,B,C;
			GetTriangle(*Mesh->GetProcMeshSection(Ref.Section),Ref.Triangle,A,B,C);
			for (const FVector& P : {A,B,C})
			{
				for (const int32 N : Adjacency[WeldKey(P)])
				{
					if (!Visited.Contains(N)) { Visited.Add(N); Queue.Add(N); }
				}
			}
		}
		Selected.SetNum(Mesh->GetNumSections()); Remainder.SetNum(Mesh->GetNumSections());
		for (int32 S=0; S<Mesh->GetNumSections(); ++S)
		{
			TArray<int32> Keep,Other;
			for (int32 I=0; I<Triangles.Num(); ++I)
			{
				if (Triangles[I].Section!=S) continue;
				(Visited.Contains(I) ? Keep : Other).Add(Triangles[I].Triangle);
			}
			if (const FProcMeshSection* Section=Mesh->GetProcMeshSection(S))
			{
				Selected[S]=CopyTriangles(*Section,Keep);
				Remainder[S]=CopyTriangles(*Section,Other);
			}
		}
	}

	/** Flood only the surface on the detached side of the plane. Crossing triangles
	 * belonging to that island form the local cut patch; every other original
	 * triangle stays intact, even when the infinite plane intersects another branch. */
	bool SelectBranchCut(UProceduralMeshComponent* Mesh, int32 SegmentIndex,
		const FVector& PlanePoint, const FVector& KeepNormal,
		TArray<FProcMeshSection>& Selected, TArray<FProcMeshSection>& Remainder)
	{
		struct FTriangle { int32 Section, Index; TArray<FVector> FarPolygon; };
		TArray<FTriangle> Triangles;
		TMap<FIntVector,TArray<int32>> Adjacency;
		int32 Seed=INDEX_NONE;
		double Best=TNumericLimits<double>::Max();
		for (int32 S=0; S<Selected.Num(); ++S)
		{
			for (int32 T=0; T<Selected[S].ProcIndexBuffer.Num()/3; ++T)
			{
				FVector A,B,C;
				if (!GetTriangle(Selected[S],T,A,B,C)) return false;
				const FVector P[]={A,B,C};
				FTriangle Triangle; Triangle.Section=S; Triangle.Index=T;
				double Furthest=0.0;
				for (int32 E=0; E<3; ++E)
				{
					const FVector X=P[E], Y=P[(E+1)%3];
					const double DX=-FVector::DotProduct(X-PlanePoint,KeepNormal);
					const double DY=-FVector::DotProduct(Y-PlanePoint,KeepNormal);
					Furthest=FMath::Max(Furthest,DX);
					if (DX>=0.0) Triangle.FarPolygon.Add(X);
					if ((DX>0.0 && DY<0.0) || (DX<0.0 && DY>0.0))
						Triangle.FarPolygon.Add(X+(Y-X)*(DX/(DX-DY)));
				}
				if (Furthest<0.0001 || Triangle.FarPolygon.Num()<3) continue;
				double Area=0.0;
				for (int32 I=1; I+1<Triangle.FarPolygon.Num(); ++I)
					Area+=FVector::CrossProduct(Triangle.FarPolygon[I]-Triangle.FarPolygon[0],
						Triangle.FarPolygon[I+1]-Triangle.FarPolygon[0]).SizeSquared();
				if (Area<1.e-12) continue;
				const int32 Index=Triangles.Add(MoveTemp(Triangle));
				const TArray<FVector>& Polygon=Triangles[Index].FarPolygon;
				for (const FVector& V : Polygon) Adjacency.FindOrAdd(WeldKey(V)).AddUnique(Index);
				for (int32 I=1; I+1<Polygon.Num(); ++I)
				{
					const double Distance=FVector::DistSquared(PlanePoint,
						FMath::ClosestPointOnTriangleToPoint(PlanePoint,Polygon[0],Polygon[I],Polygon[I+1]));
					if (Distance<Best) { Best=Distance; Seed=Index; }
				}
			}
		}
		if (Seed==INDEX_NONE || Best>25.0) return false;
		TSet<int32> Visited; Visited.Add(Seed);
		TArray<int32> Queue; Queue.Add(Seed);
		for (int32 I=0; I<Queue.Num(); ++I)
		{
			for (const FVector& V : Triangles[Queue[I]].FarPolygon)
				for (int32 N : Adjacency[WeldKey(V)])
					if (!Visited.Contains(N)) { Visited.Add(N); Queue.Add(N); }
		}
		// Require one closed boundary, entirely owned by the hit segment. A plane
		// through an ambiguous junction must not take a sibling branch or the trunk.
		TMap<FIntVector,TSet<FIntVector>> Boundary;
		for (int32 Index : Queue)
		{
			const FTriangle& Ref=Triangles[Index];
			FVector A,B,C; GetTriangle(Selected[Ref.Section],Ref.Index,A,B,C);
			const FVector P[]={A,B,C}; TArray<FVector> Crossings;
			bool bNear=false, bFar=false;
			for (int32 E=0; E<3; ++E)
			{
				const FVector X=P[E],Y=P[(E+1)%3];
				const double DX=FVector::DotProduct(X-PlanePoint,KeepNormal);
				const double DY=FVector::DotProduct(Y-PlanePoint,KeepNormal);
				bNear|=DX>0.0001; bFar|=DX<-0.0001;
				FVector Q; bool bCross=false;
				if (FMath::Abs(DX)<0.0001) { Q=X; bCross=true; }
				else if (DX*DY<0.0) { Q=X+(Y-X)*(DX/(DX-DY)); bCross=true; }
				if (bCross && !Crossings.ContainsByPredicate([&](const FVector& V){return WeldKey(V)==WeldKey(Q);})) Crossings.Add(Q);
			}
			if (!bNear || !bFar) continue;
			if (Crossings.Num()!=2) return false;
			FVector Axis; int32 Owner=INDEX_NONE;
			// A long junction triangle can cross this segment's plane while its
			// centre still belongs to the trunk/sibling. Do not clip that shared face.
			if (!FindLocalAxis(Mesh,(A+B+C)/3.0,KeepNormal,Axis,&Owner) || Owner!=SegmentIndex) return false;
			if (!FindLocalAxis(Mesh,(Crossings[0]+Crossings[1])*0.5,KeepNormal,Axis,&Owner) || Owner!=SegmentIndex) return false;
			const FIntVector X=WeldKey(Crossings[0]), Y=WeldKey(Crossings[1]);
			if (X==Y) return false;
			Boundary.FindOrAdd(X).Add(Y); Boundary.FindOrAdd(Y).Add(X);
		}
		if (Boundary.Num()<3) return false;
		TArray<FIntVector> Loop; TSet<FIntVector> Seen;
		for (const auto& Entry : Boundary)
		{
			if (Entry.Value.Num()!=2) return false;
			if (Loop.IsEmpty()) { Loop.Add(Entry.Key); Seen.Add(Entry.Key); }
		}
		for (int32 I=0; I<Loop.Num(); ++I)
			for (const FIntVector& N : Boundary[Loop[I]])
				if (!Seen.Contains(N)) { Seen.Add(N); Loop.Add(N); }
		if (Seen.Num()!=Boundary.Num()) return false;
		TArray<TSet<int32>> Patch; Patch.SetNum(Selected.Num());
		for (int32 I : Queue) Patch[Triangles[I].Section].Add(Triangles[I].Index);
		for (int32 S=0; S<Selected.Num(); ++S)
		{
			TArray<int32> Keep,Other;
			for (int32 T=0; T<Selected[S].ProcIndexBuffer.Num()/3; ++T)
				(Patch[S].Contains(T) ? Keep : Other).Add(T);
			AppendSection(Remainder[S],CopyTriangles(Selected[S],Other));
			Selected[S]=CopyTriangles(Selected[S],Keep);
		}
		return true;
	}

	double DistanceToGeometry(const TArray<FProcMeshSection>& Geometry, const FVector& Point)
	{
		double Best=TNumericLimits<double>::Max();
		for (const FProcMeshSection& Section : Geometry)
			for (int32 T=0; T<Section.ProcIndexBuffer.Num()/3; ++T)
			{
				FVector A,B,C;
				if (GetTriangle(Section,T,A,B,C)) Best=FMath::Min(Best,
					FVector::DistSquared(Point,FMath::ClosestPointOnTriangleToPoint(Point,A,B,C)));
			}
		return Best;
	}

	void RebuildConvexCollision(UProceduralMeshComponent* Mesh)
	{
		Mesh->ClearCollisionConvexMeshes();
		const UTOLCutMeshData* Data=GetCutData(Mesh);
		if (!Data) return;
		TArray<TArray<FVector>> Hulls; Hulls.SetNum(Data->Segments.Num());
		TArray<TSet<FIntVector>> Seen; Seen.SetNum(Data->Segments.Num());
		for (int32 S=0; S<Mesh->GetNumSections(); ++S)
		{
			const FProcMeshSection* Section=Mesh->GetProcMeshSection(S);
			if (!Section) continue;
			for (int32 T=0; T<Section->ProcIndexBuffer.Num()/3; ++T)
			{
				FVector A,B,C;
				if (!GetTriangle(*Section,T,A,B,C)) continue;
				const FVector Center=(A+B+C)/3.0;
				int32 BestIndex=INDEX_NONE; double Best=TNumericLimits<double>::Max();
				for (int32 I=0; I<Data->Segments.Num(); ++I)
				{
					const FTOLCutSegment& Segment=Data->Segments[I];
					const double Distance=FVector::DistSquared(Center,FMath::ClosestPointOnSegment(Center,Segment.Start,Segment.End));
					if (Distance<Best) { Best=Distance; BestIndex=I; }
				}
				if (BestIndex==INDEX_NONE) continue;
				for (const FVector& P : {A,B,C})
				{
					if (!Seen[BestIndex].Contains(WeldKey(P)))
					{ Seen[BestIndex].Add(WeldKey(P)); Hulls[BestIndex].Add(P); }
				}
			}
		}
		for (TArray<FVector>& Hull : Hulls)
			if (Hull.Num()>=4) Mesh->AddCollisionConvexMesh(MoveTemp(Hull));
	}

	bool ReadRowNumber(const FDataTableRowHandle& Handle, const FString& Name, double& Out)
	{
		if (!Handle.DataTable || Handle.RowName.IsNone()) return false;
		const uint8* const* Row=Handle.DataTable->GetRowMap().Find(Handle.RowName);
		if (!Row) return false;
		for (TFieldIterator<FProperty> It(Handle.DataTable->GetRowStruct()); It; ++It)
		{
			FString Key=It->GetAuthoredName().ToLower().Replace(TEXT(" "),TEXT(""));
			if (Key!=Name) continue;
			const FNumericProperty* Number=CastField<FNumericProperty>(*It);
			if (!Number) return false;
			const void* Value=Number->ContainerPtrToValuePtr<void>(*Row);
			Out=Number->IsFloatingPoint() ? Number->GetFloatingPointPropertyValue(Value) :
				static_cast<double>(Number->GetSignedIntPropertyValue(Value));
			return FMath::IsFinite(Out);
		}
		return false;
	}

	bool HasCutClearance(UProceduralMeshComponent* Mesh, int32 SegmentIndex,
		const FVector& PlanePoint, const FVector& Axis, double MinimumLength)
	{
		const FTransform Transform=Mesh->GetComponentTransform();
		const FVector WorldAxis=Transform.TransformVector(Axis).GetSafeNormal();
		double Min=TNumericLimits<double>::Max(), Max=TNumericLimits<double>::Lowest();
		for (int32 S=0; S<Mesh->GetNumSections(); ++S)
		{
			const FProcMeshSection* Section=Mesh->GetProcMeshSection(S);
			if (!Section) continue;
			for (int32 T=0; T<Section->ProcIndexBuffer.Num()/3; ++T)
			{
				FVector A,B,C,Detected; int32 Index=INDEX_NONE;
				if (!GetTriangle(*Section,T,A,B,C) || !FindLocalAxis(Mesh,(A+B+C)/3.0,Axis,Detected,&Index) || Index!=SegmentIndex) continue;
				for (const FVector& P : {A,B,C})
				{
					const double Along=FVector::DotProduct(Transform.TransformPosition(P)-Transform.TransformPosition(PlanePoint),WorldAxis);
					Min=FMath::Min(Min,Along); Max=FMath::Max(Max,Along);
				}
			}
		}
		return Min < -FMath::Max(MinimumLength,0.01) && Max > FMath::Max(MinimumLength,0.01);
	}

	void RefreshCutMarks(UProceduralMeshComponent* Mesh, UTOLCutMeshData* Data, UMaterialInterface* Material)
	{
		if (!Data->CutMarks && !Data->ActiveCuts.IsEmpty() && Material)
		{
			Data->CutMarks=NewObject<UProceduralMeshComponent>(Mesh->GetOwner());
			Mesh->GetOwner()->AddInstanceComponent(Data->CutMarks);
			Data->CutMarks->SetMobility(EComponentMobility::Movable);
			Data->CutMarks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Data->CutMarks->RegisterComponent();
			Data->CutMarks->AttachToComponent(Mesh,FAttachmentTransformRules::SnapToTargetIncludingScale);
			Data->CutMarks->SetRelativeTransform(FTransform::Identity);
			UMaterialInstanceDynamic* Black=UMaterialInstanceDynamic::Create(Material,Data->CutMarks);
			Black->SetVectorParameterValue(TEXT("Color"),FLinearColor::Black);
			Data->CutMarks->SetMaterial(0,Black);
		}
		if (!Data->CutMarks) return;
		Data->CutMarks->ClearAllMeshSections();
		TArray<FVector> Vertices,Normals;
		TArray<int32> Indices;
		TArray<FVector2D> UV;
		TArray<FLinearColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		const FTransform Transform=Mesh->GetComponentTransform();
		for (const FTOLActiveCut& Cut : Data->ActiveCuts)
		{
			const FVector WorldAxis=Transform.TransformVector(Cut.AxisLocal).GetSafeNormal();
			const FVector Normal=(Transform.GetScale3D()*Transform.InverseTransformVectorNoScale(WorldAxis)).GetSafeNormal();
			const double Scale=FMath::Max(Transform.TransformVector(Cut.AxisLocal).Size(),0.001);
			const FVector HalfWidth=Cut.AxisLocal*(0.15+0.85*FMath::Clamp(Cut.Progress/Cut.RequiredProgress,0.0,1.0))/Scale;
			for (int32 S=0; S<Mesh->GetNumSections(); ++S)
			{
				const FProcMeshSection* Section=Mesh->GetProcMeshSection(S);
				if (!Section) continue;
				for (int32 T=0; T<Section->ProcIndexBuffer.Num()/3; ++T)
				{
					FVector A,B,C,Axis; int32 Segment=INDEX_NONE;
					if (!GetTriangle(*Section,T,A,B,C) || !FindLocalAxis(Mesh,(A+B+C)/3.0,Cut.AxisLocal,Axis,&Segment) || Segment!=Cut.SegmentIndex) continue;
					const FVector P[]={A,B,C}; TArray<FVector> Crossings;
					for (int32 E=0; E<3; ++E)
					{
						const FVector X=P[E],Y=P[(E+1)%3];
						const double DX=FVector::DotProduct(X-Cut.LocalPlanePoint,Normal), DY=FVector::DotProduct(Y-Cut.LocalPlanePoint,Normal);
						FVector Q; bool Has=false;
						if (FMath::Abs(DX)<0.0001) { Q=X; Has=true; }
						else if (DX*DY<0.0) { Q=X+(Y-X)*(DX/(DX-DY)); Has=true; }
						if (Has && !Crossings.ContainsByPredicate([&](const FVector& V){return V.Equals(Q,0.001);})) Crossings.Add(Q);
					}
					if (Crossings.Num()!=2) continue;
					const FVector SurfaceNormal=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
					// Draw both offsets: source winding can face inward after import.
					for (const double Sign : {-1.0,1.0})
					{
						const FVector Offset=SurfaceNormal*Sign*0.08/Scale;
						const int32 Base=Vertices.Num();
						for (const FVector& V : {Crossings[0]-HalfWidth+Offset,Crossings[0]+HalfWidth+Offset,Crossings[1]+HalfWidth+Offset,Crossings[1]-HalfWidth+Offset})
						{ Vertices.Add(V); Normals.Add(SurfaceNormal); UV.Add(FVector2D::ZeroVector); Colors.Add(FLinearColor::Black); Tangents.Add(FProcMeshTangent()); }
						for (const int32 I : {0,1,2,0,2,3,2,1,0,3,2,0}) Indices.Add(Base+I);
					}
				}
			}
		}
		if (!Indices.IsEmpty()) Data->CutMarks->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UV,Colors,Tangents,false);
	}

}

UTOLCutTraceComponent::UTOLCutTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UDataTable> Axes(TEXT("/Game/Blueprints/BPStructure/DT_Axe.DT_Axe"));
	static ConstructorHelpers::FObjectFinder<UDataTable> Trees(TEXT("/Game/Blueprints/BPStructure/DT_Tree.DT_Tree"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mark(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	AxeData.DataTable=Axes.Object; AxeData.RowName=TEXT("Wa");
	DefaultTreeData.DataTable=Trees.Object; DefaultTreeData.RowName=TEXT("Tree_Birch");
	CutMarkMaterial=Mark.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Wood(TEXT("/Game/Meshes/SM_Tree_Wood.SM_Tree_Wood"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Iron(TEXT("/Game/Meshes/SM_Tree_Iron.SM_Tree_Iron"));
	FTOLDormantTreeProfile WoodProfile;
	WoodProfile.Model.Mesh=Wood.Object;
	WoodProfile.TreeData=DefaultTreeData;
	DormantTreeProfiles.Add(WoodProfile);
	FTOLDormantTreeProfile IronProfile;
	IronProfile.Model.Mesh=Iron.Object;
	IronProfile.Model.SegmentMode=ETOLCutSegmentMode::Authored;
	IronProfile.TreeData.DataTable=Trees.Object;
	IronProfile.TreeData.RowName=TEXT("Tree_Iron");
	DormantTreeProfiles.Add(IronProfile);
}

void UTOLCutTraceComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World=GetWorld();
	if (!World || !World->IsGameWorld() || DormantTreeProfiles.IsEmpty()) return;
	UTOLTreeActivationSubsystem* Registry=World->GetSubsystem<UTOLTreeActivationSubsystem>();
	if (!Registry || !Registry->PrepareDormantTreesOnce()) return;
	// One lightweight pass: no procedural meshes, per-tree components or ticking manager.
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* Source=It->GetStaticMeshComponent();
		if (!Source || !DormantTreeProfiles.ContainsByPredicate([&](const FTOLDormantTreeProfile& Profile)
			{ return Profile.Model.Mesh && Profile.Model.Mesh==Source->GetStaticMesh(); })) continue;
		Source->SetSimulatePhysics(false);
		Source->SetComponentTickEnabled(false);
		It->SetActorTickEnabled(false);
	}
}

bool UTOLCutTraceComponent::TraceCutFromPlayerView(FTOLCutTraceResult& OutResult)
{
	OutResult = FTOLCutTraceResult();
	LastCutFragment = nullptr;
	LastChopResult=FTOLChopResult();

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PlayerController || !World)
	{
		OnCutTraceCompleted.Broadcast(OutResult);
		return false;
	}

	FVector TraceStart;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(TraceStart, ViewRotation);
	const FVector TraceEnd = TraceStart + ViewRotation.Vector() * FMath::Max(TraceDistance, 1.0f);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TOLCutTrace), true);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bReturnFaceIndex = true;

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, TraceChannel.GetValue(), QueryParams))
	{
		OutResult.Hit = Hit;
		if (bCutOnValidTrace)
		{
			FTOLChopResult Chop;
			ApplyTreeHit(Hit,OutResult,Chop);
		}
		else ComputeCutPlaneFromHit(Hit,OutResult);
	}

	if (bDrawDebug)
	{
		DrawCutDebug(TraceStart, TraceEnd, OutResult);
	}

	OnCutTraceCompleted.Broadcast(OutResult);
	return OutResult.bValidTreeHit;
}

bool UTOLCutTraceComponent::ComputeCutPlaneFromHit(const FHitResult& Hit, FTOLCutTraceResult& OutResult) const
{
	OutResult = FTOLCutTraceResult();
	OutResult.Hit = Hit;

	const AActor* HitActor = Hit.GetActor();
	UProceduralMeshComponent* HitMesh = Cast<UProceduralMeshComponent>(Hit.GetComponent());
	const UTOLCutMeshData* ExistingData=HitActor ? HitActor->FindComponentByClass<UTOLCutMeshData>() : nullptr;
	const FVector PreferredDirection = ExistingData ? ExistingData->GetTrunkAxis(TrunkAxisLocal) : TrunkAxisLocal.GetSafeNormal();
	if (!Hit.bBlockingHit || !HitActor || !HitMesh ||
		!(HitActor->IsA<ATOLCutFragment>() || (CuttableTreeClass && HitActor->IsA(CuttableTreeClass))) ||
		PreferredDirection.IsNearlyZero())
	{
		return false;
	}

	const FTransform ComponentTransform = HitMesh->GetComponentTransform();
	const FVector Scale = ComponentTransform.GetScale3D();
	if (Hit.ImpactPoint.ContainsNaN() || ComponentTransform.ContainsNaN() ||
		FMath::IsNearlyZero(Scale.X) || FMath::IsNearlyZero(Scale.Y) || FMath::IsNearlyZero(Scale.Z))
	{
		return false;
	}
	OutResult.LocalHitPoint = ComponentTransform.InverseTransformPosition(Hit.ImpactPoint);
	int32 SectionIndex = INDEX_NONE;
	int32 TriangleIndex = INDEX_NONE;
	if (!FindHitTriangle(HitMesh, Hit.FaceIndex, OutResult.LocalHitPoint, SectionIndex, TriangleIndex))
	{
		return false;
	}
	const FProcMeshSection* Section = HitMesh->GetProcMeshSection(SectionIndex);
	FVector Axis;
	if (!Section || !FindLocalAxis(HitMesh, OutResult.LocalHitPoint, PreferredDirection, Axis))
	{
		return false;
	}

	OutResult.DistanceAlongTrunkLocal = FVector::DotProduct(OutResult.LocalHitPoint, Axis);
	OutResult.LocalPlanePoint = OutResult.LocalHitPoint;
	OutResult.WorldPlanePoint = Hit.ImpactPoint;
	// The world plane is perpendicular to the actual transformed segment direction.
	OutResult.WorldPlaneNormal = ComponentTransform.TransformVector(Axis).GetSafeNormal();
	// Pull that plane normal back with the transpose, keeping both plane outputs
	// consistent even when the component has nonuniform or mirrored scale.
	OutResult.LocalPlaneNormal = (Scale * ComponentTransform.InverseTransformVectorNoScale(
		OutResult.WorldPlaneNormal)).GetSafeNormal();
	OutResult.bValidTreeHit = !OutResult.WorldPlaneNormal.IsNearlyZero();
	return OutResult.bValidTreeHit;
}

bool UTOLCutTraceComponent::EquipAxe(FDataTableRowHandle Row, double RemainingDurability)
{
	double Power=0.0, Tier=0.0;
	if (!FMath::IsFinite(RemainingDurability) || RemainingDurability<0.0 ||
		!ReadRowNumber(Row,TEXT("chopdamage"),Power) || !ReadRowNumber(Row,TEXT("requirelevel"),Tier) || Power<=0.0) return false;
	AxeData=Row; ToolDurability=RemainingDurability;
	return true;
}

bool UTOLCutTraceComponent::CalculateChopValues(FDataTableRowHandle Material, double Resistance,
	double& OutGain, double& OutWear, double& OutHealth) const
{
	OutGain=0.0; OutWear=0.0; OutHealth=0.0;
	double Power=0.0, ToolTier=0.0, RequiredTier=0.0;
	if (!FMath::IsFinite(ToolDurability) || ToolDurability<=0.0 ||
		!ReadRowNumber(AxeData,TEXT("chopdamage"),Power) || !ReadRowNumber(AxeData,TEXT("requirelevel"),ToolTier) ||
		!ReadRowNumber(Material,TEXT("maxhealth"),OutHealth) || !ReadRowNumber(Material,TEXT("requiredaxelevel"),RequiredTier) ||
		Power<=0.0 || OutHealth<=0.0 || !FMath::IsFinite(Resistance) || Resistance<=0.0 ||
		!FMath::IsFinite(WeakToolEfficiency) || !FMath::IsFinite(ExtraWearPerMissingTier) ||
		!FMath::IsFinite(DurabilityLossPerHit)) return false;
	const double MissingTiers=FMath::Clamp(RequiredTier-ToolTier,0.0,32.0);
	OutGain=Power/Resistance*FMath::Pow(FMath::Clamp(WeakToolEfficiency,0.001,1.0),MissingTiers);
	OutWear=FMath::Min(ToolDurability,FMath::Max(DurabilityLossPerHit,0.0)*
		(1.0+MissingTiers*FMath::Max(ExtraWearPerMissingTier,0.0)));
	return FMath::IsFinite(OutGain) && OutGain>0.0 && FMath::IsFinite(OutWear);
}

bool UTOLCutTraceComponent::ApplyTreeHit(const FHitResult& Hit, FTOLCutTraceResult& OutTrace, FTOLChopResult& OutChop)
{
	LastCutFragment=nullptr;
	OutChop=FTOLChopResult();
	if (ComputeCutPlaneFromHit(Hit,OutTrace)) return ApplyChop(OutTrace,OutChop);
	if (TryActivateTree(Hit,OutTrace,OutChop)) return true;
	if (OutTrace.bValidTreeHit) return false; // Existing active tree already published a rejected chop.
	OutChop.DurabilityRemaining=ToolDurability;
	OutChop.bToolBroken=ToolDurability<=0.0;
	LastChopResult=OutChop;
	OnChopCompleted.Broadcast(OutChop);
	return false;
}

bool UTOLCutTraceComponent::TryActivateTree(const FHitResult& Hit, FTOLCutTraceResult& OutTrace, FTOLChopResult& OutChop)
{
	AStaticMeshActor* Original=Cast<AStaticMeshActor>(Hit.GetActor());
	UStaticMeshComponent* Source=Original ? Original->GetStaticMeshComponent() : nullptr;
	UWorld* World=GetWorld();
	if (!Hit.bBlockingHit || !World || !Source || Source!=Hit.GetComponent() ||
		Source->GetWorld()!=World || Hit.ImpactPoint.ContainsNaN() || !CuttableTreeClass) return false;
	UTOLTreeActivationSubsystem* Registry=World->GetSubsystem<UTOLTreeActivationSubsystem>();
	if (!Registry) return false;
	auto RemapHit=[&](AActor* Actor, UProceduralMeshComponent* Mesh)
	{
		FHitResult Contact(Actor,Mesh,Hit.ImpactPoint,Hit.ImpactNormal);
		Contact.bBlockingHit=true; Contact.ImpactPoint=Hit.ImpactPoint;
		Contact.TraceStart=Hit.TraceStart; Contact.TraceEnd=Hit.TraceEnd; Contact.FaceIndex=INDEX_NONE;
		return Contact;
	};
	// A stale reference to the hidden source routes to its existing active body.
	if (AActor* Active=Registry->GetActiveTree(Source))
	{
		UTOLCutMeshData* Data=Active->FindComponentByClass<UTOLCutMeshData>();
		UProceduralMeshComponent* Mesh=Data ? Data->GetBoundMesh() : nullptr;
		return Data && Mesh && ComputeCutPlaneFromHit(RemapHit(Active,Mesh),OutTrace) && ApplyChop(OutTrace,OutChop);
	}
	if (!Source->IsVisible() || Source->bHiddenInGame || Original->IsHidden()) return false;
	const FTOLDormantTreeProfile* Profile=nullptr;
	for (const FTOLDormantTreeProfile& Entry : DormantTreeProfiles)
	{
		if (!Entry.Model.Mesh || Entry.Model.Mesh!=Source->GetStaticMesh()) continue;
		if (Profile) return false; // Ambiguous configuration must not select arbitrary material data.
		Profile=&Entry;
	}
	double Gain=0.0, Wear=0.0, Health=0.0;
	if (!Profile || !CalculateChopValues(Profile->TreeData,Profile->ResistanceMultiplier,Gain,Wear,Health)) return false;
	const FTransform Transform=Source->GetComponentTransform();
	const FVector Scale=Transform.GetScale3D();
	if (Transform.ContainsNaN() || FMath::IsNearlyZero(Scale.X) || FMath::IsNearlyZero(Scale.Y) ||
		FMath::IsNearlyZero(Scale.Z) || !Registry->BeginActivation(Source)) return false;
	const double DurabilityBefore=ToolDurability;
	AActor* Candidate=World->SpawnActorDeferred<AActor>(CuttableTreeClass,Transform,nullptr,nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn,ESpawnActorScaleMethod::OverrideRootScale);
	auto Rollback=[&]()
	{
		if (IsValid(LastCutFragment)) LastCutFragment->Destroy();
		LastCutFragment=nullptr;
		if (IsValid(Candidate)) Candidate->Destroy();
		Registry->AbortActivation(Source);
		ToolDurability=DurabilityBefore;
		OutTrace=FTOLCutTraceResult(); OutTrace.Hit=Hit; OutChop=FTOLChopResult();
		return false;
	};
	if (!Candidate) return Rollback();
	Candidate->SetActorHiddenInGame(true);
	Candidate->SetActorEnableCollision(false);
	Candidate->Tags=Original->Tags;
	Candidate->FinishSpawning(Transform,false,nullptr,ESpawnActorScaleMethod::OverrideRootScale);
	UTOLCutMeshData* Data=Candidate->FindComponentByClass<UTOLCutMeshData>();
	UStaticMeshComponent* CandidateSource=Candidate->FindComponentByClass<UStaticMeshComponent>();
	UProceduralMeshComponent* Mesh=Candidate->FindComponentByClass<UProceduralMeshComponent>();
	FString Error;
	if (!Data || !CandidateSource || !Mesh || !Data->ConfigureMaterial(Profile->TreeData,Profile->ResistanceMultiplier)) return Rollback();
	CandidateSource->EmptyOverrideMaterials();
	for (int32 I=0; I<Source->GetNumMaterials(); ++I) CandidateSource->SetMaterial(I,Source->GetMaterial(I));
	if (!Data->ConfigureModel(Profile->Model,CandidateSource,Mesh,Error) ||
		!Mesh->GetComponentTransform().Equals(Transform,0.01) ||
		!ComputeCutPlaneFromHit(RemapHit(Candidate,Mesh),OutTrace)) return Rollback();
	{
		// Publish notifications only after committing, so callbacks cannot observe a half-activated tree.
		TGuardValue<bool> Deferred(bDeferChopNotifications,true);
		if (!ApplyChop(OutTrace,OutChop) || OutChop.ProgressGain<=0.0) return Rollback();
	}
	if (!Registry->CommitActivation(Source,Candidate)) return Rollback();
	// The original stays intact and visible until conversion AND the first chop both succeed.
	Source->SetSimulatePhysics(false);
	Source->SetVisibility(false,true);
	Source->SetHiddenInGame(true,true);
	Source->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Candidate->SetActorHiddenInGame(false);
	Candidate->SetActorEnableCollision(true);
	// While actor collision was disabled, SetCollisionEnabled(NoCollision) could early-out
	// against the actor's effective state without updating the component's body setting.
	CandidateSource->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Data->CutMarks) Data->CutMarks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	LastActivatedTree=Candidate;
	LastChopResult=OutChop;
	if (OutChop.bTreeCut && LastCutFragment) OnSegmentCutCompleted.Broadcast(OutTrace,LastCutFragment);
	OnChopCompleted.Broadcast(OutChop);
	return true;
}

bool UTOLCutTraceComponent::ApplyChop(const FTOLCutTraceResult& Cut, FTOLChopResult& OutResult)
{
	OutResult=FTOLChopResult(); LastCutFragment=nullptr;
	auto Finish=[&](bool Applied)
	{
		OutResult.bApplied=Applied;
		OutResult.DurabilityRemaining=ToolDurability;
		OutResult.bToolBroken=ToolDurability<=0.0;
		LastChopResult=OutResult;
		if (!bDeferChopNotifications) OnChopCompleted.Broadcast(OutResult);
		return Applied;
	};
	FTOLCutTraceResult Current;
	if (!FMath::IsFinite(ToolDurability) || ToolDurability<=0.0 || !Cut.bValidTreeHit ||
		!ComputeCutPlaneFromHit(Cut.Hit,Current) || !Current.WorldPlanePoint.Equals(Cut.WorldPlanePoint,0.1)) return Finish(false);
	UProceduralMeshComponent* Mesh=Cast<UProceduralMeshComponent>(Current.Hit.GetComponent());
	UTOLCutMeshData* Data=GetCutData(Mesh);
	if (!Data) return Finish(false);
	if (!Data->TreeData.DataTable) Data->TreeData=DefaultTreeData;
	double Gain=0.0, Wear=0.0, Health=0.0;
	if (!CalculateChopValues(Data->TreeData,Data->ResistanceMultiplier,Gain,Wear,Health)) return Finish(false);
	FVector Axis; int32 Segment=INDEX_NONE;
	if (!FindLocalAxis(Mesh,Current.LocalHitPoint,TrunkAxisLocal.GetSafeNormal(),Axis,&Segment)) return Finish(false);
	const FTransform Transform=Mesh->GetComponentTransform();
	const FVector WorldAxis=Transform.TransformVector(Axis).GetSafeNormal();
	int32 CutIndex=INDEX_NONE; double Nearest=FMath::Max(CutMergeDistance,0.0);
	for (int32 I=0; I<Data->ActiveCuts.Num(); ++I)
	{
		const FTOLActiveCut& Active=Data->ActiveCuts[I];
		if (Active.SegmentIndex!=Segment) continue;
		const double Distance=FMath::Abs(FVector::DotProduct(Current.WorldPlanePoint-
			Transform.TransformPosition(Active.LocalPlanePoint),WorldAxis));
		if (Distance<=Nearest) { Nearest=Distance; CutIndex=I; }
	}
	const FVector PlanePoint=CutIndex==INDEX_NONE ? Current.LocalHitPoint : Data->ActiveCuts[CutIndex].LocalPlanePoint;
	if (!HasCutClearance(Mesh,Segment,PlanePoint,Axis,MinimumPieceLength)) return Finish(false);
	if (CutIndex==INDEX_NONE)
	{
		FTOLActiveCut Active; Active.Id=FGuid::NewGuid(); Active.SegmentIndex=Segment;
		Active.LocalPlanePoint=PlanePoint; Active.AxisLocal=Axis; Active.RequiredProgress=Health;
		CutIndex=Data->ActiveCuts.Add(Active);
	}
	FTOLActiveCut Active=Data->ActiveCuts[CutIndex];
	const double Previous=Active.Progress;
	Active.Progress=FMath::Min(Active.RequiredProgress,Previous+Gain);
	OutResult.CutId=Active.Id;
	OutResult.ProgressGain=Active.Progress-Previous;
	OutResult.ProgressNormalized=Active.Progress/Active.RequiredProgress;
	OutResult.WorldPlanePoint=Transform.TransformPosition(Active.LocalPlanePoint);
	if (Active.Progress>=Active.RequiredProgress)
	{
		ATOLCutFragment* Fragment=nullptr;
		if (!SliceAtPlane(Current,OutResult.WorldPlanePoint,Fragment))
		{
			// An unsuccessful geometric operation neither spends wear nor completes progress.
			if (Previous==0.0) Data->ActiveCuts.RemoveAt(CutIndex);
			OutResult.ProgressGain=0.0; OutResult.ProgressNormalized=Previous/Active.RequiredProgress;
			return Finish(false);
		}
		Data->ActiveCuts.RemoveAll([&](const FTOLActiveCut& Item){return Item.Id==Active.Id;});
		UTOLCutMeshData* Other=Fragment->FindComponentByClass<UTOLCutMeshData>();
		Other->ActiveCuts.RemoveAll([&](const FTOLActiveCut& Item){return Item.Id==Active.Id;});
		RefreshCutMarks(Mesh,Data,CutMarkMaterial); RefreshCutMarks(Fragment->CutMesh,Other,CutMarkMaterial);
		LastCutFragment=Fragment; OutResult.bTreeCut=true;
		FTOLCutTraceResult Completed=Current;
		Completed.WorldPlanePoint=OutResult.WorldPlanePoint; Completed.LocalPlanePoint=Active.LocalPlanePoint;
		if (!bDeferChopNotifications) OnSegmentCutCompleted.Broadcast(Completed,Fragment);
	}
	else
	{
		Data->ActiveCuts[CutIndex]=Active;
		RefreshCutMarks(Mesh,Data,CutMarkMaterial);
	}
	Data->MarkChopped();
	ToolDurability=FMath::Max(0.0,ToolDurability-Wear); OutResult.DurabilityLoss=Wear;
	return Finish(true);
}

bool UTOLCutTraceComponent::CutHitSegment(const FTOLCutTraceResult& Cut, ATOLCutFragment*& OutFragment)
{
	OutFragment = nullptr;
	FTOLCutTraceResult Current;
	if (!Cut.bValidTreeHit || !ComputeCutPlaneFromHit(Cut.Hit, Current) ||
		!Current.WorldPlanePoint.Equals(Cut.WorldPlanePoint, 0.1) ||
		!Current.WorldPlaneNormal.Equals(Cut.WorldPlaneNormal, 0.01))
	{
		return false;
	}

	return SliceAtPlane(Current,Current.WorldPlanePoint,OutFragment);
}

bool UTOLCutTraceComponent::SliceAtPlane(const FTOLCutTraceResult& Current, const FVector& WorldPlanePoint, ATOLCutFragment*& OutFragment)
{
	OutFragment=nullptr;
	UProceduralMeshComponent* SourceMesh = Cast<UProceduralMeshComponent>(Current.Hit.GetComponent());
	UWorld* World = GetWorld();
	if (!SourceMesh || !World)
	{
		return false;
	}
	int32 SectionIndex = INDEX_NONE;
	int32 TriangleIndex = INDEX_NONE;
	if (!FindHitTriangle(SourceMesh, Current.Hit.FaceIndex, Current.LocalHitPoint, SectionIndex, TriangleIndex))
	{
		return false;
	}
	FVector SegmentAxis; int32 HitSegment=INDEX_NONE;
	UTOLCutMeshData* SourceData=GetCutData(SourceMesh);
	if (!SourceData || !FindLocalAxis(SourceMesh,Current.LocalHitPoint,TrunkAxisLocal.GetSafeNormal(),SegmentAxis,&HitSegment)) return false;
	TArray<FProcMeshSection> Selected, Remainder;
	ConnectedGeometry(SourceMesh,SectionIndex,TriangleIndex,Selected,Remainder);
	const int32 OriginalSections=SourceMesh->GetNumSections();
	UMaterialInterface* BarkMaterial=SourceMesh->GetMaterial(SectionIndex);
	UMaterialInterface* CapMaterial=CutFaceMaterial ? CutFaceMaterial.Get() : BarkMaterial;
	// Keep the root side. Centerline endpoints survive cuts and give a stable
	// reference even when a branch is shorter than its cross-section.
	FVector Root=SourceData->Segments[0].Start;
	const FVector RootAxis=SourceData->GetTrunkAxis(TrunkAxisLocal);
	for (const FTOLCutSegment& Segment : SourceData->Segments)
	{
		for (const FVector& P : {Segment.Start,Segment.End})
			if (FVector::DotProduct(P,RootAxis)<FVector::DotProduct(Root,RootAxis)) Root=P;
	}
	const FVector RootWorld=SourceMesh->GetComponentTransform().TransformPosition(Root);
	const FVector KeepNormal=FVector::DotProduct(RootWorld-WorldPlanePoint,Current.WorldPlaneNormal)>=0.0
		? Current.WorldPlaneNormal : -Current.WorldPlaneNormal;
	const FTransform Transform=SourceMesh->GetComponentTransform();
	const FVector PlaneLocal=Transform.InverseTransformPosition(WorldPlanePoint);
	const FVector KeepLocal=(Transform.GetScale3D()*Transform.InverseTransformVectorNoScale(KeepNormal)).GetSafeNormal();
	if (!SelectBranchCut(SourceMesh,HitSegment,PlaneLocal,KeepLocal,Selected,Remainder))
	{
		UE_LOG(LogTemp, Display, TEXT("Cutting: rejected ambiguous/open branch boundary on %s, segment %d"),
			*GetNameSafe(SourceMesh->GetOwner()),HitSegment);
		return false;
	}
	UProceduralMeshComponent* Preview=NewObject<UProceduralMeshComponent>(SourceMesh->GetOwner());
	Preview->bUseComplexAsSimpleCollision=false;
	Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Preview->SetVisibility(false);
	Preview->RegisterComponentWithWorld(World);
	Preview->SetWorldTransform(SourceMesh->GetComponentTransform());
	for (int32 S=0; S<OriginalSections; ++S)
	{
		Preview->SetProcMeshSection(S,Selected[S]);
		Preview->SetMaterial(S,SourceMesh->GetMaterial(S));
	}
	UProceduralMeshComponent* OtherHalf=nullptr;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(Preview,WorldPlanePoint,KeepNormal,
		true,OtherHalf,EProcMeshSliceCapOption::CreateNewSectionForCap,CapMaterial);
	const FProcMeshSection* KeptCap=Preview->GetProcMeshSection(OriginalSections);
	const FProcMeshSection* OtherCap=OtherHalf ? OtherHalf->GetProcMeshSection(OtherHalf->GetNumSections()-1) : nullptr;
	const bool Valid=KeptCap && OtherCap && !KeptCap->ProcIndexBuffer.IsEmpty() && !OtherCap->ProcIndexBuffer.IsEmpty();
	if (!Valid)
	{
		if (OtherHalf) OtherHalf->DestroyComponent();
		Preview->DestroyComponent();
		return false;
	}
	ATOLCutFragment* Fragment=World->SpawnActor<ATOLCutFragment>(ATOLCutFragment::StaticClass(),SourceMesh->GetComponentTransform());
	if (!Fragment)
	{
		OtherHalf->DestroyComponent(); Preview->DestroyComponent(); return false;
	}
	UTOLCutMeshData* FragmentData=NewObject<UTOLCutMeshData>(Fragment);
	FragmentData->Segments=SourceData->Segments;
	FragmentData->TreeData=SourceData->TreeData;
	FragmentData->ResistanceMultiplier=SourceData->ResistanceMultiplier;
	FragmentData->InheritBinding(SourceData,Fragment->CutMesh);
	TArray<FProcMeshSection> KeptGeometry=Remainder, DetachedGeometry;
	for (int32 S=0; S<Preview->GetNumSections(); ++S)
	{
		KeptGeometry.SetNum(FMath::Max(KeptGeometry.Num(),S+1));
		if (const FProcMeshSection* Section=Preview->GetProcMeshSection(S)) AppendSection(KeptGeometry[S],*Section);
	}
	for (int32 S=0; S<OtherHalf->GetNumSections(); ++S)
		if (const FProcMeshSection* Section=OtherHalf->GetProcMeshSection(S)) DetachedGeometry.Add(*Section);
	TArray<FTOLActiveCut> KeptCuts;
	for (const FTOLActiveCut& Active : SourceData->ActiveCuts)
	{
		// The plane also extends through untouched branches: migrate progress by
		// membership in the actual output surface, never by a global half-space test.
		if (DistanceToGeometry(DetachedGeometry,Active.LocalPlanePoint)+0.000001 <
			DistanceToGeometry(KeptGeometry,Active.LocalPlanePoint)) FragmentData->ActiveCuts.Add(Active);
		else KeptCuts.Add(Active);
	}
	Fragment->AddInstanceComponent(FragmentData); FragmentData->RegisterComponent();
	for (int32 S=0; S<OtherHalf->GetNumSections(); ++S)
	{
		FProcMeshSection Section=*OtherHalf->GetProcMeshSection(S);
		Section.bEnableCollision=true; Section.bSectionVisible=true;
		Fragment->CutMesh->SetProcMeshSection(S,Section);
		Fragment->CutMesh->SetMaterial(S,OtherHalf->GetMaterial(S));
	}
	const bool WasSimulating=SourceMesh->IsSimulatingPhysics();
	const FVector LinearVelocity=SourceMesh->GetPhysicsLinearVelocity();
	const FVector AngularVelocity=SourceMesh->GetPhysicsAngularVelocityInRadians();
	if (WasSimulating) SourceMesh->SetSimulatePhysics(false);
	for (int32 S=0; S<OriginalSections; ++S)
	{
		if (const FProcMeshSection* Kept=Preview->GetProcMeshSection(S)) AppendSection(Remainder[S],*Kept);
		SourceMesh->SetProcMeshSection(S,Remainder[S]);
	}
	FProcMeshSection Cap=*KeptCap; Cap.bEnableCollision=true; Cap.bSectionVisible=true;
	SourceMesh->SetProcMeshSection(OriginalSections,Cap);
	SourceMesh->SetMaterial(OriginalSections,CapMaterial);
	OtherHalf->DestroyComponent(); Preview->DestroyComponent();
	SourceData->MarkGeometryModified();
	RebuildConvexCollision(Fragment->CutMesh);
	if (!SourceMesh->bUseComplexAsSimpleCollision) RebuildConvexCollision(SourceMesh);
	if (WasSimulating)
	{
		SourceMesh->SetSimulatePhysics(true);
		SourceMesh->SetPhysicsLinearVelocity(LinearVelocity);
		SourceMesh->SetPhysicsAngularVelocityInRadians(AngularVelocity);
	}
	Fragment->CutMesh->SetSimulatePhysics(true);
	Fragment->CutMesh->SetPhysicsLinearVelocity(LinearVelocity);
	Fragment->CutMesh->SetPhysicsAngularVelocityInRadians(AngularVelocity);
	SourceData->ActiveCuts=MoveTemp(KeptCuts);
	RefreshCutMarks(SourceMesh,SourceData,CutMarkMaterial);
	RefreshCutMarks(Fragment->CutMesh,FragmentData,CutMarkMaterial);
	OutFragment = Fragment;
	UE_LOG(LogTemp, Display, TEXT("Cutting: split %s into %s; %d preserved segment axes; physics=%s"),
		*GetNameSafe(SourceMesh->GetOwner()),*Fragment->GetName(),SourceData->Segments.Num(),
		Fragment->CutMesh->IsSimulatingPhysics() ? TEXT("on") : TEXT("off"));
	return true;
}

void UTOLCutTraceComponent::DrawCutDebug(const FVector& TraceStart, const FVector& TraceEnd, const FTOLCutTraceResult& Result) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// A valid contact can still reject a chop when the shared axe is broken.
	// Keep this separate from the green trace/plane, which only confirms targeting.
	const uint64 FeedbackKey = (uint64(GetUniqueID()) << 32) | 0x435554;
	if (GEngine) GEngine->RemoveOnScreenDebugMessage(FeedbackKey);
	const UStaticMeshComponent* StaticSource = Cast<UStaticMeshComponent>(Result.Hit.GetComponent());
	const bool bDormantTree = StaticSource && DormantTreeProfiles.ContainsByPredicate(
		[&](const FTOLDormantTreeProfile& Profile) { return Profile.Model.Mesh && Profile.Model.Mesh == StaticSource->GetStaticMesh(); });
	if (GEngine && bCutOnValidTrace && (Result.bValidTreeHit || bDormantTree))
	{
		const FTOLChopResult& Chop = LastChopResult;
		const FString State = Chop.bToolBroken
			? (Chop.bApplied ? TEXT("AXE BROKEN after this hit - equip/repair axe")
				: TEXT("AXE BROKEN - no progress; equip/repair axe"))
			: (Chop.bTreeCut ? TEXT("CUT COMPLETE") : (Chop.bApplied ? TEXT("CHOP APPLIED") : TEXT("CHOP REJECTED")));
		const FString Progress = Chop.bApplied
			? FString::Printf(TEXT("%.4f%%"), Chop.ProgressNormalized * 100.0) : TEXT("unchanged");
		const FString Message = FString::Printf(TEXT("%s\n%s\ngain %.4f | cut %s | axe %.2f (-%.2f)"),
			*GetNameSafe(Result.Hit.GetActor()), *State, Chop.ProgressGain,
			*Progress, Chop.DurabilityRemaining, Chop.DurabilityLoss);
		GEngine->AddOnScreenDebugMessage(FeedbackKey, FMath::Max(DebugDuration, 0.1f),
			Chop.bToolBroken ? FColor::Red : (Chop.bApplied ? FColor::Green : FColor::Yellow), Message);
	}

	const FVector LineEnd = Result.Hit.bBlockingHit ? Result.Hit.ImpactPoint : TraceEnd;
	DrawDebugLine(World, TraceStart, LineEnd, Result.bValidTreeHit ? FColor::Green : FColor::Red,
		false, DebugDuration, 0, 2.0f);
	if (!Result.Hit.bBlockingHit)
	{
		return;
	}

	DrawDebugSphere(World, Result.Hit.ImpactPoint, 8.0f, 12, FColor::Yellow, false, DebugDuration);
	if (!Result.bValidTreeHit)
	{
		return;
	}

	const FVector Center = Result.WorldPlanePoint;
	const FVector Normal = Result.WorldPlaneNormal;
	const FMatrix PlaneBasis = FRotationMatrix::MakeFromZ(Normal);
	const FVector PlaneX = PlaneBasis.GetUnitAxis(EAxis::X) * 55.0;
	const FVector PlaneY = PlaneBasis.GetUnitAxis(EAxis::Y) * 55.0;
	DrawDebugLine(World, Center - PlaneX, Center + PlaneX, FColor::Cyan, false, DebugDuration, 0, 2.0f);
	DrawDebugLine(World, Center - PlaneY, Center + PlaneY, FColor::Cyan, false, DebugDuration, 0, 2.0f);
	DrawDebugDirectionalArrow(World, Center, Center + Normal * 75.0, 16.0f,
		FColor::Blue, false, DebugDuration, 0, 2.0f);
}

