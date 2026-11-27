#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "TOLGameUserSettings.generated.h"

/** One rebound key: the Nth keyboard/mouse mapping of an Input Action in the rebindable mapping context. */
USTRUCT()
struct FTOLKeyOverride
{
	GENERATED_BODY()

	/** Input Action asset name, e.g. IA_Jump. */
	UPROPERTY(Config)
	FName Action;

	/** Index among this action's mappings in the context (counted over all of them, gamepad included). */
	UPROPERTY(Config)
	int32 Slot = 0;

	UPROPERTY(Config)
	FKey Key;
};

/**
 * Game user settings (Saved/Config/<Platform>/GameUserSettings.ini). Adds audio, camera, mouse, brightness,
 * anti-aliasing, motion blur, FPS counter and key binding options to the engine's video/scalability settings.
 * Registered through GameUserSettingsClassName in DefaultEngine.ini. Edited by the Settings menu
 * (UTOLSettingsWidget) and applied in game by UTOLSettingsSubsystem.
 */
UCLASS()
class TREE_OF_LEGENDS_API UTOLGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
public:
	static UTOLGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	/** Applies everything that does not need a world (volumes, brightness, anti-aliasing method).
	 * Camera, mouse and key settings are picked up by UTOLSettingsSubsystem through ChangeCounter. */
	void ApplyCustomSettings();

	/** Applies only the audio volumes (live preview while dragging a slider). */
	void ApplyAudio() const;

	/** Applies only the brightness (live preview). */
	void ApplyBrightness() const;

	/** Effective anti-aliasing method: 0 off, 1 FXAA, 2 TAA, 4 TSR. */
	int32 GetEffectiveAntiAliasingMethod() const;

	/** Increments every time the custom settings are applied. */
	int32 GetChangeCounter() const { return ChangeCounter; }

	const FKey* FindKeyOverride(FName Action, int32 Slot) const;
	void SetKeyOverride(FName Action, int32 Slot, const FKey& Key, const FKey& DefaultKey);

	// Audio, 0..1.
	UPROPERTY(Config) float MasterVolume = 1.f;
	UPROPERTY(Config) float MusicVolume = 1.f;
	UPROPERTY(Config) float EffectsVolume = 1.f;
	UPROPERTY(Config) float AmbientVolume = 1.f;
	UPROPERTY(Config) float VoiceVolume = 1.f;
	UPROPERTY(Config) bool bMuteWhenUnfocused = false;

	// Controls and camera.
	UPROPERTY(Config) float MouseSensitivity = 1.f;
	UPROPERTY(Config) bool bInvertMouseY = false;
	UPROPERTY(Config) float FieldOfView = 90.f;

	// Picture.
	/** 0..1, 0.5 = neutral. */
	UPROPERTY(Config) float Brightness = 0.5f;
	/** -1 = project default, otherwise r.AntiAliasingMethod (0 off, 1 FXAA, 2 TAA, 4 TSR). */
	UPROPERTY(Config) int32 AntiAliasingMethod = -1;
	UPROPERTY(Config) bool bMotionBlur = true;
	UPROPERTY(Config) bool bShowFPS = false;

	UPROPERTY(Config) TArray<FTOLKeyOverride> KeyOverrides;

private:
	int32 ChangeCounter = 0;
};
