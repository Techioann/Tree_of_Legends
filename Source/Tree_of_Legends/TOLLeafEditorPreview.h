#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "TOLCutMeshData.h"
#include "TOLLeafEditorPreview.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * Shows tree leaves in the level editor viewport (editor worlds only, never in PIE or a packaged game).
 * Polls twice a second, so it follows foliage painting, placing/moving trees and Leaves settings changes.
 * Preview leaves are transient and are never saved into the level.
 */
UCLASS()
class TREE_OF_LEGENDS_API UTOLLeafEditorPreview : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Refresh();
	void ClearAll();
	const FTOLTreeMeshConfiguration* FindModel(UStaticMesh* Mesh);

	struct FEntry
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Leaves;
		uint32 Signature = 0;
	};
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FEntry> Entries;
	TMap<TObjectKey<UStaticMesh>, TOptional<FTOLTreeMeshConfiguration>> Models;
	TMap<TObjectKey<UStaticMesh>, uint32> SocketHashes;
	uint32 SettingsSignature = 0;
	double SinceRefresh = 1.0;
	int32 LoggedCount = -1;
};
