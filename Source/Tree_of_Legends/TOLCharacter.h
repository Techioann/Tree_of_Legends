#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TOLCharacter.generated.h"

/** Base class of B_Character: a plain ACharacter that uses UTOLCharacterMovementComponent. */
UCLASS()
class TREE_OF_LEGENDS_API ATOLCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATOLCharacter(const FObjectInitializer& ObjectInitializer);
};
