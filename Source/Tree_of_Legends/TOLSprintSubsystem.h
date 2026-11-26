#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TOLSprintSubsystem.generated.h"

/**
 * Gives every locally controlled player character a UTOLSprintComponent at runtime (game and PIE worlds only),
 * unless its Blueprint already has one. Keeps sprint working without editing B_Character.
 */
UCLASS()
class TREE_OF_LEGENDS_API UTOLSprintSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	float SinceCheck = 1.f;
};
