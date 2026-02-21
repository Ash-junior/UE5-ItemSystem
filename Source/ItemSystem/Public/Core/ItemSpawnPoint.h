#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Core/ItemSpawnPointRoutingTypes.h"
#include "ItemSpawnPoint.generated.h"

class UInventoryComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USceneComponent;
class USphereComponent;
class UBillboardComponent;
class UTextRenderComponent;
class UItemDefinition;
class UItemDistributionPolicy;
class AItemSpawnPoint;
struct FHitResult;

UENUM(BlueprintType)
enum class EItemSpawnConstraintMode : uint8
{
    PreferFilteredThenFallback UMETA(DisplayName = "Prefer Filtered, Fallback"),
    StrictFilteredOnly UMETA(DisplayName = "Strict Filtered Only")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpawnPointItemChanged, AItemSpawnPoint*, SpawnPoint, UItemDefinition*, NewItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSpawnPointItemGranted, AItemSpawnPoint*, SpawnPoint, AActor*, RecipientActor, UItemDefinition*, GrantedItem);

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

    UFUNCTION(BlueprintPure, Category = "Item Spawn|Pickup Routing")
    FItemSpawnPointPickupRoutingSettings GetPickupRoutingSettings() const { return PickupRoutingSettings; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item Spawn|Pickup Routing")
    void ApplyPickupRoutingSettings(const FItemSpawnPointPickupRoutingSettings& NewSettings);

    UPROPERTY(BlueprintAssignable, Category = "Item Spawn|Events")
    FOnSpawnPointItemChanged OnAssignedItemChanged;

    UPROPERTY(BlueprintAssignable, Category = "Item Spawn|Events")
    FOnSpawnPointItemGranted OnItemGranted;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION()
    void OnRep_AssignedItem();

    UFUNCTION()
    void HandlePickupTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    AActor* ResolveRecipientActor(AActor* OverlapActor) const;
    AActor* ResolveDesignatedRecipient(AActor* OverlapActor) const;
    AActor* FindTaggedRecipientInWorld(AActor* OverlapActor) const;
    int32 ResolvePickupGrantAmount() const;
    bool DoesActorMatchRecipientRelation(AActor* OverlapActor, AActor* CandidateActor) const;
    bool ActorHasGameplayTagForRouting(AActor* Actor, const FGameplayTag& Tag) const;
    UInventoryComponent* FindRecipientInventory(AActor* CandidateActor) const;

    void RefreshVisual();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Spawn")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Spawn")
    TObjectPtr<UStaticMeshComponent> ItemPreviewMesh = nullptr;

    // Trigger used to detect pickup on overlap.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Spawn")
    TObjectPtr<USphereComponent> PickupTrigger = nullptr;

#if WITH_EDITORONLY_DATA
    // Editor-only visual helpers to make spawn points easy to identify/select in the level.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Spawn|Editor")
    TObjectPtr<UBillboardComponent> EditorBillboard = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Spawn|Editor")
    TObjectPtr<UTextRenderComponent> EditorLabel = nullptr;
#endif

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

    // Runtime settings propagated from the authoritative GameState manager.
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Item Spawn|Pickup Routing")
    FItemSpawnPointPickupRoutingSettings PickupRoutingSettings;

    // Local optional override. If enabled, this value takes priority over item default and manager routing config.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Pickup Routing|Local Override")
    bool bUseLocalGrantAmountOverride = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Pickup Routing|Local Override",
        meta = (ClampMin = "1", EditCondition = "bUseLocalGrantAmountOverride", EditConditionHides))
    int32 LocalGrantAmountOverride = 1;

    UPROPERTY(ReplicatedUsing = OnRep_AssignedItem, BlueprintReadOnly, Category = "Item Spawn")
    TObjectPtr<UItemDefinition> AssignedItem = nullptr;
};
