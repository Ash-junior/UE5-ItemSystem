#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/ItemSpawnPointRoutingTypes.h"
#include "ItemSpawnPointRoutingConfig.generated.h"

UCLASS(BlueprintType)
class ITEMSYSTEM_API UItemSpawnPointRoutingConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpawnPoint Pickup + Recipient Routing")
    FItemSpawnPointPickupRoutingSettings Settings;
};
