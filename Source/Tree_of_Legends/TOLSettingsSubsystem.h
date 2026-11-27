#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "TOLSettingsSubsystem.generated.h"

class APawn;
class APlayerController;
class SWidget;
class UGameViewportClient;
class UInputMappingContext;

/** A keyboard/mouse mapping that the player can rebind. */
struct FTOLBindingSlot
{
	FName Action;          // Input Action asset name
	int32 Slot = 0;        // index among this action's mappings
	int32 MappingIndex = 0; // index in the context's mapping list
	FKey DefaultKey;
	FText Label;
};

/**
 * Applies the player's UTOLGameUserSettings in game:
 * - mouse sensitivity and inverted Y (legacy input scales on local player controllers);
 * - field of view and motion blur on the cameras of the locally controlled pawn;
 * - rebound keys, by swapping the rebindable mapping context for a runtime copy with the player's keys;
 * - the optional FPS counter in the top-right corner.
 * Also applies audio/brightness/anti-aliasing once when the game instance starts.
 */
UCLASS()
class TREE_OF_LEGENDS_API UTOLSettingsSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	/** Rebindable keyboard/mouse mappings of the context, in the context's order. */
	static TArray<FTOLBindingSlot> BuildBindingSlots(const UInputMappingContext* Context);

private:
	void UpdatePlayer(APlayerController* Controller, int32 Counter);
	void UpdateMappingContext(APlayerController* Controller, int32 Counter);
	void UpdateFpsCounter(bool bShow);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeContext;

	int32 RuntimeContextCounter = INDEX_NONE;
	TWeakObjectPtr<APawn> CameraPawn;
	int32 CameraCounter = INDEX_NONE;
	float SinceUpdate = 1.f;

	TSharedPtr<SWidget> FpsWidget;
	TWeakObjectPtr<UGameViewportClient> FpsViewport;
	TWeakObjectPtr<UWorld> FpsWorld;
};
