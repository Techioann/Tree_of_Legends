#include "TOLLeafEditorPreview.h"
#include "TOLTreeLeafSubsystem.h"
#include "TOLCutTraceComponent.h"
#include "TOLCutSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Crc.h"

namespace
{
	uint32 HashTransform(uint32 Hash, const FTransform& T)
	{
		const FVector L=T.GetLocation(), S=T.GetScale3D();
		const FQuat Q=T.GetRotation();
		const double Values[]={L.X,L.Y,L.Z,S.X,S.Y,S.Z,Q.X,Q.Y,Q.Z,Q.W};
		return FCrc::MemCrc32(Values,sizeof(Values),Hash);
	}

	uint32 HashSettings()
	{
		const UTOLCutSettings* S=GetDefault<UTOLCutSettings>();
		uint32 Hash=GetTypeHash(S->bEnableLeaves);
		Hash=HashCombine(Hash,GetTypeHash(S->LeafMesh.ToString()));
		Hash=HashCombine(Hash,GetTypeHash(S->LeafMaterial.ToString()));
		Hash=HashCombine(Hash,GetTypeHash(S->LeafRadiusFraction));
		return HashCombine(Hash,GetTypeHash(S->LeafSocketPrefix));
	}

	uint32 HashSockets(const UStaticMesh* Mesh)
	{
		uint32 Hash=GetTypeHash(Mesh->Sockets.Num());
		for (const UStaticMeshSocket* Socket : Mesh->Sockets)
		{
			if (!Socket) continue;
			Hash=HashCombine(Hash,GetTypeHash(Socket->SocketName));
			Hash=HashTransform(Hash,FTransform(Socket->RelativeRotation,Socket->RelativeLocation,Socket->RelativeScale));
		}
		return Hash;
	}
}

bool UTOLLeafEditorPreview::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return GIsEditor && WorldType==EWorldType::Editor;
}

TStatId UTOLLeafEditorPreview::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTOLLeafEditorPreview, STATGROUP_Tickables);
}

void UTOLLeafEditorPreview::Tick(float DeltaTime)
{
	SinceRefresh+=DeltaTime;
	if (SinceRefresh<0.5) return;
	SinceRefresh=0.0;
	Refresh();
}

const FTOLTreeMeshConfiguration* UTOLLeafEditorPreview::FindModel(UStaticMesh* Mesh)
{
	if (!Mesh) return nullptr;
	if (const TOptional<FTOLTreeMeshConfiguration>* Cached=Models.Find(Mesh))
		return Cached->IsSet() ? &Cached->GetValue() : nullptr;
	// Trees can be configured on the player's CutTrace component (B_Character's DormantTreeProfiles), not only
	// in DT_Tree. Make sure the map's pawn Blueprint is loaded, then ask every CutTrace template, the
	// Blueprint ones first and the native default (which only knows DT_Tree) last.
	if (Models.IsEmpty())
		if (const AWorldSettings* Settings=GetWorld()->GetWorldSettings())
			if (const UClass* GameMode=Settings->DefaultGameMode.Get())
				(void)GameMode->GetDefaultObject<AGameModeBase>()->DefaultPawnClass.Get();
	const UTOLCutTraceComponent* Native=GetDefault<UTOLCutTraceComponent>();
	FTOLTreeMeshConfiguration Model;
	bool bFound=false;
	for (TObjectIterator<UTOLCutTraceComponent> It; It && !bFound; ++It)
	{
		if (*It==Native || !It->IsTemplate() || It->GetOutermost()==GetTransientPackage() ||
			It->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists)) continue;
		bFound=It->FindTreeModel(Mesh,Model);
	}
	if (!bFound) bFound=Native->FindTreeModel(Mesh,Model);
	TOptional<FTOLTreeMeshConfiguration>& Entry=Models.Add(Mesh);
	if (bFound) Entry=Model;
	return Entry.IsSet() ? &Entry.GetValue() : nullptr;
}

void UTOLLeafEditorPreview::Refresh()
{
	UWorld* World=GetWorld();
	UTOLTreeLeafSubsystem* Leaves=World ? World->GetSubsystem<UTOLTreeLeafSubsystem>() : nullptr;
	if (!Leaves) return;
	const uint32 Settings=HashSettings();
	if (Settings!=SettingsSignature)
	{
		// Rebuild everything: new size, mesh or material, or leaves switched on/off.
		SettingsSignature=Settings;
		Leaves->InvalidateAnchors();
		Models.Empty(); SocketHashes.Empty();
	}
	if (!GetDefault<UTOLCutSettings>()->bEnableLeaves) { ClearAll(); return; }

	TSet<UStaticMeshComponent*> Seen;
	auto Visit=[&](UStaticMeshComponent* Source)
	{
		if (!Source || !Source->IsRegistered() || !Source->IsVisibleInEditor() || Source->ComponentHasTag(UTOLTreeLeafSubsystem::LeafTag))
			return;
		UStaticMesh* Mesh=Source->GetStaticMesh();
		const FTOLTreeMeshConfiguration* Model=FindModel(Mesh);
		if (!Model) return;
		Seen.Add(Source);
		// Hand-placed "Leaf*" sockets edited in the Static Mesh Editor show up without reopening the map.
		const uint32 Sockets=HashSockets(Mesh);
		if (const uint32* Previous=SocketHashes.Find(Mesh); Previous && *Previous!=Sockets) Leaves->InvalidateAnchors(Mesh);
		SocketHashes.Add(Mesh,Sockets);

		uint32 Signature=HashCombine(Settings,HashCombine(GetTypeHash(Mesh),Sockets));
		Signature=HashTransform(Signature,Source->GetComponentTransform());
		if (const UInstancedStaticMeshComponent* Instances=Cast<UInstancedStaticMeshComponent>(Source))
		{
			Signature=HashCombine(Signature,GetTypeHash(Instances->GetInstanceCount()));
			for (int32 I=0; I<Instances->GetInstanceCount(); ++I)
			{
				FTransform Instance;
				if (Instances->GetInstanceTransform(I,Instance,/*bWorldSpace=*/false)) Signature=HashTransform(Signature,Instance);
			}
		}
		FEntry& Entry=Entries.FindOrAdd(Source);
		if (Entry.Leaves.IsValid() && Entry.Signature==Signature) return;
		if (UInstancedStaticMeshComponent* Old=Entry.Leaves.Get()) Old->DestroyComponent();
		Entry.Leaves=Leaves->CreateLeafInstances(Source,*Model,/*bEditorPreview=*/true);
		Entry.Signature=Signature;
	};

	// Same dormant tree kinds the game handles: Static Mesh Actors and instanced (foliage) components.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsHiddenEd()) continue;
		if (AStaticMeshActor* Actor=Cast<AStaticMeshActor>(*It)) Visit(Actor->GetStaticMeshComponent());
		TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
		for (UInstancedStaticMeshComponent* Instances : Components) Visit(Instances);
	}
	for (auto It=Entries.CreateIterator(); It; ++It)
	{
		if (UStaticMeshComponent* Source=It.Key().Get(); Source && Seen.Contains(Source)) continue;
		if (UInstancedStaticMeshComponent* Old=It.Value().Leaves.Get()) Old->DestroyComponent();
		It.RemoveCurrent();
	}
	if (Entries.Num()!=LoggedCount)
	{
		LoggedCount=Entries.Num();
		UE_LOG(LogTemp, Display, TEXT("Leaves: editor preview on %d tree source(s) in %s."),LoggedCount,*GetNameSafe(World));
	}
}

void UTOLLeafEditorPreview::ClearAll()
{
	for (const TPair<TWeakObjectPtr<UStaticMeshComponent>,FEntry>& Pair : Entries)
		if (UInstancedStaticMeshComponent* Old=Pair.Value.Leaves.Get()) Old->DestroyComponent();
	Entries.Empty();
}

void UTOLLeafEditorPreview::Deinitialize()
{
	ClearAll();
	Models.Empty(); SocketHashes.Empty();
	Super::Deinitialize();
}
