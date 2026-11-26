#include "TOLSprintSubsystem.h"

#include "TOLSprintComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

bool UTOLSprintSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType==EWorldType::Game || WorldType==EWorldType::PIE;
}

TStatId UTOLSprintSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTOLSprintSubsystem, STATGROUP_Tickables);
}

void UTOLSprintSubsystem::Tick(float DeltaTime)
{
	SinceCheck+=DeltaTime;
	if (SinceCheck<0.25f) return;
	SinceCheck=0.f;
	UWorld* World=GetWorld();
	if (!World) return;
	for (FConstPlayerControllerIterator It=World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller=It->Get();
		ACharacter* Character=Controller && Controller->IsLocalController() ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (!Character || Character->FindComponentByClass<UTOLSprintComponent>()) continue;
		UTOLSprintComponent* Sprint=NewObject<UTOLSprintComponent>(Character,TEXT("Sprint"));
		Character->AddInstanceComponent(Sprint);
		Sprint->RegisterComponent();
	}
}
