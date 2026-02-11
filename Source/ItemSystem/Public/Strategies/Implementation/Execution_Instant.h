#pragma once

#include "CoreMinimal.h"
#include "Strategies/ExecutionStrategy.h"
#include "Execution_Instant.generated.h"

/**
 * Immediate execution. Finds target and applies payload in the same frame as spawn.
 * Useful for hitscan weapons, self-buffs, or immediate area effects.
 */
UCLASS()
class ITEMSYSTEM_API AExecution_Instant : public AItemExecutionStrategy
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void ResetForReuse() override;
};
