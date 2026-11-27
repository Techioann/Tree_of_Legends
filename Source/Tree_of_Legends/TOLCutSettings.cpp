#include "TOLCutSettings.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/IAssetRegistry.h"

UTOLCutSettings::UTOLCutSettings()
{
	AxeTable=TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Axe.DT_Axe")));
	TreeTable=TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Tree.DT_Tree")));
	LeafMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	LeafMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/MI_Leaves.MI_Leaves")));
}

UDataTable* UTOLCutSettings::ResolveTable(const TSoftObjectPtr<UDataTable>& Path)
{
	if (Path.IsNull()) return nullptr;
	if (UDataTable* Table=Path.LoadSynchronous()) return Table;

	const FName AssetName(*Path.GetAssetName());
	TArray<FAssetData> Tables;
	if (IAssetRegistry* Registry=IAssetRegistry::Get())
		Registry->GetAssetsByClass(UDataTable::StaticClass()->GetClassPathName(),Tables);
	Tables.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
	for (const FAssetData& Asset : Tables)
	{
		if (Asset.AssetName!=AssetName || Asset.PackageName.ToString().StartsWith(TEXT("/Game/_Archive/"))) continue;
		if (UDataTable* Table=Cast<UDataTable>(Asset.GetAsset()))
		{
			UE_LOG(LogTemp, Warning, TEXT("TOL: %s not found, using %s. Update Project Settings > Game > Tree of Legends Cutting."),
				*Path.ToString(),*Asset.GetSoftObjectPath().ToString());
			return Table;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("TOL: data table %s not found. Set it in Project Settings > Game > Tree of Legends Cutting."),
		*Path.ToString());
	return nullptr;
}
