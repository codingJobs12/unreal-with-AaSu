#include "AI/SOTMIsabelDarkBurstProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ASOTMIsabelDarkBurstProjectile::ASOTMIsabelDarkBurstProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(15.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComponent->OnComponentHit.AddDynamic(this, &ASOTMIsabelDarkBurstProjectile::HandleHit);
	SetRootComponent(CollisionComponent);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 1800.0f;
	ProjectileMovement->MaxSpeed = 1800.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	TrailEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailEffect"));
	TrailEffect->SetupAttachment(RootComponent);

	SetReplicates(false);
}

void ASOTMIsabelDarkBurstProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSpanSeconds);

	if (TrailSystem && TrailEffect)
	{
		TrailEffect->SetAsset(TrailSystem);
		TrailEffect->Activate(true);
	}
}

void ASOTMIsabelDarkBurstProjectile::InitializeDamage(
	const float InDamage, AController* InInstigatorController, const TSubclassOf<UDamageType> InDamageTypeClass)
{
	Damage = InDamage;
	InstigatorControllerRef = InInstigatorController;
	DamageTypeClass = InDamageTypeClass;
}

void ASOTMIsabelDarkBurstProjectile::HandleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const FVector NormalImpulse,
	const FHitResult& Hit)
{
	(void)HitComponent;
	(void)OtherComponent;
	(void)NormalImpulse;

	if (OtherActor && OtherActor != this && OtherActor != GetOwner())
	{
		UGameplayStatics::ApplyDamage(OtherActor, Damage, InstigatorControllerRef, this, DamageTypeClass);
	}

	if (ImpactSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactSystem, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	}

	Destroy();
}
