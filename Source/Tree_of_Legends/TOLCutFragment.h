#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TOLCutFragment.generated.h"

class UProceduralMeshComponent;

/** A detached, physically simulated half of one cut tree segment. */
UCLASS(BlueprintType)
class TREE_OF_LEGENDS_API ATOLCutFragment : public AActor
{
	GENERATED_BODY()

public:
	ATOLCutFragment();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cutting")
	TObjectPtr<UProceduralMeshComponent> CutMesh;
};
