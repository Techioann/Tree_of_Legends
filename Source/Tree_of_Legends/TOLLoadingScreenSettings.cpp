#include "TOLLoadingScreenSettings.h"

#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "TOLLoadingScreen"

UTOLLoadingScreenSettings::UTOLLoadingScreenSettings()
{
	BackgroundImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Background.Background")));
	Tips = {
		LOCTEXT("Tip0", "Sprinting drains stamina. Slow down for a moment and it refills."),
		LOCTEXT("Tip1", "Logs heavier than 250 kg are too heavy to carry. Cut them into smaller pieces first."),
		LOCTEXT("Tip2", "Wood is sold by volume: every cubic metre you bring back counts."),
		LOCTEXT("Tip3", "Different tree species fetch different prices per cubic metre."),
		LOCTEXT("Tip4", "The forest is bigger with two axes. Invite a friend."),
	};
}

#undef LOCTEXT_NAMESPACE
