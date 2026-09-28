#include "AI/SOTMIsabelBossAIController.h"

#include "AI/SOTMBossVitalComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISenseConfig_Sight.h"

const FName ASOTMIsabelBossAIController::TargetActorKey(TEXT("TargetActor"));
const FName ASOTMIsabelBossAIController::DistanceToTargetKey(TEXT("DistanceToTarget"));
const FName ASOTMIsabelBossAIController::CanSeeTargetKey(TEXT("CanSeeTarget"));
const FName ASOTMIsabelBossAIController::HealthPercentKey(TEXT("HealthPercent"));

ASOTMIsabelBossAIController::ASOTMIsabelBossAIController()
{
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	SetPerceptionComponent(*PerceptionComp);
}

void ASOTMIsabelBossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (PerceptionComp)
	{
		PerceptionComp->OnPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandlePerceptionUpdated);
		PerceptionComp->OnPerceptionUpdated.AddDynamic(this, &ThisClass::HandlePerceptionUpdated);
	}

	if (BehaviorTreeAsset)
	{
		UBlackboardData* BBAsset = BlackboardOverride ? BlackboardOverride.Get() : BehaviorTreeAsset->BlackboardAsset.Get();
		if (BBAsset)
		{
			UBlackboardComponent* BBComp = nullptr;
			UseBlackboard(BBAsset, BBComp);
		}
		RunBehaviorTree(BehaviorTreeAsset);
	}

	if (InPawn)
	{
		if (USOTMBossVitalComponent* Vital = InPawn->FindComponentByClass<USOTMBossVitalComponent>())
		{
			Vital->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleTargetHealthChanged);
			Vital->OnHealthChanged.AddDynamic(this, &ThisClass::HandleTargetHealthChanged);
		}
	}
	UpdateHealthPercentFromPossessedPawn();
}

void ASOTMIsabelBossAIController::OnUnPossess()
{
	if (APawn* PreviousPawn = GetPawn())
	{
		if (USOTMBossVitalComponent* Vital = PreviousPawn->FindComponentByClass<USOTMBossVitalComponent>())
		{
			Vital->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleTargetHealthChanged);
		}
	}
	if (PerceptionComp)
	{
		PerceptionComp->OnPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandlePerceptionUpdated);
	}
	Super::OnUnPossess();
}

AActor* ASOTMIsabelBossAIController::GetSensedTargetActor() const
{
	const UBlackboardComponent* BB = GetBlackboardComponent();
	return BB ? Cast<AActor>(BB->GetValueAsObject(TargetActorKey)) : nullptr;
}

void ASOTMIsabelBossAIController::HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB || !PerceptionComp)
	{
		return;
	}

	for (AActor* Actor : UpdatedActors)
	{
		if (!Actor)
		{
			continue;
		}

		FActorPerceptionBlueprintInfo Info;
		PerceptionComp->GetActorsPerception(Actor, Info);

		bool bCurrentlySensed = false;
		for (const FAIStimulus& Stimulus : Info.LastSensedStimuli)
		{
			if (Stimulus.WasSuccessfullySensed())
			{
				bCurrentlySensed = true;
				break;
			}
		}

		if (bCurrentlySensed)
		{
			BB->SetValueAsObject(TargetActorKey, Actor);
			BB->SetValueAsBool(CanSeeTargetKey, true);
		}
		else if (Cast<AActor>(BB->GetValueAsObject(TargetActorKey)) == Actor)
		{
			BB->SetValueAsBool(CanSeeTargetKey, false);
		}
	}
}

void ASOTMIsabelBossAIController::HandleTargetHealthChanged(
	USOTMBossVitalComponent* VitalComponent,
	const float PreviousHealth,
	const float CurrentHealth,
	const float MaximumHealth)
{
	(void)VitalComponent;
	(void)PreviousHealth;
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsFloat(HealthPercentKey, MaximumHealth > 0.0f ? CurrentHealth / MaximumHealth : 0.0f);
	}
}

void ASOTMIsabelBossAIController::UpdateHealthPercentFromPossessedPawn()
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		return;
	}
	if (const APawn* MyPawn = GetPawn())
	{
		if (const USOTMBossVitalComponent* Vital = MyPawn->FindComponentByClass<USOTMBossVitalComponent>())
		{
			BB->SetValueAsFloat(HealthPercentKey, Vital->GetHealthNormalized());
		}
	}
}
