#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SOTMBossVitalComponent.generated.h"

class AController;
class USOTMBossVitalComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FSOTMBossHealthChangedSignature,
	USOTMBossVitalComponent*, VitalComponent,
	float, PreviousHealth,
	float, CurrentHealth,
	float, MaximumHealth);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FSOTMBossDamagedSignature,
	USOTMBossVitalComponent*, VitalComponent,
	float, AppliedDamage,
	AController*, InstigatedBy,
	AActor*, DamageCauser);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSOTMBossDeathSignature,
	USOTMBossVitalComponent*, VitalComponent,
	AController*, InstigatedBy,
	AActor*, DamageCauser);

/**
 * Event-driven boss health component - same shape and conventions as
 * USOTMPlayerVitalComponent (accepts Unreal's standard damage events via
 * OnTakeAnyDamage, no per-frame Tick), but for an AI-controlled pawn rather
 * than the player. Add this as a component on the boss's Character Blueprint
 * (e.g. Isabel's pawn) in the Details panel - it needs no C++ wiring beyond
 * that to start receiving damage from anything that calls
 * UGameplayStatics::ApplyDamage on the owning actor.
 */
UCLASS(ClassGroup=(SOTM), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SOTM1_API USOTMBossVitalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USOTMBossVitalComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="SOTM|Boss|Vitals")
	void InitializeVitals(float InMaximumHealth, float InCurrentHealth = -1.0f);

	/** Direct damage entry point for testing/console commands - normal gameplay
	 * damage should go through UGameplayStatics::ApplyDamage on the owning
	 * actor instead, which this component picks up automatically via
	 * OnTakeAnyDamage. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Boss|Vitals")
	bool ApplySOTMDamage(float Damage, AController* InstigatedBy = nullptr, AActor* DamageCauser = nullptr);

	UFUNCTION(BlueprintCallable, Category="SOTM|Boss|Vitals")
	void ResetToFullHealth();

	UFUNCTION(BlueprintCallable, Category="SOTM|Boss|Vitals")
	void SetInvulnerable(bool bNewInvulnerable);

	UFUNCTION(BlueprintPure, Category="SOTM|Boss|Vitals")
	float GetMaximumHealth() const { return MaximumHealth; }

	UFUNCTION(BlueprintPure, Category="SOTM|Boss|Vitals")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="SOTM|Boss|Vitals")
	float GetHealthNormalized() const;

	UFUNCTION(BlueprintPure, Category="SOTM|Boss|Vitals")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category="SOTM|Boss|Vitals")
	bool IsInvulnerable() const { return bInvulnerable; }

	UPROPERTY(BlueprintAssignable, Category="SOTM|Boss|Vitals")
	FSOTMBossHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Boss|Vitals")
	FSOTMBossDamagedSignature OnDamaged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Boss|Vitals")
	FSOTMBossDeathSignature OnDeath;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SOTM|Boss|Vitals", meta=(ClampMin="1.0"))
	float MaximumHealth = 300.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Boss|Vitals")
	float CurrentHealth = 300.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Boss|Vitals")
	bool bIsDead = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Boss|Vitals")
	bool bInvulnerable = false;

private:
	UFUNCTION()
	void HandleAnyDamage(
		AActor* DamagedActor,
		float Damage,
		const UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser);

	void BroadcastHealthChanged(float PreviousHealth);
};
