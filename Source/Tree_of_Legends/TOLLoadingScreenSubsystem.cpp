#include "TOLLoadingScreenSubsystem.h"

#include "TOLLoadingScreenSettings.h"
#include "TOLMenuBackdropSlate.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "MoviePlayer.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
	const FLinearColor Gold(1.f, 0.7f, 0.26f, 1.f);
	const FLinearColor Cream(0.97f, 0.93f, 0.84f, 1.f);
	const FLinearColor TextShadow(0.f, 0.f, 0.f, 0.6f);

	FSlateFontInfo Font(const FName Typeface, const int32 Size, const int32 LetterSpacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		Info.LetterSpacing = LetterSpacing;
		return Info;
	}
}

/** Thin track with a gold segment sweeping across it (progress is unknown, so it just shows activity). */
class STOLLoadingBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STOLLoadingBar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) { StartTime = FPlatformTime::Seconds(); }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(320.0, 3.0); }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
		int32 LayerId, const FWidgetStyle& Style, bool) const override
	{
		const FVector2f Size = Geometry.GetLocalSize();
		const float Alpha = Style.GetColorAndOpacityTint().A;
		FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.14f * Alpha));

		const float T = FMath::Frac(float(FPlatformTime::Seconds() - StartTime) / 1.7f);
		const float Eased = T * T * (3.f - 2.f * T);
		constexpr float Segment = 0.32f;
		const float From = FMath::Lerp(-Segment, 1.f, Eased);
		const float A = FMath::Clamp(From, 0.f, 1.f), B = FMath::Clamp(From + Segment, 0.f, 1.f);
		if (B > A)
		{
			FLinearColor Color = Gold;
			Color.A *= Alpha;
			FSlateDrawElement::MakeBox(Out, LayerId + 1,
				Geometry.ToPaintGeometry(FVector2f((B - A) * Size.X, Size.Y), FSlateLayoutTransform(FVector2f(A * Size.X, 0.f))),
				&Brush, ESlateDrawEffect::None, Color);
		}
		return LayerId + 1;
	}

private:
	FSlateColorBrush Brush = FSlateColorBrush(FLinearColor::White);
	double StartTime = 0.0;
};

/**
 * The loading screen itself. Pure Slate (no UObjects besides the background texture, which the subsystem keeps
 * alive), because the MoviePlayer draws it on its own thread while the game thread is busy loading.
 * Every animation is a function of wall-clock time, so it moves whether it is painted by the loading thread or
 * by the game viewport.
 */
class STOLLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STOLLoadingScreen) : _Background(nullptr), _BackgroundTint(FLinearColor::White), _TipInterval(7.f) {}
		SLATE_ARGUMENT(UTexture2D*, Background)
		SLATE_ARGUMENT(FLinearColor, BackgroundTint)
		SLATE_ARGUMENT(TArray<FText>, Tips)
		SLATE_ARGUMENT(float, TipInterval)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		StartTime = FPlatformTime::Seconds();
		Tips = Args._Tips;
		TipInterval = FMath::Max(Args._TipInterval, 2.f);
		FirstTip = Tips.Num() > 0 ? FMath::RandHelper(Tips.Num()) : 0;

		TSharedRef<SWidget> Picture = SNullWidget::NullWidget;
		if (UTexture2D* Texture = Args._Background)
		{
			BackgroundBrush.SetResourceObject(Texture);
			const int32 W = Texture->GetSizeX(), H = Texture->GetSizeY();
			BackgroundBrush.ImageSize = (W > 0 && H > 0) ? FVector2D(W, H) : FVector2D(1920.0, 1080.0);
			Picture = SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFill)
				[
					SNew(SImage)
					.Image(&BackgroundBrush)
					.ColorAndOpacity(Args._BackgroundTint)
					.RenderTransform(this, &STOLLoadingScreen::GetBackgroundTransform)
					.RenderTransformPivot(FVector2D(0.5, 0.5))
				];
		}

		FTOLBackdropSettings Leaves;
		Leaves.ShadeColor = FLinearColor(0.012f, 0.02f, 0.012f, 0.88f);
		Leaves.ShadeWidth = 0.7f;
		Leaves.VignetteStrength = 0.75f;
		Leaves.LeafCount = 30;
		Leaves.LeafLength = FVector2f(16.f, 32.f);
		Leaves.FallSpeed = 55.f;
		Leaves.Wind = 18.f;
		Leaves.LeafColors = {
			FLinearColor(0.86f, 0.42f, 0.10f, 0.85f),
			FLinearColor(0.95f, 0.66f, 0.18f, 0.80f),
			FLinearColor(0.62f, 0.20f, 0.06f, 0.85f),
			FLinearColor(0.50f, 0.58f, 0.16f, 0.75f),
		};
		TSharedRef<STOLMenuBackdrop> Backdrop = SNew(STOLMenuBackdrop);
		Backdrop->SetSettings(Leaves);

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot() [ SNew(SImage).Image(&SolidBrush) ]
			+ SOverlay::Slot() [ Picture ]
			+ SOverlay::Slot() [ Backdrop ]

			// Bottom left: title and a gameplay tip.
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(110.f, 0.f, 0.f, 92.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 0.f, -10.f)
				[
					SNew(STextBlock).Text(INVTEXT("TREE OF")).Font(Font("Bold", 26, 900))
					.ColorAndOpacity(Gold).ShadowOffset(FVector2D(2.0, 2.0)).ShadowColorAndOpacity(TextShadow)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(INVTEXT("LEGENDS")).Font(Font("Bold", 96, 30))
					.ColorAndOpacity(Cream).ShadowOffset(FVector2D(3.0, 5.0)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.65f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 4.f, 0.f, 0.f).HAlign(HAlign_Left)
				[
					SNew(SBox).WidthOverride(140.f).HeightOverride(4.f) [ SNew(SImage).Image(&GoldBrush) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 22.f, 0.f, 0.f).HAlign(HAlign_Left)
				[
					SNew(SBox).MaxDesiredWidth(760.f)
					[
						SNew(STextBlock).Text(this, &STOLLoadingScreen::GetTip).Font(Font("Italic", 20))
						.ColorAndOpacity(FLinearColor(0.88f, 0.84f, 0.76f, 0.85f)).AutoWrapText(true)
						.ShadowOffset(FVector2D(1.0, 2.0)).ShadowColorAndOpacity(TextShadow)
					]
				]
			]

			// Bottom right: LOADING... and the activity bar.
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(0.f, 0.f, 110.f, 100.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Text(INVTEXT("LOADING")).Font(Font("Bold", 26, 500))
						.ColorAndOpacity(Cream).ShadowOffset(FVector2D(2.0, 2.0)).ShadowColorAndOpacity(TextShadow)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(48.f)
						[
							SNew(STextBlock).Text(this, &STOLLoadingScreen::GetDots).Font(Font("Bold", 26, 300))
							.ColorAndOpacity(Gold).ShadowOffset(FVector2D(2.0, 2.0)).ShadowColorAndOpacity(TextShadow)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(320.f).HeightOverride(3.f) [ SNew(STOLLoadingBar) ]
				]
			]
		];

		SetColorAndOpacity(TAttribute<FLinearColor>::CreateSP(this, &STOLLoadingScreen::GetFade));
		SetVisibility(EVisibility::HitTestInvisible);
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &STOLLoadingScreen::Repaint));
	}

	void StartFadeOut(const float Duration)
	{
		FadeDuration = FMath::Max(Duration, 0.01f);
		FadeStart = FPlatformTime::Seconds();
	}

private:
	float Age() const { return float(FPlatformTime::Seconds() - StartTime); }

	FText GetTip() const
	{
		return Tips.Num() > 0 ? Tips[(FirstTip + int32(Age() / TipInterval)) % Tips.Num()] : FText::GetEmpty();
	}

	FText GetDots() const
	{
		static const FText Dots[] = {INVTEXT(""), INVTEXT("."), INVTEXT(".."), INVTEXT("...")};
		return Dots[int32(Age() * 2.5f) % 4];
	}

	TOptional<FSlateRenderTransform> GetBackgroundTransform() const
	{
		const float T = Age();
		return FSlateRenderTransform(FScale2f(1.04f + 0.04f * FMath::Sin(T * 0.18f)), FVector2f(FMath::Sin(T * 0.11f) * 24.f, 0.f));
	}

	FLinearColor GetFade() const
	{
		if (FadeStart < 0.0) return FLinearColor::White;
		const float T = FMath::Clamp(float(FPlatformTime::Seconds() - FadeStart) / FadeDuration, 0.f, 1.f);
		return FLinearColor(1.f, 1.f, 1.f, 1.f - T * T * (3.f - 2.f * T));
	}

	EActiveTimerReturnType Repaint(double, float)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
		return EActiveTimerReturnType::Continue;
	}

	FSlateBrush BackgroundBrush;
	FSlateColorBrush SolidBrush = FSlateColorBrush(FLinearColor(0.018f, 0.024f, 0.018f, 1.f));
	FSlateColorBrush GoldBrush = FSlateColorBrush(Gold);
	TArray<FText> Tips;
	float TipInterval = 7.f;
	int32 FirstTip = 0;
	double StartTime = 0.0;
	double FadeStart = -1.0;
	float FadeDuration = 0.6f;
};

void UTOLLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreLoadHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UTOLLoadingScreenSubsystem::OnPreLoadMap);
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTOLLoadingScreenSubsystem::OnPostLoadMap);
}

void UTOLLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
	HideNow();
	Super::Deinitialize();
}

void UTOLLoadingScreenSubsystem::OnPreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance() || IsRunningDedicatedServer()) return;
	const UTOLLoadingScreenSettings* Settings = GetDefault<UTOLLoadingScreenSettings>();
	if (!Settings->bEnabled) return;

	HideNow(); // a previous screen may still be fading out
	if (!BackgroundTexture && !Settings->BackgroundImage.IsNull())
		BackgroundTexture = Settings->BackgroundImage.LoadSynchronous();
	if (BackgroundTexture)
		BackgroundTexture->SetForceMipLevelsToBeResident(30.f); // full resolution, not a blurry streamed mip

	Screen = SNew(STOLLoadingScreen)
		.Background(BackgroundTexture)
		.BackgroundTint(Settings->BackgroundTint)
		.Tips(Settings->Tips)
		.TipInterval(Settings->TipInterval);
	ShownTime = FPlatformTime::Seconds();
	FadeStartTime = -1.0;
	LoadedWorld.Reset();

	if (IsMoviePlayerEnabled() && GetMoviePlayer())
	{
		// The MoviePlayer's own PreLoadMap handler runs right after this one and starts playing it.
		FLoadingScreenAttributes Attributes;
		Attributes.WidgetLoadingScreen = Screen;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		Attributes.MinimumLoadingScreenDisplayTime = -1.f; // the minimum time is enforced after the load, on the viewport
		Attributes.bMoviesAreSkippable = false;
		Attributes.bAllowEngineTick = false;
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
	}
	else
	{
		// Editor (PIE): no MoviePlayer. Put the screen on the viewport and draw one frame now, because the map load
		// blocks the game thread and the last drawn frame is what stays visible (same approach as the editor's
		// slow-task dialog).
		AddToViewport();
		if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsTicking())
		{
			FSlateApplication::Get().Tick(ESlateTickType::TimeAndWidgets);
			if (GIsRHIInitialized)
			{
				ENQUEUE_RENDER_COMMAND(TOLLoadingScreenEndFrame)([](FRHICommandListImmediate& RHICmdList) { RHICmdList.EndFrame(); });
			}
		}
	}
}

void UTOLLoadingScreenSubsystem::OnPostLoadMap(UWorld* World)
{
	if (!Screen.IsValid()) return;
	if (World && World->GetGameInstance() != GetGameInstance()) return;

	// The MoviePlayer would stop on its own right after this; stop it first so the widget is free to move to the viewport.
	if (IsMoviePlayerEnabled() && GetMoviePlayer() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
		GetMoviePlayer()->WaitForMovieToFinish();

	if (!World) { HideNow(); return; } // the load failed

	LoadedWorld = World;
	LoadedTime = FPlatformTime::Seconds();
	StreamedFrames = 0;
	AddToViewport(); // (re)add: loading a map can clear the viewport's widgets
	if (!TickHandle.IsValid())
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UTOLLoadingScreenSubsystem::TickAfterLoad));
}

bool UTOLLoadingScreenSubsystem::TickAfterLoad(float)
{
	if (!Screen.IsValid()) { TickHandle.Reset(); return false; }
	const UTOLLoadingScreenSettings* Settings = GetDefault<UTOLLoadingScreenSettings>();
	const double Now = FPlatformTime::Seconds();

	if (FadeStartTime < 0.0)
	{
		bool bReady = Now - ShownTime >= Settings->MinimumDisplayTime && Now - LoadedTime >= 0.5;
		if (bReady && Settings->bWaitForWorldStreaming)
		{
			// Streaming is only meaningful once the player exists (its pawn is the streaming source).
			const UWorld* World = LoadedWorld.Get();
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			const UWorldPartitionSubsystem* Partition = World ? World->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
			const bool bStreamed = PC && PC->GetPawn() && (!Partition || Partition->IsStreamingCompleted());
			StreamedFrames = bStreamed ? StreamedFrames + 1 : 0;
			bReady = StreamedFrames >= 10 || Now - LoadedTime >= Settings->StreamingTimeout;
		}
		if (bReady)
		{
			FadeStartTime = Now;
			Screen->StartFadeOut(Settings->FadeOutTime);
		}
		return true;
	}

	if (Now - FadeStartTime < Settings->FadeOutTime) return true;
	TickHandle.Reset(); // returning false removes the ticker
	RemoveFromViewport();
	Screen.Reset();
	return false;
}

void UTOLLoadingScreenSubsystem::AddToViewport()
{
	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (!Viewport || !Screen.IsValid()) return;
	if (bInViewport) Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), 10000); // above every UMG widget
	bInViewport = true;
}

void UTOLLoadingScreenSubsystem::RemoveFromViewport()
{
	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (bInViewport && Viewport && Screen.IsValid())
		Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	bInViewport = false;
}

void UTOLLoadingScreenSubsystem::HideNow()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	RemoveFromViewport();
	Screen.Reset();
	FadeStartTime = -1.0;
}
