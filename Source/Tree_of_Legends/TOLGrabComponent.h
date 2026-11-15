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
	float LinearDamping = 1800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="1"))
	float AngularStiffness = 12000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grab|Physics", meta=(ClampMin="1"))
	float AngularDamping = 1500.f;
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
	void Status(const FString& Message, bool bFailure = false);
	UFUNCTION()
	void OnTreeCut(const FTOLCutTraceResult& Result, ATOLCutFragment* Fragment);
	UFUNCTION()
	void OnHeldPhysicsChanged(UPrimitiveComponent* Component, EComponentPhysicsStateChange StateChange);
};
