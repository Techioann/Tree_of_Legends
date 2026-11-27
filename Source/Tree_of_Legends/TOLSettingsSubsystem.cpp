#include "TOLSettingsSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "TOLGameUserSettings.h"
#include "TOLSettingsConfig.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Small smoothed FPS readout; updates its text twice a second. */
	class STOLFpsCounter : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(STOLFpsCounter) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&)
		{
			FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 15);
			ChildSlot
			.HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 14.f, 18.f, 0.f)
			[
				SAssignNew(Text, STextBlock).Font(Font).ColorAndOpacity(FLinearColor(0.97f, 0.93f, 0.84f, 0.9f))
				.ShadowOffset(FVector2D(1.0, 1.0)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f))
			];
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
		{
			SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
			Frames++;
			Accum += FApp::GetDeltaTime();
			if (Accum < 0.5) return;
			const int32 Fps = FMath::RoundToInt(Frames / Accum);
			const double Ms = 1000.0 * Accum / Frames;
			Text->SetText(FText::FromString(FString::Printf(TEXT("%d FPS  %.1f ms"), Fps, Ms)));
			Text->SetColorAndOpacity(Fps >= 50 ? FLinearColor(0.97f, 0.93f, 0.84f, 0.9f)
				: Fps >= 30 ? FLinearColor(1.f, 0.72f, 0.28f, 0.95f) : FLinearColor(1.f, 0.35f, 0.25f, 0.95f));
			Frames = 0;
			Accum = 0.0;
		}

	private:
		TSharedPtr<STextBlock> Text;
		int32 Frames = 0;
		double Accum = 0.0;
	};

	/** Direction an Axis2D/3D mapping pushes in, after its swizzle/negate modifiers. */
	FText DirectionLabel(const FEnhancedActionKeyMapping& Mapping)
	{
		FVector V(1.0, 0.0, 0.0);
		for (const UInputModifier* Modifier : Mapping.Modifiers)
		{
			if (const UInputModifierSwizzleAxis* Swizzle = Cast<UInputModifierSwizzleAxis>(Modifier))
			{
				switch (Swizzle->Order)
				{
				case EInputAxisSwizzle::YXZ: V = FVector(V.Y, V.X, V.Z); break;
				case EInputAxisSwizzle::ZYX: V = FVector(V.Z, V.Y, V.X); break;
				case EInputAxisSwizzle::XZY: V = FVector(V.X, V.Z, V.Y); break;
				case EInputAxisSwizzle::YZX: V = FVector(V.Y, V.Z, V.X); break;
				case EInputAxisSwizzle::ZXY: V = FVector(V.Z, V.X, V.Y); break;
				default: break;
				}
			}
			else if (const UInputModifierNegate* Negate = Cast<UInputModifierNegate>(Modifier))
			{
				V = FVector(Negate->bX ? -V.X : V.X, Negate->bY ? -V.Y : V.Y, Negate->bZ ? -V.Z : V.Z);
			}
		}
		if (V.Y > 0.5) return INVTEXT("Forward");
		if (V.Y < -0.5) return INVTEXT("Backward");
		if (V.X < -0.5) return INVTEXT("Left");
		if (V.Z > 0.5) return INVTEXT("Up");
		if (V.Z < -0.5) return INVTEXT("Down");
		return INVTEXT("Right");
	}

	bool IsRebindableKey(const FKey& Key)
	{
		return Key.IsValid() && !Key.IsGamepadKey() && !Key.IsTouch() && !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D();
	}
}

void UTOLSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UTOLGameUserSettings* Settings = UTOLGameUserSettings::Get())
		Settings->ApplyCustomSettings();
}

void UTOLSettingsSubsystem::Deinitialize()
{
	UpdateFpsCounter(false);
	Super::Deinitialize();
}

ETickableTickType UTOLSettingsSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Always;
}

UWorld* UTOLSettingsSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

TStatId UTOLSettingsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTOLSettingsSubsystem, STATGROUP_Tickables);
}

void UTOLSettingsSubsystem::Tick(float DeltaTime)
{
	SinceUpdate += DeltaTime;
	if (SinceUpdate < 0.2f) return;
	SinceUpdate = 0.f;

	const UTOLGameUserSettings* Settings = UTOLGameUserSettings::Get();
	UWorld* World = GetTickableGameObjectWorld();
	if (!Settings || !World) return;

	const int32 Counter = Settings->GetChangeCounter();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		if (APlayerController* Controller = It->Get(); Controller && Controller->IsLocalPlayerController())
		{
			UpdatePlayer(Controller, Counter);
			UpdateMappingContext(Controller, Counter);
			break; // one local player
		}
	UpdateFpsCounter(Settings->bShowFPS);
}

void UTOLSettingsSubsystem::UpdatePlayer(APlayerController* Controller, int32 Counter)
{
	const UTOLGameUserSettings* Settings = UTOLGameUserSettings::Get();

	// Mouse: BP_Character feeds IA_Mouse into AddController{Yaw,Pitch}Input, which the legacy scales multiply.
	if (GetDefault<UInputSettings>()->bEnableLegacyInputScales)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const APlayerController* Defaults = Controller->GetClass()->GetDefaultObject<APlayerController>();
		const float Sensitivity = FMath::Clamp(Settings->MouseSensitivity, 0.05f, 10.f);
		const float Yaw = Defaults->GetDeprecatedInputYawScale() * Sensitivity;
		const float Pitch = Defaults->GetDeprecatedInputPitchScale() * Sensitivity * (Settings->bInvertMouseY ? -1.f : 1.f);
		if (Controller->GetDeprecatedInputYawScale() != Yaw) Controller->SetDeprecatedInputYawScale(Yaw);
		if (Controller->GetDeprecatedInputPitchScale() != Pitch) Controller->SetDeprecatedInputPitchScale(Pitch);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	// Camera: field of view and motion blur, once per pawn and whenever the settings change.
	APawn* Pawn = Controller->GetPawn();
	if (!Pawn || (Pawn == CameraPawn.Get() && Counter == CameraCounter)) return;
	CameraPawn = Pawn;
	CameraCounter = Counter;
	const UTOLSettingsConfig* Config = GetDefault<UTOLSettingsConfig>();
	const float Fov = FMath::Clamp(Settings->FieldOfView, FMath::Min(Config->MinFieldOfView, Config->MaxFieldOfView), FMath::Max(Config->MinFieldOfView, Config->MaxFieldOfView));
	TInlineComponentArray<UCameraComponent*> Cameras(Pawn);
	for (UCameraComponent* Camera : Cameras)
	{
		Camera->SetFieldOfView(Fov);
		FPostProcessSettings& Post = Camera->PostProcessSettings;
		if (!Settings->bMotionBlur)
		{
			Post.bOverride_MotionBlurAmount = true;
			Post.MotionBlurAmount = 0.f;
		}
		else if (Post.bOverride_MotionBlurAmount && Post.MotionBlurAmount == 0.f)
		{
			Post.bOverride_MotionBlurAmount = false; // undo our own override
		}
	}
}

void UTOLSettingsSubsystem::UpdateMappingContext(APlayerController* Controller, int32 Counter)
{
	const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UTOLSettingsConfig* Config = GetDefault<UTOLSettingsConfig>();
	UInputMappingContext* Original = Input && !Config->RebindableContext.IsNull() ? Config->RebindableContext.LoadSynchronous() : nullptr;
	const UTOLGameUserSettings* Settings = UTOLGameUserSettings::Get();
	if (!Original || !Settings) return;

	int32 Priority = 0;
	if (!RuntimeContext || RuntimeContextCounter != Counter)
	{
		// Rebuild the copy with the player's keys; if the old copy is active, put the new one in its place.
		UInputMappingContext* Copy = DuplicateObject<UInputMappingContext>(Original, this);
		for (const FTOLBindingSlot& Slot : BuildBindingSlots(Original))
			if (const FKey* Key = Settings->FindKeyOverride(Slot.Action, Slot.Slot))
				Copy->GetMapping(Slot.MappingIndex).Key = *Key;
		if (RuntimeContext && Input->HasMappingContext(RuntimeContext, Priority))
		{
			Input->RemoveMappingContext(RuntimeContext);
			Input->AddMappingContext(Copy, Priority);
		}
		RuntimeContext = Copy;
		RuntimeContextCounter = Counter;
	}

	// Without rebound keys the original asset is used as is.
	UInputMappingContext* Wanted = Settings->KeyOverrides.Num() > 0 ? RuntimeContext.Get() : Original;
	UInputMappingContext* Other = Wanted == Original ? RuntimeContext.Get() : Original;
	if (Other && Input->HasMappingContext(Other, Priority))
	{
		Input->RemoveMappingContext(Other);
		if (!Input->HasMappingContext(Wanted)) Input->AddMappingContext(Wanted, Priority);
	}
}

void UTOLSettingsSubsystem::UpdateFpsCounter(bool bShow)
{
	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	UWorld* World = GetTickableGameObjectWorld();
	const bool bStale = FpsWidget.IsValid() && (!bShow || FpsViewport.Get() != Viewport || FpsWorld.Get() != World);
	if (bStale)
	{
		if (UGameViewportClient* Old = FpsViewport.Get()) Old->RemoveViewportWidgetContent(FpsWidget.ToSharedRef());
		FpsWidget.Reset();
	}
	if (bShow && !FpsWidget.IsValid() && Viewport)
	{
		FpsWidget = SNew(STOLFpsCounter);
		Viewport->AddViewportWidgetContent(FpsWidget.ToSharedRef(), 1000);
		FpsViewport = Viewport;
		FpsWorld = World;
	}
}

TArray<FTOLBindingSlot> UTOLSettingsSubsystem::BuildBindingSlots(const UInputMappingContext* Context)
{
	TArray<FTOLBindingSlot> Slots;
	if (!Context) return Slots;

	TSet<FName> Hidden;
	for (const TSoftObjectPtr<UInputAction>& Action : GetDefault<UTOLSettingsConfig>()->HiddenActions)
		if (!Action.IsNull()) Hidden.Add(FName(*Action.ToSoftObjectPath().GetAssetName()));

	TMap<FName, int32> Ordinals;
	const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
	for (int32 Index = 0; Index < Mappings.Num(); ++Index)
	{
		const FEnhancedActionKeyMapping& Mapping = Mappings[Index];
		if (!Mapping.Action) continue;
		const FName ActionName = Mapping.Action->GetFName();
		const int32 Ordinal = Ordinals.FindOrAdd(ActionName)++;
		if (Hidden.Contains(ActionName) || !IsRebindableKey(Mapping.Key)) continue;

		FTOLBindingSlot& Slot = Slots.AddDefaulted_GetRef();
		Slot.Action = ActionName;
		Slot.Slot = Ordinal;
		Slot.MappingIndex = Index;
		Slot.DefaultKey = Mapping.Key;

		FText Name = Mapping.Action->ActionDescription;
		if (Name.IsEmpty())
		{
			FString Plain = ActionName.ToString();
			Plain.RemoveFromStart(TEXT("IA_"));
			Name = FText::FromString(FName::NameToDisplayString(Plain, false));
		}
		const EInputActionValueType Type = Mapping.Action->ValueType;
		Slot.Label = (Type == EInputActionValueType::Axis2D || Type == EInputActionValueType::Axis3D)
			? FText::Format(INVTEXT("{0} ({1})"), Name, DirectionLabel(Mapping)) : Name;
	}
	return Slots;
}
