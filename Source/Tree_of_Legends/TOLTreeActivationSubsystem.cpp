#include "TOLTreeActivationSubsystem.h"
#include "Components/StaticMeshComponent.h"

AActor* UTOLTreeActivationSubsystem::GetActiveTree(UStaticMeshComponent* Source) const
{
	const TWeakObjectPtr<AActor>* Active=Activated.Find(Source);
	return Active ? Active->Get() : nullptr;
}

bool UTOLTreeActivationSubsystem::BeginActivation(UStaticMeshComponent* Source)
{
	if (!IsValid(Source) || Source->GetWorld()!=GetWorld() || Activated.Contains(Source) || Activating.Contains(Source)) return false;
	Activating.Add(Source);
	return true;
}

bool UTOLTreeActivationSubsystem::CommitActivation(UStaticMeshComponent* Source, AActor* Active)
{
	if (!IsValid(Source) || !IsValid(Active) || Active->GetWorld()!=GetWorld() || !Activating.Contains(Source)) return false;
	Activated.Add(Source,Active);
	Activating.Remove(Source);
	return true;
}

void UTOLTreeActivationSubsystem::AbortActivation(UStaticMeshComponent* Source)
{
	Activating.Remove(Source);
}

bool UTOLTreeActivationSubsystem::PrepareDormantTreesOnce()
{
	if (bDormantPrepared) return false;
	bDormantPrepared=true;
	return true;
}

void UTOLTreeActivationSubsystem::Deinitialize()
{
	Activated.Empty(); Activating.Empty(); bDormantPrepared=false;
	Super::Deinitialize();
}
