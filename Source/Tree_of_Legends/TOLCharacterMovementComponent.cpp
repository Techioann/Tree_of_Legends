#include "TOLCharacterMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"

namespace
{
	FBodyInstance* GetSimulatingBody(UPrimitiveComponent* Component, FName BoneName)
	{
		FBodyInstance* Body = Component ? Component->GetBodyInstance(BoneName) : nullptr;
		return Body && Body->IsInstanceSimulatingPhysics() ? Body : nullptr;
	}
}

void UTOLCharacterMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity)
{
	if (!bEnablePhysicsInteraction || !Impact.bBlockingHit) return;
	UPrimitiveComponent* Component = Impact.GetComponent();
	FBodyInstance* Body = GetSimulatingBody(Component, Impact.BoneName);
	if (!Body)
	{
		// Geometry collections and other special cases keep the engine behaviour.
		Super::ApplyImpactPhysicsForces(Impact, ImpactAcceleration, ImpactVelocity);
		return;
	}
	// Walking over a piece: its top is touched every frame, and pushing there is what made it spin away.
	if (Component == CurrentFloor.HitResult.GetComponent()) return;

	const FVector Up = -GetGravityDirection();
	const FVector Direction = FVector::VectorPlaneProject(-Impact.ImpactNormal, Up).GetSafeNormal();
	if (Direction.IsZero()) return; // Contact from straight above: the standing weight covers it.

	// Push only as hard as needed to bring the piece up to the speed the player is moving or trying to move at.
	const double WantedSpeed = FMath::Max3(0.0, ImpactVelocity | Direction,
		ImpactAcceleration.IsZero() ? 0.0 : (ImpactAcceleration.GetSafeNormal() | Direction) * GetMaxSpeed());
	const double BodySpeed = Component->GetPhysicsLinearVelocity(Impact.BoneName) | Direction;
	const double MissingSpeed = WantedSpeed - BodySpeed;
	const double DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0;
	if (MissingSpeed <= 0.0 || DeltaSeconds <= 0.0) return;

	const double BodyMass = FMath::Max(static_cast<double>(Body->GetBodyMass()), 0.1);
	const double ForceUnits = FMath::Min(MaxPushForceNewtons * 100.0, BodyMass * MissingSpeed / DeltaSeconds); // kg*cm/s^2
	// At centre-of-mass height so the push slides or rolls the piece instead of flipping it.
	const FVector CenterOfMass = Body->GetCOMPosition();
	const FVector ForcePoint = FVector::VectorPlaneProject(Impact.ImpactPoint, Up) + Up * (CenterOfMass | Up);
	Component->AddForceAtLocation(Direction * ForceUnits, ForcePoint, Impact.BoneName);
}

void UTOLCharacterMovementComponent::ApplyDownwardForce(float DeltaSeconds)
{
	if (StandingDownwardForceScale == 0.f || !CurrentFloor.HitResult.IsValidBlockingHit()) return;
	UPrimitiveComponent* Base = CurrentFloor.HitResult.GetComponent();
	FBodyInstance* Body = GetSimulatingBody(Base, CurrentFloor.HitResult.BoneName);
	if (!Body)
	{
		Super::ApplyDownwardForce(DeltaSeconds);
		return;
	}
	const FVector Gravity = -GetGravityDirection() * GetGravityZ();
	if (Gravity.IsZero()) return;
	// A 100 kg player on a 10 kg piece must not press 10x its weight into one edge of it.
	const float PressedMass = FMath::Min(Mass, Body->GetBodyMass() * MaxStandingWeightToBodyWeight);
	// Through the centre of mass: off-centre, the weight of a player walking across rolled light logs out from under them.
	Base->AddForceAtLocation(Gravity * PressedMass * StandingDownwardForceScale, Body->GetCOMPosition(), CurrentFloor.HitResult.BoneName);
}
