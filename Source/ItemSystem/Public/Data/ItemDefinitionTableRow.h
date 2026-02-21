#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemDefinitionTableRow.generated.h"

class UItemDefinition;

USTRUCT(BlueprintType)
struct ITEMSYSTEM_API FItemDefinitionTableRow : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    TSoftObjectPtr<UItemDefinition> ItemDefinition;
};
