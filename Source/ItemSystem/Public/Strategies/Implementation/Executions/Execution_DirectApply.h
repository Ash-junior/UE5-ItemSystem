#pragma once

#include "CoreMinimal.h"
#include "Data/ItemPayloadRoutingTypes.h"
#include "Strategies/ExecutionStrategy.h"
#include "Execution_DirectApply.generated.h"

/**
 * Direct payload execution for non-projectile/non-trap items (shield, speed boost, etc.).
 * Applies payload immediately to routed recipients, then finishes.
 */
UCLASS(meta = (DisplayName = "Execution: Direct Apply"))
class ITEMSYSTEM_API AExecution_DirectApply : public AItemExecutionStrategy
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void ResetForReuse() override;

private:
    void ResolveRecipients(TArray<AActor*>& OutRecipients) const;
    void AddTargetingRecipientIfValid(TArray<AActor*>& OutRecipients) const;
    void AddSearchRecipients(TArray<AActor*>& OutRecipients) const;
    void AddInstigatorRecipientIfValid(TArray<AActor*>& OutRecipients) const;

    bool DoesActorMatchRoutingTag(AActor* Candidate, const FGameplayTag& RequiredTag) const;
    bool DoesActorMatchRoutingRelation(AActor* Candidate, EItemPayloadRecipientRelation Relation) const;
};
