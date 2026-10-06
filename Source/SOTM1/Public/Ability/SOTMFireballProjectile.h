#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOTMFireballProjectile.generated.h"

class UParticleSystem;
class UParticleSystemComponent;
class UProjectileMovementComponent;
class USphereComponent;

/**
 * Player fireball. Spawn it from Blueprint at the hand (this class never spawns itself); on
 * BeginPlay it launches toward the centre of the player's screen (camera aim point), shows
 * P_ContinueFire, deals Damage to Isabella on contact, and destroys itself on ANY hit or
 * after LifeSeconds (5 s).
 *
 * Use as parent for BP_Fireball (Class Settings -> Parent Class) or spawn it directly.
 */
UCLASS(Blueprintable)
class SOTM1_API ASOTMFireballProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASOTMFireballProjectile();

	virtual void BeginPlay() override;

	/** Fired right before the fireball is destroyed by a hit (VFX/SFX hook for Blueprint). */
	UFUNCTION(BlueprintImplementableEvent, Category="SOTM|Fireball")
	void OnFireballHit(AActor* HitActor, bool bHitIsabella, FVector ImpactPoint);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Fireball")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Fireball")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Fireball")
	TObjectPtr<UParticleSystemComponent> FireEffect;

	/** Defaults to P_ContinueFire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SOTM|Fireball")
	TObjectPtr<UParticleSystem> FireParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SOTM|Fireball", meta=(ClampMin="0.0"))
	float Damage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SOTM|Fireball", meta=(ClampMin="1.0"))
	float Speed = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SOTM|Fireball", meta=(ClampMin="0.1"))
	float LifeSeconds = 5.0f;

	/** How far the aim trace looks for something under the crosshair. */
	UPROPERTY(EditAnywhere, Category="SOTM|Fireball", meta=(ClampMin="100.0"))
	float AimTraceDistance = 10000.0f;

private:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	void LaunchTowardAim();
	void ApplyLaunch();
	FVector LaunchDirection = FVector::ZeroVector;
	static bool IsIsabella(const AActor* Actor);

	bool bConsumed = false;
};
