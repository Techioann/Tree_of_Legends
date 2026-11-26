#include "TOLTreeLeafSubsystem.h"
#include "TOLCutMeshData.h"
#include "TOLCutSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"

const FName UTOLTreeLeafSubsystem::LeafTag(TEXT("TOLLeaf"));
const FName UTOLTreeLeafSubsystem::PreviewTag(TEXT("TOLLeafPreview"));

namespace
{
	void ConfigureLeaf(UStaticMeshComponent* Leaf, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		Leaf->SetStaticMesh(Mesh);
		if (Material) Leaf->SetMaterial(0,Material);
		// Purely visual: no blocking, no overlaps, no traces (axe/grab), no welded mass, no navmesh.
		Leaf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Leaf->SetCollisionResponseToAllChannels(ECR_Ignore);
		Leaf->SetGenerateOverlapEvents(false);
		Leaf->SetCanEverAffectNavigation(false);
		Leaf->bReceivesDecals=false;
		Leaf->ComponentTags.AddUnique(UTOLTreeLeafSubsystem::LeafTag);
	}

	void Attach(UStaticMeshComponent* Leaf, USceneComponent* Parent, const FTransform& Relative)
	{
		Leaf->SetupAttachment(Parent);
		Leaf->SetRelativeTransform(Relative);
		Leaf->RegisterComponent();
		Parent->GetOwner()->AddInstanceComponent(Leaf);
	}
}

bool UTOLTreeLeafSubsystem::ResolveAssets()
{
	const UTOLCutSettings* Settings=GetDefault<UTOLCutSettings>();
	if (!Settings->bEnableLeaves) return false;
	if (!bAssetsResolved)
	{
		bAssetsResolved=true;
		LeafMesh=Settings->LeafMesh.LoadSynchronous();
		LeafMaterial=Settings->LeafMaterial.LoadSynchronous();
		if (!LeafMesh)
			UE_LOG(LogTemp, Warning, TEXT("Leaves: LeafMesh %s not found; leaves disabled. Project Settings > Game > Tree of Legends Cutting."),
				*Settings->LeafMesh.ToString());
	}
	return LeafMesh!=nullptr;
}

const TArray<FTransform>& UTOLTreeLeafSubsystem::GetAnchors(const FTOLTreeMeshConfiguration& Model)
{
	static const TArray<FTransform> None;
	UStaticMesh* Mesh=Model.Mesh.Get();
	if (!Mesh || !ResolveAssets()) return None;
	if (const TArray<FTransform>* Cached=AnchorCache.Find(Mesh)) return *Cached;

	const UTOLCutSettings* Settings=GetDefault<UTOLCutSettings>();
	FVector Axis=Model.TrunkAxisLocal.GetSafeNormal();
	if (Axis.IsNearlyZero()) Axis=FVector::UpVector;
	const FVector Size=Mesh->GetBoundingBox().GetSize();
	const double Height=FMath::Max(FMath::Abs(Axis.X)*Size.X+FMath::Abs(Axis.Y)*Size.Y+FMath::Abs(Axis.Z)*Size.Z,1.0);
	const double LeafSize=FMath::Max(LeafMesh->GetBoundingBox().GetSize().GetMax(),1.0);
	const double Radius=Height*FMath::Clamp<double>(Settings->LeafRadiusFraction,0.01,1.0);
	const FVector BaseScale(2.0*Radius/LeafSize);

	TArray<FTransform> Anchors;
	const TCHAR* How=TEXT("sockets");
	// 1. Hand-placed sockets win.
	for (const UStaticMeshSocket* Socket : Mesh->Sockets)
	{
		if (!Socket || !Socket->SocketName.ToString().StartsWith(Settings->LeafSocketPrefix)) continue;
		Anchors.Emplace(Socket->RelativeRotation.Quaternion(),Socket->RelativeLocation,BaseScale*Socket->RelativeScale);
	}
	// 2. Otherwise branch tips: segment endpoints far from every other segment, except the root.
	if (Anchors.IsEmpty())
	{
		How=TEXT("branch tips");
		const TArray<FTOLCutSegment> Segments=UTOLCutMeshData::ComputeModelSegments(Mesh,Model);
		const double JoinDistance=Height*0.08;
		FVector Root=FVector::ZeroVector, Top=FVector::ZeroVector, TopDir=Axis;
		double RootDot=TNumericLimits<double>::Max(), TopDot=TNumericLimits<double>::Lowest();
		for (const FTOLCutSegment& S : Segments)
			for (int32 E=0; E<2; ++E)
			{
				const FVector P=E ? S.End : S.Start, Other=E ? S.Start : S.End;
				const double Dot=FVector::DotProduct(P,Axis);
				if (Dot<RootDot) { RootDot=Dot; Root=P; }
				if (Dot>TopDot) { TopDot=Dot; Top=P; TopDir=(P-Other).GetSafeNormal(); }
			}
		TArray<FVector> Tips;
		for (int32 I=0; I<Segments.Num(); ++I)
			for (int32 E=0; E<2; ++E)
			{
				const FVector P=E ? Segments[I].End : Segments[I].Start;
				const FVector Other=E ? Segments[I].Start : Segments[I].End;
				if (P.Equals(Root,0.01)) continue;
				double Nearest=TNumericLimits<double>::Max();
				for (int32 J=0; J<Segments.Num(); ++J)
					if (J!=I) Nearest=FMath::Min(Nearest,FMath::PointDistToSegment(P,Segments[J].Start,Segments[J].End));
				if (Nearest<=JoinDistance || Tips.ContainsByPredicate([&](const FVector& T){ return FVector::Dist(T,P)<=JoinDistance; }))
					continue;
				Tips.Add(P);
				// Push the ball slightly past the tip so it caps the branch end.
				Anchors.Emplace(FQuat::Identity,P+(P-Other).GetSafeNormal()*Radius*0.4,BaseScale);
			}
		if (Anchors.IsEmpty() && !Segments.IsEmpty())
			Anchors.Emplace(FQuat::Identity,Top+TopDir*Radius*0.4,BaseScale);
	}
	UE_LOG(LogTemp, Display, TEXT("Leaves: %s -> %d leaves (%s), radius %.0f local cm."),*Mesh->GetName(),Anchors.Num(),How,Radius);
	return AnchorCache.Add(Mesh,MoveTemp(Anchors));
}

void UTOLTreeLeafSubsystem::InvalidateAnchors(const UStaticMesh* Mesh)
{
	if (Mesh) { AnchorCache.Remove(Mesh); return; }
	AnchorCache.Empty();
	LeafMesh=nullptr; LeafMaterial=nullptr; bAssetsResolved=false;
}

void UTOLTreeLeafSubsystem::DestroyPreviewLeaves(AActor* Owner)
{
	if (!Owner) return;
	TInlineComponentArray<UInstancedStaticMeshComponent*> Components(Owner);
	for (UInstancedStaticMeshComponent* Component : Components)
		if (Component->ComponentHasTag(PreviewTag)) Component->DestroyComponent();
}

void UTOLTreeLeafSubsystem::AddDormantLeaves(UStaticMeshComponent* Source, const FTOLTreeMeshConfiguration& Model)
{
	if (!IsValid(Source) || !Source->GetOwner() || DormantLeaves.Contains(Source)) return;
	DestroyPreviewLeaves(Source->GetOwner());
	if (UInstancedStaticMeshComponent* Leaves=CreateLeafInstances(Source,Model,false)) DormantLeaves.Add(Source,Leaves);
}

UInstancedStaticMeshComponent* UTOLTreeLeafSubsystem::CreateLeafInstances(UStaticMeshComponent* Source,
	const FTOLTreeMeshConfiguration& Model, bool bEditorPreview)
{
	if (!IsValid(Source) || !Source->GetOwner()) return nullptr;
	const TArray<FTransform>& Anchors=GetAnchors(Model);
	if (Anchors.IsEmpty()) return nullptr;
	const EObjectFlags Flags=bEditorPreview ? RF_Transient|RF_DuplicateTransient|RF_TextExportTransient : RF_Transient;
	UInstancedStaticMeshComponent* Leaves=NewObject<UInstancedStaticMeshComponent>(Source->GetOwner(),NAME_None,Flags);
	Leaves->SetMobility(Source->Mobility);
	ConfigureLeaf(Leaves,LeafMesh,LeafMaterial);
	if (bEditorPreview)
	{
		// Unattached on purpose: attaching/detaching calls Modify() on the tree and would dirty the level.
		// The preview tracks the tree's transform and rebuilds when it changes.
		Leaves->ComponentTags.Add(PreviewTag);
		Leaves->bIsEditorOnly=true;
		Leaves->bSelectable=false;
		Leaves->SetWorldTransform(Source->GetComponentTransform());
		Leaves->RegisterComponent();
	}
	// Attached: a Static Mesh Actor's SetVisibility(false, true) on activation hides its leaves too.
	else Attach(Leaves,Source,FTransform::Identity);
	if (UInstancedStaticMeshComponent* Instances=Cast<UInstancedStaticMeshComponent>(Source))
	{
		TArray<FTransform> All;
		All.Reserve(Instances->GetInstanceCount()*Anchors.Num());
		for (int32 I=0; I<Instances->GetInstanceCount(); ++I)
		{
			FTransform Instance;
			if (!Instances->GetInstanceTransform(I,Instance,/*bWorldSpace=*/false)) continue;
			for (const FTransform& Anchor : Anchors) All.Add(Anchor*Instance);
		}
		Leaves->AddInstances(All,false);
	}
	else Leaves->AddInstances(Anchors,false);
	return Leaves;
}

void UTOLTreeLeafSubsystem::RemoveDormantInstanceLeaves(UInstancedStaticMeshComponent* Source,
	const FTransform& InstanceWorld, const FTOLTreeMeshConfiguration& Model)
{
	const TWeakObjectPtr<UInstancedStaticMeshComponent>* Found=DormantLeaves.Find(Source);
	UInstancedStaticMeshComponent* Leaves=Found ? Found->Get() : nullptr;
	if (!Leaves) return;
	TArray<int32> Remove;
	for (const FTransform& Anchor : GetAnchors(Model))
	{
		const FVector Expected=(Anchor*InstanceWorld).GetLocation();
		for (int32 I=0; I<Leaves->GetInstanceCount(); ++I)
		{
			FTransform Leaf;
			if (Remove.Contains(I) || !Leaves->GetInstanceTransform(I,Leaf,/*bWorldSpace=*/true)) continue;
			if (Leaf.GetLocation().Equals(Expected,1.0)) { Remove.Add(I); break; }
		}
	}
	if (!Remove.IsEmpty()) Leaves->RemoveInstances(Remove);
}

void UTOLTreeLeafSubsystem::SpawnLeaves(UPrimitiveComponent* Parent, const FTOLTreeMeshConfiguration& Model)
{
	if (!IsValid(Parent) || !Parent->GetOwner()) return;
	for (const FTransform& Anchor : GetAnchors(Model))
	{
		UStaticMeshComponent* Leaf=NewObject<UStaticMeshComponent>(Parent->GetOwner(),NAME_None,RF_Transient);
		Leaf->SetMobility(EComponentMobility::Movable);
		ConfigureLeaf(Leaf,LeafMesh,LeafMaterial);
		Attach(Leaf,Parent,Anchor);
	}
}

int32 UTOLTreeLeafSubsystem::TransferLeaves(UPrimitiveComponent* From, UPrimitiveComponent* To,
	TFunctionRef<bool(const FVector& LocalLocation)> Predicate)
{
	if (!IsValid(From) || !IsValid(To) || !To->GetOwner()) return 0;
	const FTransform FromTransform=From->GetComponentTransform();
	const FTransform ToTransform=To->GetComponentTransform();
	int32 Moved=0;
	const TArray<TObjectPtr<USceneComponent>> Children=From->GetAttachChildren();
	for (USceneComponent* Child : Children)
	{
		UStaticMeshComponent* Leaf=Cast<UStaticMeshComponent>(Child);
		if (!IsValid(Leaf) || Leaf->IsA<UInstancedStaticMeshComponent>() || !Leaf->ComponentHasTag(LeafTag)) continue;
		if (!Predicate(FromTransform.InverseTransformPosition(Leaf->GetComponentLocation()))) continue;
		// Re-created on the new owner: a component must belong to the actor whose lifetime it shares.
		UStaticMeshComponent* Copy=NewObject<UStaticMeshComponent>(To->GetOwner(),NAME_None,RF_Transient);
		Copy->SetMobility(EComponentMobility::Movable);
		ConfigureLeaf(Copy,Leaf->GetStaticMesh(),Leaf->GetMaterial(0));
		Attach(Copy,To,Leaf->GetComponentTransform().GetRelativeTransform(ToTransform));
		Leaf->DestroyComponent();
		++Moved;
	}
	return Moved;
}

void UTOLTreeLeafSubsystem::Deinitialize()
{
	AnchorCache.Empty(); DormantLeaves.Empty();
	LeafMesh=nullptr; LeafMaterial=nullptr; bAssetsResolved=false;
	Super::Deinitialize();
}
