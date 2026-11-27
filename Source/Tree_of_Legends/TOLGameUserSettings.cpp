#include "TOLGameUserSettings.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "TOLSettingsConfig.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	TStrongObjectPtr<USoundMix>& VolumeMix()
	{
		static TStrongObjectPtr<USoundMix> Mix;
		if (!Mix.IsValid())
			Mix.Reset(NewObject<USoundMix>(GetTransientPackage(), TEXT("TOLVolumeMix")));
		return Mix;
	}

	void SetCVarInt(const TCHAR* Name, int32 Value, EConsoleVariableFlags Priority = ECVF_SetByGameSetting)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
			CVar->Set(Value, Priority);
	}

	void SetCVarFloat(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
			CVar->Set(Value, ECVF_SetByGameSetting);
	}

	/** r.AntiAliasingMethod as the project configured it, read before this class first overrides it. */
	int32 ProjectAntiAliasingMethod()
	{
		static int32 Value = INDEX_NONE;
		if (Value == INDEX_NONE)
		{
			const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
			Value = CVar ? CVar->GetInt() : 4;
		}
		return Value;
	}
}

UTOLGameUserSettings* UTOLGameUserSettings::Get()
{
	return Cast<UTOLGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

void UTOLGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MasterVolume = MusicVolume = EffectsVolume = AmbientVolume = VoiceVolume = 1.f;
	bMuteWhenUnfocused = false;
	MouseSensitivity = 1.f;
	bInvertMouseY = false;
	FieldOfView = 90.f;
	Brightness = 0.5f;
	AntiAliasingMethod = -1;
	bMotionBlur = true;
	bShowFPS = false;
	KeyOverrides.Reset();
}

void UTOLGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	ApplyCustomSettings();
}

void UTOLGameUserSettings::ApplyCustomSettings()
{
	ApplyAudio();
	ApplyBrightness();
	// The project sets r.AntiAliasingMethod in Project Settings (SetByProjectSetting), which outranks
	// SetByGameSetting, so a game-setting write was silently ignored. GameOverride is the priority meant for this.
	SetCVarInt(TEXT("r.AntiAliasingMethod"), GetEffectiveAntiAliasingMethod(), ECVF_SetByGameOverride);
	++ChangeCounter;
}

int32 UTOLGameUserSettings::GetEffectiveAntiAliasingMethod() const
{
	// Always read the project value first, so it is cached before our own override changes the cvar.
	const int32 ProjectMethod = ProjectAntiAliasingMethod();
	return AntiAliasingMethod >= 0 ? AntiAliasingMethod : ProjectMethod;
}

void UTOLGameUserSettings::ApplyBrightness() const
{
	// r.TonemapperGamma 0 keeps the engine's display gamma (2.2); larger values brighten the picture.
	const float Offset = FMath::Clamp(Brightness, 0.f, 1.f) - 0.5f;
	SetCVarFloat(TEXT("r.TonemapperGamma"), FMath::Abs(Offset) < 0.01f ? 0.f : 2.2f + Offset * 1.6f);
}

void UTOLGameUserSettings::ApplyAudio() const
{
	FApp::SetUnfocusedVolumeMultiplier(bMuteWhenUnfocused ? 0.f : 1.f);
	FAudioDeviceManager* Manager = FAudioDeviceManager::Get();
	if (!Manager) return;

	const UTOLSettingsConfig* Config = GetDefault<UTOLSettingsConfig>();
	TArray<TPair<USoundClass*, float>> Classes;
	auto AddClass = [&Classes](const TSoftObjectPtr<USoundClass>& Class, float Volume)
	{
		if (USoundClass* Loaded = Class.IsNull() ? nullptr : Class.LoadSynchronous())
			Classes.Emplace(Loaded, FMath::Clamp(Volume, 0.f, 1.f));
	};
	AddClass(Config->MusicClass, MusicVolume);
	AddClass(Config->EffectsClass, EffectsVolume);
	AddClass(Config->AmbientClass, AmbientVolume);
	AddClass(Config->VoiceClass, VoiceVolume);
	USoundMix* Mix = Classes.Num() > 0 ? VolumeMix().Get() : nullptr;

	const float Master = FMath::Clamp(MasterVolume, 0.f, 1.f);
	Manager->IterateOverAllDevices([Master, Mix, &Classes](Audio::FDeviceId, FAudioDevice* Device)
	{
		if (!Device) return;
		Device->SetTransientPrimaryVolume(Master);
		if (!Mix) return;
		for (const TPair<USoundClass*, float>& Entry : Classes)
			Device->SetSoundMixClassOverride(Mix, Entry.Key, Entry.Value, 1.f, 0.f, true);
		Device->PushSoundMixModifier(Mix);
	});
}

const FKey* UTOLGameUserSettings::FindKeyOverride(FName Action, int32 Slot) const
{
	for (const FTOLKeyOverride& Override : KeyOverrides)
		if (Override.Action == Action && Override.Slot == Slot) return &Override.Key;
	return nullptr;
}

void UTOLGameUserSettings::SetKeyOverride(FName Action, int32 Slot, const FKey& Key, const FKey& DefaultKey)
{
	KeyOverrides.RemoveAll([&](const FTOLKeyOverride& Override) { return Override.Action == Action && Override.Slot == Slot; });
	if (Key != DefaultKey)
	{
		FTOLKeyOverride& Override = KeyOverrides.AddDefaulted_GetRef();
		Override.Action = Action;
		Override.Slot = Slot;
		Override.Key = Key;
	}
}
