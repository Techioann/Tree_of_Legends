#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TOLLoadingScreenSubsystem.generated.h"

class STOLLoadingScreen;
class UTexture2D;
struct FWorldContext;

/**
 * Shows the "Loading..." screen whenever a map is loaded (Start game, Join, back to Main menu). Nothing has to be
 * called from Blueprints: it hooks the engine's map-load delegates.
 * - Packaged/standalone game: the screen is handed to the MoviePlayer, which keeps it animating on its own
 *   thread while the game thread is blocked loading the map.
 * - Editor (PIE): the MoviePlayer is disabled there, so the screen is put on the game viewport and drawn once
 *   before the load starts; it is frozen until the map is in.
 * After the load the screen stays on the viewport until World Partition has streamed the area around the
 * player, then fades out. Settings: Project Settings > Game > Loading Screen.
 */
UCLASS()
class TREE_OF_LEGENDS_API UTOLLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Loading Screen")
	bool IsLoadingScreenVisible() const { return Screen.IsValid(); }

private:
	void OnPreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void OnPostLoadMap(UWorld* World);
	bool TickAfterLoad(float DeltaTime);
	void AddToViewport();
	void RemoveFromViewport();
	void HideNow();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BackgroundTexture;

	TSharedPtr<STOLLoadingScreen> Screen;
	TWeakObjectPtr<UWorld> LoadedWorld;
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle PreLoadHandle, PostLoadHandle;
	double ShownTime = 0.0;
	double LoadedTime = 0.0;
	double FadeStartTime = -1.0;
	int32 StreamedFrames = 0;
	bool bInViewport = false;
};
