#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TOLCutMeshData.h"
#include "TOLTreeRow.generated.h"

class UStaticMesh;

/**
 * One DT_Tree row = one tree type. A placed Static Mesh Actor becomes a cuttable tree when:
 *  1. a row's Mesh column names its exact Static Mesh, or
 *  2. its mesh is called SM_Tree_<X> and a row Tree_<X> exists with an empty Mesh column.
 * Run "TOL.SyncTreeTable" (or the Sync Tree Table button on CutTrace) to add rows for new SM_Tree_* meshes.
 */
USTRUCT(BlueprintType)
struct TREE_OF_LEGENDS_API FTOLTreeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree", meta=(ClampMin="0.001"))
	double MaxHealth = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree", meta=(ClampMin="0"))
	int32 RequiredAxeLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree", meta=(ClampMin="0"))
	int32 WoodAmount = 3;

	/** Coins per cubic metre of wood. 0 = pieces of this tree are worth nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Value", meta=(ClampMin="0"))
	double PricePerM3 = 0.0;

	/** Physical mass of cut pieces = volume x density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Physics", meta=(ClampMin="1"))
	double DensityKgM3 = 700.0;

	/** Higher = more chops needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree", meta=(ClampMin="0.001"))
	double ResistanceMultiplier = 1.0;

	/** Exact mesh for this row. Empty = matched by name (SM_Tree_<X> for row Tree_<X>). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Automatic finds straight trunk rails; Authored uses AuthoredSegments (mesh local space, LOD 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	ETOLCutSegmentMode SegmentMode = ETOLCutSegmentMode::Automatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	TArray<FTOLCutSegment> AuthoredSegments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	FVector TrunkAxisLocal = FVector::UpVector;
};
