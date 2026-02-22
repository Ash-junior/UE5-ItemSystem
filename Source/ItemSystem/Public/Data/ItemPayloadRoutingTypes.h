#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ItemPayloadRoutingTypes.generated.h"

UENUM(BlueprintType)
enum class EItemPayloadRoutingPolicy : uint8
{
    TargetingResultOnly UMETA(DisplayName = "Targeting Result Only"),
    InstigatorOnly UMETA(DisplayName = "Instigator Only"),
    SearchByRules UMETA(DisplayName = "Search By Rules")
};

UENUM(BlueprintType)
enum class EItemPayloadRecipientRelation : uint8
{
    Any UMETA(DisplayName = "Any"),
    SameTeamAsInstigator UMETA(DisplayName = "Same Team As Instigator"),
    EnemyOfInstigator UMETA(DisplayName = "Enemy Of Instigator")
};

UENUM(BlueprintType)
enum class EItemPayloadSearchSelection : uint8
{
    NearestSingle UMETA(DisplayName = "Nearest Single"),
    AllMatching UMETA(DisplayName = "All Matching")
};

USTRUCT(BlueprintType)
struct ITEMSYSTEM_API FItemPayloadRoutingSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload Routing")
    EItemPayloadRoutingPolicy RoutingPolicy = EItemPayloadRoutingPolicy::TargetingResultOnly;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload Routing|Search",
        meta = (EditCondition = "RoutingPolicy == EItemPayloadRoutingPolicy::SearchByRules", EditConditionHides))
    FGameplayTag RequiredRecipientTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload Routing|Search",
        meta = (EditCondition = "RoutingPolicy == EItemPayloadRoutingPolicy::SearchByRules", EditConditionHides))
    EItemPayloadRecipientRelation RecipientRelation = EItemPayloadRecipientRelation::Any;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload Routing|Search",
        meta = (EditCondition = "RoutingPolicy == EItemPayloadRoutingPolicy::SearchByRules", EditConditionHides))
    EItemPayloadSearchSelection SearchSelection = EItemPayloadSearchSelection::NearestSingle;

    // 0 means no distance limit.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload Routing|Search",
        meta = (ClampMin = "0.0", EditCondition = "RoutingPolicy == EItemPayloadRoutingPolicy::SearchByRules", EditConditionHides))
    float SearchRadius = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload Routing|Fallbacks",
        meta = (EditCondition = "RoutingPolicy == EItemPayloadRoutingPolicy::SearchByRules", EditConditionHides))
    bool bFallbackToTargetingResultIfNoSearchMatch = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload Routing|Fallbacks")
    bool bFallbackToInstigatorIfNoRecipient = false;
};
