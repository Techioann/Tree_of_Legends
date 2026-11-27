#pragma once

// Slate side of the menu backdrop (shade gradient, vignette, falling leaves). Shared by UTOLMenuBackdrop
// and the loading screen, which must be pure Slate because it is drawn on the loading thread.

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Math/RandomStream.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

struct FTOLBackdropSettings
{
	FLinearColor ShadeColor;
	float ShadeWidth = 0.6f;
	float VignetteStrength = 0.5f;
	int32 LeafCount = 0;
	FVector2f LeafLength = FVector2f(12.f, 26.f);
	float FallSpeed = 50.f;
	float Wind = 15.f;
	TArray<FLinearColor> LeafColors;
	bool bAnimate = true;
};

class STOLMenuBackdrop : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STOLMenuBackdrop) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		SetCanTick(false);
		// Capsule: corner radius = half the leaf's height (a fixed radius larger than that draws nothing).
		LeafBrush = FSlateRoundedBoxBrush(FLinearColor::White, 4.f);
		LeafBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		LeafBrush.OutlineSettings.Width = 0.f;
		StartTime = FPlatformTime::Seconds();
	}

	void SetSettings(const FTOLBackdropSettings& In)
	{
		Settings = In;
		if (Settings.bAnimate && !Timer.IsValid())
			Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &STOLMenuBackdrop::Animate));
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64.0, 64.0); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2f Size = Geometry.GetLocalSize();
		if (Size.X < 1.f || Size.Y < 1.f) return LayerId;
		const FLinearColor Tint = Style.GetColorAndOpacityTint();
		auto Faded = [&Tint](FLinearColor C, float Alpha) { C.A *= Alpha * Tint.A; return C; };

		// Left shade: strong behind the menu column, then an eased falloff.
		if (Settings.ShadeColor.A > 0.f && Settings.ShadeWidth > 0.f)
		{
			const float W = Size.X * Settings.ShadeWidth;
			TArray<FSlateGradientStop> Stops;
			const float Keys[][2] = {{0.f, 1.f}, {0.35f, 0.92f}, {0.6f, 0.62f}, {0.8f, 0.28f}, {1.f, 0.f}};
			for (const auto& K : Keys) Stops.Emplace(FVector2f(W * K[0], 0.f), Faded(Settings.ShadeColor, K[1]));
			Stops.Emplace(FVector2f(Size.X, 0.f), Faded(Settings.ShadeColor, 0.f));
			FSlateDrawElement::MakeGradient(Out, LayerId, Geometry.ToPaintGeometry(), MoveTemp(Stops), Orient_Vertical);
		}
		// Top/bottom vignette.
		if (Settings.VignetteStrength > 0.f)
		{
			const FLinearColor Dark(0.f, 0.f, 0.f, Settings.VignetteStrength);
			TArray<FSlateGradientStop> Stops;
			Stops.Emplace(FVector2f(0.f, 0.f), Faded(Dark, 0.7f));
			Stops.Emplace(FVector2f(0.f, Size.Y * 0.18f), Faded(Dark, 0.f));
			Stops.Emplace(FVector2f(0.f, Size.Y * 0.62f), Faded(Dark, 0.f));
			Stops.Emplace(FVector2f(0.f, Size.Y), Faded(Dark, 1.f));
			FSlateDrawElement::MakeGradient(Out, LayerId + 1, Geometry.ToPaintGeometry(), MoveTemp(Stops), Orient_Horizontal);
		}

		// Leaves: positions are a pure function of time, so nothing has to be simulated or stored.
		const int32 Count = Settings.LeafColors.Num() > 0 ? Settings.LeafCount : 0;
		const float Scale = Size.Y / 1080.f;
		const float Time = Settings.bAnimate ? float(FPlatformTime::Seconds() - StartTime) : 7.5f;
		for (int32 i = 0; i < Count; ++i)
		{
			FRandomStream R(9173 + i * 7919);
			const float Depth = R.FRandRange(0.55f, 1.f); // farther leaves: smaller, slower, fainter
			const float Length = FMath::Lerp(Settings.LeafLength.X, Settings.LeafLength.Y, R.FRand()) * Scale * Depth;
			const FVector2f LeafSize(Length, Length * R.FRandRange(0.42f, 0.55f));
			const float Margin = Length * 2.f;
			const float TravelY = Size.Y + Margin * 2.f;
			const float Speed = Settings.FallSpeed * Scale * Depth * R.FRandRange(0.75f, 1.25f);
			const float Y = FMath::Fmod(R.FRand() * TravelY + Time * Speed, TravelY) - Margin;
			const float SwayFreq = R.FRandRange(0.35f, 0.8f), Phase = R.FRand() * UE_TWO_PI;
			const float TravelX = Size.X + Margin * 2.f;
			float X = R.FRand() * TravelX + Time * Settings.Wind * Scale * Depth + FMath::Sin(Time * SwayFreq + Phase) * 38.f * Scale;
			X = FMath::Fmod(FMath::Fmod(X, TravelX) + TravelX, TravelX) - Margin;
			// Tumbling: rotation plus a squash that imitates the leaf turning edge-on.
			const float Angle = Phase + Time * R.FRandRange(-1.4f, 1.4f) + FMath::Sin(Time * SwayFreq * 1.7f + Phase) * 0.6f;
			const float Flip = 0.35f + 0.65f * FMath::Abs(FMath::Cos(Time * R.FRandRange(0.6f, 1.6f) + Phase));
			FLinearColor Color = Settings.LeafColors[R.RandHelper(Settings.LeafColors.Num())];
			Color.A *= FMath::Lerp(0.45f, 1.f, Depth);
			const FSlateRenderTransform Spin{FScale2f(1.f, Flip), FVector2f::ZeroVector};
			const FSlateRenderTransform Rotate{FQuat2f(Angle)};
			FSlateDrawElement::MakeBox(Out, LayerId + 2,
				Geometry.ToPaintGeometry(LeafSize, FSlateLayoutTransform(FVector2f(X, Y)), Concatenate(Spin, Rotate), FVector2f(0.5f, 0.5f)),
				&LeafBrush, ESlateDrawEffect::None, Faded(Color, 1.f));
		}
		return LayerId + 2;
	}

private:
	EActiveTimerReturnType Animate(double, float)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
		return Settings.bAnimate ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}

	FTOLBackdropSettings Settings;
	FSlateBrush LeafBrush;
	TSharedPtr<FActiveTimerHandle> Timer;
	double StartTime = 0.0;
};
