#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TOLTreeActivationSubsystem.generated.h"

class UStaticMeshComponent;

/** Per-world activation lock/registry shared by all players. No tick or per-tree component. */
UCLASS()
class TREE_OF_LEGENDS_API UTOLTreeActivationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Cutting|Activation")
	AActor* GetActiveTree(UStaticMeshComponent* Source) const;
	bool BeginActivation(UStaticMeshComponent* Source);
	bool CommitActivation(UStaticMeshComponent* Source, AActor* Active);
	void AbortActivation(UStaticMeshComponent* Source);
	bool PrepareDormantTreesOnce();
	virtual void Deinitialize() override;

private:
	TMap<TWeakObjectPtr<UStaticMeshComponent>,TWeakObjectPtr<AActor>> Activated;
	TSet<TWeakObjectPtr<UStaticMeshComponent>> Activating;
	bool bDormantPrepared = false;
};
