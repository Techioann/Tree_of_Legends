#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "InputCoreTypes.h"
#include "TOLCutTraceComponent.h"
#include "TOLGrabComponent.generated.h"

class UPhysicsHandleComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class APlayerController;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTOLGrabChanged, UPrimitiveComponent*, Component, bool, bGrabbed);

/** Single-player physical carry prototype. Reuses the character's existing handle and input assets. */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class TREE_OF_LEGENDS_API UTOLGrabComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTOLGrabComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Input")
	TObjectPtr<UInputAction> GrabAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Input")
	TObjectPtr<UInputAction> LookAction;
	/** Runtime-only mapping for RotateKey; existing input assets remain untouched. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab|Input")
	TObjectPtr<UInputAction> RotateAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Input")
	FKey RotateKey = EKeys::R;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Input", meta=(ClampMin="0.01"))
	float RotationDegreesPerUnit = 1.5f;
	/** Runtime-only mouse wheel action: moves the held object away from / toward the view. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab|Input")
	TObjectPtr<UInputAction> DistanceAction;
	/** Change of the desired hold distance per mouse wheel notch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Input", meta=(ClampMin="1"))
	float WheelStepCm = 25.f;
	/** Linear speed at which the hold distance follows the wheel for a ReferenceMassKg object (slower when heavier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Input", meta=(ClampMin="1"))
	float HoldDistanceSpeed = 400.f;
	/** Provisional prototype limits, independent of the Cutting material/mass configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Limits", meta=(ClampMin="1"))
	float MaxGrabDistance = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Limits", meta=(ClampMin="1"))
	float MinHoldDistance = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Limits", meta=(ClampMin="1"))
	float MaxHoldDistance = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Limits", meta=(ClampMin="0.01"))
	float MaxGrabMassKg = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Limits", meta=(ClampMin="1"))
	float BreakDistance = 250.f;
	/** Chaos handle drives are acceleration based. Scale their response by mass without overriding body mass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="0.1"))
	float ReferenceMassKg = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="1"))
	float LinearStiffness = 15000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="1"))
	float LinearDamping = 245.f; // ~critical damping (2*sqrt(LinearStiffness)); scaled with mass at grab time
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="1"))
	float AngularStiffness = 12000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="1"))
	float AngularDamping = 220.f; // ~critical damping (2*sqrt(AngularStiffness))
	/** Walk speed while carrying = normal speed * max(MinCarrySpeedScale, (mass / ReferenceMassKg)^-exponent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="0"))
	float CarrySpeedMassExponent = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="0.05", ClampMax="1"))
	float MinCarrySpeedScale = .25f;
	/** Jump height while carrying a ReferenceMassKg object, as a fraction of the normal jump height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="0", ClampMax="1"))
	float MaxCarryJumpHeightScale = .5f;
	/** Jump height (cm) just below NoJumpMassKg. Heights in between follow log(mass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="0"))
	float MinCarryJumpHeightCm = 1.f;
	/** Carrying an object at least this heavy disables jumping entirely (restored on release). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="0.1"))
	float NoJumpMassKg = 100.f;
	/** Carrying anything disables jumping (restored on release). When off, jumping is limited by mass instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics")
	bool bDisableJumpWhileCarrying = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Debug")
	bool bDrawDebug = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Debug")
	bool bShowStatus = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab")
	TObjectPtr<UPrimitiveComponent> HeldComponent;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab")
	float HeldMassKg = 0.f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab")
	float HoldDistance = 0.f;
	/** Distance the wheel asks for; HoldDistance moves toward it at a constant speed. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab")
	float DesiredHoldDistance = 0.f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab")
	bool bRotating = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab")
	bool bInputBound = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab")
	FString LastStatus;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Grab")
	FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintAssignable, Category="Grab")
	FTOLGrabChanged OnGrabChanged;

	UFUNCTION(BlueprintCallable, Category="Grab")
	bool TryGrabFromPlayerView();
	/** Validates the hit again against range, obstruction, body mobility and mass. */
	UFUNCTION(BlueprintCallable, Category="Grab")
	bool TryGrabHit(const FHitResult& Hit);
	UFUNCTION(BlueprintCallable, Category="Grab")
	void ReleaseGrab();
	UFUNCTION(BlueprintCallable, Category="Grab")
	void SetRotating(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category="Grab")
	void RotateHeldObject(FVector2D MouseDelta);
	UFUNCTION(BlueprintCallable, Category="Grab")
	void AdjustHoldDistance(float DeltaCentimeters);

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> Handle;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RotateMapping;
	TWeakObjectPtr<APlayerController> LookLockController;
	TArray<uint32> ActionBindings;
	FTimerHandle InputTimer;
	FVector LocalGrabPoint = FVector::ZeroVector;
	/** Rate-limited hold direction (unit vector). Stored as a direction so strafing never reads as turning. */
	FVector HoldDirection = FVector::ForwardVector;
	/** MaxWalkSpeed before the carry slowdown; 0 when nothing to restore. */
	float WalkSpeedBeforeCarry = 0.f;
	/** JumpZVelocity before the carry reduction; negative when nothing to restore. */
	float JumpZBeforeCarry = -1.f;
	/** Whether jumping was allowed before a too-heavy carry disabled it. */
	bool bJumpAllowedBeforeCarry = true;
	bool bJumpDisabledByCarry = false;
	FQuat HeldRotation = FQuat::Identity;
	FName HeldBone;
	ECollisionResponse PreviousPawnResponse = ECR_Block;
	float OldLinearStiffness = 0, OldLinearDamping = 0, OldAngularStiffness = 0, OldAngularDamping = 0;
	float OldInterpolationSpeed = 0;
	bool bOldInterpolate = false;
	bool bOwnsHandle = false;
	void UpdateInputBinding();
	void UnbindInput();
	bool GetPlayerView(FVector& Location, FRotator& Rotation) const;
	void InputGrab();
	void InputRotateStart();
	void InputRotateEnd();
	void InputLook(const FInputActionValue& Value);
	void InputDistance(const FInputActionValue& Value);
	void Status(const FString& Message, bool bFailure = false);
	UFUNCTION()
	void OnTreeCut(const FTOLCutTraceResult& Result, ATOLCutFragment* Fragment);
	UFUNCTION()
	void OnHeldPhysicsChanged(UPrimitiveComponent* Component, EComponentPhysicsStateChange StateChange);
};
