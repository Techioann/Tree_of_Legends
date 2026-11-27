#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TOLSettingsWidget.generated.h"

class STOLSettingsMenu;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTOLSettingsClosedSignature);

/**
 * Full-screen settings menu with Display, Graphics, Audio and Controls tabs. The whole layout is built in C++
 * (Slate), so the class can be created directly (Create Widget > TOL Settings Widget) or through a Blueprint
 * subclass. Values are edited on UTOLGameUserSettings and written to GameUserSettings.ini on Apply.
 * Keys: Esc = back / cancel, Gamepad shoulders = switch tab.
 */
UCLASS(Blueprintable, meta=(DisplayName="TOL Settings Widget"))
class TREE_OF_LEGENDS_API UTOLSettingsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Fired after the menu has removed itself from the screen. */
	UPROPERTY(BlueprintAssignable, Category="Settings")
	FTOLSettingsClosedSignature OnClosed;

	/** Same as the Back button: asks about unapplied changes, then closes. */
	UFUNCTION(BlueprintCallable, Category="Settings")
	void RequestClose();

	/** Creates the settings menu for the first local player and adds it to the viewport. */
	UFUNCTION(BlueprintCallable, Category="Settings", meta=(WorldContext="WorldContextObject"))
	static UTOLSettingsWidget* OpenSettings(UObject* WorldContextObject, TSubclassOf<UTOLSettingsWidget> WidgetClass, int32 ZOrder = 50);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void HandleMenuClosed();

	TSharedPtr<STOLSettingsMenu> Menu;
};
