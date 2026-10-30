#pragma once
#include "CoreMinimal.h"
struct FProcMeshSection;

namespace TOLCutValuation
{
	// Money is integer minor units; keep conversion/multiplication in the exact double integer range.
	constexpr int64 MaxMoney = 9007199254740991LL;
	bool Measure(const TArray<FProcMeshSection>& Sections, const FTransform& Transform,
		double& OutCubicMeters, FString& OutError);
}
