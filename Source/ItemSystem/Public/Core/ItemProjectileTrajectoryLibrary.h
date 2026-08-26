#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/ItemSystemTypes.h"
#include "ItemProjectileTrajectoryLibrary.generated.h"

class UItemDefinition;

/**
 * Shared projectile arc math used by aim preview and by server-side activation.
 */
UCLASS()
class ITEMSYSTEM_API UItemProjectileTrajectoryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Item System|Projectile Arc", meta = (WorldContext = "WorldContextObject"))
    static bool BuildArcParamsForItem(
        const UObject* WorldContextObject,
        AActor* Instigator,
        AController* InstigatorController,
        UItemDefinition* ItemDefinition,
        FItemProjectileArcParams& OutParams);

    UFUNCTION(BlueprintCallable, Category = "Item System|Projectile Arc", meta = (WorldContext = "WorldContextObject"))
    static bool ComputeProjectileArc(
        const UObject* WorldContextObject,
        const FItemProjectileArcParams& Params,
        FItemProjectileArcResult& OutResult);

    UFUNCTION(BlueprintPure, Category = "Item System|Projectile Arc")
    static FItemAimData MakeAimDataFromArc(
        const FItemProjectileArcParams& Params,
        const FItemProjectileArcResult& Result);
};
