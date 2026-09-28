#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "SOTMBTTask_IsabelRangedAttack.generated.h"

class ASOTMIsabelDarkBurstProjectile;

/**
 * Spawns a purple dark-burst projectile (ASOTMIsabelDarkBurstProjectile /
 * your Blueprint subclass of it) aimed at the blackboard's TargetActor.
 * Fails immediately (no projectile spawned) if there is no target, no
 * ProjectileClass assigned, or the target is beyond MaxRange - so you can
 * gate this behind a Decorator/Sequence in the tree the same way as the
 * melee task.
 */
UCLASS()
class SOTM1_API UBTTask_IsabelRangedAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_IsabelRangedAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category="Attack")
	TSubclassOf<ASOTMIsabelDarkBurstProjectile> ProjectileClass;

	/** Skeletal mesh socket to spawn from, if it exists on the boss's mesh; falls back to the pawn's location. */
	UPROPERTY(EditAnywhere, Category="Attack")
	FName MuzzleSocketName = TEXT("Muzzle_Hand");

	UPROPERTY(EditAnywhere, Category="Attack", meta=(ClampMin="0.0"))
	float MaxRange = 2500.0f;

	UPROPERTY(EditAnywhere, Category="Attack", meta=(ClampMin="0.0"))
	float Damage = 15.0f;

	UPROPERTY(EditAnywhere, Category="Attack")
	TSubclassOf<UDamageType> DamageTypeClass;
};
