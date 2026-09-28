#include "AI/SOTMBTTask_IsabelMeleeAttack.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UBTTask_IsabelMeleeAttack::UBTTask_IsabelMeleeAttack()
{
	NodeName = TEXT("Isabel Melee Attack");
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
}

EBTNodeResult::Type UBTTask_IsabelMeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	APawn* MyPawn = AIController ? AIController->GetPawn() : nullptr;
	AActor* TargetActor = BB ? Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;

	if (!MyPawn || !TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	const float Distance = FVector::Dist(MyPawn->GetActorLocation(), TargetActor->GetActorLocation());
	if (Distance > MeleeRange)
	{
		return EBTNodeResult::Failed;
	}

	if (AttackMontage)
	{
		if (const ACharacter* MyCharacter = Cast<ACharacter>(MyPawn))
		{
			if (USkeletalMeshComponent* Mesh = MyCharacter->GetMesh())
			{
				if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
				{
					AnimInstance->Montage_Play(AttackMontage);
				}
			}
		}
	}

	UGameplayStatics::ApplyDamage(TargetActor, MeleeDamage, AIController, MyPawn, DamageTypeClass);

	return EBTNodeResult::Succeeded;
}
