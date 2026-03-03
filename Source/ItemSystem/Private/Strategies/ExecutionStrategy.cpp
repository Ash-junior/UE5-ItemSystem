#include "Strategies/ExecutionStrategy.h"
#include "Strategies/ItemTargetingStrategy.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "Core/ItemInterface.h"
#include "Core/ItemSystemManager.h"
#include "Core/ItemSystemLog.h"
#include "Core/TargetableInterface.h"
#include "Data/ItemDefinition.h"

#include "Net/UnrealNetwork.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

AItemExecutionStrategy::AItemExecutionStrategy()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Enable Replication for multiplayer support
    bReplicates = true;
    
    // Note: We do NOT enable bReplicateMovement here by default,
    // because not all strategies move (e.g. an instant laser or AoE trap).
    // Subclasses like Execution_Projectile must enable it.

    // Create a root component so the actor has a transform
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

void AItemExecutionStrategy::ResetForReuse()
{
    // Replay spawn sound and trail VFX so pooled actors behave identically to
    // freshly spawned ones. Derived classes should call Super::ResetForReuse()
    // at the END of their override, after resetting all state.
    PlaySpawnEffects();
}

void AItemExecutionStrategy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replicate the Context to all clients (needed for VFX or UI feedback on clients)
    DOREPLIFETIME(AItemExecutionStrategy, ItemContext);
}

void AItemExecutionStrategy::BeginPlay()
{
    Super::BeginPlay();
    
    // Server-Side Only Logic: Create Strategies
    if (HasAuthority())
    {
        // 1. Validate Context
        if (!ItemContext.ItemDefinition)
        {
            UE_LOG(LogTemp, Error, TEXT("ExecutionStrategy: Missing Item Definition in Context! Destroying."));
            Destroy();
            return;
        }

        const UItemDefinition* Def = ItemContext.ItemDefinition;

        // 2. Instantiate Targeting Strategy (Logic)
        if (!Def->TargetingClass.IsNull())
        {
            // Load the class synchronously (if not already loaded)
            UClass* TargetingClass = Def->TargetingClass.LoadSynchronous();
            if (TargetingClass)
            {
                TargetingInstance = NewObject<UItemTargetingStrategy>(this, TargetingClass);
            }
        }

        // 3. Instantiate Payload Strategy (Effect)
        if (!Def->PayloadClass.IsNull())
        {
            UClass* PayloadClass = Def->PayloadClass.LoadSynchronous();
            if (PayloadClass)
            {
                PayloadInstance = NewObject<UItemPayloadStrategy>(this, PayloadClass);
            }
        }
    }    

    // 4. Spawn Audio & VFX
    PlaySpawnEffects();
    
    // 5. Ignore Instigator Collision (Optional but recommended)
    // Needs a PrimitiveComponent (Mesh/Sphere) to work, usually added in Blueprint children.
    /*
    if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(RootComponent))
    {
        if (ItemContext.Instigator)
        {
            Primitive->IgnoreActorWhenMoving(ItemContext.Instigator, true);
        }
    }
    */
}

void AItemExecutionStrategy::PlaySpawnEffects()
{
    if (SpawnSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, SpawnSound, GetActorLocation());
    }

    if (TrailVFX)
    {
        // Deactivate the previous trail component so it doesn't linger after reuse.
        if (ActiveTrailVFX && !ActiveTrailVFX->IsPendingKillOrUnreachable())
        {
            ActiveTrailVFX->DeactivateImmediate();
        }

        ActiveTrailVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
            TrailVFX,
            RootComponent,
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true
        );
    }
}

void AItemExecutionStrategy::FinishExecution()
{
    // Hook for cleanup (e.g. death VFX)
    // Only the server should decide to destroy the actor
    if (HasAuthority())
    {
        UItemSystemManager* Manager = UItemSystemManager::Get(this);
        if (Manager)
        {
            Manager->ReleaseExecutionActor(this);
            if (IsItemSystemQAEnabled())
            {
                UE_LOG(LogItemSystem, Log, TEXT("QA: Released execution actor %s to pool"), *GetName());
            }
        }
        else
        {
            Destroy();
        }
    }
}

void AItemExecutionStrategy::PlayImpactFX(const FVector& Location)
{
    if (HasAuthority())
    {
        Multicast_PlayImpactFX(Location);
    }
    else
    {
        SpawnImpactFX(Location);
    }
}

void AItemExecutionStrategy::Multicast_PlayImpactFX_Implementation(const FVector& Location)
{
    SpawnImpactFX(Location);
}

void AItemExecutionStrategy::SpawnImpactFX(const FVector& Location)
{
    if (ImpactSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Location);
    }

    if (ImpactVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, Location);
    }
}

bool AItemExecutionStrategy::ShouldAffectActor(AActor* OtherActor) const
{
    if (!OtherActor)
    {
        return false;
    }

    // Team filter (if item says to ignore teammates)
    if (ItemContext.ItemDefinition && ItemContext.ItemDefinition->IdentityTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Rule.Ignore.Teammates"), false)))
    {
        const AActor* InstigatorActor = ItemContext.Instigator;
        if (InstigatorActor && InstigatorActor->Implements<UItemInterface>() && OtherActor->Implements<UItemInterface>())
        {
            const int32 InstigatorTeam = IItemInterface::Execute_GetTeamID(InstigatorActor);
            const int32 TargetTeam = IItemInterface::Execute_GetTeamID(OtherActor);
            if (InstigatorTeam == TargetTeam)
            {
                if (IsItemSystemQAEnabled())
                {
                    UE_LOG(LogItemSystem, Log, TEXT("QA: Team filter blocked actor %s"), *OtherActor->GetName());
                }
                return false;
            }
        }
    }

    // Immunity filter on target
    if (OtherActor->Implements<UTargetableInterface>())
    {
        if (ITargetableInterface::Execute_IsImmuneTo(OtherActor, ItemContext.ContextTags))
        {
            if (IsItemSystemQAEnabled())
            {
                UE_LOG(LogItemSystem, Log, TEXT("QA: Immunity blocked actor %s"), *OtherActor->GetName());
            }
            return false;
        }
    }

    return true;
}

// Note: The Tick function is available for child classes (Projectiles)
// to use TargetingInstance->FindTarget() and adjust trajectory.

