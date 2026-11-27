#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TOLCutSettings.generated.h"

class UDataTable;
class UStaticMesh;
class UMaterialInterface;

/** Project Settings > Game > Tree of Legends Cutting.
 * Tables a CutTrace component uses when its own AxeData / DefaultTreeData / TreeTable are left empty.
 * Replaces the old hard-coded ConstructorHelpers paths, which silently broke chopping whenever Content moved. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Tree of Legends Cutting"))
class TREE_OF_LEGENDS_API UTOLCutSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UTOLCutSettings();

	/** DT_Axe (axe stats: chopdamage, requirelevel). */
	UPROPERTY(Config, EditAnywhere, Category="Defaults")
	TSoftObjectPtr<UDataTable> AxeTable;

	/** DT_Tree (row type FTOLTreeRow). */
	UPROPERTY(Config, EditAnywhere, Category="Defaults")
	TSoftObjectPtr<UDataTable> TreeTable;

	/** Visual-only leaf balls on branch tips (no collision, no mass, ignored by axe and grab traces). */
	UPROPERTY(Config, EditAnywhere, Category="Leaves")
	bool bEnableLeaves = true;

	UPROPERTY(Config, EditAnywhere, Category="Leaves", meta=(EditCondition="bEnableLeaves"))
	TSoftObjectPtr<UStaticMesh> LeafMesh;

	UPROPERTY(Config, EditAnywhere, Category="Leaves", meta=(EditCondition="bEnableLeaves"))
	TSoftObjectPtr<UMaterialInterface> LeafMaterial;

	/** Leaf ball radius as a fraction of the tree mesh height. A "Leaf*" socket's scale multiplies it. */
	UPROPERTY(Config, EditAnywhere, Category="Leaves", meta=(EditCondition="bEnableLeaves", ClampMin="0.01", ClampMax="1"))
	float LeafRadiusFraction = 0.1f;

	/** Static Mesh sockets with this prefix place leaves by hand; without any, leaves go on detected branch tips. */
	UPROPERTY(Config, EditAnywhere, Category="Leaves", meta=(EditCondition="bEnableLeaves"))
	FString LeafSocketPrefix = TEXT("Leaf");

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Loads the configured table. If it was moved without a redirector, falls back to the first
	 * DataTable with the same asset name outside /Game/_Archive (with a warning). Null + error otherwise. */
	static UDataTable* ResolveTable(const TSoftObjectPtr<UDataTable>& Path);
};
