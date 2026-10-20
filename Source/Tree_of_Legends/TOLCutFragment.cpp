#include "TOLCutFragment.h"

#include "ProceduralMeshComponent.h"

ATOLCutFragment::ATOLCutFragment()
{
	PrimaryActorTick.bCanEverTick = false;
	CutMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CutMesh"));
	RootComponent = CutMesh;
	CutMesh->bUseComplexAsSimpleCollision = false;
	CutMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
}
