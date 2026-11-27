#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TOLMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;
class UImage;

/**
 * Parent class of WB_MainMenu: motion for an otherwise static layout. Widgets are found by name, so the
 * layout stays editable in the Designer and the button logic stays in the Blueprint graph.
 * - Intro: the background fades in from black, the title drops in, buttons slide in one after another.
 * - The background slowly zooms and drifts (Ken Burns).
 * - Buttons under the mouse slide right and their label turns gold; pressing squeezes them slightly.
 */
UCLASS()
class TREE_OF_LEGENDS_API UTOLMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Names")
	FName BackgroundName = TEXT("Image_0");

	/** Widgets that drop in from above, in order (title first). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Names")
	TArray<FName> HeaderNames = {TEXT("Title"), TEXT("TitleRule"), TEXT("Subtitle")};

	/** Panel holding the menu buttons; every UButton inside gets the hover effect and the staggered intro. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Names")
	FName ButtonPanelName = TEXT("VerticalBox_0");

	/** Widgets that just fade in at the end (version label etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Names")
	TArray<FName> FooterNames = {TEXT("Footer")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Intro", meta=(ClampMin="0"))
	float BackgroundFadeTime = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Intro", meta=(ClampMin="0.01"))
	float ItemIntroTime = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Intro", meta=(ClampMin="0"))
	float ItemStagger = 0.09f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Intro")
	float SlideDistance = 48.f;

	/** Relative zoom amplitude of the background (0.06 = up to 6% larger). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Background", meta=(ClampMin="0"))
	float BackgroundZoom = 0.06f;

	/** Seconds for one full zoom-in/zoom-out cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Background", meta=(ClampMin="1"))
	float BackgroundPeriod = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Buttons")
	FLinearColor HoverTextColor = FLinearColor(1.f, 0.72f, 0.28f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Buttons")
	float HoverShift = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu|Buttons", meta=(ClampMin="0.1"))
	float HoverSpeed = 14.f;

	/** Replays the intro (e.g. when returning to this screen). */
	UFUNCTION(BlueprintCallable, Category="Menu")
	void PlayIntro();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FButtonFx
	{
		TWeakObjectPtr<UButton> Button;
		TWeakObjectPtr<UTextBlock> Label;
		FSlateColor BaseColor;
		float Hover = 0.f;
		float Press = 0.f;
		float Delay = 0.f;
	};
	struct FItemFx
	{
		TWeakObjectPtr<UWidget> Widget;
		float Delay = 0.f;
		FVector2D From = FVector2D::ZeroVector;
	};

	void CollectWidgets();
	void ApplyIntro(float Time);

	TWeakObjectPtr<UImage> Background;
	FLinearColor BackgroundColor = FLinearColor::White;
	TArray<FButtonFx> Buttons;
	TArray<FItemFx> Items;
	float Elapsed = 0.f;
	bool bCollected = false;
};
