#include "Strategies/Implementation/Executions/Execution_DirectApply.h"

#include "Core/ItemInterface.h"
#include "Core/ItemSystemLog.h"
#include "Data/ItemDefinition.h"
#include "EngineUtils.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "Strategies/ItemTargetingStrategy.h"

void AExecution_DirectApply::BeginPlay()
{
    Super::BeginPlay(); // Creates TargetingInstance and PayloadInstance
    Execute();
}

void AExecution_DirectApply::ResetForReuse()
{
    // PayloadInstance and TargetingInstance from BeginPlay are still valid
    // (pool groups actors by class, so the same payload type is always reused).
    Execute();
}

void AExecution_DirectApply::Execute()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!ItemContext.ItemDefinition)
    {
        FinishExecution();
        return;
    }

    if (!PayloadInstance)
    {
        UE_LOG(LogItemSystem, Warning, TEXT("Execution_DirectApply %s has no payload instance."), *GetName());
        FinishExecution();
        return;
    }

    TArray<AActor*> Recipients;
    ResolveRecipients(Recipients);

    int32 AppliedCount = 0;
    FVector ImpactLocation = GetActorLocation();

    for (AActor* Recipient : Recipients)
    {
        if (!Recipient || !ShouldAffectActor(Recipient))
        {
            continue;
        }

        FItemContext RoutedContext = ItemContext;
        RoutedContext.TargetActor = Recipient;
        RoutedContext.bHasImpactPoint = true;
        RoutedContext.ImpactPoint = Recipient->GetActorLocation();

        PayloadInstance->ApplyEffect(Recipient, RoutedContext);
        ImpactLocation = Recipient->GetActorLocation();
        ++AppliedCount;
    }

    if (AppliedCount == 0 && IsItemSystemQAEnabled())
    {
        UE_LOG(LogItemSystem, Log, TEXT("QA: DirectApply found no valid recipient for item %s."),
            *GetNameSafe(ItemContext.ItemDefinition));
    }

    PlayImpactFX(ImpactLocation);
    FinishExecution();
}

void AExecution_DirectApply::ResolveRecipients(TArray<AActor*>& OutRecipients) const
{
    OutRecipients.Reset();

    const UItemDefinition* Def = ItemContext.ItemDefinition;
    if (!Def)
    {
        return;
    }

    const FItemPayloadRoutingSettings& Routing = Def->PayloadRouting;
    switch (Routing.RoutingPolicy)
    {
    case EItemPayloadRoutingPolicy::TargetingResultOnly:
        AddTargetingRecipientIfValid(OutRecipients);
        break;

    case EItemPayloadRoutingPolicy::InstigatorOnly:
        AddInstigatorRecipientIfValid(OutRecipients);
        break;

    case EItemPayloadRoutingPolicy::SearchByRules:
        AddSearchRecipients(OutRecipients);
        if (OutRecipients.Num() == 0 && Routing.bFallbackToTargetingResultIfNoSearchMatch)
        {
            AddTargetingRecipientIfValid(OutRecipients);
        }
        break;

    default:
        AddTargetingRecipientIfValid(OutRecipients);
        break;
    }

    if (OutRecipients.Num() == 0 && Routing.bFallbackToInstigatorIfNoRecipient)
    {
        AddInstigatorRecipientIfValid(OutRecipients);
    }
}

void AExecution_DirectApply::AddTargetingRecipientIfValid(TArray<AActor*>& OutRecipients) const
{
    if (!TargetingInstance)
    {
        return;
    }

    AActor* Target = TargetingInstance->FindTarget(ItemContext, GetActorLocation());
    if (Target)
    {
        OutRecipients.AddUnique(Target);
    }
}

void AExecution_DirectApply::AddSearchRecipients(TArray<AActor*>& OutRecipients) const
{
    const UItemDefinition* Def = ItemContext.ItemDefinition;
    if (!Def)
    {
        return;
    }

    const FItemPayloadRoutingSettings& Routing = Def->PayloadRouting;
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector Origin = GetActorLocation();
    const float MaxDistanceSq = (Routing.SearchRadius > 0.0f)
        ? FMath::Square(Routing.SearchRadius)
        : TNumericLimits<float>::Max();

    AActor* BestActor = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!Candidate || Candidate == this)
        {
            continue;
        }

        const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), Origin);
        if (DistSq > MaxDistanceSq)
        {
            continue;
        }

        if (!DoesActorMatchRoutingTag(Candidate, Routing.RequiredRecipientTag))
        {
            continue;
        }

        if (!DoesActorMatchRoutingRelation(Candidate, Routing.RecipientRelation))
        {
            continue;
        }

        if (!ShouldAffectActor(Candidate))
        {
            continue;
        }

        if (Routing.SearchSelection == EItemPayloadSearchSelection::AllMatching)
        {
            OutRecipients.AddUnique(Candidate);
            continue;
        }

        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestActor = Candidate;
        }
    }

    if (Routing.SearchSelection == EItemPayloadSearchSelection::NearestSingle && BestActor)
    {
        OutRecipients.AddUnique(BestActor);
    }
}

void AExecution_DirectApply::AddInstigatorRecipientIfValid(TArray<AActor*>& OutRecipients) const
{
    if (ItemContext.Instigator)
    {
        OutRecipients.AddUnique(ItemContext.Instigator);
    }
}

bool AExecution_DirectApply::DoesActorMatchRoutingTag(AActor* Candidate, const FGameplayTag& RequiredTag) const
{
    if (!Candidate)
    {
        return false;
    }

    if (!RequiredTag.IsValid())
    {
        return true;
    }

    if (!Candidate->Implements<UItemInterface>())
    {
        return false;
    }

    return IItemInterface::Execute_HasGameplayTag(Candidate, RequiredTag);
}

bool AExecution_DirectApply::DoesActorMatchRoutingRelation(AActor* Candidate, EItemPayloadRecipientRelation Relation) const
{
    if (!Candidate)
    {
        return false;
    }

    if (Relation == EItemPayloadRecipientRelation::Any)
    {
        return true;
    }

    AActor* InstigatorActor = ItemContext.Instigator;
    if (!InstigatorActor || !InstigatorActor->Implements<UItemInterface>() || !Candidate->Implements<UItemInterface>())
    {
        return false;
    }

    const int32 InstigatorTeam = IItemInterface::Execute_GetTeamID(InstigatorActor);
    const int32 CandidateTeam = IItemInterface::Execute_GetTeamID(Candidate);
    if (InstigatorTeam == INDEX_NONE || CandidateTeam == INDEX_NONE)
    {
        return false;
    }

    if (Relation == EItemPayloadRecipientRelation::SameTeamAsInstigator)
    {
        return InstigatorTeam == CandidateTeam;
    }

    if (Relation == EItemPayloadRecipientRelation::EnemyOfInstigator)
    {
        return InstigatorTeam != CandidateTeam;
    }

    return true;
}
