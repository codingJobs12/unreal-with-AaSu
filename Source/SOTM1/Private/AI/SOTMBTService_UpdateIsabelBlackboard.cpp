#include "AI/SOTMBTService_UpdateIsabelBlackboard.h"

#include "AI/SOTMBossVitalComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UBTService_UpdateIsabelBlackboard::UBTService_UpdateIsabelBlackboard()
{
	NodeName = TEXT("Update Isabel Blackboard");
	Interval = 0.2f;
	RandomDeviation = 0.05f;

	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
	DistanceToTargetKey.SelectedKeyName = TEXT("DistanceToTarget");
	HealthPercentKey.SelectedKeyName = TEXT("HealthPercent");
}

void UBTService_UpdateIsabelBlackboard::TickNode(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, const float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!BB || !AIController)
	{
		return;
	}

	const APawn* MyPawn = AIController->GetPawn();
	AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));

	if (MyPawn && TargetActor)
	{
		const float Distance = FVector::Dist(MyPawn->GetActorLocation(), TargetActor->GetActorLocation());
		BB->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, Distance);
	}

	if (MyPawn)
	{
		if (const USOTMBossVitalComponent* Vital = MyPawn->FindComponentByClass<USOTMBossVitalComponent>())
		{
			BB->SetValueAsFloat(HealthPercentKey.SelectedKeyName, Vital->GetHealthNormalized());
		}
	}
}
