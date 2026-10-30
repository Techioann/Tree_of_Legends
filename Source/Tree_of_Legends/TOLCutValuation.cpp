#include "TOLCutValuation.h"
#include "ProceduralMeshComponent.h"

namespace
{
	struct FValueEdge { FVector A,B; int32 Count=0, Balance=0; };
	FIntVector Key(const FVector& P) { return FIntVector(FMath::RoundToInt(P.X*100),FMath::RoundToInt(P.Y*100),FMath::RoundToInt(P.Z*100)); }
	void AddEdge(TMap<FString,FValueEdge>& Edges,FVector A,FVector B,int32 Count=1,int32 Balance=1)
	{
		FString X=Key(A).ToString(),Y=Key(B).ToString();
		if (X==Y) return;
		if (X>Y) { Swap(X,Y); Swap(A,B); Balance=-Balance; }
		FValueEdge& E=Edges.FindOrAdd(X+TEXT("|")+Y); E.A=A; E.B=B; E.Count+=Count; E.Balance+=Balance;
	}
}

bool TOLCutValuation::Measure(const TArray<FProcMeshSection>& Sections,const FTransform& Transform,
	double& OutCubicMeters,FString& OutError)
{
	OutCubicMeters=0; OutError.Empty();
	auto Reject=[&](const TCHAR* Why){OutError=Why; return false;};
	const FVector Scale=Transform.GetScale3D();
	const double Determinant=FMath::Abs(Scale.X*Scale.Y*Scale.Z);
	if (Transform.ContainsNaN() || !FMath::IsFinite(Determinant) || Determinant<=1.e-12)
		return Reject(TEXT("Volume requires a finite, nonzero mesh scale."));
	TMap<FString,FValueEdge> Edges;
	TArray<double> Contributions;
	TArray<TArray<FIntVector>> TriangleKeys;
	TMap<FIntVector,TArray<int32>> Adjacency;
	FVector Origin=FVector::ZeroVector; bool HasOrigin=false;
	for (const FProcMeshSection& S : Sections)
	{
		if (S.ProcIndexBuffer.Num()%3) return Reject(TEXT("Incomplete triangle buffer."));
		for (int32 T=0; T<S.ProcIndexBuffer.Num(); T+=3)
		{
			FVector P[3];
			for (int32 C=0; C<3; ++C)
			{
				const uint32 I=S.ProcIndexBuffer[T+C];
				if (I>=uint32(S.ProcVertexBuffer.Num())) return Reject(TEXT("Invalid triangle index."));
				P[C]=FVector(S.ProcVertexBuffer[I].Position);
				if (P[C].ContainsNaN() || P[C].GetAbsMax()>20000000)
					return Reject(TEXT("Non-finite or unsupported local vertex range."));
			}
			if (FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).SizeSquared()<1.e-16) continue;
			if (!HasOrigin) { Origin=P[0]; HasOrigin=true; }
			const int32 Id=Contributions.Add(FVector::DotProduct(P[0]-Origin,
				FVector::CrossProduct(P[1]-Origin,P[2]-Origin))/6.0);
			TArray<FIntVector> Keys;
			for (int32 C=0; C<3; ++C)
			{
				if (Key(P[C])==Key(P[(C+1)%3])) return Reject(TEXT("Geometry is below valuation weld precision."));
				Keys.Add(Key(P[C])); Adjacency.FindOrAdd(Keys.Last()).Add(Id);
				AddEdge(Edges,P[C],P[(C+1)%3]);
			}
			TriangleKeys.Add(MoveTemp(Keys));
		}
	}
	if (Contributions.Num()<4) return Reject(TEXT("No closed solid to measure."));
	TArray<FValueEdge> Unmatched;
	TMap<FIntVector,FVector> Endpoints;
	for (const auto& Pair : Edges)
	{
		const FValueEdge& E=Pair.Value;
		if (E.Count==2 && E.Balance==0) continue;
		if (E.Count!=1) return Reject(TEXT("Nonmanifold or inconsistently wound mesh."));
		Unmatched.Add(E); Endpoints.Add(Key(E.A),E.A); Endpoints.Add(Key(E.B),E.B);
	}
	// The engine's cap triangulator removes collinear vertices. Validate closure
	// after splitting unmatched edges at those T-junctions, without changing the mesh.
	TMap<FString,FValueEdge> Split;
	for (const FValueEdge& E : Unmatched)
	{
		struct FCut { double T; FVector P; };
		TArray<FCut> Cuts; Cuts.Add({0,E.A}); Cuts.Add({1,E.B});
		const FVector Direction=E.B-E.A; const double Length2=Direction.SizeSquared();
		for (const auto& Point : Endpoints)
		{
			const double T=FVector::DotProduct(Point.Value-E.A,Direction)/Length2;
			if (T>1.e-6 && T<1-1.e-6 && FVector::DistSquared(Point.Value,E.A+Direction*T)<0.0009)
				Cuts.Add({T,Point.Value});
		}
		Cuts.Sort([](const FCut& A,const FCut& B){return A.T<B.T;});
		for (int32 I=1; I<Cuts.Num(); ++I) AddEdge(Split,Cuts[I-1].P,Cuts[I].P,E.Count,E.Balance);
	}
	for (const auto& Pair : Split)
		if (Pair.Value.Count!=2 || Pair.Value.Balance!=0) return Reject(TEXT("Open or inconsistently wound surface; value unchanged."));
	TSet<int32> Seen; double LocalVolume=0;
	for (int32 Seed=0; Seed<Contributions.Num(); ++Seed)
	{
		if (Seen.Contains(Seed)) continue;
		TArray<int32> Queue; Queue.Add(Seed); Seen.Add(Seed); double SignedVolume=0;
		for (int32 I=0; I<Queue.Num(); ++I)
		{
			SignedVolume+=Contributions[Queue[I]];
			for (const FIntVector& V : TriangleKeys[Queue[I]])
				for (int32 N : Adjacency[V]) if (!Seen.Contains(N)) { Seen.Add(N); Queue.Add(N); }
		}
		if (FMath::Abs(SignedVolume)<1.e-9) return Reject(TEXT("Degenerate closed component."));
		LocalVolume+=FMath::Abs(SignedVolume);
	}
	const double Volume=LocalVolume*Determinant/1000000.0; // cm^3 -> m^3
	if (!FMath::IsFinite(Volume) || Volume<=1.e-12) return Reject(TEXT("Invalid solid volume."));
	OutCubicMeters=Volume;
	return true;
}
