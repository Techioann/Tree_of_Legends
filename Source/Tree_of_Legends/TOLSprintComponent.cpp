#include "TOLSprintComponent.h"

#include "TOLGrabComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/ProgressBar.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
	// BPC_PlayerStats is a Blueprint component; its variables are read and written by name.
	const FName StaminaName(TEXT("Stamina"));
	const FName MaxStaminaName(TEXT("Max Stamina"));
	const FName HealthName(TEXT("Health"));
	const FName MaxHealthName(TEXT("Max Health"));

	FNumericProperty* FindNumeric(const UObject* Object, FName Name)
	{
		return Object ? CastField<FNumericProperty>(Object->GetClass()->FindPropertyByName(Name)) : nullptr;
	}

	bool ReadStat(const UObject* Object, FName Name, double& Out)
	{
		const FNumericProperty* Property=FindNumeric(Object,Name);
		if (!Property) return false;
		const void* Value=Property->ContainerPtrToValuePtr<void>(Object);
		Out=Property->IsFloatingPoint() ? Property->GetFloatingPointPropertyValue(Value) : double(Property->GetSignedIntPropertyValue(Value));
		return FMath::IsFinite(Out);
	}

	void WriteStat(UObject* Object, FName Name, double Value)
	{
		const FNumericProperty* Property=FindNumeric(Object,Name);
		if (!Property) return;
		void* Target=Property->ContainerPtrToValuePtr<void>(Object);
		if (Property->IsFloatingPoint()) Property->SetFloatingPointPropertyValue(Target,Value);
		else Property->SetIntPropertyValue(Target,int64(FMath::RoundToInt64(Value)));
	}

	bool ReadRatio(const UObject* Stats, FName Value, FName Max, float& Out)
	{
		double Current=0, Maximum=0;
		if (!ReadStat(Stats,Value,Current) || !ReadStat(Stats,Max,Maximum) || Maximum<=0) return false;
		Out=float(FMath::Clamp(Current/Maximum,0.0,1.0));
		return true;
	}
}

UTOLSprintComponent::UTOLSprintComponent()
{
	PrimaryComponentTick.bCanEverTick=true;
	static ConstructorHelpers::FObjectFinder<UInputAction> Sprint(TEXT("/Game/Inputs/Keys/IA_Sprint.IA_Sprint"));
	if (Sprint.Succeeded()) SprintAction=Sprint.Object;
}

bool UTOLSprintComponent::CanSprintNow() const
{
	const ACharacter* Character=Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Move=Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move || !Move->IsMovingOnGround() || Move->IsCrouching()) return false;
	// Only while actually moving under input; standing still with Shift held costs nothing.
	if (Move->GetCurrentAcceleration().SizeSquared2D()<1.0 || Character->GetVelocity().SizeSquared2D()<100.0) return false;
	if (!bAllowSprintWhileCarrying)
		if (const UTOLGrabComponent* Grab=Character->FindComponentByClass<UTOLGrabComponent>(); Grab && Grab->HeldComponent)
			return false;
	return true;
}

void UTOLSprintComponent::BindInput()
{
	const APawn* Pawn=Cast<APawn>(GetOwner());
	UEnhancedInputComponent* Input=Pawn ? Cast<UEnhancedInputComponent>(Pawn->InputComponent) : nullptr;
	if (!Input || !SprintAction || BoundInput.Get()==Input) return;
	if (UEnhancedInputComponent* Old=BoundInput.Get())
		for (uint32 Handle : ActionBindings) Old->RemoveBindingByHandle(Handle);
	ActionBindings.Reset();
	ActionBindings.Add(Input->BindAction(SprintAction,ETriggerEvent::Started,this,&UTOLSprintComponent::SprintStarted).GetHandle());
	ActionBindings.Add(Input->BindAction(SprintAction,ETriggerEvent::Completed,this,&UTOLSprintComponent::SprintStopped).GetHandle());
	ActionBindings.Add(Input->BindAction(SprintAction,ETriggerEvent::Canceled,this,&UTOLSprintComponent::SprintStopped).GetHandle());
	BoundInput=Input;
	bSprintHeld=false;
}

void UTOLSprintComponent::ApplySpeed(bool bOn)
{
	const ACharacter* Character=Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move=Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move || bOn==bSpeedApplied) return;
	if (bOn)
	{
		WalkSpeedBeforeSprint=Move->MaxWalkSpeed;
		Move->MaxWalkSpeed=WalkSpeedBeforeSprint*SpeedMultiplier;
	}
	else
	{
		// Someone else changed the speed meanwhile: remove only our factor and keep their change.
		const float Sprinted=WalkSpeedBeforeSprint*SpeedMultiplier;
		Move->MaxWalkSpeed=FMath::IsNearlyEqual(Move->MaxWalkSpeed,Sprinted,0.01f)
			? WalkSpeedBeforeSprint : Move->MaxWalkSpeed/FMath::Max(SpeedMultiplier,1.f);
	}
	bSpeedApplied=bOn;
}

void UTOLSprintComponent::StopSprintSpeed()
{
	bSprinting=false;
	ApplySpeed(false);
}

void UTOLSprintComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSprintSpeed();
	if (UEnhancedInputComponent* Old=BoundInput.Get())
		for (uint32 Handle : ActionBindings) Old->RemoveBindingByHandle(Handle);
	ActionBindings.Reset(); BoundInput=nullptr;
	Super::EndPlay(EndPlayReason);
}

void UTOLSprintComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
	const APawn* Pawn=Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled()) { StopSprintSpeed(); return; }
	BindInput();

	if (!Stats.IsValid())
		for (UActorComponent* Component : GetOwner()->GetComponents())
			if (FindNumeric(Component,StaminaName) && FindNumeric(Component,MaxStaminaName)) { Stats=Component; break; }
	UObject* StatsObject=Stats.Get();
	double Stamina=0, MaxStamina=0;
	const bool bHasStamina=StatsObject && ReadStat(StatsObject,StaminaName,Stamina) &&
		ReadStat(StatsObject,MaxStaminaName,MaxStamina) && MaxStamina>0;

	if (bExhausted && (!bHasStamina || Stamina>=MaxStamina*ResumeFraction)) bExhausted=false;
	bSprinting=bSprintHeld && !bExhausted && CanSprintNow() && (!bHasStamina || Stamina>0);
	if (bHasStamina)
	{
		if (bSprinting)
		{
			SinceSprint=0.f;
			Stamina=FMath::Max(0.0,Stamina-double(StaminaDrainPerSecond)*DeltaTime);
			if (Stamina<=0.0) { bExhausted=true; bSprinting=false; }
		}
		else
		{
			SinceSprint+=DeltaTime;
			if (SinceSprint>=RegenDelay) Stamina=FMath::Min(MaxStamina,Stamina+double(StaminaRegenPerSecond)*DeltaTime);
		}
		WriteStat(StatsObject,StaminaName,Stamina);
	}
	ApplySpeed(bSprinting);
	UpdateHud(DeltaTime);
}

void UTOLSprintComponent::UpdateHud(float DeltaTime)
{
	if (!StaminaBar.IsValid() && !HealthBar.IsValid())
	{
		HudSearchCooldown-=DeltaTime;
		if (HudSearchCooldown>0.f) return;
		HudSearchCooldown=0.5f;
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UUserWidget::StaticClass(),/*TopLevelOnly=*/true);
		for (UUserWidget* Widget : Widgets)
		{
			if (!Widget || Widget->GetOwningPlayerPawn()!=GetOwner()) continue;
			UProgressBar* StaminaWidget=Cast<UProgressBar>(Widget->GetWidgetFromName(StaminaName));
			UProgressBar* HealthWidget=Cast<UProgressBar>(Widget->GetWidgetFromName(HealthName));
			if (!StaminaWidget && !HealthWidget) continue;
			StaminaBar=StaminaWidget; HealthBar=HealthWidget;
			ShownStamina=ShownHealth=-1.f; // start at the real value, no sweep from zero
			break;
		}
	}
	const UObject* StatsObject=Stats.Get();
	auto Ease=[&](UProgressBar* Bar, float& Shown, FName Value, FName Max)
	{
		float Target=0.f;
		if (!Bar || !ReadRatio(StatsObject,Value,Max,Target)) return;
		Shown=Shown<0.f ? Target : FMath::FInterpTo(Shown,Target,DeltaTime,BarInterpSpeed);
		if (FMath::IsNearlyEqual(Shown,Target,0.0005f)) Shown=Target;
		Bar->SetPercent(Shown);
	};
	Ease(StaminaBar.Get(),ShownStamina,StaminaName,MaxStaminaName);
	Ease(HealthBar.Get(),ShownHealth,HealthName,MaxHealthName);
}
