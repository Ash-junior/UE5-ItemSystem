#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ItemSystemTypes.h"
#include "ItemAimPreviewComponent.generated.h"

class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAimPreviewUpdated, const FItemProjectileArcResult&, Result, const FItemAimData&, AimData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemAimPreviewStopped);

/**
 * Local-only helper that continuously predicts the current projectile arc while
 * the player is holding an aim input. Blueprints can bind to the result and draw
 * it with a spline, mesh segments, or Niagara vector arrays.
 */
UCLASS(ClassGroup = (ItemSystem), meta = (BlueprintSpawnableComponent))
class ITEMSYSTEM_API UItemAimPreviewComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UItemAimPreviewComponent();

    UPROPERTY(BlueprintAssignable, Category = "Item System|Aim Preview")
    FOnItemAimPreviewUpdated OnAimPreviewUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Item System|Aim Preview")
    FOnItemAimPreviewStopped OnAimPreviewStopped;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item System|Aim Preview")
    bool bTraceWithCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item System|Aim Preview", meta = (ClampMin = "0.1"))
    float MaxSimTime = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item System|Aim Preview", meta = (ClampMin = "1.0"))
    float SimFrequency = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item System|Aim Preview")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item System|Aim Preview")
    bool bDrawDebugPreview = false;

    UFUNCTION(BlueprintCallable, Category = "Item System|Aim Preview")
    void StartAiming(UInventoryComponent* InventoryOverride = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Item System|Aim Preview")
    void StopAiming();

    UFUNCTION(BlueprintCallable, Category = "Item System|Aim Preview")
    bool UpdateAimPreview();

    UFUNCTION(BlueprintPure, Category = "Item System|Aim Preview")
    bool IsAiming() const { return bIsAiming; }

    UFUNCTION(BlueprintPure, Category = "Item System|Aim Preview")
    FItemAimData GetLastAimData() const { return LastAimData; }

    UFUNCTION(BlueprintPure, Category = "Item System|Aim Preview")
    FItemProjectileArcResult GetLastArcResult() const { return LastArcResult; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY()
    UInventoryComponent* InventoryComponent = nullptr;

    UPROPERTY()
    bool bIsAiming = false;

    UPROPERTY()
    FItemAimData LastAimData;

    UPROPERTY()
    FItemProjectileArcResult LastArcResult;

    UInventoryComponent* ResolveInventoryComponent(UInventoryComponent* InventoryOverride) const;
    AController* ResolveInstigatorController() const;
    void DrawDebugArc() const;
};
