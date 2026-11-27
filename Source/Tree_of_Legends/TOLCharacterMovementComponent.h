#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TOLCharacterMovementComponent.generated.h"

/**
 * Character movement with sane contact forces on loose simulated pieces (cut logs).
 *
 * The engine pushes every touched body with a fixed force at the contact point (no mass or speed limit) and
 * presses the full player weight onto whatever the player stands on, also at the contact point. Light pieces
 * then spin and fly, especially while the player walks on top of them. Here:
 * - pushes are horizontal, through the body's centre-of-mass height, limited to the player's strength and to
 *   what brings the body up to the player's own speed (a piece never outruns the player);
 * - the body the player is standing on is not pushed at all;
 * - the standing weight is capped relative to the body's mass and applied through its centre of mass.
 */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class TREE_OF_LEGENDS_API UTOLCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/** Largest horizontal push the player can exert on a simulated body, in newtons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Movement: Physics Interaction|TOL", meta=(ClampMin="0", Units="Newtons"))
	float MaxPushForceNewtons = 600.f;

	/** The standing weight pressed onto a simulated floor is at most this multiple of that body's own weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Movement: Physics Interaction|TOL", meta=(ClampMin="0"))
	float MaxStandingWeightToBodyWeight = 1.f;

protected:
	virtual void ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity) override;
	virtual void ApplyDownwardForce(float DeltaSeconds) override;
};
