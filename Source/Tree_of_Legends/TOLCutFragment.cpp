#include "TOLCutFragment.h"

#include "ProceduralMeshComponent.h"

ATOLCutFragment::ATOLCutFragment()
{
	PrimaryActorTick.bCanEverTick = false;
	CutMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CutMesh"));
	RootComponent = CutMesh;
	CutMesh->bUseComplexAsSimpleCollision = false;
	CutMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	// Loose logs are not stairs: pieces under MaxStepHeight (45 cm) were auto-stepped onto, lifting the player
	// on top of them. Walking into a piece now blocks and pushes it instead; landing on one still stands on it.
	CutMesh->CanCharacterStepUpOn = ECB_No;
	// Thin pieces can cross a contact plane between discrete physics steps.
	CutMesh->SetUseCCD(true);
}
