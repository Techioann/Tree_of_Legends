#include "TOLGrabComponent.h"

#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTOLGrab, Log, All);

UTOLGrabComponent::UTOLGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UTOLGrabComponent::BeginPlay()
{
	Super::BeginPlay();
	Handle = GetOwner()->FindComponentByClass<UPhysicsHandleComponent>();
	if (!Handle)
	{
		Handle = NewObject<UPhysicsHandleComponent>(GetOwner());
		Handle->RegisterComponent();
		bOwnsHandle = true;
	}
	Handle->AddTickPrerequisiteComponent(this);
	if (auto* Cut = GetOwner()->FindComponentByClass<UTOLCutTraceComponent>())
		Cut->OnSegmentCutCompleted.AddUniqueDynamic(this, &UTOLGrabComponent::OnTreeCut);
	GetWorld()->GetTimerManager().SetTimer(InputTimer, this, &UTOLGrabComponent::UpdateInputBinding, .1f, true);
	UpdateInputBinding();
}

void UTOLGrabComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseGrab();
	UnbindInput();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(InputTimer);
	if (auto* Cut = GetOwner()->FindComponentByClass<UTOLCutTraceComponent>())
		Cut->OnSegmentCutCompleted.RemoveDynamic(this, &UTOLGrabComponent::OnTreeCut);
	if (Handle)
	{
		Handle->RemoveTickPrerequisiteComponent(this);
		if (bOwnsHandle) Handle->DestroyComponent();
	}
	Super::EndPlay(Reason);
}

bool UTOLGrabComponent::GetPlayerView(FVector& Location, FRotator& Rotation) const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	const auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !Pawn->IsLocallyControlled() || GetNetMode() != NM_Standalone) return false;
	PC->GetPlayerViewPoint(Location, Rotation);
	return !Location.ContainsNaN() && !Rotation.ContainsNaN();
}

void UTOLGrabComponent::UpdateInputBinding()
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	auto* Input = Pawn && Pawn->IsLocallyControlled() && GetNetMode() == NM_Standalone
		? Cast<UEnhancedInputComponent>(Pawn->InputComponent) : nullptr;
	if (bInputBound && BoundInput.IsValid() && BoundInput.Get() == Input) return;
	// Only a lost/changed binding cancels the grab. While unbound (no input or no actions
	// assigned) keep grabs made from Blueprint via TryGrabHit alive.
	if (bInputBound)
	{
		ReleaseGrab();
		UnbindInput();
	}
	if (!Input || !GrabAction || !LookAction) return;
	auto* PC = Cast<APlayerController>(Pawn->GetController());
	auto* Subsystem = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem) return;
	RotateAction = NewObject<UInputAction>(this);
	RotateAction->ValueType = EInputActionValueType::Boolean;
	RotateAction->bConsumeInput = false;
	RotateMapping = NewObject<UInputMappingContext>(this);
	RotateMapping->MapKey(RotateAction, RotateKey);
	Subsystem->AddMappingContext(RotateMapping, 0);
	InputSubsystem = Subsystem;
	BoundInput = Input;
	ActionBindings.Add(Input->BindAction(GrabAction, ETriggerEvent::Started, this, &UTOLGrabComponent::InputGrab).GetHandle());
	ActionBindings.Add(Input->BindAction(GrabAction, ETriggerEvent::Completed, this, &UTOLGrabComponent::ReleaseGrab).GetHandle());
	ActionBindings.Add(Input->BindAction(GrabAction, ETriggerEvent::Canceled, this, &UTOLGrabComponent::ReleaseGrab).GetHandle());
	ActionBindings.Add(Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &UTOLGrabComponent::InputLook).GetHandle());
	ActionBindings.Add(Input->BindAction(RotateAction, ETriggerEvent::Started, this, &UTOLGrabComponent::InputRotateStart).GetHandle());
	ActionBindings.Add(Input->BindAction(RotateAction, ETriggerEvent::Completed, this, &UTOLGrabComponent::InputRotateEnd).GetHandle());
	ActionBindings.Add(Input->BindAction(RotateAction, ETriggerEvent::Canceled, this, &UTOLGrabComponent::InputRotateEnd).GetHandle());
	bInputBound = true;
}

void UTOLGrabComponent::UnbindInput()
{
	if (auto* Input = BoundInput.Get())
	{
		for (uint32 Id : ActionBindings) Input->RemoveBindingByHandle(Id);
	}
	if (auto* Subsystem = InputSubsystem.Get()) if (RotateMapping) Subsystem->RemoveMappingContext(RotateMapping);
	InputSubsystem.Reset(); RotateMapping = nullptr; RotateAction = nullptr;
	ActionBindings.Reset();
	BoundInput.Reset();
	bInputBound = false;
}

void UTOLGrabComponent::Status(const FString& Message, bool bFailure)
{
	LastStatus = Message;
	UE_LOG(LogTOLGrab, Log, TEXT("%s: %s"), *GetNameSafe(GetOwner()), *Message);
	if (bShowStatus && GEngine)
		GEngine->AddOnScreenDebugMessage(uint64(GetUniqueID()), 3.f, bFailure ? FColor::Yellow : FColor::Cyan, Message);
}

void UTOLGrabComponent::InputGrab() { TryGrabFromPlayerView(); }
void UTOLGrabComponent::InputRotateStart() { SetRotating(true); }
void UTOLGrabComponent::InputRotateEnd() { SetRotating(false); }
void UTOLGrabComponent::InputLook(const FInputActionValue& Value)
{
	if (bRotating && Value.GetValueType() == EInputActionValueType::Axis2D) RotateHeldObject(Value.Get<FVector2D>());
}

bool UTOLGrabComponent::TryGrabFromPlayerView()
{
	if (HeldComponent) return false;
	FVector View; FRotator Rotation;
	if (!GetPlayerView(View, Rotation)) { Status(TEXT("Grab: wymagany lokalny gracz (singleplayer)."), true); return false; }
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOLGrabTrace), true, GetOwner());
	const FVector End = View + Rotation.Vector() * FMath::Max(1.f, MaxGrabDistance);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, View, End, ECC_Visibility, Params);
	if (bDrawDebug) DrawDebugLine(GetWorld(), View, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 1.f);
	if (!bHit) { Status(TEXT("Grab: brak obiektu w zasiegu."), true); return false; }
	return TryGrabHit(Hit);
}

bool UTOLGrabComponent::TryGrabHit(const FHitResult& Hit)
{
	if (HeldComponent || !Handle || Handle->GetGrabbedComponent()) return false;
	FVector View; FRotator ViewRotation;
	auto* Component = Hit.GetComponent();
	if (!GetPlayerView(View, ViewRotation) || !Hit.bBlockingHit || !IsValid(Component) || Component->GetOwner() == GetOwner()
		|| Hit.ImpactPoint.ContainsNaN() || FVector::Dist(View, Hit.ImpactPoint) > FMath::Max(1.f, MaxGrabDistance))
	{ Status(TEXT("Grab: nieprawidlowe trafienie lub poza zasiegiem."), true); return false; }
	if (Component->Mobility != EComponentMobility::Movable || !Component->IsSimulatingPhysics(Hit.BoneName))
	{ Status(TEXT("Grab: obiekt musi miec aktywna fizyke. Najpierw odetnij fragment."), true); return false; }
	const float Mass = Component->GetMass();
	if (!FMath::IsFinite(Mass) || Mass <= 0.f || Mass > FMath::Max(.01f, MaxGrabMassKg))
	{ Status(FString::Printf(TEXT("Grab: za ciezki (%.1f kg / limit %.1f kg)."), Mass, MaxGrabMassKg), true); return false; }
	// Check supplied hits too: callers cannot grab a body through a different blocking object.
	FHitResult Obstruction;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOLGrabValidate), true, GetOwner());
	const FVector Direction = (Hit.ImpactPoint - View).GetSafeNormal();
	if (!GetWorld()->LineTraceSingleByChannel(Obstruction, View, Hit.ImpactPoint + Direction * 2.f, ECC_Visibility, Params)
		|| Obstruction.GetComponent() != Component)
	{ Status(TEXT("Grab: obiekt zasloniety."), true); return false; }

	OldLinearStiffness = Handle->LinearStiffness; OldLinearDamping = Handle->LinearDamping;
	OldAngularStiffness = Handle->AngularStiffness; OldAngularDamping = Handle->AngularDamping;
	OldInterpolationSpeed = Handle->InterpolationSpeed; bOldInterpolate = Handle->bInterpolateTarget;
	const float MassFactor = FMath::Clamp(Mass / FMath::Max(.1f, ReferenceMassKg), 1.f, 100.f);
	const float ResponseFactor = FMath::Sqrt(MassFactor);
	Handle->SetLinearStiffness(FMath::Max(1.f, LinearStiffness) / MassFactor);
	Handle->SetLinearDamping(FMath::Max(1.f, LinearDamping) / ResponseFactor);
	Handle->SetAngularStiffness(FMath::Max(1.f, AngularStiffness) / MassFactor);
	Handle->SetAngularDamping(FMath::Max(1.f, AngularDamping) / ResponseFactor);
	Handle->bInterpolateTarget = true;
	Handle->SetInterpolationSpeed(12.f / ResponseFactor);
	Handle->GrabComponentAtLocationWithRotation(Component, Hit.BoneName, Hit.ImpactPoint, Component->GetComponentRotation());
	if (Handle->GetGrabbedComponent() != Component)
	{
		Handle->SetLinearStiffness(OldLinearStiffness); Handle->SetLinearDamping(OldLinearDamping);
		Handle->SetAngularStiffness(OldAngularStiffness); Handle->SetAngularDamping(OldAngularDamping);
		Handle->SetInterpolationSpeed(OldInterpolationSpeed); Handle->bInterpolateTarget = bOldInterpolate;
		Status(TEXT("Grab: nie mozna utworzyc chwytu."), true); return false;
	}
	HeldComponent = Component; HeldBone = Hit.BoneName; HeldMassKg = Mass;
	PreviousPawnResponse = Component->GetCollisionResponseToChannel(ECC_Pawn);
	Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Component->OnComponentPhysicsStateChanged.AddUniqueDynamic(this, &UTOLGrabComponent::OnHeldPhysicsChanged);
	LocalGrabPoint = Component->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
	HeldRotation = Component->GetComponentQuat();
	HoldDistance = FMath::Clamp(float(FVector::Dist(View, Hit.ImpactPoint)), FMath::Max(1.f, MinHoldDistance), FMath::Max(MinHoldDistance, MaxHoldDistance));
	TargetLocation = Hit.ImpactPoint;
	SetComponentTickEnabled(true);
	Status(FString::Printf(TEXT("Chwyt: %.1f kg | przytrzymaj srodkowy przycisk | R + mysz: obrot"), Mass));
	OnGrabChanged.Broadcast(Component, true);
	return true;
}

void UTOLGrabComponent::ReleaseGrab()
{
	SetRotating(false);
	UPrimitiveComponent* Previous = HeldComponent;
	if (Previous)
	{
		Previous->OnComponentPhysicsStateChanged.RemoveDynamic(this, &UTOLGrabComponent::OnHeldPhysicsChanged);
		if (Handle && Handle->GetGrabbedComponent() == Previous) Handle->ReleaseComponent();
		if (IsValid(Previous)) Previous->SetCollisionResponseToChannel(ECC_Pawn, PreviousPawnResponse);
		if (Handle)
		{
			Handle->SetLinearStiffness(OldLinearStiffness); Handle->SetLinearDamping(OldLinearDamping);
			Handle->SetAngularStiffness(OldAngularStiffness); Handle->SetAngularDamping(OldAngularDamping);
			Handle->SetInterpolationSpeed(OldInterpolationSpeed); Handle->bInterpolateTarget = bOldInterpolate;
		}
	}
	HeldComponent = nullptr; HeldMassKg = 0.f; HoldDistance = 0.f; HeldBone = NAME_None;
	SetComponentTickEnabled(false);
	if (Previous) { Status(TEXT("Grab: puszczono obiekt.")); OnGrabChanged.Broadcast(Previous, false); }
}

void UTOLGrabComponent::SetRotating(bool bEnabled)
{
	bEnabled = bEnabled && IsValid(HeldComponent);
	if (bRotating == bEnabled) return;
	bRotating = bEnabled;
	if (bEnabled)
	{
		if (const auto* Pawn = Cast<APawn>(GetOwner()))
			if (auto* PC = Cast<APlayerController>(Pawn->GetController())) { LookLockController = PC; PC->SetIgnoreLookInput(true); }
	}
	else
	{
		// Balance our own lock only. Never reset another system's ignore-look stack.
		if (auto* PC = LookLockController.Get()) PC->SetIgnoreLookInput(false);
		LookLockController.Reset();
	}
}

void UTOLGrabComponent::RotateHeldObject(FVector2D MouseDelta)
{
	if (!bRotating || !FMath::IsFinite(MouseDelta.X) || !FMath::IsFinite(MouseDelta.Y)) return;
	FVector View; FRotator Rotation;
	if (!GetPlayerView(View, Rotation)) return;
	const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(MouseDelta.X * RotationDegreesPerUnit));
	const FQuat Pitch(FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y), FMath::DegreesToRadians(-MouseDelta.Y * RotationDegreesPerUnit));
	HeldRotation = (Yaw * Pitch * HeldRotation).GetNormalized();
}

void UTOLGrabComponent::AdjustHoldDistance(float DeltaCentimeters)
{
	if (HeldComponent && FMath::IsFinite(DeltaCentimeters))
		HoldDistance = FMath::Clamp(HoldDistance + DeltaCentimeters, FMath::Max(1.f, MinHoldDistance), FMath::Max(MinHoldDistance, MaxHoldDistance));
}

void UTOLGrabComponent::OnTreeCut(const FTOLCutTraceResult& Result, ATOLCutFragment* Fragment)
{
	// A recut rebuilds the body's collision/constraint. Release instead of retaining a stale physics handle.
	if (HeldComponent && Result.Hit.GetComponent() == HeldComponent) ReleaseGrab();
}

void UTOLGrabComponent::OnHeldPhysicsChanged(UPrimitiveComponent* Component, EComponentPhysicsStateChange StateChange)
{
	if (Component == HeldComponent && StateChange == EComponentPhysicsStateChange::Destroyed) ReleaseGrab();
}

void UTOLGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	FVector View; FRotator Rotation;
	if (!IsValid(HeldComponent) || !Handle || Handle->GetGrabbedComponent() != HeldComponent
		|| !HeldComponent->IsSimulatingPhysics(HeldBone) || !GetPlayerView(View, Rotation))
	{ ReleaseGrab(); return; }
	const FVector CurrentGrab = HeldComponent->GetComponentTransform().TransformPosition(LocalGrabPoint);
	if (FVector::Dist(View, CurrentGrab) > FMath::Max(1.f, MaxGrabDistance) + FMath::Max(1.f, BreakDistance))
	{ ReleaseGrab(); Status(TEXT("Grab: puszczono - obiekt poza zasiegiem."), true); return; }
	TargetLocation = View + Rotation.Vector() * HoldDistance;
	// Sweep the complete candidate bounds, not just the grip point: the body can extend beyond the hand.
	const FTransform Candidate(HeldRotation, FVector::ZeroVector, HeldComponent->GetComponentScale());
	const FBoxSphereBounds Bounds = HeldComponent->CalcBounds(Candidate);
	const FVector CenterOffset = Bounds.Origin - Candidate.TransformPosition(LocalGrabPoint);
	FHitResult Wall;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOLGrabTarget), false, GetOwner());
	Params.AddIgnoredActor(HeldComponent->GetOwner());
	if (GetWorld()->SweepSingleByChannel(Wall, View + CenterOffset, TargetLocation + CenterOffset, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeBox(Bounds.BoxExtent + FVector(2.f)), Params))
	{
		if (Wall.bStartPenetrating) { ReleaseGrab(); Status(TEXT("Grab: brak miejsca na przenoszony obiekt."), true); return; }
		TargetLocation = Wall.Location - CenterOffset + Wall.ImpactNormal * 2.f;
	}
	if (FVector::Dist(CurrentGrab, TargetLocation) > FMath::Max(1.f, BreakDistance))
	{ ReleaseGrab(); Status(TEXT("Grab: puszczono - przekroczono odleglosc chwytu."), true); return; }
	Handle->SetTargetLocationAndRotation(TargetLocation, HeldRotation.Rotator());
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), TargetLocation, 5.f, 8, FColor::Cyan);
		DrawDebugLine(GetWorld(), CurrentGrab, TargetLocation, FColor::Yellow);
	}
}
