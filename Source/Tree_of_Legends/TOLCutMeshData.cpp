#include "TOLCutMeshData.h"
#include "ProceduralMeshComponent.h"

bool UTOLCutMeshData::ConfigureMaterial(FDataTableRowHandle MaterialRow, double Resistance)
{
	if (!ActiveCuts.IsEmpty() || !MaterialRow.DataTable || MaterialRow.RowName.IsNone() ||
		!MaterialRow.DataTable->GetRowMap().Contains(MaterialRow.RowName) ||
		!FMath::IsFinite(Resistance) || Resistance<=0.0) return false;
	TreeData=MaterialRow; ResistanceMultiplier=Resistance;
	return true;
}

void UTOLCutMeshData::CancelAllCuts()
{
	ActiveCuts.Empty();
	if (CutMarks) CutMarks->ClearAllMeshSections();
}

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "KismetProceduralMeshLibrary.h"

namespace
{
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


struct FMeshEdge { FVector A, B; TArray<FVector> Normals; };
}

TArray<FTOLCutSegment> UTOLCutMeshData::DetectSegments(UProceduralMeshComponent* Mesh)
{
		// Use only original geometry. Never re-estimate an axis from a shortened log.
		TArray<FMeshEdge> Edges;
		TMap<FString, int32> EdgeMap;
		for (int32 S = 0; S < Mesh->GetNumSections(); ++S)
		{
			const FProcMeshSection* Section = Mesh->GetProcMeshSection(S);
			if (!Section) continue;
			for (int32 T = 0; T < Section->ProcIndexBuffer.Num() / 3; ++T)
			{
				FVector A, B, C;
				if (!GetTriangle(*Section, T, A, B, C)) continue;
				const FVector Normal = FVector::CrossProduct(B-A, C-A).GetSafeNormal();
				const FVector Points[] = {A, B, C};
				for (int32 E = 0; E < 3; ++E)
				{
					FVector P = Points[E], Q = Points[(E+1)%3];
					FString K1 = WeldKey(P).ToString(), K2 = WeldKey(Q).ToString();
					if (K1 > K2) { Swap(K1, K2); Swap(P, Q); }
					const FString Key = K1 + TEXT("|") + K2;
					int32* Index = EdgeMap.Find(Key);
					if (!Index)
					{
						FMeshEdge Edge; Edge.A=P; Edge.B=Q;
						Index = &EdgeMap.Add(Key, Edges.Add(MoveTemp(Edge)));
					}
					Edges[*Index].Normals.Add(Normal);
				}
			}
		}
		struct FRails { FVector Axis; TArray<FMeshEdge> Edges; };
		TArray<FRails> Groups;
		for (const FMeshEdge& Edge : Edges)
		{
			// Ignore triangulation diagonals on planar faces.
			if (Edge.Normals.Num() != 2 ||
				FMath::Abs(FVector::DotProduct(Edge.Normals[0], Edge.Normals[1])) > 0.9999) continue;
			FVector Axis = (Edge.B-Edge.A).GetSafeNormal();
			if (Axis.IsNearlyZero()) continue;
			if (Axis.Z < 0.0 || (FMath::IsNearlyZero(Axis.Z) && Axis.X < 0.0)) Axis = -Axis;
			FRails* Group = Groups.FindByPredicate([&](const FRails& G)
			{ return FMath::Abs(FVector::DotProduct(G.Axis, Axis)) > 0.99999; });
			if (!Group) { FRails G; G.Axis=Axis; Group=&Groups[Groups.Add(MoveTemp(G))]; }
			Group->Edges.Add(Edge);
		}
		TArray<FTOLCutSegment> Segments;
		for (const FRails& Group : Groups)
		{
			if (Group.Edges.Num() < 4) continue;
			FVector Start=FVector::ZeroVector, End=FVector::ZeroVector;
			for (const FMeshEdge& Edge : Group.Edges)
			{
				const bool Forward = FVector::DotProduct(Edge.B-Edge.A, Group.Axis) > 0.0;
				Start += Forward ? Edge.A : Edge.B;
				End += Forward ? Edge.B : Edge.A;
			}
			Start /= Group.Edges.Num(); End /= Group.Edges.Num();
			const FVector Center=(Start+End)*0.5;
			double Radius=0.0;
			for (const FMeshEdge& Edge : Group.Edges)
			{
				for (const FVector& P : {Edge.A, Edge.B})
				{
					const FVector Offset=P-Center;
					Radius=FMath::Max(Radius, (Offset-Group.Axis*FVector::DotProduct(Offset,Group.Axis)).Size());
				}
			}
			// Four long parallel sharp rails define a straight branch. Cross-section
			// edges and incidental diagonals fail this elongation check.
			if ((End-Start).Size() <= 4.0*Radius) continue;
			FTOLCutSegment Segment; Segment.Start=Start; Segment.End=End;
			Segments.Add(Segment);
		}

return Segments;
}

uint32 UTOLCutMeshData::ConfigurationHash() const
{
	uint32 Hash = HashCombine(GetTypeHash(Model.Mesh.Get()), GetTypeHash(static_cast<uint8>(Model.SegmentMode)));
	Hash = HashCombine(Hash, GetTypeHash(Model.TrunkAxisLocal));
	for (const TArray<FTOLCutSegment>* List : {&Model.AuthoredSegments, &Segments})
	{
		Hash = HashCombine(Hash, GetTypeHash(List->Num()));
		for (const FTOLCutSegment& Segment : *List)
		{
			Hash = HashCombine(Hash, GetTypeHash(Segment.Start));
			Hash = HashCombine(Hash, GetTypeHash(Segment.End));
		}
	}
	return Hash;
}

bool UTOLCutMeshData::ValidateSegments(const TArray<FTOLCutSegment>& InSegments,
	UProceduralMeshComponent* Mesh, FString& OutError)
{
	OutError.Empty();
	FBox Bounds(ForceInit);
	int32 TriangleCount = 0;
	for (int32 S=0; S<Mesh->GetNumSections(); ++S)
	{
		const FProcMeshSection* Section=Mesh->GetProcMeshSection(S);
		if (!Section) continue;
		if (Section->ProcIndexBuffer.Num()%3 != 0)
		{ OutError=TEXT("Mesh has an incomplete triangle index buffer."); return false; }
		for (const uint32 Index : Section->ProcIndexBuffer)
		{
			if (Index>=static_cast<uint32>(Section->ProcVertexBuffer.Num()))
			{ OutError=TEXT("Mesh has an invalid vertex index."); return false; }
			const FVector P(Section->ProcVertexBuffer[Index].Position);
			if (P.ContainsNaN()) { OutError=TEXT("Mesh contains non-finite vertices."); return false; }
			Bounds+=P;
		}
		TriangleCount+=Section->ProcIndexBuffer.Num()/3;
	}
	if (TriangleCount<4 || !Bounds.IsValid)
	{ OutError=TEXT("LOD 0 has no usable solid geometry."); return false; }
	if (InSegments.IsEmpty())
	{
		OutError=TEXT("No straight segments detected. Select Authored mode and supply local centerlines for this model.");
		return false;
	}
	const FBox Allowed=Bounds.ExpandBy(FMath::Max(1.0, Bounds.GetSize().GetMax()*0.01));
	for (int32 I=0; I<InSegments.Num(); ++I)
	{
		const FTOLCutSegment& Segment=InSegments[I];
		if (Segment.Start.ContainsNaN() || Segment.End.ContainsNaN() ||
			FVector::DistSquared(Segment.Start,Segment.End)<=0.01)
		{ OutError=FString::Printf(TEXT("Segment %d is non-finite or shorter than 0.1 local cm."),I); return false; }
		if (!Allowed.IsInsideOrOn(Segment.Start) || !Allowed.IsInsideOrOn(Segment.End))
		{ OutError=FString::Printf(TEXT("Segment %d lies outside this model's local bounds."),I); return false; }
		for (int32 J=0; J<I; ++J)
		{
			const FTOLCutSegment& Other=InSegments[J];
			if ((Segment.Start.Equals(Other.Start,0.01) && Segment.End.Equals(Other.End,0.01)) ||
				(Segment.Start.Equals(Other.End,0.01) && Segment.End.Equals(Other.Start,0.01)))
			{ OutError=FString::Printf(TEXT("Segments %d and %d are duplicates."),J,I); return false; }
		}
	}
	return true;
}

namespace
{
	UProceduralMeshComponent* MakeModelPreview(UObject* Outer, UStaticMesh* Mesh)
	{
		UProceduralMeshComponent* Preview=NewObject<UProceduralMeshComponent>(Outer);
		Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		for (int32 S=0; S<Mesh->GetNumSections(0); ++S)
		{
			TArray<FVector> Vertices, Normals; TArray<int32> Triangles;
			TArray<FVector2D> UVs; TArray<FProcMeshTangent> Tangents; TArray<FLinearColor> Colors;
			UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Mesh,0,S,Vertices,Triangles,Normals,UVs,Tangents);
			Preview->CreateMeshSection_LinearColor(S,Vertices,Triangles,Normals,UVs,Colors,Tangents,false);
		}
		return Preview;
	}
}

bool UTOLCutMeshData::ValidateTreeConfiguration(UStaticMeshComponent* SourceTree, FString& OutError) const
{
	OutError.Empty();
	if (!SourceTree || SourceTree->GetOwner()!=GetOwner())
	{ OutError=TEXT("SourceTree must belong to the same actor as TOLCutMeshData."); return false; }
	UStaticMesh* Selected=Model.Mesh ? Model.Mesh.Get() : static_cast<UStaticMesh*>(SourceTree->GetStaticMesh());
	if (!Selected || !Selected->GetRenderData() || Selected->GetNumLODs()<1 || Selected->GetNumSections(0)<1)
	{ OutError=TEXT("The selected Static Mesh has no readable LOD 0."); return false; }
	if (!Selected->bAllowCPUAccess)
	{ OutError=TEXT("Enable Allow CPU Access on the selected model before using runtime cutting."); return false; }
	if (Model.TrunkAxisLocal.ContainsNaN() || Model.TrunkAxisLocal.IsNearlyZero())
	{ OutError=TEXT("The model's local trunk axis must be finite and nonzero."); return false; }
	if (!FMath::IsFinite(ResistanceMultiplier) || ResistanceMultiplier<=0.0)
	{ OutError=TEXT("ResistanceMultiplier must be finite and positive."); return false; }
	if ((TreeData.DataTable || !TreeData.RowName.IsNone()) &&
		(!TreeData.DataTable || TreeData.RowName.IsNone() || !TreeData.DataTable->GetRowMap().Contains(TreeData.RowName)))
	{ OutError=TEXT("TreeData must reference an existing row, or be completely empty for the legacy default."); return false; }
	if (Model.SegmentMode!=ETOLCutSegmentMode::Automatic && Model.SegmentMode!=ETOLCutSegmentMode::Authored)
	{ OutError=TEXT("Unknown segment mode."); return false; }
	UProceduralMeshComponent* Preview=MakeModelPreview(GetTransientPackage(),Selected);
	const TArray<FTOLCutSegment> Candidate=Model.SegmentMode==ETOLCutSegmentMode::Authored
		? Model.AuthoredSegments : DetectSegments(Preview);
	return ValidateSegments(Candidate,Preview,OutError);
}

bool UTOLCutMeshData::InitializeTree(UStaticMeshComponent* SourceTree,
	UProceduralMeshComponent* ProceduralTree, FString& OutError)
{
	OutError.Empty();
	auto Reject=[&](const FString& Error) { OutError=Error; ValidationMessage=Error; return false; };
	if (!SourceTree || !ProceduralTree || SourceTree->GetOwner()!=GetOwner() || ProceduralTree->GetOwner()!=GetOwner())
		return Reject(TEXT("Both tree mesh components must belong to TOLCutMeshData's actor."));
	if (bFragment || bHasBeenChopped || bGeometryModified || !ActiveCuts.IsEmpty())
		return Reject(TEXT("Model reinitialization is blocked after a chop or slice, including cancelled marks."));
	if (!ValidateTreeConfiguration(SourceTree,OutError)) return Reject(OutError);
	const FTransform SourceTransform=SourceTree->GetComponentTransform();
	const FVector Scale=SourceTransform.GetScale3D();
	if (SourceTransform.ContainsNaN() || FMath::IsNearlyZero(Scale.X) ||
		FMath::IsNearlyZero(Scale.Y) || FMath::IsNearlyZero(Scale.Z))
		return Reject(TEXT("SourceTree has a non-finite or zero-scale transform."));
	UStaticMesh* Selected=Model.Mesh ? Model.Mesh.Get() : static_cast<UStaticMesh*>(SourceTree->GetStaticMesh());
	UProceduralMeshComponent* Preview=MakeModelPreview(GetTransientPackage(),Selected);
	TArray<FTOLCutSegment> Candidate=Model.SegmentMode==ETOLCutSegmentMode::Authored
		? Model.AuthoredSegments : DetectSegments(Preview);
	// All checks finish before changing source, collision, materials or geometry.
	SourceTree->SetStaticMesh(Selected);
	ProceduralTree->SetWorldTransform(SourceTransform);
	ProceduralTree->ClearAllMeshSections();
	ProceduralTree->EmptyOverrideMaterials();
	ProceduralTree->bUseComplexAsSimpleCollision=true;
	UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(SourceTree,0,ProceduralTree,true);
	Segments=MoveTemp(Candidate);
	BoundSource=SourceTree; BoundSourceMesh=Selected; BoundMesh=ProceduralTree;
	bModelConfigured=true;
	BoundConfigurationHash=ConfigurationHash();
	bSegmentsValid=true;
	ValidationMessage=FString::Printf(TEXT("%s: %d structurally valid segments; branch coverage requires visual verification."),
		*Selected->GetName(),Segments.Num());
	SourceTree->SetVisibility(false);
	SourceTree->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProceduralTree->SetVisibility(true);
	ProceduralTree->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	return true;
}

bool UTOLCutMeshData::CheckBinding(UProceduralMeshComponent* Mesh)
{
	if (BoundMesh!=Mesh || BoundConfigurationHash!=ConfigurationHash() ||
		(!bFragment && BoundSource && (BoundSource->GetStaticMesh()!=BoundSourceMesh ||
		!BoundSource->GetComponentTransform().Equals(Mesh->GetComponentTransform(),0.01))))
	{
		bSegmentsValid=false;
		ValidationMessage=TEXT("Model, component, transform or segment configuration changed; initialize an uncut tree explicitly.");
		return false;
	}
	bSegmentsValid=true;
	return true;
}

bool UTOLCutMeshData::ConfigureModel(const FTOLTreeMeshConfiguration& Configuration,
	UStaticMeshComponent* SourceTree, UProceduralMeshComponent* ProceduralTree, FString& OutError)
{
	const FTOLTreeMeshConfiguration Previous=Model;
	Model=Configuration;
	if (InitializeTree(SourceTree,ProceduralTree,OutError)) return true;
	Model=Previous;
	return false;
}

bool UTOLCutMeshData::EnsureSegments(UProceduralMeshComponent* Mesh)
{
	if (!Mesh || Mesh->GetOwner()!=GetOwner()) return false;
	if (BoundMesh) return CheckBinding(Mesh);
	// Legacy actors remain supported. Explicit configured actors use InitializeTree first.
	if (Model.Mesh || Model.SegmentMode==ETOLCutSegmentMode::Authored)
	{
		ValidationMessage=TEXT("Configured models must call InitializeTree before tracing."); bSegmentsValid=false; return false;
	}
	TArray<FTOLCutSegment> Candidate=Segments.IsEmpty() ? DetectSegments(Mesh) : Segments;
	if (!ValidateSegments(Candidate,Mesh,ValidationMessage)) { bSegmentsValid=false; return false; }
	Segments=MoveTemp(Candidate); BoundMesh=Mesh;
	BoundSource=GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	BoundSourceMesh=BoundSource ? BoundSource->GetStaticMesh() : nullptr;
	BoundConfigurationHash=ConfigurationHash();
	bSegmentsValid=true;
	ValidationMessage=TEXT("Legacy segment binding validated; branch coverage requires visual verification.");
	return CheckBinding(Mesh);
}

void UTOLCutMeshData::InheritBinding(const UTOLCutMeshData* Parent, UProceduralMeshComponent* FragmentMesh)
{
	Model=Parent->Model;
	bModelConfigured=Parent->bModelConfigured;
	BoundSourceMesh=Parent->BoundSourceMesh;
	BoundSource=nullptr; BoundMesh=FragmentMesh; bFragment=true;
	MarkGeometryModified();
	BoundConfigurationHash=ConfigurationHash(); bSegmentsValid=true;
	ValidationMessage=TEXT("Validated segment binding inherited from the source tree.");
}

FVector UTOLCutMeshData::GetTrunkAxis(const FVector& LegacyAxis) const
{
	return bModelConfigured ? Model.TrunkAxisLocal.GetSafeNormal() : LegacyAxis.GetSafeNormal();
}

