#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "Engine/DataTable.h"
#include "TOLCutMeshData.h"
#include "TOLCutTraceComponent.generated.h"

class UPrimitiveComponent;
class ATOLCutFragment;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FTOLDormantTreeProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	FTOLTreeMeshConfiguration Model;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	FDataTableRowHandle TreeData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting", meta=(ClampMin="0.001"))
	double ResistanceMultiplier = 1.0;
};

USTRUCT(BlueprintType)
struct FTOLChopResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	bool bApplied = false;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	bool bTreeCut = false;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	bool bToolBroken = false;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FGuid CutId;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	double ProgressGain = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	double ProgressNormalized = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	double DurabilityLoss = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	double DurabilityRemaining = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FVector WorldPlanePoint = FVector::ZeroVector;
};

/** A contact point and a plane perpendicular to the hit tree segment, before any mesh modification. */
USTRUCT(BlueprintType)
struct FTOLCutTraceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	bool bValidTreeHit = false;

	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FHitResult Hit;

	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FVector LocalHitPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FVector LocalPlanePoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FVector LocalPlaneNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FVector WorldPlanePoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FVector WorldPlaneNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	double DistanceAlongTrunkLocal = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTOLCutTraceCompleted, const FTOLCutTraceResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTOLChopCompleted, const FTOLChopResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTOLSegmentCutCompleted,
	const FTOLCutTraceResult&, Result, ATOLCutFragment*, Fragment);

/** Reusable trace and cut-plane calculation for the existing tree Blueprint. */
UCLASS(ClassGroup=(Cutting), meta=(BlueprintSpawnableComponent))
class TREE_OF_LEGENDS_API UTOLCutTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTOLCutTraceComponent();
	virtual void BeginPlay() override;

	/** Exact Static Mesh references, never actor labels or substring matches. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cutting|Activation")
	TArray<FTOLDormantTreeProfile> DormantTreeProfiles;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Cutting|Activation")
	TObjectPtr<AActor> LastActivatedTree;

	/** The existing BP_Tree_Procedural class; configure on B_Character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Target")
	TSubclassOf<AActor> CuttableTreeClass;

	/** Reference direction for orienting preserved segment centerlines; SM_Tree's trunk runs along local +Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Target")
	FVector TrunkAxisLocal = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Trace", meta=(ClampMin="1.0"))
	float TraceDistance = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Debug", meta=(ClampMin="0.0"))
	float DebugDuration = 3.0f;

	/** Existing IA_Cutting path calls TraceCutFromPlayerView. Keep the Blueprint
	 * graph intact and apply one chop for each valid button press. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Geometry")
	bool bCutOnValidTrace = true;

	/** Uses the existing DT_Axe schema: chop damage and require level (prototype tier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Axe")
	FDataTableRowHandle AxeData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Material")
	FDataTableRowHandle DefaultTreeData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Axe", meta=(ClampMin="0.0"))
	double ToolDurability = 100.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Axe", meta=(ClampMin="0.0"))
	double DurabilityLossPerHit = 1.0;
	/** Each missing tier multiplies progress by this factor, without integer rounding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Axe", meta=(ClampMin="0.001", ClampMax="1.0"))
	double WeakToolEfficiency = 0.1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Axe", meta=(ClampMin="0.0"))
	double ExtraWearPerMissingTier = 3.0;
	/** Group by distance along the segment in world centimetres; opposite sides share a cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Progress", meta=(ClampMin="0.0"))
	double CutMergeDistance = 8.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Progress", meta=(ClampMin="0.0"))
	double MinimumPieceLength = 10.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Progress")
	TObjectPtr<UMaterialInterface> CutMarkMaterial;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Cutting|Progress")
	FTOLChopResult LastChopResult;
	UPROPERTY(BlueprintAssignable, Category="Cutting")
	FTOLChopCompleted OnChopCompleted;

	/** Optional cross-section material; falls back to the hit section's material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Geometry")
	TObjectPtr<UMaterialInterface> CutFaceMaterial;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Cutting|Geometry")
	TObjectPtr<ATOLCutFragment> LastCutFragment;

	UPROPERTY(BlueprintAssignable, Category="Cutting")
	FTOLCutTraceCompleted OnCutTraceCompleted;

	UPROPERTY(BlueprintAssignable, Category="Cutting")
	FTOLSegmentCutCompleted OnSegmentCutCompleted;

	/** Traces from the owning player's view, ignoring the owner. Returns true only for a configured tree. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Trace")
	bool TraceCutFromPlayerView(FTOLCutTraceResult& OutResult);

	/** Shared axe-hit path: activates dormant trees only after a successful positive chop. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Activation")
	bool ApplyTreeHit(const FHitResult& Hit, FTOLCutTraceResult& OutTrace, FTOLChopResult& OutChop);

	/** Can also consume a hit supplied by an axe animation or another Blueprint. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Trace")
	bool ComputeCutPlaneFromHit(const FHitResult& Hit, FTOLCutTraceResult& OutResult) const;

	/** Slices the connected body using the hit segment axis, including previous caps. The source keeps the root half;
	 * the detached half becomes a simulated actor. Returns false without changing the
	 * source if the plane does not create two nonempty, capped halves. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Geometry")
	bool CutHitSegment(const FTOLCutTraceResult& Cut, ATOLCutFragment*& OutFragment);

	/** Adds progress and spends durability once. Geometry changes only at the threshold. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Progress")
	bool ApplyChop(const FTOLCutTraceResult& Cut, FTOLChopResult& OutResult);
	/** Inventory/Blueprint runtime entry point; equipping explicitly supplies saved durability. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Axe")
	bool EquipAxe(FDataTableRowHandle Row, double RemainingDurability);

private:
	bool CalculateChopValues(FDataTableRowHandle Material, double Resistance,
		double& OutGain, double& OutWear, double& OutHealth) const;
	bool TryActivateTree(const FHitResult& Hit, FTOLCutTraceResult& OutTrace, FTOLChopResult& OutChop);
	bool bDeferChopNotifications = false;
	bool SliceAtPlane(const FTOLCutTraceResult& Contact, const FVector& WorldPlanePoint, ATOLCutFragment*& OutFragment);
	void DrawCutDebug(const FVector& TraceStart, const FVector& TraceEnd, const FTOLCutTraceResult& Result) const;
};
