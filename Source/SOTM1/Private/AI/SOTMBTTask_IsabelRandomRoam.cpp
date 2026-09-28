#include "AI/SOTMBTTask_IsabelRandomRoam.h"

#include "AIController.h"
#include "AITypes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

namespace SOTMIsabelRoamPrivate
{
	struct FBTIsabelRoamMemory
	{
		float ElapsedTime = 0.0f;
	};
}

UBTTask_IsabelRandomRoam::UBTTask_IsabelRandomRoam()
{
	NodeName = TEXT("Isabel Random Roam");
	bNotifyTick = true;
}

uint16 UBTTask_IsabelRandomRoam::GetInstanceMemorySize() const
{
	return sizeof(SOTMIsabelRoamPrivate::FBTIsabelRoamMemory);
}

EBTNodeResult::Type UBTTask_IsabelRandomRoam::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	using namespace SOTMIsabelRoamPrivate;

	FBTIsabelRoamMemory* Memory = reinterpret_cast<FBTIsabelRoamMemory*>(NodeMemory);
	Memory->ElapsedTime = 0.0f;

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* MyPawn = AIController ? AIController->GetPawn() : nullptr;
	if (!AIController || !MyPawn)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(MyPawn->GetWorld());
	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation RandomLocation;
	const bool bFound = NavSystem->GetRandomReachablePointInRadius(MyPawn->GetActorLocation(), RoamRadius, RandomLocation);
	if (!bFound)
	{
		return EBTNodeResult::Failed;
	}

	if (FVector::Dist(MyPawn->GetActorLocation(), RandomLocation.Location) < MinRoamDistance)
	{
		// Too close to be worth moving - the tree will simply re-evaluate next tick.
		return EBTNodeResult::Failed;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(RandomLocation.Location);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetUsePathfinding(true);

	const FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest);
	if (RequestResult.Code == EPathFollowingRequestResult::Failed)
	{
		return EBTNodeResult::Failed;
	}
	if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_IsabelRandomRoam::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, const float DeltaSeconds)
{
	using namespace SOTMIsabelRoamPrivate;

	FBTIsabelRoamMemory* Memory = reinterpret_cast<FBTIsabelRoamMemory*>(NodeMemory);
	Memory->ElapsedTime += DeltaSeconds;

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (Memory->ElapsedTime >= TimeoutSeconds)
	{
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_IsabelRandomRoam::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	(void)NodeMemory;
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		AIController->StopMovement();
	}
	return EBTNodeResult::Aborted;
}
