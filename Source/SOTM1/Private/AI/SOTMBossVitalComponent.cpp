#include "AI/SOTMBossVitalComponent.h"

#include "GameFramework/Actor.h"

USOTMBossVitalComponent::USOTMBossVitalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USOTMBossVitalComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaximumHealth;

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &USOTMBossVitalComponent::HandleAnyDamage);
	}
}

void USOTMBossVitalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &USOTMBossVitalComponent::HandleAnyDamage);
	}
	Super::EndPlay(EndPlayReason);
}

void USOTMBossVitalComponent::InitializeVitals(const float InMaximumHealth, const float InCurrentHealth)
{
	const float PreviousHealth = CurrentHealth;
	MaximumHealth = FMath::Max(1.0f, InMaximumHealth);
	CurrentHealth = FMath::Clamp(InCurrentHealth < 0.0f ? MaximumHealth : InCurrentHealth, 0.0f, MaximumHealth);
	bIsDead = CurrentHealth <= 0.0f;
	bInvulnerable = false;
	BroadcastHealthChanged(PreviousHealth);
}

bool USOTMBossVitalComponent::ApplySOTMDamage(
	const float Damage,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	if (Damage <= 0.0f || bInvulnerable || bIsDead)
	{
		return false;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaximumHealth);
	const float AppliedDamage = PreviousHealth - CurrentHealth;

	OnDamaged.Broadcast(this, AppliedDamage, InstigatedBy, DamageCauser);
	BroadcastHealthChanged(PreviousHealth);

	if (CurrentHealth <= 0.0f && !bIsDead)
	{
		bIsDead = true;
		OnDeath.Broadcast(this, InstigatedBy, DamageCauser);
	}

	return AppliedDamage > 0.0f;
}

void USOTMBossVitalComponent::ResetToFullHealth()
{
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = MaximumHealth;
	bIsDead = false;
	BroadcastHealthChanged(PreviousHealth);
}

void USOTMBossVitalComponent::SetInvulnerable(const bool bNewInvulnerable)
{
	bInvulnerable = bNewInvulnerable;
}

float USOTMBossVitalComponent::GetHealthNormalized() const
{
	return MaximumHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaximumHealth, 0.0f, 1.0f) : 0.0f;
}

void USOTMBossVitalComponent::HandleAnyDamage(
	AActor* DamagedActor,
	const float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	(void)DamagedActor;
	(void)DamageType;
	ApplySOTMDamage(Damage, InstigatedBy, DamageCauser);
}

void USOTMBossVitalComponent::BroadcastHealthChanged(const float PreviousHealth)
{
	OnHealthChanged.Broadcast(this, PreviousHealth, CurrentHealth, MaximumHealth);
}
