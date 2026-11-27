#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "TOLMenuBackdrop.generated.h"

class STOLMenuBackdrop;

/**
 * Animated layer for menu screens, placed between the background picture and the buttons:
 * - a dark shade fading out from the left edge, so the title and buttons stay readable;
 * - a soft vignette at the top and bottom;
 * - leaves drifting down with a gentle sway.
 * Everything is drawn procedurally (no textures). Leaves move only at runtime, not in the Designer.
 */
UCLASS(meta=(DisplayName="TOL Menu Backdrop"))
class TREE_OF_LEGENDS_API UTOLMenuBackdrop : public UWidget
{
	GENERATED_BODY()
public:
	UTOLMenuBackdrop();

	/** Colour of the shade at the left edge (alpha = strength). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backdrop")
	FLinearColor ShadeColor = FLinearColor(0.012f, 0.02f, 0.012f, 0.92f);

	/** Fraction of the width covered by the left shade; it fades to nothing at this point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backdrop", meta=(ClampMin="0", ClampMax="1"))
	float ShadeWidth = 0.62f;

	/** Darkening at the very top and bottom edges (0 = off). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backdrop", meta=(ClampMin="0", ClampMax="1"))
	float VignetteStrength = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backdrop|Leaves", meta=(ClampMin="0", ClampMax="200"))
	int32 LeafCount = 26;

	/** Leaf length range in pixels at 1080p (scaled with the widget height). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backdrop|Leaves")
	FVector2D LeafLength = FVector2D(12.0, 26.0);

	/** Average fall speed in pixels per second at 1080p. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backdrop|Leaves", meta=(ClampMin="0"))
	float FallSpeed = 55.f;

	/** Horizontal drift in pixels per second at 1080p (positive = to the right). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backdrop|Leaves")
	float Wind = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Backdrop|Leaves")
	TArray<FLinearColor> LeafColors;

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<STOLMenuBackdrop> Backdrop;
};
