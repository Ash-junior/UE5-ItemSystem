#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ItemSpawnPoint.generated.h"

class UInventoryComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USceneComponent;
class USphereComponent;
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

UENUM(BlueprintType)
enum class EItemSpawnRecipientPolicy : uint8
{
    OverlappingActorOnly UMETA(DisplayName = "Overlapping Actor Only"),
    OverlapActorIfHasTagElseDesignated UMETA(DisplayName = "Overlap If Tagged, Else Designated"),
    DesignatedActorOnly UMETA(DisplayName = "Designated Actor Only")
};

UENUM(BlueprintType)
enum class EItemSpawnRecipientRelation : uint8
{
    Any UMETA(DisplayName = "Any"),
    SameTeamAsOverlappingActor UMETA(DisplayName = "Same Team As Overlap"),
    EnemyOfOverlappingActor UMETA(DisplayName = "Enemy Of Overlap")
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

    UPROPERTY(BlueprintAssignable, Category = "Item Spawn|Events")
    FOnSpawnPointItemChanged OnAssignedItemChanged;

    UPROPERTY(BlueprintAssignable, Category = "Item Spawn|Events")
    FOnSpawnPointItemGranted OnItemGranted;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Pickup", meta = (ClampMin = "1"))
    int32 GrantAmount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Pickup")
    bool bConsumeOnSuccessfulGrant = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Pickup", meta = (EditCondition = "bConsumeOnSuccessfulGrant", EditConditionHides))
    bool bRequestImmediateRespawnOnConsume = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Pickup")
    EItemSpawnRecipientPolicy RecipientPolicy = EItemSpawnRecipientPolicy::OverlappingActorOnly;

    // For OverlapActorIfHasTagElseDesignated:
    // if the overlapping actor has this tag, it receives the item.
    // If not set, overlapping actor is considered valid by default.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Recipient Rules",
        meta = (EditCondition = "RecipientPolicy == EItemSpawnRecipientPolicy::OverlapActorIfHasTagElseDesignated", EditConditionHides))
    FGameplayTag OverlapReceivesItemTag;

    // Optional explicit designated recipient.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Recipient Rules",
        meta = (EditCondition = "RecipientPolicy != EItemSpawnRecipientPolicy::OverlappingActorOnly", EditConditionHides))
    TObjectPtr<AActor> DesignatedRecipientActor = nullptr;

    // Optional tag used to search designated recipients in world.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Recipient Rules",
        meta = (EditCondition = "RecipientPolicy != EItemSpawnRecipientPolicy::OverlappingActorOnly", EditConditionHides))
    FGameplayTag DesignatedRecipientTag;

    // Team relation filter applied when selecting designated recipients.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Recipient Rules",
        meta = (EditCondition = "RecipientPolicy != EItemSpawnRecipientPolicy::OverlappingActorOnly", EditConditionHides))
    EItemSpawnRecipientRelation DesignatedRecipientRelation = EItemSpawnRecipientRelation::Any;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawn|Recipient Rules",
        meta = (EditCondition = "RecipientPolicy != EItemSpawnRecipientPolicy::OverlappingActorOnly", EditConditionHides))
    bool bFallbackToOverlapIfDesignatedNotFound = true;

    UPROPERTY(ReplicatedUsing = OnRep_AssignedItem, BlueprintReadOnly, Category = "Item Spawn")
    TObjectPtr<UItemDefinition> AssignedItem = nullptr;
};
