#include "TOLSettingsWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "TOLGameUserSettings.h"
#include "TOLMenuBackdropSlate.h"
#include "TOLSettingsConfig.h"
#include "TOLSettingsSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

namespace TOLSettingsStyle
{
	const FLinearColor Gold(1.f, 0.7f, 0.26f, 1.f);
	const FLinearColor Cream(0.97f, 0.93f, 0.84f, 1.f);
	const FLinearColor Muted(0.74f, 0.71f, 0.64f, 1.f);
	const FLinearColor Faint(0.45f, 0.43f, 0.39f, 1.f);
	const FLinearColor Ink(0.07f, 0.05f, 0.02f, 1.f);
	const FLinearColor Shadow(0.f, 0.f, 0.f, 0.6f);

	FSlateFontInfo Font(const FName Typeface, const int32 Size, const int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FSlateRoundedBoxBrush Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
	}

	FButtonStyle MakeButtonStyle(const FSlateBrush& Normal, const FSlateBrush& Hovered, const FSlateBrush& Pressed, const FSlateBrush& Disabled)
	{
		return FButtonStyle().SetNormal(Normal).SetHovered(Hovered).SetPressed(Pressed).SetDisabled(Disabled)
			.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
	}

	/** Brushes and widget styles shared by every settings menu; lives for the whole session. */
	struct FStyles
	{
		FSlateColorBrush White = FSlateColorBrush(FLinearColor::White);
		FSlateNoResource None;
		FSlateRoundedBoxBrush RowHover = Rounded(FLinearColor(1.f, 1.f, 1.f, 0.055f), 6.f);
		FSlateRoundedBoxBrush Panel = Rounded(FLinearColor(0.f, 0.f, 0.f, 0.36f), 10.f, FLinearColor(1.f, 1.f, 1.f, 0.07f), 1.f);
		FSlateRoundedBoxBrush DialogPanel = Rounded(FLinearColor(0.03f, 0.035f, 0.028f, 0.98f), 12.f, FLinearColor(1.f, 0.7f, 0.26f, 0.55f), 1.5f);
		FButtonStyle Plain;
		FButtonStyle Secondary;
		FButtonStyle Primary;
		FButtonStyle Key;
		FSliderStyle Slider;
		FScrollBarStyle ScrollBar;

		FStyles()
		{
			Plain = MakeButtonStyle(None, None, None, None);
			Secondary = MakeButtonStyle(
				Rounded(FLinearColor(1.f, 1.f, 1.f, 0.05f), 6.f, FLinearColor(1.f, 1.f, 1.f, 0.24f), 1.f),
				Rounded(FLinearColor(1.f, 0.7f, 0.26f, 0.14f), 6.f, FLinearColor(1.f, 0.7f, 0.26f, 0.85f), 1.f),
				Rounded(FLinearColor(1.f, 0.7f, 0.26f, 0.26f), 6.f, FLinearColor(1.f, 0.7f, 0.26f, 1.f), 1.f),
				Rounded(FLinearColor(1.f, 1.f, 1.f, 0.02f), 6.f, FLinearColor(1.f, 1.f, 1.f, 0.08f), 1.f));
			Primary = MakeButtonStyle(
				Rounded(FLinearColor(0.93f, 0.63f, 0.22f, 1.f), 6.f),
				Rounded(FLinearColor(1.f, 0.76f, 0.36f, 1.f), 6.f),
				Rounded(FLinearColor(0.8f, 0.53f, 0.17f, 1.f), 6.f),
				Rounded(FLinearColor(1.f, 1.f, 1.f, 0.05f), 6.f, FLinearColor(1.f, 1.f, 1.f, 0.08f), 1.f));
			Key = MakeButtonStyle(
				Rounded(FLinearColor(0.f, 0.f, 0.f, 0.4f), 5.f, FLinearColor(1.f, 1.f, 1.f, 0.22f), 1.f),
				Rounded(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, FLinearColor(1.f, 0.7f, 0.26f, 0.9f), 1.f),
				Rounded(FLinearColor(1.f, 0.7f, 0.26f, 0.2f), 5.f, FLinearColor(1.f, 0.7f, 0.26f, 1.f), 1.f),
				Rounded(FLinearColor(0.f, 0.f, 0.f, 0.2f), 5.f));

			FSlateRoundedBoxBrush Bar = Rounded(FLinearColor::White, 2.f);
			FSlateRoundedBoxBrush Thumb(FLinearColor::White, 7.9f, FVector2f(16.f, 16.f));
			FSlateRoundedBoxBrush ThumbHover(FLinearColor::White, 9.9f, FVector2f(20.f, 20.f));
			Slider = FSliderStyle()
				.SetNormalBarImage(Bar).SetHoveredBarImage(Bar).SetDisabledBarImage(Bar)
				.SetNormalThumbImage(Thumb).SetHoveredThumbImage(ThumbHover).SetDisabledThumbImage(Thumb)
				.SetBarThickness(4.f);

			ScrollBar = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
			ScrollBar.SetVerticalBackgroundImage(None).SetVerticalTopSlotImage(None).SetVerticalBottomSlotImage(None)
				.SetHorizontalBackgroundImage(None).SetHorizontalTopSlotImage(None).SetHorizontalBottomSlotImage(None)
				.SetNormalThumbImage(Rounded(FLinearColor(1.f, 1.f, 1.f, 0.16f), 3.f))
				.SetHoveredThumbImage(Rounded(FLinearColor(1.f, 1.f, 1.f, 0.3f), 3.f))
				.SetDraggedThumbImage(Rounded(FLinearColor(1.f, 0.7f, 0.26f, 0.7f), 3.f));
		}
	};

	const FStyles& Get()
	{
		static FStyles Styles;
		return Styles;
	}

	FText Percent(float Value) { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f))); }

	bool IsBindableKey(const FKey& Key)
	{
		return Key.IsValid() && !Key.IsGamepadKey() && !Key.IsTouch() && !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D()
			&& Key != EKeys::Escape;
	}
}

using namespace TOLSettingsStyle;

class STOLSettingsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STOLSettingsMenu) {}
		SLATE_EVENT(FSimpleDelegate, OnClosed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

	/** Back: asks what to do with unapplied changes, then closes. */
	void RequestClose();

private:
	struct FDialogButton
	{
		FText Label;
		TFunction<void()> Action;
		bool bPrimary = false;
	};

	static UTOLGameUserSettings* Settings() { return UTOLGameUserSettings::Get(); }
	void MarkDirty() { bDirty = true; }

	// Layout
	TSharedRef<SWidget> BuildTabBar();
	TSharedRef<SWidget> BuildDisplayTab();
	TSharedRef<SWidget> BuildGraphicsTab();
	TSharedRef<SWidget> BuildAudioTab();
	TSharedRef<SWidget> BuildControlsTab();
	TSharedRef<SWidget> BuildBottomBar();
	TSharedRef<SWidget> Page(const TSharedRef<SVerticalBox>& Rows) const;

	// Rows and controls
	TSharedRef<SWidget> Row(const FText& Label, const FText& Description, const TSharedRef<SWidget>& Control);
	TSharedRef<SWidget> Section(const FText& Title) const;
	TSharedRef<SWidget> Selector(TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set, FText Fallback = FText::GetEmpty());
	TSharedRef<SWidget> Toggle(TFunction<bool()> Get, TFunction<void(bool)> Set);
	TSharedRef<SWidget> Slider(float Min, float Max, float Step, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FText(float)> Format);
	TSharedRef<SWidget> TextButton(const FText& Label, TFunction<void()> OnClick, bool bPrimary = false, TFunction<bool()> Enabled = nullptr, float MinWidth = 150.f);
	TSharedRef<SWidget> ArrowButton(bool bNext, TFunction<void()> OnClick);
	TSharedRef<SWidget> KeyBinding(int32 Index);
	TSharedRef<SWidget> QualityRow(const FText& Label, const FText& Description,
		int32 (UGameUserSettings::*Get)() const, void (UGameUserSettings::*Set)(int32));
	TSharedRef<SWidget> VolumeRow(const FText& Label, const FText& Description, float UTOLGameUserSettings::*Volume);

	// Actions
	void Apply(TFunction<void()> AfterApply = nullptr);
	void Revert();
	void AskResetToDefaults();
	void Close();
	FKey EffectiveKey(int32 Index) const;
	FText SharedKeyNote(int32 Index) const;
	void AssignKey(const FKey& Key);
	void StopListening();

	// Dialog
	void ShowDialog(const FText& Title, const TAttribute<FText>& Body, const TArray<FDialogButton>& Buttons,
		TFunction<void()> OnCancel = nullptr, double TimeoutSeconds = 0.0, TFunction<void()> OnTimeout = nullptr);
	void CloseDialog();
	void CancelDialog();

	FSimpleDelegate OnClosed;
	TArray<FIntPoint> Resolutions;
	TArray<FText> ResolutionNames;
	TArray<FTOLBindingSlot> Bindings;
	int32 ActiveTab = 0;
	bool bDirty = false;
	int32 ListeningBinding = INDEX_NONE;
	FText Hint;

	bool bDialogOpen = false;
	TSharedPtr<SBox> DialogBox;
	double DialogDeadline = 0.0;
	TFunction<void()> DialogCancel;
	TFunction<void()> DialogTimeout;
	TSharedPtr<FActiveTimerHandle> DialogTimer;
};

void STOLSettingsMenu::Construct(const FArguments& InArgs)
{
	OnClosed = InArgs._OnClosed;
	const FStyles& Style = TOLSettingsStyle::Get();

	// Resolutions offered in the Display tab (plus the current one, so it can always be shown).
	TArray<FIntPoint> Found;
	if (!UKismetSystemLibrary::GetSupportedFullscreenResolutions(Found) || Found.Num() == 0)
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Found);
	if (const UTOLGameUserSettings* S = Settings()) Found.Add(S->GetScreenResolution());
	for (const FIntPoint& Size : Found)
		if (Size.X > 0 && Size.Y > 0) Resolutions.AddUnique(Size);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
	for (const FIntPoint& Size : Resolutions)
		ResolutionNames.Add(FText::FromString(FString::Printf(TEXT("%d x %d"), Size.X, Size.Y)));

	const UTOLSettingsConfig* Config = GetDefault<UTOLSettingsConfig>();
	Bindings = UTOLSettingsSubsystem::BuildBindingSlots(Config->RebindableContext.IsNull() ? nullptr : Config->RebindableContext.LoadSynchronous());

	TSharedRef<STOLMenuBackdrop> Backdrop = SNew(STOLMenuBackdrop);
	FTOLBackdropSettings Leaves;
	Leaves.ShadeColor = FLinearColor::Transparent;
	Leaves.ShadeWidth = 0.f;
	Leaves.VignetteStrength = 0.65f;
	Leaves.LeafCount = 14;
	Leaves.FallSpeed = 40.f;
	Leaves.LeafColors = {
		FLinearColor(0.86f, 0.42f, 0.10f, 0.55f), FLinearColor(0.95f, 0.66f, 0.18f, 0.5f),
		FLinearColor(0.62f, 0.20f, 0.06f, 0.55f), FLinearColor(0.50f, 0.58f, 0.16f, 0.45f)};
	Backdrop->SetSettings(Leaves);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SImage).Image(&Style.White).ColorAndOpacity(FLinearColor(0.008f, 0.011f, 0.008f, 0.86f)) ]
		+ SOverlay::Slot()[ Backdrop ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Fill).Padding(40.f, 46.f, 40.f, 40.f)
		[
			SNew(SBox).WidthOverride(1100.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(INVTEXT("TREE OF LEGENDS")).Font(Font("Bold", 15, 600)).ColorAndOpacity(Gold)
					.ShadowOffset(FVector2D(1.0, 2.0)).ShadowColorAndOpacity(Shadow)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(INVTEXT("SETTINGS")).Font(Font("Bold", 50, 120)).ColorAndOpacity(Cream)
					.ShadowOffset(FVector2D(2.0, 4.0)).ShadowColorAndOpacity(Shadow)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f).HAlign(HAlign_Left)
				[
					SNew(SBox).WidthOverride(120.f).HeightOverride(4.f)[ SNew(SImage).Image(&Style.White).ColorAndOpacity(Gold) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 28.f, 0.f, 0.f)[ BuildTabBar() ]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(1.f)[ SNew(SImage).Image(&Style.White).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.12f)) ]
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(SBorder).BorderImage(&Style.Panel).Padding(FMargin(10.f, 6.f, 4.f, 6.f))
					[
						SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return ActiveTab; })
						+ SWidgetSwitcher::Slot()[ BuildDisplayTab() ]
						+ SWidgetSwitcher::Slot()[ BuildGraphicsTab() ]
						+ SWidgetSwitcher::Slot()[ BuildAudioTab() ]
						+ SWidgetSwitcher::Slot()[ BuildControlsTab() ]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 12.f, 6.f, 0.f)
				[
					SNew(SBox).MinDesiredHeight(26.f)
					[
						SNew(STextBlock).Text_Lambda([this] { return Hint; }).Font(Font("Italic", 15)).ColorAndOpacity(Muted)
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[ BuildBottomBar() ]
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SBox).Visibility_Lambda([this] { return bDialogOpen ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ SNew(SImage).Image(&Style.White).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f)) ]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(640.f)
					[
						SNew(SBorder).BorderImage(&Style.DialogPanel).Padding(FMargin(34.f, 28.f))
						[
							SAssignNew(DialogBox, SBox)
						]
					]
				]
			]
		]
	];

	// Take keyboard focus once the menu is on screen, so Esc and key binding work right away.
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		FSlateApplication::Get().SetAllUserFocus(SharedThis(this), EFocusCause::SetDirectly);
		return EActiveTimerReturnType::Stop;
	}));
}

// ---------------------------------------------------------------------------------------------------------------
// Tabs

TSharedRef<SWidget> STOLSettingsMenu::BuildTabBar()
{
	const FStyles& Style = TOLSettingsStyle::Get();
	const FText Names[] = {INVTEXT("DISPLAY"), INVTEXT("GRAPHICS"), INVTEXT("AUDIO"), INVTEXT("CONTROLS")};
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		TSharedPtr<SButton> Button;
		Bar->AddSlot().AutoWidth().Padding(0.f, 0.f, 38.f, 0.f)
		[
			SAssignNew(Button, SButton).ButtonStyle(&Style.Plain).ContentPadding(FMargin(2.f, 4.f, 2.f, 0.f))
			.OnClicked_Lambda([this, Index]
			{
				StopListening();
				ActiveTab = Index;
				Hint = FText::GetEmpty();
				return FReply::Handled();
			})
		];
		TWeakPtr<SButton> Weak = Button;
		Button->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Names[Index]).Font(Font("Bold", 19, 180))
				.ColorAndOpacity_Lambda([this, Index, Weak]
				{
					if (ActiveTab == Index) return FSlateColor(Gold);
					const TSharedPtr<SButton> Pinned = Weak.Pin();
					return FSlateColor(Pinned && Pinned->IsHovered() ? Cream : Muted);
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 9.f, 0.f, 0.f)
			[
				SNew(SBox).HeightOverride(3.f)
				[
					SNew(SImage).Image(&Style.White)
					.ColorAndOpacity_Lambda([this, Index] { return FSlateColor(ActiveTab == Index ? Gold : FLinearColor::Transparent); })
				]
			]);
	}
	return Bar;
}

TSharedRef<SWidget> STOLSettingsMenu::BuildDisplayTab()
{
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto Add = [&Rows](const TSharedRef<SWidget>& Widget) { Rows->AddSlot().AutoHeight()[ Widget ]; };

	Add(Section(INVTEXT("SCREEN")));
	Add(Row(INVTEXT("Window Mode"), INVTEXT("Fullscreen usually runs best. Windowed Fullscreen makes switching to other apps instant."),
		Selector({INVTEXT("Fullscreen"), INVTEXT("Windowed Fullscreen"), INVTEXT("Windowed")},
			[]() -> int32 { const UTOLGameUserSettings* S = Settings(); return S ? int32(S->GetFullscreenMode()) : 0; },
			[](int32 I) { if (UTOLGameUserSettings* S = Settings()) S->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(I)); })));
	Add(Row(INVTEXT("Resolution"), INVTEXT("Number of pixels the game is drawn at. Lower is faster, higher is sharper."),
		Selector(ResolutionNames,
			[this]() -> int32 { const UTOLGameUserSettings* S = Settings(); return S ? Resolutions.IndexOfByKey(S->GetScreenResolution()) : int32(INDEX_NONE); },
			[this](int32 I) { if (UTOLGameUserSettings* S = Settings(); S && Resolutions.IsValidIndex(I)) S->SetScreenResolution(Resolutions[I]); },
			INVTEXT("Custom"))));
	Add(Row(INVTEXT("Vertical Sync"), INVTEXT("Removes screen tearing by waiting for the monitor. Can add a little input delay."),
		Toggle([] { const UTOLGameUserSettings* S = Settings(); return S && S->IsVSyncEnabled(); },
			[](bool b) { if (UTOLGameUserSettings* S = Settings()) S->SetVSyncEnabled(b); })));

	static const float Limits[] = {30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 240.f, 0.f};
	Add(Row(INVTEXT("Frame Rate Limit"), INVTEXT("Caps the frame rate. A cap saves power and keeps the fans quiet."),
		Selector({INVTEXT("30"), INVTEXT("60"), INVTEXT("90"), INVTEXT("120"), INVTEXT("144"), INVTEXT("165"), INVTEXT("240"), INVTEXT("Unlimited")},
			[]() -> int32 {
				const UTOLGameUserSettings* S = Settings();
				const float Limit = S ? S->GetFrameRateLimit() : 0.f;
				for (int32 I = 0; I < UE_ARRAY_COUNT(Limits); ++I)
					if (FMath::IsNearlyEqual(Limit, Limits[I], 0.5f)) return I;
				return Limit <= 0.f ? int32(UE_ARRAY_COUNT(Limits)) - 1 : INDEX_NONE;
			},
			[](int32 I) { if (UTOLGameUserSettings* S = Settings()) S->SetFrameRateLimit(Limits[I]); },
			INVTEXT("Custom"))));

	// Since UE 5.3 a resolution quality of 0 means "project default" (r.ScreenPercentage.Default), which is also
	// what SetToDefaults picks. Show it as Auto with the handle at 100% instead of a meaningless 0%.
	static auto CurrentScale = []() -> float
	{
		float N = 1.f, C = 0.f, Lo = 0.f, Hi = 100.f;
		if (const UTOLGameUserSettings* S = Settings()) S->GetResolutionScaleInformationEx(N, C, Lo, Hi);
		return C;
	};
	Add(Row(INVTEXT("Resolution Scale"), INVTEXT("3D rendering resolution as a percentage of the screen resolution. The interface stays sharp. Auto uses the project default; Reset to Defaults brings Auto back."),
		Slider(25.f, 100.f, 1.f,
			[]() -> float { const float C = CurrentScale(); return C > 0.f ? C : 100.f; },
			[](float V) { if (UTOLGameUserSettings* S = Settings()) S->SetResolutionScaleValueEx(FMath::RoundToFloat(V)); },
			[](float V) { return CurrentScale() > 0.f ? FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V))) : INVTEXT("Auto"); })));

	Add(Section(INVTEXT("PICTURE")));
	Add(Row(INVTEXT("Brightness"), INVTEXT("Adjust until dark areas are dark but details are still visible. 50% is neutral."),
		Slider(0.f, 1.f, 0.01f,
			[] { const UTOLGameUserSettings* S = Settings(); return S ? S->Brightness : 0.5f; },
			[](float V) { if (UTOLGameUserSettings* S = Settings()) { S->Brightness = V; S->ApplyBrightness(); } },
			&Percent)));
	const UTOLSettingsConfig* Config = GetDefault<UTOLSettingsConfig>();
	const float MinFov = FMath::Min(Config->MinFieldOfView, Config->MaxFieldOfView);
	const float MaxFov = FMath::Max(Config->MaxFieldOfView, MinFov + 1.f);
	Add(Row(INVTEXT("Field of View"), INVTEXT("How wide the camera sees, in degrees. Wider shows more but makes things look smaller."),
		Slider(MinFov, MaxFov, 1.f,
			[] { const UTOLGameUserSettings* S = Settings(); return S ? S->FieldOfView : 90.f; },
			[](float V) { if (UTOLGameUserSettings* S = Settings()) S->FieldOfView = FMath::RoundToFloat(V); },
			[](float V) { return FText::FromString(FString::Printf(TEXT("%d°"), FMath::RoundToInt(V))); })));
	Add(Row(INVTEXT("Show FPS"), INVTEXT("Shows frames per second and frame time in the top-right corner."),
		Toggle([] { const UTOLGameUserSettings* S = Settings(); return S && S->bShowFPS; },
			[](bool b) { if (UTOLGameUserSettings* S = Settings()) S->bShowFPS = b; })));
	return Page(Rows);
}

TSharedRef<SWidget> STOLSettingsMenu::BuildGraphicsTab()
{
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto Add = [&Rows](const TSharedRef<SWidget>& Widget) { Rows->AddSlot().AutoHeight()[ Widget ]; };
	const TArray<FText> Levels = {INVTEXT("Low"), INVTEXT("Medium"), INVTEXT("High"), INVTEXT("Epic"), INVTEXT("Cinematic")};

	Add(Section(INVTEXT("QUALITY")));
	Add(Row(INVTEXT("Quality Preset"), INVTEXT("Sets every detail option below at once. Changing a single option makes it Custom."),
		// The engine's overall level also requires the resolution scale to match the preset (Epic = 100%), so with
		// Auto (0) it always reported Custom, and applying a preset overwrote the Resolution Scale on the Display tab.
		// Here the preset covers only the detail groups below; the resolution scale stays the player's own choice.
		Selector(Levels,
			[]() -> int32 {
				const UTOLGameUserSettings* S = Settings();
				if (!S) return INDEX_NONE;
				const int32 Groups[] = {S->GetViewDistanceQuality(), S->GetShadowQuality(), S->GetGlobalIlluminationQuality(),
					S->GetReflectionQuality(), S->GetAntiAliasingQuality(), S->GetTextureQuality(), S->GetVisualEffectQuality(),
					S->GetFoliageQuality(), S->GetPostProcessingQuality(), S->GetShadingQuality(), S->GetLandscapeQuality()};
				for (const int32 Level : Groups)
					if (Level != Groups[0]) return INDEX_NONE;
				return Groups[0] >= 0 && Groups[0] <= 4 ? Groups[0] : int32(INDEX_NONE);
			},
			[](int32 I)
			{
				UTOLGameUserSettings* S = Settings();
				if (!S) return;
				float N = 1.f, Scale = 0.f, Lo = 0.f, Hi = 100.f;
				S->GetResolutionScaleInformationEx(N, Scale, Lo, Hi);
				S->SetOverallScalabilityLevel(I);
				S->SetResolutionScaleValueEx(Scale);
			},
			INVTEXT("Custom"))));
	Add(Row(INVTEXT("Auto-Detect"), INVTEXT("Runs a short benchmark and picks settings for this PC. Press Apply to keep them."),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			TextButton(INVTEXT("DETECT"), [this]
			{
				if (UTOLGameUserSettings* S = Settings())
				{
					S->RunHardwareBenchmark();
					MarkDirty();
					Hint = INVTEXT("Recommended settings selected. Press Apply to use them.");
				}
			})
		]));

	Add(Section(INVTEXT("DETAILS")));
	Add(QualityRow(INVTEXT("View Distance"), INVTEXT("How far away objects keep their full detail."),
		&UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality));
	Add(QualityRow(INVTEXT("Shadows"), INVTEXT("Shadow sharpness and range. One of the most expensive options."),
		&UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality));
	Add(QualityRow(INVTEXT("Global Illumination"), INVTEXT("Bounced light and soft indirect shadows (Lumen)."),
		&UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality));
	Add(QualityRow(INVTEXT("Reflections"), INVTEXT("Quality of reflections on water and shiny surfaces."),
		&UGameUserSettings::GetReflectionQuality, &UGameUserSettings::SetReflectionQuality));
	Add(QualityRow(INVTEXT("Anti-Aliasing Quality"), INVTEXT("How well jagged edges are smoothed."),
		&UGameUserSettings::GetAntiAliasingQuality, &UGameUserSettings::SetAntiAliasingQuality));
	Add(QualityRow(INVTEXT("Textures"), INVTEXT("Texture resolution. Higher needs more video memory."),
		&UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality));
	Add(QualityRow(INVTEXT("Effects"), INVTEXT("Particles such as wood chips, dust and leaves."),
		&UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality));
	Add(QualityRow(INVTEXT("Foliage"), INVTEXT("Amount and draw distance of grass and small plants."),
		&UGameUserSettings::GetFoliageQuality, &UGameUserSettings::SetFoliageQuality));
	Add(QualityRow(INVTEXT("Post Processing"), INVTEXT("Bloom, depth of field, ambient occlusion and other screen effects."),
		&UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality));
	Add(QualityRow(INVTEXT("Shading"), INVTEXT("Material and lighting detail on surfaces."),
		&UGameUserSettings::GetShadingQuality, &UGameUserSettings::SetShadingQuality));
	Add(QualityRow(INVTEXT("Landscape"), INVTEXT("Terrain detail."),
		&UGameUserSettings::GetLandscapeQuality, &UGameUserSettings::SetLandscapeQuality));

	Add(Section(INVTEXT("EFFECTS")));
	static const int32 Methods[] = {0, 1, 2, 4};
	Add(Row(INVTEXT("Anti-Aliasing Method"), INVTEXT("TSR is the sharpest, TAA is lighter, FXAA is the fastest but blurrier."),
		Selector({INVTEXT("Off"), INVTEXT("FXAA"), INVTEXT("TAA"), INVTEXT("TSR")},
			[]() -> int32 {
				const UTOLGameUserSettings* S = Settings();
				const int32 Method = S ? S->GetEffectiveAntiAliasingMethod() : 4;
				for (int32 I = 0; I < UE_ARRAY_COUNT(Methods); ++I)
					if (Methods[I] == Method) return I;
				return INDEX_NONE;
			},
			[](int32 I) { if (UTOLGameUserSettings* S = Settings()) S->AntiAliasingMethod = Methods[I]; },
			INVTEXT("Other"))));
	Add(Row(INVTEXT("Motion Blur"), INVTEXT("Blurs fast camera movement."),
		Toggle([] { const UTOLGameUserSettings* S = Settings(); return !S || S->bMotionBlur; },
			[](bool b) { if (UTOLGameUserSettings* S = Settings()) S->bMotionBlur = b; })));
	return Page(Rows);
}

TSharedRef<SWidget> STOLSettingsMenu::BuildAudioTab()
{
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto Add = [&Rows](const TSharedRef<SWidget>& Widget) { Rows->AddSlot().AutoHeight()[ Widget ]; };

	Add(Section(INVTEXT("VOLUME")));
	Add(VolumeRow(INVTEXT("Master"), INVTEXT("Overall game volume."), &UTOLGameUserSettings::MasterVolume));
	Add(VolumeRow(INVTEXT("Music"), INVTEXT("Music volume."), &UTOLGameUserSettings::MusicVolume));
	Add(VolumeRow(INVTEXT("Effects"), INVTEXT("Chopping, footsteps and other sound effects."), &UTOLGameUserSettings::EffectsVolume));
	Add(VolumeRow(INVTEXT("Ambience"), INVTEXT("Wind, birds and other background sounds."), &UTOLGameUserSettings::AmbientVolume));
	Add(VolumeRow(INVTEXT("Voice"), INVTEXT("Voice chat and dialogue."), &UTOLGameUserSettings::VoiceVolume));

	Add(Section(INVTEXT("OTHER")));
	Add(Row(INVTEXT("Mute in Background"), INVTEXT("Silences the game while its window is not focused."),
		Toggle([] { const UTOLGameUserSettings* S = Settings(); return S && S->bMuteWhenUnfocused; },
			[](bool b) { if (UTOLGameUserSettings* S = Settings()) { S->bMuteWhenUnfocused = b; S->ApplyAudio(); } })));
	return Page(Rows);
}

TSharedRef<SWidget> STOLSettingsMenu::BuildControlsTab()
{
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto Add = [&Rows](const TSharedRef<SWidget>& Widget) { Rows->AddSlot().AutoHeight()[ Widget ]; };

	Add(Section(INVTEXT("MOUSE")));
	Add(Row(INVTEXT("Mouse Sensitivity"), INVTEXT("How fast the camera turns when you move the mouse."),
		Slider(0.1f, 3.f, 0.05f,
			[] { const UTOLGameUserSettings* S = Settings(); return S ? S->MouseSensitivity : 1.f; },
			[](float V) { if (UTOLGameUserSettings* S = Settings()) S->MouseSensitivity = V; },
			[](float V) { return FText::FromString(FString::Printf(TEXT("%.2f"), V)); })));
	Add(Row(INVTEXT("Invert Mouse Y"), INVTEXT("Moving the mouse forward looks down instead of up."),
		Toggle([] { const UTOLGameUserSettings* S = Settings(); return S && S->bInvertMouseY; },
			[](bool b) { if (UTOLGameUserSettings* S = Settings()) S->bInvertMouseY = b; })));

	Add(Section(INVTEXT("KEY BINDINGS")));
	if (Bindings.Num() == 0)
	{
		Add(SNew(SBox).Padding(FMargin(22.f, 10.f))
			[
				SNew(STextBlock).Text(INVTEXT("No rebindable keys found (Project Settings > Game > Settings Menu > Rebindable Context)."))
				.Font(Font("Italic", 16)).ColorAndOpacity(Muted).AutoWrapText(true)
			]);
	}
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
		Add(Row(Bindings[Index].Label, INVTEXT("Click, then press a key or mouse button. Esc cancels."), KeyBinding(Index)));
	if (Bindings.Num() > 0)
	{
		Add(SNew(SBox).Padding(FMargin(22.f, 14.f, 22.f, 4.f)).HAlign(HAlign_Right)
			[
				TextButton(INVTEXT("RESET KEYS"), [this]
				{
					StopListening();
					if (UTOLGameUserSettings* S = Settings(); S && S->KeyOverrides.Num() > 0)
					{
						S->KeyOverrides.Reset();
						MarkDirty();
					}
				}, false, nullptr, 170.f)
			]);
	}
	return Page(Rows);
}

TSharedRef<SWidget> STOLSettingsMenu::BuildBottomBar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[ TextButton(INVTEXT("RESET TO DEFAULTS"), [this] { AskResetToDefaults(); }, false, nullptr, 230.f) ]
		+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 18.f, 0.f)
		[
			SNew(STextBlock).Text(INVTEXT("Unsaved changes")).Font(Font("Italic", 15)).ColorAndOpacity(Gold)
			.Visibility_Lambda([this] { return bDirty ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)[ TextButton(INVTEXT("BACK"), [this] { RequestClose(); }) ]
		+ SHorizontalBox::Slot().AutoWidth()[ TextButton(INVTEXT("APPLY"), [this] { Apply(); }, true, [this] { return bDirty; }) ];
}

TSharedRef<SWidget> STOLSettingsMenu::Page(const TSharedRef<SVerticalBox>& Rows) const
{
	const FStyles& Style = TOLSettingsStyle::Get();
	return SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar).ScrollBarThickness(FVector2D(5.0, 5.0))
		+ SScrollBox::Slot().Padding(6.f, 4.f, 14.f, 12.f)[ Rows ];
}

// ---------------------------------------------------------------------------------------------------------------
// Rows and controls

TSharedRef<SWidget> STOLSettingsMenu::Row(const FText& Label, const FText& Description, const TSharedRef<SWidget>& Control)
{
	const FStyles& Style = TOLSettingsStyle::Get();
	TSharedRef<SBorder> Border = SNew(SBorder).Padding(FMargin(22.f, 7.f, 16.f, 7.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.45f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Label).Font(Font("Regular", 18)).ColorAndOpacity(Cream)
		]
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)[ Control ]
	];
	TWeakPtr<SBorder> Weak = Border;
	Border->SetBorderImage(TAttribute<const FSlateBrush*>::CreateLambda([Weak, &Style]
	{
		const TSharedPtr<SBorder> Pinned = Weak.Pin();
		return Pinned && Pinned->IsHovered() ? static_cast<const FSlateBrush*>(&Style.RowHover) : static_cast<const FSlateBrush*>(&Style.None);
	}));
	Border->SetOnMouseEnter(FNoReplyPointerEventHandler::CreateLambda([this, Description](const FGeometry&, const FPointerEvent&)
	{
		if (ListeningBinding == INDEX_NONE) Hint = Description;
	}));
	return SNew(SBox).MinDesiredHeight(56.f)[ Border ];
}

TSharedRef<SWidget> STOLSettingsMenu::Section(const FText& Title) const
{
	return SNew(SBox).Padding(FMargin(22.f, 20.f, 22.f, 6.f))
	[
		SNew(STextBlock).Text(Title).Font(Font("Bold", 14, 320)).ColorAndOpacity(Gold)
	];
}

TSharedRef<SWidget> STOLSettingsMenu::ArrowButton(bool bNext, TFunction<void()> OnClick)
{
	const FStyles& Style = TOLSettingsStyle::Get();
	TSharedPtr<SButton> Button;
	SAssignNew(Button, SButton).ButtonStyle(&Style.Plain).ContentPadding(FMargin(14.f, 0.f)).VAlign(VAlign_Center)
		.OnClicked_Lambda([OnClick] { OnClick(); return FReply::Handled(); });
	TWeakPtr<SButton> Weak = Button;
	Button->SetContent(
		SNew(STextBlock).Text(bNext ? INVTEXT("›") : INVTEXT("‹")).Font(Font("Bold", 30))
		.ColorAndOpacity_Lambda([Weak]
		{
			const TSharedPtr<SButton> Pinned = Weak.Pin();
			return FSlateColor(Pinned && Pinned->IsHovered() ? Gold : Muted);
		}));
	return Button.ToSharedRef();
}

TSharedRef<SWidget> STOLSettingsMenu::Selector(TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set, FText Fallback)
{
	const FStyles& Style = TOLSettingsStyle::Get();
	TSharedRef<TArray<FText>> Shared = MakeShared<TArray<FText>>(MoveTemp(Options));
	auto Step = [this, Shared, Get, Set](int32 Direction)
	{
		const int32 Count = Shared->Num();
		if (Count == 0) return;
		const int32 Current = Get();
		const int32 Next = Shared->IsValidIndex(Current) ? (Current + Direction + Count) % Count : (Direction > 0 ? 0 : Count - 1);
		Set(Next);
		MarkDirty();
	};

	// Little segments under the value show where it sits in the list (only for short lists).
	TSharedRef<SHorizontalBox> Dots = SNew(SHorizontalBox);
	if (Shared->Num() <= 8)
		for (int32 Index = 0; Index < Shared->Num(); ++Index)
			Dots->AddSlot().AutoWidth().Padding(2.f, 0.f)
			[
				SNew(SBox).WidthOverride(20.f).HeightOverride(3.f)
				[
					SNew(SImage).Image(&Style.White)
					.ColorAndOpacity_Lambda([Get, Index] { return FSlateColor(Get() == Index ? Gold : FLinearColor(1.f, 1.f, 1.f, 0.16f)); })
				]
			];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[ ArrowButton(false, [Step] { Step(-1); }) ]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font("Bold", 17)).ColorAndOpacity(Cream)
				.Text_Lambda([Shared, Get, Fallback]
				{
					const int32 Index = Get();
					return Shared->IsValidIndex(Index) ? (*Shared)[Index] : Fallback;
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)[ Dots ]
		]
		+ SHorizontalBox::Slot().AutoWidth()[ ArrowButton(true, [Step] { Step(1); }) ];
}

TSharedRef<SWidget> STOLSettingsMenu::Toggle(TFunction<bool()> Get, TFunction<void(bool)> Set)
{
	return Selector({INVTEXT("Off"), INVTEXT("On")}, [Get] { return Get() ? 1 : 0; }, [Set](int32 Index) { Set(Index == 1); });
}

TSharedRef<SWidget> STOLSettingsMenu::Slider(float Min, float Max, float Step, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FText(float)> Format)
{
	const FStyles& Style = TOLSettingsStyle::Get();
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(14.f, 0.f, 18.f, 0.f)
		[
			SNew(SSlider).Style(&Style.Slider).MinValue(Min).MaxValue(Max).StepSize(Step).MouseUsesStep(true)
			.SliderBarColor(FLinearColor(1.f, 1.f, 1.f, 0.22f)).SliderHandleColor(Gold).IndentHandle(false)
			.Value_Lambda([Get] { return Get(); })
			.OnValueChanged_Lambda([this, Set](float Value) { Set(Value); MarkDirty(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(72.f).HAlign(HAlign_Right)
			[
				SNew(STextBlock).Font(Font("Bold", 17)).ColorAndOpacity(Cream).Text_Lambda([Get, Format] { return Format(Get()); })
			]
		];
}

TSharedRef<SWidget> STOLSettingsMenu::TextButton(const FText& Label, TFunction<void()> OnClick, bool bPrimary, TFunction<bool()> Enabled, float MinWidth)
{
	const FStyles& Style = TOLSettingsStyle::Get();
	TSharedPtr<SButton> Button;
	SAssignNew(Button, SButton).ButtonStyle(bPrimary ? &Style.Primary : &Style.Secondary)
		.HAlign(HAlign_Center).VAlign(VAlign_Center).ContentPadding(FMargin(22.f, 11.f))
		.OnClicked_Lambda([OnClick] { if (OnClick) OnClick(); return FReply::Handled(); });
	if (Enabled) Button->SetEnabled(TAttribute<bool>::CreateLambda(Enabled));
	TWeakPtr<SButton> Weak = Button;
	Button->SetContent(
		SNew(SBox).MinDesiredWidth(FMath::Max(MinWidth - 44.f, 0.f)).HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(Label).Font(Font("Bold", 16, 140)).Justification(ETextJustify::Center)
			.ColorAndOpacity_Lambda([Weak, bPrimary]
			{
				const TSharedPtr<SButton> Pinned = Weak.Pin();
				if (!Pinned || !Pinned->IsEnabled()) return FSlateColor(Faint);
				if (bPrimary) return FSlateColor(Ink);
				return FSlateColor(Pinned->IsHovered() ? Gold : Cream);
			})
		]);
	return Button.ToSharedRef();
}

TSharedRef<SWidget> STOLSettingsMenu::KeyBinding(int32 Index)
{
	const FStyles& Style = TOLSettingsStyle::Get();
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).WidthOverride(230.f).HeightOverride(42.f)
			[
				SNew(SButton).ButtonStyle(&Style.Key).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.OnClicked_Lambda([this, Index]
				{
					ListeningBinding = Index;
					Hint = INVTEXT("Press a key or mouse button to bind it. Esc cancels.");
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Font(Font("Bold", 16, 60))
					.Text_Lambda([this, Index]
					{
						return ListeningBinding == Index ? INVTEXT("PRESS A KEY...") : EffectiveKey(Index).GetDisplayName(false);
					})
					.ColorAndOpacity_Lambda([this, Index] { return FSlateColor(ListeningBinding == Index ? Gold : Cream); })
				]
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock).Font(Font("Italic", 14)).ColorAndOpacity(Muted).Text_Lambda([this, Index] { return SharedKeyNote(Index); })
		];
}

TSharedRef<SWidget> STOLSettingsMenu::QualityRow(const FText& Label, const FText& Description,
	int32 (UGameUserSettings::*Get)() const, void (UGameUserSettings::*Set)(int32))
{
	return Row(Label, Description,
		Selector({INVTEXT("Low"), INVTEXT("Medium"), INVTEXT("High"), INVTEXT("Epic"), INVTEXT("Cinematic")},
			[Get] { const UTOLGameUserSettings* S = Settings(); return S ? FMath::Clamp((S->*Get)(), 0, 4) : 0; },
			[Set](int32 I) { if (UTOLGameUserSettings* S = Settings()) (S->*Set)(I); }));
}

TSharedRef<SWidget> STOLSettingsMenu::VolumeRow(const FText& Label, const FText& Description, float UTOLGameUserSettings::*Volume)
{
	return Row(Label, Description,
		Slider(0.f, 1.f, 0.01f,
			[Volume] { const UTOLGameUserSettings* S = Settings(); return S ? S->*Volume : 1.f; },
			[Volume](float V) { if (UTOLGameUserSettings* S = Settings()) { S->*Volume = V; S->ApplyAudio(); } },
			&Percent));
}

// ---------------------------------------------------------------------------------------------------------------
// Key binding

FKey STOLSettingsMenu::EffectiveKey(int32 Index) const
{
	if (!Bindings.IsValidIndex(Index)) return EKeys::Invalid;
	const FTOLBindingSlot& Binding = Bindings[Index];
	const UTOLGameUserSettings* S = Settings();
	const FKey* Override = S ? S->FindKeyOverride(Binding.Action, Binding.Slot) : nullptr;
	return Override ? *Override : Binding.DefaultKey;
}

FText STOLSettingsMenu::SharedKeyNote(int32 Index) const
{
	if (ListeningBinding == Index) return FText::GetEmpty();
	const FKey Key = EffectiveKey(Index);
	for (int32 Other = 0; Other < Bindings.Num(); ++Other)
		if (Other != Index && EffectiveKey(Other) == Key)
			return FText::Format(INVTEXT("Also: {0}"), Bindings[Other].Label);
	return FText::GetEmpty();
}

void STOLSettingsMenu::AssignKey(const FKey& Key)
{
	UTOLGameUserSettings* S = Settings();
	if (S && Bindings.IsValidIndex(ListeningBinding))
	{
		const FTOLBindingSlot& Binding = Bindings[ListeningBinding];
		if (EffectiveKey(ListeningBinding) != Key)
		{
			S->SetKeyOverride(Binding.Action, Binding.Slot, Key, Binding.DefaultKey);
			MarkDirty();
		}
	}
	StopListening();
}

void STOLSettingsMenu::StopListening()
{
	if (ListeningBinding == INDEX_NONE) return;
	ListeningBinding = INDEX_NONE;
	Hint = FText::GetEmpty();
}

FReply STOLSettingsMenu::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (ListeningBinding == INDEX_NONE || bDialogOpen) return FReply::Unhandled();
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape) StopListening();
	else if (IsBindableKey(Key)) AssignKey(Key);
	return FReply::Handled();
}

FReply STOLSettingsMenu::OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (ListeningBinding == INDEX_NONE || bDialogOpen) return FReply::Unhandled();
	AssignKey(Event.GetEffectingButton());
	return FReply::Handled();
}

FReply STOLSettingsMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		if (bDialogOpen) CancelDialog();
		else RequestClose();
		return FReply::Handled();
	}
	if (!bDialogOpen && (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder))
	{
		StopListening();
		ActiveTab = (ActiveTab + (Key == EKeys::Gamepad_RightShoulder ? 1 : 3)) % 4;
		Hint = FText::GetEmpty();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, Event);
}

// ---------------------------------------------------------------------------------------------------------------
// Actions

void STOLSettingsMenu::Apply(TFunction<void()> AfterApply)
{
	StopListening();
	UTOLGameUserSettings* S = Settings();
	if (!S) return;

	// A new resolution or window mode must be confirmed, or it reverts by itself (in case the screen went black).
	// The editor ignores resolution changes, so PIE skips the question.
	const bool bVideoChanged = !GIsEditor && (S->GetScreenResolution() != S->GetLastConfirmedScreenResolution()
		|| S->GetFullscreenMode() != S->GetLastConfirmedFullscreenMode());
	S->ApplySettings(false);
	bDirty = false;
	Hint = INVTEXT("Settings applied.");

	if (!bVideoChanged)
	{
		S->ConfirmVideoMode();
		S->SaveSettings();
		if (AfterApply) AfterApply();
		return;
	}

	auto Keep = [AfterApply]
	{
		if (UTOLGameUserSettings* Current = Settings()) { Current->ConfirmVideoMode(); Current->SaveSettings(); }
		if (AfterApply) AfterApply();
	};
	auto RevertVideo = [this, AfterApply]
	{
		if (UTOLGameUserSettings* Current = Settings())
		{
			Current->RevertVideoMode();
			Current->ApplyResolutionSettings(false);
			Current->SaveSettings();
		}
		Hint = INVTEXT("Display settings were reverted.");
		if (AfterApply) AfterApply();
	};
	ShowDialog(INVTEXT("KEEP THESE DISPLAY SETTINGS?"),
		TAttribute<FText>::CreateLambda([this]
		{
			const int32 Left = FMath::Max(0, FMath::CeilToInt(DialogDeadline - FPlatformTime::Seconds()));
			return FText::Format(INVTEXT("The previous resolution and window mode come back in {0} seconds."), FText::AsNumber(Left));
		}),
		{{INVTEXT("REVERT"), RevertVideo}, {INVTEXT("KEEP"), Keep, true}},
		RevertVideo, 15.0, RevertVideo);
}

void STOLSettingsMenu::Revert()
{
	StopListening();
	if (UTOLGameUserSettings* S = Settings())
	{
		S->LoadSettings(true);
		S->ApplyCustomSettings(); // undo live previews (volume, brightness)
	}
	bDirty = false;
}

void STOLSettingsMenu::AskResetToDefaults()
{
	StopListening();
	ShowDialog(INVTEXT("RESET TO DEFAULTS?"),
		INVTEXT("Every option and key binding goes back to its default value. Your resolution and window mode stay as they are. Press Apply afterwards to keep the result."),
		{{INVTEXT("CANCEL"), nullptr},
		 {INVTEXT("RESET"), [this]
			{
				UTOLGameUserSettings* S = Settings();
				if (!S) return;
				const FIntPoint Resolution = S->GetLastConfirmedScreenResolution();
				const EWindowMode::Type Mode = S->GetLastConfirmedFullscreenMode();
				S->SetToDefaults();
				S->SetScreenResolution(Resolution);
				S->SetFullscreenMode(Mode);
				S->ConfirmVideoMode();
				S->ApplyAudio();
				S->ApplyBrightness();
				MarkDirty();
			}, true}});
}

void STOLSettingsMenu::RequestClose()
{
	StopListening();
	if (!bDirty)
	{
		Close();
		return;
	}
	ShowDialog(INVTEXT("UNSAVED CHANGES"), INVTEXT("Apply your changes before leaving?"),
		{{INVTEXT("DISCARD"), [this] { Revert(); Close(); }},
		 {INVTEXT("CANCEL"), nullptr},
		 {INVTEXT("APPLY"), [this] { Apply([this] { Close(); }); }, true}});
}

void STOLSettingsMenu::Close()
{
	OnClosed.ExecuteIfBound();
}

// ---------------------------------------------------------------------------------------------------------------
// Dialog

void STOLSettingsMenu::ShowDialog(const FText& Title, const TAttribute<FText>& Body, const TArray<FDialogButton>& Buttons,
	TFunction<void()> OnCancel, double TimeoutSeconds, TFunction<void()> OnTimeout)
{
	CloseDialog();
	TSharedRef<SHorizontalBox> ButtonRow = SNew(SHorizontalBox);
	for (const FDialogButton& Button : Buttons)
	{
		TFunction<void()> Action = Button.Action;
		ButtonRow->AddSlot().AutoWidth().Padding(12.f, 0.f, 0.f, 0.f)
		[
			TextButton(Button.Label, [this, Action]
			{
				const TFunction<void()> Run = Action; // the dialog may be rebuilt by the action
				CloseDialog();
				if (Run) Run();
			}, Button.bPrimary, nullptr, 140.f)
		];
	}
	DialogBox->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(Title).Font(Font("Bold", 24, 100)).ColorAndOpacity(Gold)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(Body).Font(Font("Regular", 18)).ColorAndOpacity(Cream).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 28.f, 0.f, 0.f).HAlign(HAlign_Right)[ ButtonRow ]);

	DialogCancel = MoveTemp(OnCancel);
	bDialogOpen = true;
	if (TimeoutSeconds > 0.0 && OnTimeout)
	{
		DialogDeadline = FPlatformTime::Seconds() + TimeoutSeconds;
		DialogTimeout = MoveTemp(OnTimeout);
		DialogTimer = RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
		{
			if (FPlatformTime::Seconds() < DialogDeadline) return EActiveTimerReturnType::Continue;
			const TFunction<void()> Run = DialogTimeout;
			DialogTimer.Reset();
			CloseDialog();
			if (Run) Run();
			return EActiveTimerReturnType::Stop;
		}));
	}
}

void STOLSettingsMenu::CloseDialog()
{
	if (DialogTimer.IsValid())
	{
		UnRegisterActiveTimer(DialogTimer.ToSharedRef());
		DialogTimer.Reset();
	}
	const bool bWasOpen = bDialogOpen;
	bDialogOpen = false;
	DialogDeadline = 0.0;
	DialogCancel = nullptr;
	DialogTimeout = nullptr;
	if (bWasOpen && FSlateApplication::IsInitialized())
		FSlateApplication::Get().SetAllUserFocus(SharedThis(this), EFocusCause::SetDirectly);
}

void STOLSettingsMenu::CancelDialog()
{
	const TFunction<void()> Run = DialogCancel;
	CloseDialog();
	if (Run) Run();
}

// ---------------------------------------------------------------------------------------------------------------
// UMG wrapper

TSharedRef<SWidget> UTOLSettingsWidget::RebuildWidget()
{
	Menu = SNew(STOLSettingsMenu).OnClosed(FSimpleDelegate::CreateUObject(this, &UTOLSettingsWidget::HandleMenuClosed));
	return Menu.ToSharedRef();
}

void UTOLSettingsWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Menu.Reset();
}

void UTOLSettingsWidget::RequestClose()
{
	if (Menu.IsValid()) Menu->RequestClose();
	else HandleMenuClosed();
}

void UTOLSettingsWidget::HandleMenuClosed()
{
	RemoveFromParent();
	OnClosed.Broadcast();
}

UTOLSettingsWidget* UTOLSettingsWidget::OpenSettings(UObject* WorldContextObject, TSubclassOf<UTOLSettingsWidget> WidgetClass, int32 ZOrder)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World) return nullptr;
	TSubclassOf<UTOLSettingsWidget> Class = WidgetClass ? WidgetClass : TSubclassOf<UTOLSettingsWidget>(StaticClass());
	UTOLSettingsWidget* Widget = nullptr;
	if (APlayerController* Controller = World->GetFirstPlayerController())
		Widget = CreateWidget<UTOLSettingsWidget>(Controller, Class);
	else
		Widget = CreateWidget<UTOLSettingsWidget>(World, Class);
	if (Widget) Widget->AddToViewport(ZOrder);
	return Widget;
}
