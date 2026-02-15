#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ItemSpawnPoint.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UItemDefinition;
class UItemDistributionPolicy;
class AItemSpawnPoint;

UENUM(BlueprintType)
enum class EItemSpawnConstraintMode : uint8
{
    PreferFilteredThenFallback UMETA(DisplayName = "Prefer Filtered, Fallback"),
    StrictFilteredOnly UMETA(DisplayName = "Strict Filtered Only")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpawnPointItemChanged, AItemSpawnPoint*, SpawnPoint, UItemDefinition*, NewItem);

/**
 * Actor placed in the level to host world-spawned items.
 * The manager assigns item definitions to this actor.
 */
UCLASS(Blueprintable)
class ITEMSYSTEM_API AItemSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    AItemSpawnPoint();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Item Spawn")
    UItemDefinition* GetAssignedItem() const { return AssignedItem; }

    UFUNCTION(BlueprintPure, Category = "Item Spawn")
    bool HasAssignedItem() const { return AssignedItem != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Item Spawn")
    bool HasAdditionalFilter() const { return !AdditionalItemFilter.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "Item Spawn")
    EItemSpawnConstraintMode GetConstraintMode() const { return ConstraintMode; }

    UFUNCTION(BlueprintPure, Category = "Item Spawn")
    UItemDistributionPolicy* GetDistributionPolicyOverride() const { return DistributionPolicyOverride; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item Spawn")
    void SetAssignedItem(UItemDefinition* NewItem);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item Spawn")
    void ConsumeAssignedItem(bool bRequestImmediateRespawn = true);

    UFUNCTION(BlueprintPure, Category = "Item Spawn")
    bool MatchesAdditionalFilter(const UItemDefinition* Item) const;

    UPROPERTY(BlueprintAssignable, Category = "Item Spawn|Events")
    FOnSpawnPointItemChanged OnAssignedItemChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnRep_AssignedItem();

    void RefreshVisual();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Spawn")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Spawn")
    TObjectPtr<UStaticMeshComponent> ItemPreviewMesh = nullptr;

    // Additional per-spawn filter applied by the manager before assignment.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Rules")
    FGameplayTagQuery AdditionalItemFilter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Rules")
    EItemSpawnConstraintMode ConstraintMode = EItemSpawnConstraintMode::PreferFilteredThenFallback;

    // Optional override policy used for this specific spawn point.
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Item Spawn|Rules")
    TObjectPtr<UItemDistributionPolicy> DistributionPolicyOverride = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Visuals")
    bool bHidePreviewWhenNoItem = true;

    UPROPERTY(ReplicatedUsing = OnRep_AssignedItem, BlueprintReadOnly, Category = "Item Spawn")
    TObjectPtr<UItemDefinition> AssignedItem = nullptr;
};
