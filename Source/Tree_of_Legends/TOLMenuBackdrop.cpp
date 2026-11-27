#include "TOLMenuBackdrop.h"

#include "TOLMenuBackdropSlate.h"

#define LOCTEXT_NAMESPACE "TOLMenuBackdrop"


UTOLMenuBackdrop::UTOLMenuBackdrop()
{
	SetVisibilityInternal(ESlateVisibility::HitTestInvisible);
	LeafColors = {
		FLinearColor(0.86f, 0.42f, 0.10f, 0.85f),  // orange
		FLinearColor(0.95f, 0.66f, 0.18f, 0.80f),  // amber
		FLinearColor(0.62f, 0.20f, 0.06f, 0.85f),  // rust
		FLinearColor(0.50f, 0.58f, 0.16f, 0.75f),  // late green
	};
}

TSharedRef<SWidget> UTOLMenuBackdrop::RebuildWidget()
{
	Backdrop = SNew(STOLMenuBackdrop);
	return Backdrop.ToSharedRef();
}

void UTOLMenuBackdrop::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!Backdrop.IsValid()) return;
	FTOLBackdropSettings S;
	S.ShadeColor = ShadeColor;
	S.ShadeWidth = ShadeWidth;
	S.VignetteStrength = VignetteStrength;
	S.LeafCount = LeafCount;
	S.LeafLength = FVector2f(float(FMath::Min(LeafLength.X, LeafLength.Y)), float(FMath::Max(LeafLength.X, LeafLength.Y)));
	S.FallSpeed = FallSpeed;
	S.Wind = Wind;
	S.LeafColors = LeafColors;
	S.bAnimate = !IsDesignTime();
	Backdrop->SetSettings(S);
}

void UTOLMenuBackdrop::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Backdrop.Reset();
}

#if WITH_EDITOR
const FText UTOLMenuBackdrop::GetPaletteCategory()
{
	return LOCTEXT("Category", "Tree of Legends");
}
#endif

#undef LOCTEXT_NAMESPACE
