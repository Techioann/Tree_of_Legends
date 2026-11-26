#pragma once
#include "CoreMinimal.h"
struct FProcMeshSection;

namespace TOLCutValuation
{
	// Money is integer minor units; keep conversion/multiplication in the exact double integer range.
	constexpr int64 MaxMoney = 9007199254740991LL;
	/** Exact volume of a closed, consistently wound solid; rejects any open or nonmanifold surface. */
	bool Measure(const TArray<FProcMeshSection>& Sections, const FTransform& Transform,
		double& OutCubicMeters, FString& OutError);
	/**
	 * Volume that tolerates small cracks, T-junctions or welding gaps (e.g. slicer caps that do not
	 * exactly meet the bark). Divergence-theorem sum per connected piece; the error is proportional to
	 * the area of the gaps, so it is negligible for real cut pieces. Fails only for empty/flat geometry.
	 */
	bool MeasureTolerant(const TArray<FProcMeshSection>& Sections, const FTransform& Transform,
		double& OutCubicMeters, FString& OutError);
	/** Exact volume when possible, otherwise the tolerant one. OutExact reports which was used. */
	bool MeasureBest(const TArray<FProcMeshSection>& Sections, const FTransform& Transform,
		double& OutCubicMeters, FString& OutError, bool& OutExact);
}
