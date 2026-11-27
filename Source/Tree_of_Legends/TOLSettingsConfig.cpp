#include "TOLSettingsConfig.h"

#include "InputMappingContext.h"
#include "InputAction.h"
#include "Sound/SoundClass.h"

UTOLSettingsConfig::UTOLSettingsConfig()
{
	RebindableContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Inputs/IMC_Mapping.IMC_Mapping")));
	HiddenActions.Add(TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Inputs/Keys/IA_Mouse.IA_Mouse"))));
}
