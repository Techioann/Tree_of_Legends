#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "TOLCutMeshData.generated.h"

/** Centerline in the procedural mesh's local space. Preserved through subsequent cuts. */
USTRUCT(BlueprintType)
struct FTOLCutSegment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	FVector Start = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	FVector End = FVector::UpVector;
};

UENUM(BlueprintType)
enum class ETOLCutSegmentMode : uint8
{
	Automatic,
	Authored
};

/** Segment coordinates use the selected static mesh's local space (LOD 0). */
USTRUCT(BlueprintType)
struct FTOLTreeMeshConfiguration
{
	GENERATED_BODY()
	/** Empty preserves SourceTree's existing mesh, including SM_Tree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	TObjectPtr<class UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	ETOLCutSegmentMode SegmentMode = ETOLCutSegmentMode::Automatic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	TArray<FTOLCutSegment> AuthoredSegments;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	FVector TrunkAxisLocal = FVector::UpVector;
};

USTRUCT(BlueprintType)
struct FTOLActiveCut
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FGuid Id;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	int32 SegmentIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FVector LocalPlanePoint = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	FVector AxisLocal = FVector::UpVector;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	double Progress = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Cutting")
	double RequiredProgress = 1.0;
};

/** Optional authored centerlines; generated from straight mesh rails when absent. */
UCLASS(ClassGroup=(Cutting), meta=(BlueprintSpawnableComponent))
class TREE_OF_LEGENDS_API UTOLCutMeshData : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cutting|Model")
	FTOLTreeMeshConfiguration Model;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Cutting|Validation")
	FString ValidationMessage;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Cutting|Validation")
	bool bSegmentsValid = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Cutting|Validation")
	bool bGeometryModified = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting")
	TArray<FTOLCutSegment> Segments;

	/** Existing DT_Tree row. Empty uses the cutting component's default row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Material")
	FDataTableRowHandle TreeData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutting|Material", meta=(ClampMin="0.001"))
	double ResistanceMultiplier = 1.0;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Cutting")
	TArray<FTOLActiveCut> ActiveCuts;
	UPROPERTY(Transient)
	TObjectPtr<class UProceduralMeshComponent> CutMarks;

	/** Runtime configuration; refuses to change material while cuts are in progress. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Material")
	bool ConfigureMaterial(FDataTableRowHandle MaterialRow, double Resistance = 1.0);
	UFUNCTION(BlueprintCallable, Category="Cutting|Progress")
	void CancelAllCuts();

	/** Commits a validated model atomically. Rejects reconfiguration after any chop/slice. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Model")
	bool InitializeTree(class UStaticMeshComponent* SourceTree,
		class UProceduralMeshComponent* ProceduralTree, FString& OutError);
	/** Runtime alternative to editing the Blueprint's model defaults. Rolls back on failure. */
	UFUNCTION(BlueprintCallable, Category="Cutting|Model")
	bool ConfigureModel(const FTOLTreeMeshConfiguration& Configuration,
		class UStaticMeshComponent* SourceTree, class UProceduralMeshComponent* ProceduralTree, FString& OutError);
	UFUNCTION(BlueprintCallable, Category="Cutting|Validation")
	bool ValidateTreeConfiguration(class UStaticMeshComponent* SourceTree, FString& OutError) const;

	bool EnsureSegments(class UProceduralMeshComponent* Mesh);
	void MarkChopped() { bHasBeenChopped = true; }
	void MarkGeometryModified() { bGeometryModified = true; bHasBeenChopped = true; }
	void InheritBinding(const UTOLCutMeshData* Parent, class UProceduralMeshComponent* FragmentMesh);
	FVector GetTrunkAxis(const FVector& LegacyAxis) const;
	class UProceduralMeshComponent* GetBoundMesh() const { return BoundMesh.Get(); }

private:
	static TArray<FTOLCutSegment> DetectSegments(class UProceduralMeshComponent* Mesh);
	static bool ValidateSegments(const TArray<FTOLCutSegment>& InSegments,
		class UProceduralMeshComponent* Mesh, FString& OutError);
	uint32 ConfigurationHash() const;
	bool CheckBinding(class UProceduralMeshComponent* Mesh);
	UPROPERTY(Transient)
	TObjectPtr<class UStaticMesh> BoundSourceMesh;
	UPROPERTY(Transient)
	TObjectPtr<class UStaticMeshComponent> BoundSource;
	UPROPERTY(Transient)
	TObjectPtr<class UProceduralMeshComponent> BoundMesh;
	uint32 BoundConfigurationHash = 0;
	bool bHasBeenChopped = false;
	bool bFragment = false;
	bool bModelConfigured = false;
};
