#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ItemSpawnPointRoutingTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EItemSpawnPickupMethod : uint8
{
    TriggerOverlap UMETA(DisplayName = "Trigger Overlap")
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

USTRUCT(BlueprintType)
struct ITEMSYSTEM_API FItemSpawnPointPickupRoutingSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    EItemSpawnPickupMethod PickupMethod = EItemSpawnPickupMethod::TriggerOverlap;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    bool bOverrideItemGrantAmount = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup",
        meta = (ClampMin = "1", EditCondition = "bOverrideItemGrantAmount", EditConditionHides))
    int32 OverrideGrantAmount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    bool bConsumeOnSuccessfulGrant = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (EditCondition = "bConsumeOnSuccessfulGrant", EditConditionHides))
    bool bRequestImmediateRespawnOnConsume = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    EItemSpawnRecipientPolicy RecipientPolicy = EItemSpawnRecipientPolicy::OverlappingActorOnly;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recipient Rules",
        meta = (EditCondition = "RecipientPolicy == EItemSpawnRecipientPolicy::OverlapActorIfHasTagElseDesignated", EditConditionHides))
    FGameplayTag OverlapReceivesItemTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recipient Rules",
        meta = (EditCondition = "RecipientPolicy != EItemSpawnRecipientPolicy::OverlappingActorOnly", EditConditionHides))
    TObjectPtr<AActor> DesignatedRecipientActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recipient Rules",
        meta = (EditCondition = "RecipientPolicy != EItemSpawnRecipientPolicy::OverlappingActorOnly", EditConditionHides))
    FGameplayTag DesignatedRecipientTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recipient Rules",
        meta = (EditCondition = "RecipientPolicy != EItemSpawnRecipientPolicy::OverlappingActorOnly", EditConditionHides))
    EItemSpawnRecipientRelation DesignatedRecipientRelation = EItemSpawnRecipientRelation::Any;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recipient Rules",
        meta = (EditCondition = "RecipientPolicy != EItemSpawnRecipientPolicy::OverlappingActorOnly", EditConditionHides))
    bool bFallbackToOverlapIfDesignatedNotFound = true;
};
