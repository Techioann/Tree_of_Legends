#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TOLSprintComponent.generated.h"

class UInputAction;
class UProgressBar;
class UUserWidget;

/**
 * Sprint with stamina, plus smooth HUD stat bars.
 *
 * - Hold IA_Sprint (Shift) while walking on the ground: MaxWalkSpeed x SpeedMultiplier. The factor is removed
 *   exactly when sprint ends (also when the grab system starts a carry slowdown, see StopSprintSpeed).
 * - Sprinting drains the "Stamina" variable of the owner's BPC_PlayerStats; it regenerates after a short pause.
 *   When it runs out, sprint stays locked until ResumeFraction of the maximum has come back.
 * - The HUD's "Stamina" and "Health" progress bars ease toward the real values instead of jumping.
 *
 * Binds its own input on the locally controlled pawn (like UTOLGrabComponent). UTOLSprintSubsystem adds it to
 * the player's character at runtime when the Blueprint has none; add it to B_Character to tune the values.
 */
UCLASS(ClassGroup=(TOL), meta=(BlueprintSpawnableComponent))
class TREE_OF_LEGENDS_API UTOLSprintComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTOLSprintComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	TObjectPtr<UInputAction> SprintAction;

	/** Ground speed while sprinting, as a multiple of the normal speed (1.5 = +50%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint", meta=(ClampMin="1"))
	float SpeedMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint|Stamina", meta=(ClampMin="0"))
	float StaminaDrainPerSecond = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint|Stamina", meta=(ClampMin="0"))
	float StaminaRegenPerSecond = 8.f;

	/** Seconds without sprinting before stamina starts to regenerate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint|Stamina", meta=(ClampMin="0", Units="s"))
	float RegenDelay = 1.f;

	/** After running out, sprint unlocks again at this fraction of max stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint|Stamina", meta=(ClampMin="0", ClampMax="1"))
	float ResumeFraction = 0.25f;

	/** Whether sprinting is possible while holding an object with the grab system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bAllowSprintWhileCarrying = false;

	/** How fast HUD bars catch up with the real value (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint|HUD", meta=(ClampMin="0.1"))
	float BarInterpSpeed = 8.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Sprint")
	bool bSprintHeld = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Sprint")
	bool bSprinting = false;

	/** Out of stamina; sprint is locked until ResumeFraction is back. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Sprint")
	bool bExhausted = false;

	UFUNCTION(BlueprintCallable, Category="Sprint")
	void SetSprintHeld(bool bHeld) { bSprintHeld = bHeld; }

	/** Ends sprinting now and restores the walk speed. The grab system calls this before it scales the speed. */
	UFUNCTION(BlueprintCallable, Category="Sprint")
	void StopSprintSpeed();

	void SprintStarted() { SetSprintHeld(true); }
	void SprintStopped() { SetSprintHeld(false); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool CanSprintNow() const;
	void BindInput();
	void ApplySpeed(bool bOn);
	void UpdateHud(float DeltaTime);

	/** BPC_PlayerStats (a Blueprint component) holding the stamina/health variables. */
	TWeakObjectPtr<UActorComponent> Stats;
	TWeakObjectPtr<class UEnhancedInputComponent> BoundInput;
	TArray<uint32> ActionBindings;
	/** MaxWalkSpeed before the sprint factor; valid while bSpeedApplied. */
	float WalkSpeedBeforeSprint = 0.f;
	bool bSpeedApplied = false;
	TWeakObjectPtr<UProgressBar> StaminaBar;
	TWeakObjectPtr<UProgressBar> HealthBar;
	float SinceSprint = 0.f;
	float HudSearchCooldown = 0.f;
	float ShownStamina = -1.f;
	float ShownHealth = -1.f;
};
