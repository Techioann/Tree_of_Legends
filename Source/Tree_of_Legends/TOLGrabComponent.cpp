#include "TOLGrabComponent.h"
#include "TOLSprintComponent.h"

#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
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
	// Frame order while holding: controller input -> movement -> this (mass rate limit) -> spring arm/camera.
	// Otherwise the camera renders the unlimited turn first and is pulled back a frame later (visible flicker).
	if (auto* Move = GetOwner()->FindComponentByClass<UPawnMovementComponent>()) AddTickPrerequisiteComponent(Move);
	if (auto* Arm = GetOwner()->FindComponentByClass<USpringArmComponent>()) Arm->AddTickPrerequisiteComponent(this);
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
	if (auto* Move = GetOwner()->FindComponentByClass<UPawnMovementComponent>()) RemoveTickPrerequisiteComponent(Move);
	if (auto* Arm = GetOwner()->FindComponentByClass<USpringArmComponent>()) Arm->RemoveTickPrerequisiteComponent(this);
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
	// Use this frame's state, not the camera manager's cached view from the previous frame: the control rotation
	// already contains this frame's mouse input, and the camera component has already followed this frame's movement.
	PC->GetPlayerViewPoint(Location, Rotation);
	Rotation = PC->GetControlRotation();
	if (const auto* Camera = Pawn->FindComponentByClass<UCameraComponent>()) Location = Camera->GetComponentLocation();
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
	AddTickPrerequisiteActor(PC); // mouse look must be applied to the control rotation before the rate limit runs
	RotateAction = NewObject<UInputAction>(this);
	RotateAction->ValueType = EInputActionValueType::Boolean;
	RotateAction->bConsumeInput = false;
	RotateMapping = NewObject<UInputMappingContext>(this);
	RotateMapping->MapKey(RotateAction, RotateKey);
	DistanceAction = NewObject<UInputAction>(this);
	DistanceAction->ValueType = EInputActionValueType::Axis1D;
	DistanceAction->bConsumeInput = false;
	RotateMapping->MapKey(DistanceAction, EKeys::MouseWheelAxis);
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
	ActionBindings.Add(Input->BindAction(DistanceAction, ETriggerEvent::Triggered, this, &UTOLGrabComponent::InputDistance).GetHandle());
	bInputBound = true;
}

void UTOLGrabComponent::UnbindInput()
{
	if (auto* Input = BoundInput.Get())
	{
		for (uint32 Id : ActionBindings) Input->RemoveBindingByHandle(Id);
	}
	if (auto* Subsystem = InputSubsystem.Get()) if (RotateMapping) Subsystem->RemoveMappingContext(RotateMapping);
	if (const auto* Pawn = Cast<APawn>(GetOwner())) if (auto* Controller = Pawn->GetController()) RemoveTickPrerequisiteActor(Controller);
	InputSubsystem.Reset(); RotateMapping = nullptr; RotateAction = nullptr; DistanceAction = nullptr;
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
// Wheel up (+) pushes the object away, wheel down (-) pulls it closer; Tick glides HoldDistance there linearly.
void UTOLGrabComponent::InputDistance(const FInputActionValue& Value)
{
	const float Notches = Value.Get<float>();
	if (HeldComponent && FMath::IsFinite(Notches) && !FMath::IsNearlyZero(Notches)) AdjustHoldDistance(Notches * FMath::Max(1.f, WheelStepCm));
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
	// Chaos handle drives are acceleration based, so heaviness is faked by softening the drive with mass.
	// Damping scales with sqrt(stiffness) to keep the damping ratio constant: heavy bodies respond slower,
	// but are not overdamped (the old /MassFactor stiffness vs /sqrt damping gave a ratio of ~7 at 150 kg).
	const float MassFactor = FMath::Clamp(Mass / FMath::Max(.1f, ReferenceMassKg), 1.f, 100.f);
	const float StiffnessScale = 1.f / FMath::Pow(MassFactor, .75f);
	const float DampingScale = FMath::Sqrt(StiffnessScale);
	Handle->SetLinearStiffness(FMath::Max(1.f, LinearStiffness) * StiffnessScale);
	Handle->SetLinearDamping(FMath::Max(1.f, LinearDamping) * DampingScale);
	Handle->SetAngularStiffness(FMath::Max(1.f, AngularStiffness) * StiffnessScale);
	Handle->SetAngularDamping(FMath::Max(1.f, AngularDamping) * DampingScale);
	Handle->bInterpolateTarget = true;
	Handle->SetInterpolationSpeed(24.f * FMath::Sqrt(DampingScale));
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
	DesiredHoldDistance = HoldDistance;
	HoldDirection = ViewRotation.Vector();
	// Carrying is as heavy for the legs as for the camera: slow walking down with mass, restored in ReleaseGrab.
	if (auto* Move = GetOwner()->FindComponentByClass<UCharacterMovementComponent>())
	{
		// No sprinting while carrying: drop the sprint factor first, so the speed restored on release is the walk speed.
		if (auto* Sprint = GetOwner()->FindComponentByClass<UTOLSprintComponent>(); Sprint && !Sprint->bAllowSprintWhileCarrying)
			Sprint->StopSprintSpeed();
		WalkSpeedBeforeCarry = Move->MaxWalkSpeed;
		Move->MaxWalkSpeed *= FMath::Max(FMath::Clamp(MinCarrySpeedScale, .05f, 1.f), FMath::Pow(MassFactor, -FMath::Max(0.f, CarrySpeedMassExponent)));
		// Jump height (not velocity) falls from MaxCarryJumpHeightScale * normal at ReferenceMassKg
		// to MinCarryJumpHeightCm at NoJumpMassKg, linearly in log(mass). h = v^2 / 2g.
		// At NoJumpMassKg and above (or always, with bDisableJumpWhileCarrying) the player cannot jump at all.
		JumpZBeforeCarry = Move->JumpZVelocity;
		if (bDisableJumpWhileCarrying || Mass >= NoJumpMassKg)
		{
			bJumpAllowedBeforeCarry = Move->IsJumpAllowed();
			Move->SetJumpAllowed(false);
			bJumpDisabledByCarry = true;
		}
		const float Gravity = FMath::Max(1.f, FMath::Abs(Move->GetGravityZ()));
		const float NormalJumpHeight = FMath::Square(JumpZBeforeCarry) / (2.f * Gravity);
		const float MassRange = FMath::Max(1.01f, FMath::Min(NoJumpMassKg, MaxGrabMassKg) / FMath::Max(.1f, ReferenceMassKg));
		const float HeavyT = FMath::Clamp(FMath::Loge(MassFactor) / FMath::Loge(MassRange), 0.f, 1.f);
		const float LightJumpHeight = NormalJumpHeight * FMath::Clamp(MaxCarryJumpHeightScale, 0.f, 1.f);
		const float JumpHeight = FMath::Lerp(LightJumpHeight, FMath::Min(FMath::Max(0.f, MinCarryJumpHeightCm), LightJumpHeight), HeavyT);
		Move->JumpZVelocity = FMath::Sqrt(2.f * Gravity * JumpHeight);
	}
	TargetLocation = Hit.ImpactPoint;
	SetComponentTickEnabled(true);
	Status(FString::Printf(TEXT("Chwyt: %.1f kg | przytrzymaj srodkowy przycisk | R + mysz: obrot | kolko: dalej/blizej"), Mass));
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
	if (WalkSpeedBeforeCarry > 0.f)
	{
		if (auto* Move = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterMovementComponent>() : nullptr)
			Move->MaxWalkSpeed = WalkSpeedBeforeCarry;
		WalkSpeedBeforeCarry = 0.f;
	}
	if (JumpZBeforeCarry >= 0.f)
	{
		if (auto* Move = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterMovementComponent>() : nullptr)
			Move->JumpZVelocity = JumpZBeforeCarry;
		JumpZBeforeCarry = -1.f;
	}
	if (bJumpDisabledByCarry)
	{
		if (auto* Move = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterMovementComponent>() : nullptr)
			Move->SetJumpAllowed(bJumpAllowedBeforeCarry);
		bJumpDisabledByCarry = false;
	}
	HeldComponent = nullptr; HeldMassKg = 0.f; HoldDistance = 0.f; DesiredHoldDistance = 0.f; HeldBone = NAME_None;
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
	// Changes the desired distance only; TickComponent moves HoldDistance toward it at a constant, mass-scaled speed.
	if (HeldComponent && FMath::IsFinite(DeltaCentimeters))
		DesiredHoldDistance = FMath::Clamp(DesiredHoldDistance + DeltaCentimeters, FMath::Max(1.f, MinHoldDistance), FMath::Max(MinHoldDistance, MaxHoldDistance));
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
	// Chaos handle drives are acceleration based, so the drive alone would whip a 250 kg log around a fast mouse
	// flick like a 5 kg one. Limit how fast the hold point may swing around the view, falling with sqrt(mass).
	const float MassFactor = FMath::Clamp(HeldMassKg / FMath::Max(.1f, ReferenceMassKg), 1.f, 100.f);
	const float MaxSwingRad = FMath::DegreesToRadians(540.f) / FMath::Sqrt(MassFactor);
	// Wheel distance: constant-speed (linear) glide toward the desired distance, slower for heavier objects.
	HoldDistance = FMath::FInterpConstantTo(HoldDistance, DesiredHoldDistance, DeltaTime,
		FMath::Max(1.f, HoldDistanceSpeed) / FMath::Pow(MassFactor, .25f));
	const FVector DesiredDir = Rotation.Vector();
	// Previous direction, not (TargetLocation - View): moving the view sideways (A/D) must not count as turning.
	FVector HoldDir = HoldDirection.GetSafeNormal();
	if (HoldDir.IsNearlyZero()) HoldDir = DesiredDir;
	const float SwingAngle = FMath::Acos(FMath::Clamp(float(HoldDir | DesiredDir), -1.f, 1.f));
	const float MaxStep = MaxSwingRad * FMath::Max(0.f, DeltaTime);
	if (SwingAngle <= MaxStep) HoldDir = DesiredDir;
	else
	{
		HoldDir = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(HoldDir, DesiredDir), MaxStep / SwingAngle).RotateVector(HoldDir);
		// The camera is just as heavy: pull the control rotation back by the part of the turn the object could not
		// follow, so the view turns at the same mass-limited rate as the held object.
		if (auto* Pawn = Cast<APawn>(GetOwner()))
			if (auto* PC = Cast<APlayerController>(Pawn->GetController()))
			{
				FRotator Control = (FQuat::FindBetweenNormals(DesiredDir, HoldDir) * PC->GetControlRotation().Quaternion()).Rotator();
				Control.Roll = 0.f;
				PC->SetControlRotation(Control);
				Pawn->FaceRotation(Control, DeltaTime); // keep the body yaw in step with the limited view
			}
	}
	HoldDirection = HoldDir;
	TargetLocation = View + HoldDir * HoldDistance;
	// Keep only the grip target out of walls/terrain. The held body collides physically on its own; sweeping its
	// whole AABB from the camera started inside the terrain for any log and dropped the grab on the next frame.
	FHitResult Wall;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOLGrabTarget), false, GetOwner());
	Params.AddIgnoredActor(HeldComponent->GetOwner());
	if (GetWorld()->SweepSingleByChannel(Wall, View, TargetLocation, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(10.f), Params) && !Wall.bStartPenetrating)
		TargetLocation = Wall.Location;
	if (FVector::Dist(CurrentGrab, TargetLocation) > FMath::Max(1.f, BreakDistance))
	{ ReleaseGrab(); Status(TEXT("Grab: puszczono - przekroczono odleglosc chwytu."), true); return; }
	Handle->SetTargetLocationAndRotation(TargetLocation, HeldRotation.Rotator());
	// Cap the body's speed relative to the carrier and its spin, so flicks (and releasing mid-swing) cannot fling it.
	const FVector CarrierVelocity = GetOwner()->GetVelocity();
	const float MaxRelativeSpeed = MaxSwingRad * HoldDistance + 150.f;
	const FVector Relative = HeldComponent->GetPhysicsLinearVelocity(HeldBone) - CarrierVelocity;
	if (Relative.SizeSquared() > FMath::Square(MaxRelativeSpeed))
		HeldComponent->SetPhysicsLinearVelocity(CarrierVelocity + Relative.GetClampedToMaxSize(MaxRelativeSpeed), false, HeldBone);
	const float MaxSpinDeg = FMath::RadiansToDegrees(MaxSwingRad) * 1.5f;
	const FVector Spin = HeldComponent->GetPhysicsAngularVelocityInDegrees(HeldBone);
	if (Spin.SizeSquared() > FMath::Square(MaxSpinDeg))
		HeldComponent->SetPhysicsAngularVelocityInDegrees(Spin.GetClampedToMaxSize(MaxSpinDeg), false, HeldBone);
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), TargetLocation, 5.f, 8, FColor::Cyan);
		DrawDebugLine(GetWorld(), CurrentGrab, TargetLocation, FColor::Yellow);
	}
}
