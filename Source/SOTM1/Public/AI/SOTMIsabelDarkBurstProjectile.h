#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOTMIsabelDarkBurstProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Isabel's "dark powers" ranged attack projectile - a purple burst that flies
 * toward wherever it was spawned/aimed and deals damage to whatever it hits
 * via the standard UGameplayStatics::ApplyDamage flow (picked up automatically
 * by any USOTMBossVitalComponent on the player, no custom interface needed).
 * Spawned by UBTTask_IsabelRangedAttack. Assign TrailSystem/ImpactSystem to
 * a purple Niagara system in a Blueprint subclass or in the projectile's
 * spawn-time defaults for the actual VFX - this C++ base only wires the
 * plumbing (movement, collision, damage, optional Niagara playback).
 */
UCLASS(Blueprintable)
class SOTM1_API ASOTMIsabelDarkBurstProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASOTMIsabelDarkBurstProjectile();

	virtual void BeginPlay() override;

	/** Call right after spawning, before the projectile can hit anything. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Isabel|Boss AI")
	void InitializeDamage(float InDamage, AController* InInstigatorController, TSubclassOf<UDamageType> InDamageTypeClass);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<UNiagaraComponent> TrailEffect;

	/** Purple burst trail VFX, played on spawn. */
	UPROPERTY(EditDefaultsOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<UNiagaraSystem> TrailSystem;

	/** Purple burst impact VFX, spawned at the hit location. */
	UPROPERTY(EditDefaultsOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<UNiagaraSystem> ImpactSystem;

	UPROPERTY(EditAnywhere, Category="SOTM|Isabel|Boss AI", meta=(ClampMin="0.0"))
	float Damage = 15.0f;

	UPROPERTY(EditAnywhere, Category="SOTM|Isabel|Boss AI", meta=(ClampMin="0.0"))
	float LifeSpanSeconds = 5.0f;

	UPROPERTY()
	TObjectPtr<AController> InstigatorControllerRef;

	UPROPERTY()
	TSubclassOf<UDamageType> DamageTypeClass;

private:
	UFUNCTION()
	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);
};
