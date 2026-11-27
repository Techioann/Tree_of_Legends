#include "TOLCharacter.h"

#include "TOLCharacterMovementComponent.h"

ATOLCharacter::ATOLCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTOLCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
}
