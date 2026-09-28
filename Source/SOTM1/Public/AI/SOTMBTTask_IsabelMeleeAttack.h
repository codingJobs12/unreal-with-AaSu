#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "SOTMBTTask_IsabelMeleeAttack.generated.h"

class UAnimMontage;

/**
 * Atomic melee-attack task - checks the boss is within MeleeRange of the
 * blackboard's TargetActor, optionally plays an attack montage, and applies
 * damage immediately via UGameplayStatics::ApplyDamage (picked up automatically
 * by any USOTMBossVitalComponent on the target). Sequence this behind your own
 * Wait/Cooldown nodes in the tree to control attack timing/rate - this task
 * does not wait or animate-and-then-damage on its own, it is a single beat.
 */
UCLASS()
class SOTM1_API UBTTask_IsabelMeleeAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_IsabelMeleeAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category="Attack", meta=(ClampMin="0.0"))
	float MeleeDamage = 25.0f;

	UPROPERTY(EditAnywhere, Category="Attack", meta=(ClampMin="0.0"))
	float MeleeRange = 200.0f;

	UPROPERTY(EditAnywhere, Category="Attack")
	TSubclassOf<UDamageType> DamageTypeClass;

	/** Optional - played on the boss pawn's mesh AnimInstance if set. */
	UPROPERTY(EditAnywhere, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;
};
