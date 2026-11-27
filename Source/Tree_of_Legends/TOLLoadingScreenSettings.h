#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TOLLoadingScreenSettings.generated.h"

class UTexture2D;

/** Project Settings > Game > Loading Screen. Edited values are saved to Config/DefaultGame.ini. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Loading Screen"))
class TREE_OF_LEGENDS_API UTOLLoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UTOLLoadingScreenSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
	bool bEnabled = true;

	/** Dimmed, slowly zooming picture behind the loading screen. Leave empty for a plain dark background. */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
	TSoftObjectPtr<UTexture2D> BackgroundImage;

	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
	FLinearColor BackgroundTint = FLinearColor(0.42f, 0.38f, 0.32f, 1.f);

	/** One tip is shown at a time; they rotate every TipInterval seconds. */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
	TArray<FText> Tips;

	UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(ClampMin="2"))
	float TipInterval = 7.f;

	/** The screen stays at least this long, so a fast load doesn't just flash. */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(ClampMin="0"))
	float MinimumDisplayTime = 1.5f;

	/** After the map has loaded, keep the screen until World Partition has streamed in the area around the player. */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
	bool bWaitForWorldStreaming = true;

	/** Give up waiting for streaming after this many seconds. */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(ClampMin="1"))
	float StreamingTimeout = 20.f;

	UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(ClampMin="0"))
	float FadeOutTime = 0.6f;
};
