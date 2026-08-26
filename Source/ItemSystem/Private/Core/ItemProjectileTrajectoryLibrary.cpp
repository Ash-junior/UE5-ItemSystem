#include "Core/ItemProjectileTrajectoryLibrary.h"

#include "Core/ItemInterface.h"
#include "Data/ItemDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Strategies/Implementation/Executions/Execution_Projectile.h"
#include "Engine/HitResult.h"

namespace
{
bool ResolveItemSpawnLocation(AActor* Instigator, const UItemDefinition* ItemDefinition, FVector& OutStartLocation)
{
    if (!Instigator)
    {
        return false;
    }

    OutStartLocation = Instigator->GetActorLocation();

    if (!ItemDefinition || !ItemDefinition->AttachSocketTag.IsValid() || !Instigator->Implements<UItemInterface>())
    {
        return true;
    }

    FName SocketName;
    USceneComponent* ParentComp = IItemInterface::Execute_GetSocketByTag(Instigator, ItemDefinition->AttachSocketTag, SocketName);
    if (!ParentComp)
    {
        return true;
    }

    if (SocketName != NAME_None && ParentComp->DoesSocketExist(SocketName))
    {
        OutStartLocation = ParentComp->GetSocketLocation(SocketName);
    }
    else
    {
        OutStartLocation = ParentComp->GetComponentLocation();
    }

    return true;
}

bool ResolveViewPoint(AActor* Instigator, AController* InstigatorController, FVector& OutViewLocation, FRotator& OutViewRotation)
{
    if (APlayerController* PC = Cast<APlayerController>(InstigatorController))
    {
        PC->GetPlayerViewPoint(OutViewLocation, OutViewRotation);
        return true;
    }

    if (Instigator)
    {
        Instigator->GetActorEyesViewPoint(OutViewLocation, OutViewRotation);
        return true;
    }

    return false;
}
}

bool UItemProjectileTrajectoryLibrary::BuildArcParamsForItem(
    const UObject* WorldContextObject,
    AActor* Instigator,
    AController* InstigatorController,
    UItemDefinition* ItemDefinition,
    FItemProjectileArcParams& OutParams)
{
    OutParams = FItemProjectileArcParams();

    if (!WorldContextObject || !Instigator || !ItemDefinition)
    {
        return false;
    }

    if (!ResolveItemSpawnLocation(Instigator, ItemDefinition, OutParams.StartLocation))
    {
        return false;
    }

    if (!ResolveViewPoint(Instigator, InstigatorController, OutParams.ViewLocation, OutParams.ViewRotation))
    {
        return false;
    }

    if (ItemDefinition->ExecutionClass.IsNull())
    {
        return false;
    }

    UClass* ExecutionClass = ItemDefinition->ExecutionClass.LoadSynchronous();
    const AExecution_Projectile* ProjectileCDO = ExecutionClass ? Cast<AExecution_Projectile>(ExecutionClass->GetDefaultObject()) : nullptr;
    if (!ProjectileCDO || ProjectileCDO->GetLaunchMode() == EItemLaunchMode::Drop)
    {
        return false;
    }

    OutParams.Speed = ProjectileCDO->GetLaunchSpeed();
    OutParams.GravityScale = ProjectileCDO->GetGravityScale();
    OutParams.TraceDistance = ProjectileCDO->GetArcTraceDistance();
    OutParams.bFavorHighArc = ProjectileCDO->ShouldFavorHighArc();
    OutParams.ProjectileRadius = ProjectileCDO->GetProjectileRadius();

    OutParams.AimPoint = OutParams.ViewLocation + OutParams.ViewRotation.Vector() * OutParams.TraceDistance;
    OutParams.ActorsToIgnore.AddUnique(Instigator);

    return true;
}

bool UItemProjectileTrajectoryLibrary::ComputeProjectileArc(
    const UObject* WorldContextObject,
    const FItemProjectileArcParams& Params,
    FItemProjectileArcResult& OutResult)
{
    OutResult = FItemProjectileArcResult();
    OutResult.AimPoint = Params.AimPoint;

    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World)
    {
        return false;
    }

    const float EffectiveGravityZ = World->GetGravityZ() * Params.GravityScale;
    FVector ResolvedAimPoint = Params.AimPoint;

    FHitResult AimHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ItemProjectileArcAimTrace), false);
    QueryParams.AddIgnoredActors(Params.ActorsToIgnore);

    if (World->LineTraceSingleByChannel(
        AimHit,
        Params.ViewLocation,
        Params.AimPoint,
        Params.TraceChannel,
        QueryParams))
    {
        ResolvedAimPoint = AimHit.ImpactPoint;
    }

    OutResult.AimPoint = ResolvedAimPoint;

    UGameplayStatics::FSuggestProjectileVelocityParameters SuggestParams(
        WorldContextObject,
        Params.StartLocation,
        ResolvedAimPoint,
        Params.Speed);
    SuggestParams.bFavorHighArc = Params.bFavorHighArc;
    SuggestParams.OverrideGravityZ = EffectiveGravityZ;
    SuggestParams.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;

    FVector LaunchVelocity;
    const bool bSolved = UGameplayStatics::SuggestProjectileVelocity(SuggestParams, LaunchVelocity);
    if (!bSolved)
    {
        FVector DirectDirection = (ResolvedAimPoint - Params.StartLocation).GetSafeNormal();
        if (DirectDirection.IsNearlyZero())
        {
            DirectDirection = Params.ViewRotation.Vector();
        }

        LaunchVelocity = DirectDirection * Params.Speed;
    }

    OutResult.bHasSolution = bSolved;
    OutResult.LaunchVelocity = LaunchVelocity;

    FPredictProjectilePathParams PredictParams;
    PredictParams.StartLocation = Params.StartLocation;
    PredictParams.LaunchVelocity = LaunchVelocity;
    PredictParams.ProjectileRadius = Params.ProjectileRadius;
    PredictParams.MaxSimTime = Params.MaxSimTime;
    PredictParams.SimFrequency = Params.SimFrequency;
    PredictParams.bTraceWithCollision = Params.bTraceWithCollision;
    PredictParams.TraceChannel = Params.TraceChannel;
    PredictParams.ActorsToIgnore.Reset();
    for (AActor* IgnoredActor : Params.ActorsToIgnore)
    {
        PredictParams.ActorsToIgnore.Add(IgnoredActor);
    }
    PredictParams.OverrideGravityZ = EffectiveGravityZ;
    PredictParams.DrawDebugType = EDrawDebugTrace::None;

    FPredictProjectilePathResult PredictResult;
    OutResult.bHit = UGameplayStatics::PredictProjectilePath(WorldContextObject, PredictParams, PredictResult);

    OutResult.PathPoints.Reserve(PredictResult.PathData.Num());
    for (const FPredictProjectilePathPointData& PointData : PredictResult.PathData)
    {
        OutResult.PathPoints.Add(PointData.Location);
    }

    if (OutResult.bHit)
    {
        OutResult.HitResult = PredictResult.HitResult;
        OutResult.TracedPosition = PredictResult.HitResult.ImpactPoint;
        OutResult.TracedNormal = PredictResult.HitResult.ImpactNormal;
    }
    else if (OutResult.PathPoints.Num() > 0)
    {
        OutResult.TracedPosition = OutResult.PathPoints.Last();
        OutResult.TracedNormal = FVector::UpVector;
    }

    return true;
}

FItemAimData UItemProjectileTrajectoryLibrary::MakeAimDataFromArc(
    const FItemProjectileArcParams& Params,
    const FItemProjectileArcResult& Result)
{
    FItemAimData AimData;
    AimData.bIsValid = Result.LaunchVelocity.IsNearlyZero() == false;
    AimData.StartLocation = Params.StartLocation;
    AimData.AimPoint = Result.AimPoint;
    AimData.LaunchVelocity = Result.LaunchVelocity;
    AimData.ViewLocation = Params.ViewLocation;
    AimData.ViewRotation = Params.ViewRotation;
    return AimData;
}
