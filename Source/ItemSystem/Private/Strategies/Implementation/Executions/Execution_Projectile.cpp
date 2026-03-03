#include "Strategies/Implementation/Executions/Execution_Projectile.h"

#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "Core/ItemSystemLog.h"

AExecution_Projectile::AExecution_Projectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(15.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComponent->SetCanEverAffectNavigation(false);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->OnComponentHit.AddDynamic(this, &AExecution_Projectile::OnProjectileHit);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AExecution_Projectile::OnProjectileOverlap);

	RootComponent = CollisionComponent;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;

	SetReplicateMovement(true);
}

// ---------------------------------------------------------------------------

void AExecution_Projectile::BeginPlay()
{
	// Apply base movement settings before the component initialises.
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
		ProjectileMovement->ProjectileGravityScale = GravityScale;
	}

	Super::BeginPlay(); // Creates PayloadInstance, plays spawn sound/VFX.

	SetLifeSpan(10.0f);

	if (CollisionComponent && ItemContext.Instigator)
	{
		CollisionComponent->MoveIgnoreActors.Add(ItemContext.Instigator);
	}

	ApplyLaunchMode();
}

void AExecution_Projectile::ResetForReuse()
{
	bHasExploded = false;

	SetLifeSpan(10.0f);

	if (ProjectileMovement)
	{
		// UProjectileMovementComponent::StopSimulating() (called internally on blocking hits
		// when bShouldBounce=false) sets UpdatedComponent=nullptr, disconnecting the component
		// from the sphere. Without restoring this link, the component activates but has
		// nothing to move and the projectile stays frozen at the socket.
		ProjectileMovement->SetUpdatedComponent(CollisionComponent);

		// Zero velocity directly rather than StopMovementImmediately(), which calls
		// SetActive(false) and would prevent the velocity set in ApplyLaunchMode from being processed.
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->UpdateComponentVelocity();
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
		ProjectileMovement->ProjectileGravityScale = GravityScale;
		ProjectileMovement->SetActive(true);
	}

	if (CollisionComponent)
	{
		CollisionComponent->MoveIgnoreActors.Reset();
		if (ItemContext.Instigator)
		{
			CollisionComponent->MoveIgnoreActors.Add(ItemContext.Instigator);
		}
	}

	ApplyLaunchMode();

	// Replay spawn VFX/sound inherited from the base class.
	Super::ResetForReuse();
}

// ---------------------------------------------------------------------------
// Launch modes

void AExecution_Projectile::ApplyLaunchMode()
{
	switch (LaunchMode)
	{
	case EItemLaunchMode::ArcThrow:        ApplyArcThrow();        break;
	case EItemLaunchMode::Drop:            ApplyDrop();            break;
	case EItemLaunchMode::ExternalVelocity: ApplyExternalVelocity(); break;
	}
}

void AExecution_Projectile::ApplyArcThrow()
{
	if (!ProjectileMovement)
	{
		return;
	}

	// Resolve the aim point from the instigator's camera.
	APlayerController* PC = Cast<APlayerController>(ItemContext.InstigatorController);
	if (!PC)
	{
		// No controller available — shoot forward at full speed.
		ProjectileMovement->Velocity = GetActorForwardVector() * Speed;
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector TraceEnd = ViewLoc + ViewRot.Vector() * ArcTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params;
	if (ItemContext.Instigator)
	{
		Params.AddIgnoredActor(ItemContext.Instigator);
	}
	// Ignore the projectile itself: its sphere is at SpawnLoc and would return
	// ImpactPoint ≈ SpawnLoc, collapsing the aim direction to a zero vector.
	Params.AddIgnoredActor(this);

	FVector AimPoint = TraceEnd;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, TraceEnd, ECC_Visibility, Params))
	{
		AimPoint = Hit.ImpactPoint;
	}

	// Compute ballistic velocity — account for this projectile's gravity scale.
	const FVector SpawnLoc = GetActorLocation();
	const float EffectiveGravityZ = GetWorld()->GetGravityZ() * GravityScale;

	UGameplayStatics::FSuggestProjectileVelocityParameters SuggestParams(this, SpawnLoc, AimPoint, Speed);
	SuggestParams.bFavorHighArc    = bFavorHighArc;
	SuggestParams.OverrideGravityZ = EffectiveGravityZ;
	SuggestParams.TraceOption      = ESuggestProjVelocityTraceOption::DoNotTrace;

	FVector SuggestedVelocity;
	const bool bSuccess = UGameplayStatics::SuggestProjectileVelocity(SuggestParams, SuggestedVelocity);

	if (bSuccess)
	{
		ProjectileMovement->Velocity = SuggestedVelocity;
	}
	else
	{
		// Target unreachable at this speed — launch toward the aim point at full speed
		// and let gravity produce a natural arc (projectile will fall short).
		FVector Dir = (AimPoint - SpawnLoc).GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			// Degenerate aim point (e.g. character pressed against a surface):
			// fall back to the actor's forward vector so the projectile always launches.
			Dir = GetActorForwardVector();
		}
		ProjectileMovement->Velocity = Dir * Speed;

		if (IsItemSystemQAEnabled())
		{
			UE_LOG(LogItemSystem, Log, TEXT("QA: ArcThrow could not solve ballistic path to aim point — using direct aim."));
		}
	}
}

void AExecution_Projectile::ApplyDrop()
{
	if (!ProjectileMovement)
	{
		return;
	}

	// Optionally nudge forward; gravity does the rest.
	ProjectileMovement->Velocity = GetActorForwardVector() * DropForwardImpulse;
}

void AExecution_Projectile::ApplyExternalVelocity()
{
	if (!ProjectileMovement)
	{
		return;
	}

	if (ItemContext.bHasExternalLaunchVelocity)
	{
		ProjectileMovement->Velocity = ItemContext.LaunchVelocity;
		// Ensure MaxSpeed won't clamp the provided velocity.
		const float VelMagnitude = ItemContext.LaunchVelocity.Size();
		if (VelMagnitude > ProjectileMovement->MaxSpeed)
		{
			ProjectileMovement->MaxSpeed = VelMagnitude;
		}
	}
	else
	{
		// External velocity was expected but not provided — fall back to arc throw.
		UE_LOG(LogItemSystem, Warning,
			TEXT("Execution_Projectile %s: ExternalVelocity mode but context has no launch velocity. Falling back to ArcThrow."),
			*GetName());
		ApplyArcThrow();
	}
}

// ---------------------------------------------------------------------------
// Tick / debug

void AExecution_Projectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebugVisuals)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), 25.0f, 12, FColor::Yellow, false, -1.0f, 0, 2.0f);
	}
}

// ---------------------------------------------------------------------------
// Collision

bool AExecution_Projectile::CanTriggerOnActor(AActor* OtherActor) const
{
	if (!OtherActor || !OtherActor->IsValidLowLevel() || OtherActor == this)
	{
		return false;
	}

	if (ItemContext.Instigator && OtherActor == ItemContext.Instigator)
	{
		return false;
	}

	if (GetInstigator() && OtherActor == GetInstigator())
	{
		return false;
	}

	return ShouldAffectActor(OtherActor);
}

void AExecution_Projectile::TriggerExplosion(AActor* OtherActor, UPrimitiveComponent* OtherComp,
	const FHitResult* Hit, bool bFromOverlap)
{
	if (bHasExploded || !HasAuthority())
	{
		return;
	}

	bHasExploded = true;

	FVector ImpactPoint = GetActorLocation();
	if (Hit)
	{
		ImpactPoint = Hit->ImpactPoint;
	}
	else if (OtherComp)
	{
		ImpactPoint = OtherComp->GetComponentLocation();
	}

	if (bShowDebugVisuals && GEngine)
	{
		const TCHAR* TriggerLabel = bFromOverlap ? TEXT("OVERLAP") : TEXT("HIT");
		const FString ComponentName = OtherComp ? OtherComp->GetName() : TEXT("None");
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
			FString::Printf(TEXT("Projectile %s: %s (Component: %s)"), TriggerLabel, *OtherActor->GetName(), *ComponentName));
		DrawDebugPoint(GetWorld(), ImpactPoint, 20.0f, FColor::Cyan, false, 2.0f);
	}

	PlayImpactFX(ImpactPoint);

	if (PayloadInstance)
	{
		FItemContext PayloadContext = ItemContext;
		PayloadContext.bHasImpactPoint = true;
		PayloadContext.ImpactPoint = ImpactPoint;
		PayloadInstance->ApplyEffect(OtherActor, PayloadContext);
	}

	FinishExecution();
}

void AExecution_Projectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (CanTriggerOnActor(OtherActor))
	{
		TriggerExplosion(OtherActor, OtherComp, &Hit, false);
	}
}

void AExecution_Projectile::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (CanTriggerOnActor(OtherActor))
	{
		TriggerExplosion(OtherActor, OtherComp, &SweepResult, true);
	}
}
