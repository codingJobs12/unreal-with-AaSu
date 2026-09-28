#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "SOTMBTTask_IsabelRandomRoam.generated.h"

/**
 * Idle/wander behavior - picks a random reachable point within RoamRadius of
 * the boss's CURRENT location (using the navigation system) and moves there.
 * Use this as the lowest-priority branch in the Selector, for when there is
 * no sensed target, so she doesn't just stand still.
 *
 * Runs as a latent task: returns InProgress while moving, Succeeded once she
 * arrives (or TimeoutSeconds elapses as a stuck-safety-net), and Failed if no
 * reachable point could be found nearby. A Failed roam just means the
 * Selector falls through to whatever comes after it in the tree (or re-ticks
 * next frame if this is the last branch) - it is not an error state.
 */
UCLASS()
class SOTM1_API UBTTask_IsabelRandomRoam : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_IsabelRandomRoam();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;

protected:
	/** Max distance from her current spot to pick a roam destination. */
	UPROPERTY(EditAnywhere, Category="Roam", meta=(ClampMin="0.0"))
	float RoamRadius = 1200.0f;

	/** Roam points closer than this are rejected (avoids near-zero-distance "wandering"). */
	UPROPERTY(EditAnywhere, Category="Roam", meta=(ClampMin="0.0"))
	float MinRoamDistance = 200.0f;

	UPROPERTY(EditAnywhere, Category="Roam", meta=(ClampMin="0.0"))
	float AcceptanceRadius = 50.0f;

	/** Safety net - finishes the task even if she gets stuck mid-move. */
	UPROPERTY(EditAnywhere, Category="Roam", meta=(ClampMin="0.0"))
	float TimeoutSeconds = 10.0f;
};
