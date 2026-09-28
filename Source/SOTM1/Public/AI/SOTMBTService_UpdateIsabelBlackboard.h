#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "SOTMBTService_UpdateIsabelBlackboard.generated.h"

/**
 * Drop this Service on the root Selector/Sequence of Isabel's Behavior Tree
 * so it keeps running for as long as the tree is active. It refreshes the
 * DistanceToTarget and HealthPercent blackboard keys every tick from the
 * boss's own pawn/health state. TargetActor and CanSeeTarget are kept up to
 * date separately, directly by ASOTMIsabelBossAIController from AI Perception -
 * this service does not touch those two.
 */
UCLASS()
class SOTM1_API UBTService_UpdateIsabelBlackboard : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateIsabelBlackboard();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector DistanceToTargetKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector HealthPercentKey;
};
