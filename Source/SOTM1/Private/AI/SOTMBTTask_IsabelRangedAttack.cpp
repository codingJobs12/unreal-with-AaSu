#include "AI/SOTMBTTask_IsabelRangedAttack.h"

#include "AI/SOTMIsabelDarkBurstProjectile.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UBTTask_IsabelRangedAttack::UBTTask_IsabelRangedAttack()
{
	NodeName = TEXT("Isabel Ranged Attack (Dark Burst)");
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
}

EBTNodeResult::Type UBTTask_IsabelRangedAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	APawn* MyPawn = AIController ? AIController->GetPawn() : nullptr;
	AActor* TargetActor = BB ? Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;

	if (!MyPawn || !TargetActor || !ProjectileClass)
	{
		return EBTNodeResult::Failed;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	if (FVector::Dist(MyLocation, TargetLocation) > MaxRange)
	{
		return EBTNodeResult::Failed;
	}

	FVector SpawnLocation = MyLocation;
	if (const ACharacter* MyCharacter = Cast<ACharacter>(MyPawn))
	{
		if (USkeletalMeshComponent* Mesh = MyCharacter->GetMesh())
		{
			if (!MuzzleSocketName.IsNone() && Mesh->DoesSocketExist(MuzzleSocketName))
			{
				SpawnLocation = Mesh->GetSocketLocation(MuzzleSocketName);
			}
		}
	}

	const FRotator AimRotation = (TargetLocation - SpawnLocation).Rotation();

	UWorld* World = MyPawn->GetWorld();
	if (!World)
	{
		return EBTNodeResult::Failed;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = MyPawn;
	SpawnParams.Instigator = MyPawn->GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASOTMIsabelDarkBurstProjectile* Projectile =
		World->SpawnActor<ASOTMIsabelDarkBurstProjectile>(ProjectileClass, SpawnLocation, AimRotation, SpawnParams);
	if (!Projectile)
	{
		return EBTNodeResult::Failed;
	}

	Projectile->InitializeDamage(Damage, AIController, DamageTypeClass);

	return EBTNodeResult::Succeeded;
}
