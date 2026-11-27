#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TOLSettingsConfig.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundClass;

/** Project Settings > Game > Settings Menu. Edited values are saved to Config/DefaultGame.ini. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Settings Menu"))
class TREE_OF_LEGENDS_API UTOLSettingsConfig : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UTOLSettingsConfig();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Mapping context whose keyboard and mouse keys can be rebound in Controls. While playing, it is swapped
	 * for a runtime copy with the player's keys (the asset itself is never changed). */
	UPROPERTY(Config, EditAnywhere, Category="Controls")
	TSoftObjectPtr<UInputMappingContext> RebindableContext;

	/** Actions left out of the key binding list (axis keys such as the mouse are always left out). */
	UPROPERTY(Config, EditAnywhere, Category="Controls")
	TArray<TSoftObjectPtr<UInputAction>> HiddenActions;

	UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="30", ClampMax="170"))
	float MinFieldOfView = 70.f;

	UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="30", ClampMax="170"))
	float MaxFieldOfView = 110.f;

	/** Volume sliders scale these classes (and their children). The Master slider works without any class. */
	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundClass> MusicClass;

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundClass> EffectsClass;

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundClass> AmbientClass;

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundClass> VoiceClass;
};
