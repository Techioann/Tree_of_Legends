#include "TOLMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "TOLGameUserSettings.h"
#include "TOLSettingsWidget.h"

namespace
{
	float EaseOutCubic(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Cube(1.f - T); }

	UTextBlock* FindLabel(UWidget* Root)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Root)) return Text;
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Root))
			for (UWidget* Child : Panel->GetAllChildren())
				if (UTextBlock* Found = FindLabel(Child)) return Found;
		return nullptr;
	}
}

void UTOLMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Menus are shown with Set Input Mode UI Only; without this the focus request fails and keys never reach the menu.
	SetIsFocusable(true);
}

void UTOLMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CollectWidgets();
	PlayIntro();
	if (!IsDesignTime() && !SettingsButtonName.IsNone() && WidgetTree)
		if (UButton* SettingsButton = Cast<UButton>(WidgetTree->FindWidget(SettingsButtonName)))
			SettingsButton->OnClicked.AddUniqueDynamic(this, &UTOLMainMenuWidget::OpenSettings);
}

void UTOLMainMenuWidget::CollectWidgets()
{
	Buttons.Reset(); Items.Reset();
	if (!WidgetTree) return;
	Background = Cast<UImage>(WidgetTree->FindWidget(BackgroundName));
	if (Background.IsValid()) BackgroundColor = Background->GetColorAndOpacity();

	float Delay = 0.15f;
	for (const FName& Name : HeaderNames)
		if (UWidget* Widget = WidgetTree->FindWidget(Name))
		{
			Items.Add({Widget, Delay, FVector2D(0.0, -SlideDistance)});
			Delay += ItemStagger * 1.5f;
		}
	if (const UPanelWidget* Panel = Cast<UPanelWidget>(WidgetTree->FindWidget(ButtonPanelName)))
		for (UWidget* Child : Panel->GetAllChildren())
			if (UButton* Button = Cast<UButton>(Child))
			{
				FButtonFx Fx;
				Fx.Button = Button;
				Fx.Label = FindLabel(Button->GetContent());
				if (Fx.Label.IsValid()) Fx.BaseColor = Fx.Label->GetColorAndOpacity();
				Fx.Delay = Delay;
				Buttons.Add(Fx);
				Delay += ItemStagger;
			}
	for (const FName& Name : FooterNames)
		if (UWidget* Widget = WidgetTree->FindWidget(Name))
			Items.Add({Widget, Delay + 0.2f, FVector2D::ZeroVector});
	bCollected = true;
}

void UTOLMainMenuWidget::PlayIntro()
{
	Elapsed = 0.f;
	ItemElapsed = 0.f;
	if (IsDesignTime()) return; // the Designer shows the final layout
	ApplyIntro(0.f, 0.f);
}

void UTOLMainMenuWidget::ApplyIntro(float BackgroundTime, float ItemTime)
{
	if (UImage* Image = Background.Get())
	{
		const float T = BackgroundFadeTime > 0.f ? EaseOutCubic(BackgroundTime / BackgroundFadeTime) : 1.f;
		const FLinearColor From(0.f, 0.f, 0.f, BackgroundColor.A);
		Image->SetColorAndOpacity(FMath::Lerp(From, BackgroundColor, T));
	}
	for (const FItemFx& Item : Items)
		if (UWidget* Widget = Item.Widget.Get())
		{
			const float T = EaseOutCubic((ItemTime - Item.Delay) / ItemIntroTime);
			Widget->SetRenderOpacity(T);
			Widget->SetRenderTranslation(Item.From * (1.f - T));
		}
}

void UTOLMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (IsDesignTime()) return;
	if (!bCollected) CollectWidgets();
	const float Dt = FMath::Min(InDeltaTime, 0.1f); // no jump after a hitch
	Elapsed += Dt;
	ItemElapsed += Dt;
	ApplyIntro(Elapsed, ItemElapsed);

	if (UImage* Image = Background.Get())
	{
		const float Phase = UE_TWO_PI * Elapsed / BackgroundPeriod;
		Image->SetRenderScale(FVector2D(1.0 + BackgroundZoom * (0.5 - 0.5 * FMath::Cos(Phase))));
		const float Pan = MyGeometry.GetLocalSize().X * BackgroundZoom * 0.25f;
		Image->SetRenderTranslation(FVector2D(FMath::Sin(Phase * 0.5f) * Pan, FMath::Sin(Phase * 0.37f) * Pan * 0.4f));
	}

	const float Blend = 1.f - FMath::Exp(-HoverSpeed * Dt);
	for (FButtonFx& Fx : Buttons)
	{
		UButton* Button = Fx.Button.Get();
		if (!Button) continue;
		const bool bActive = Button->GetIsEnabled() && (Button->IsHovered() || Button->HasKeyboardFocus());
		Fx.Hover = FMath::Lerp(Fx.Hover, bActive ? 1.f : 0.f, Blend);
		Fx.Press = FMath::Lerp(Fx.Press, Button->IsPressed() ? 1.f : 0.f, Blend);
		const float In = EaseOutCubic((ItemElapsed - Fx.Delay) / ItemIntroTime);
		Button->SetRenderOpacity(In);
		Button->SetRenderTranslation(FVector2D(-SlideDistance * (1.f - In) + HoverShift * Fx.Hover, 0.0));
		Button->SetRenderScale(FVector2D(1.0 - 0.04 * Fx.Press));
		if (UTextBlock* Label = Fx.Label.Get())
		{
			const FLinearColor Base = Fx.BaseColor.IsColorSpecified() ? Fx.BaseColor.GetSpecifiedColor() : FLinearColor::White;
			Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(Base, HoverTextColor, Fx.Hover)));
		}
	}
}

void UTOLMainMenuWidget::OpenSettings()
{
	if (SettingsWidget && SettingsWidget->IsInViewport()) return;
	SettingsWidget = UTOLSettingsWidget::OpenSettings(this, SettingsWidgetClass, 100);
	if (!SettingsWidget) return;
	SettingsWidget->OnClosed.AddUniqueDynamic(this, &UTOLMainMenuWidget::HandleSettingsClosed);
	SetMenuItemsHidden(true);
}

bool UTOLMainMenuWidget::IsCloseKey(const FKey& Key) const
{
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right) return true;
	// The player may have moved the menu key in Settings > Controls.
	if (const UTOLGameUserSettings* Settings = UTOLGameUserSettings::Get())
		for (const FTOLKeyOverride& Override : Settings->KeyOverrides)
			if (Override.Action == TEXT("IA_Menu") && Override.Key == Key) return true;
	return false;
}

FReply UTOLMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (!IsCloseKey(Key) || InKeyEvent.IsRepeat()) return Super::NativeOnKeyDown(InGeometry, InKeyEvent);

	// Settings on top (it normally has focus itself; this covers focus lost to a click elsewhere).
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		SettingsWidget->RequestClose();
		return FReply::Handled();
	}
	const UButton* CloseButton = WidgetTree && !CloseButtonName.IsNone() ? Cast<UButton>(WidgetTree->FindWidget(CloseButtonName)) : nullptr;
	if (!CloseButton || !CloseButton->GetIsEnabled()) return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	PendingCloseKey = Key;
	return FReply::Handled();
}

FReply UTOLMainMenuWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!PendingCloseKey.IsValid() || InKeyEvent.GetKey() != PendingCloseKey) return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
	PendingCloseKey = FKey();
	// Closing on release: back in Game Only input the key is no longer held, so IA_Menu cannot reopen the menu.
	if (UButton* CloseButton = WidgetTree ? Cast<UButton>(WidgetTree->FindWidget(CloseButtonName)) : nullptr)
		CloseButton->OnClicked.Broadcast(); // same as clicking it: the Blueprint's Resume logic runs
	return FReply::Handled();
}

void UTOLMainMenuWidget::HandleSettingsClosed()
{
	SettingsWidget = nullptr;
	PendingCloseKey = FKey();
	SetMenuItemsHidden(false);
	SetKeyboardFocus(); // so Esc works again right away
	// Bring the title and buttons back with the intro motion, without fading the background again.
	ItemElapsed = 0.f;
	ApplyIntro(Elapsed, ItemElapsed);
}

void UTOLMainMenuWidget::SetMenuItemsHidden(bool bHidden)
{
	if (bHidden)
	{
		TArray<UWidget*> Widgets;
		for (const FItemFx& Item : Items) Widgets.Add(Item.Widget.Get());
		if (WidgetTree) Widgets.Add(WidgetTree->FindWidget(ButtonPanelName));
		for (UWidget* Widget : Widgets)
			if (Widget && !HiddenItems.Contains(Widget))
			{
				HiddenItems.Add(Widget, Widget->GetVisibility());
				Widget->SetVisibility(ESlateVisibility::Hidden);
			}
		return;
	}
	for (const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>& Entry : HiddenItems)
		if (UWidget* Widget = Entry.Key.Get()) Widget->SetVisibility(Entry.Value);
	HiddenItems.Reset();
}
