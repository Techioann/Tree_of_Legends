#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "TOLTreeLeafSubsystem.generated.h"

struct FTOLTreeMeshConfiguration;
class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPrimitiveComponent;
class UMaterialInterface;

/**
 * Visual-only leaf balls on tree branch tips (Project Settings > Game > Tree of Legends Cutting > Leaves).
 * Leaves never collide, carry no mass and are invisible to the axe and grab traces.
 *  - Dormant trees: one instanced component per tree / foliage component (cheap).
 *  - Activated trees: one Static Mesh Component per leaf, attached to the procedural mesh, so each
 *    leaf can follow whichever piece keeps its branch after a cut (see TransferLeaves).
 */
UCLASS()
class TREE_OF_LEGENDS_API UTOLTreeLeafSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static const FName LeafTag;
	/** Additionally on editor-preview leaves (UTOLLeafEditorPreview); never present in a game world. */
	static const FName PreviewTag;

	/** Mesh-local leaf transforms: "Leaf*" sockets if present, else detected branch tips. Cached per mesh. */
	const TArray<FTransform>& GetAnchors(const FTOLTreeMeshConfiguration& Model);
	/** Drops cached anchors (one mesh, or all plus the leaf assets) so they are rebuilt from current settings. */
	void InvalidateAnchors(const UStaticMesh* Mesh = nullptr);

	/** Instanced leaves for a dormant tree source. Preview: unattached (world space), not saved, not duplicated
	 * into PIE, not selectable, and creating/destroying it never modifies the tree or dirties the level. */
	UInstancedStaticMeshComponent* CreateLeafInstances(UStaticMeshComponent* Source,
		const FTOLTreeMeshConfiguration& Model, bool bEditorPreview);
	/** Removes editor-preview leaves that were copied along with an actor (safety net for PIE duplication). */
	static void DestroyPreviewLeaves(AActor* Owner);

	/** Static Mesh Actor tree (leaves hide with it) or a foliage component (all its instances). */
	void AddDormantLeaves(UStaticMeshComponent* Source, const FTOLTreeMeshConfiguration& Model);
	/** Removes the dormant leaves of one foliage instance (matched by position; indices are unstable). */
	void RemoveDormantInstanceLeaves(UInstancedStaticMeshComponent* Source, const FTransform& InstanceWorld,
		const FTOLTreeMeshConfiguration& Model);
	/** Movable per-leaf components for an activated tree; Parent's transform must equal the tree transform. */
	void SpawnLeaves(UPrimitiveComponent* Parent, const FTOLTreeMeshConfiguration& Model);

	/** Moves leaf children of From to To when Predicate(leaf location in From's local space) is true. */
	static int32 TransferLeaves(UPrimitiveComponent* From, UPrimitiveComponent* To,
		TFunctionRef<bool(const FVector& LocalLocation)> Predicate);

	virtual void Deinitialize() override;

private:
	bool ResolveAssets();
	UStaticMeshComponent* MakeLeaf(USceneComponent* Parent, const FTransform& Relative) const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> LeafMesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LeafMaterial;
	bool bAssetsResolved = false;
	TMap<TObjectKey<UStaticMesh>, TArray<FTransform>> AnchorCache;
	TMap<TWeakObjectPtr<UStaticMeshComponent>, TWeakObjectPtr<UInstancedStaticMeshComponent>> DormantLeaves;
};
