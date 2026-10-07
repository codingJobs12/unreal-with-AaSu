#include "AI/SOTMIsabelBossAIController.h"

#include "AI/SOTMBossVitalComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SOTMPlayerStateSubsystem.h"
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
const FName ASOTMIsabelBossAIController::IsPlayerDeadKey(TEXT("IsPlayerDead"));

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

	// Player death -> IsPlayerDead blackboard key.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USOTMPlayerStateSubsystem* PS = GI->GetSubsystem<USOTMPlayerStateSubsystem>())
		{
			BoundPlayerState = PS;
			PS->OnPlayerDeathStarted.RemoveDynamic(this, &ThisClass::HandlePlayerDeathStarted);
			PS->OnPlayerDeathStarted.AddDynamic(this, &ThisClass::HandlePlayerDeathStarted);
			PS->OnPlayerRespawned.RemoveDynamic(this, &ThisClass::HandlePlayerRespawned);
			PS->OnPlayerRespawned.AddDynamic(this, &ThisClass::HandlePlayerRespawned);
			PS->OnGameOver.RemoveDynamic(this, &ThisClass::HandleGameOver);
			PS->OnGameOver.AddDynamic(this, &ThisClass::HandleGameOver);
			SetPlayerDead(PS->IsPlayerDead() || PS->IsGameOver());
		}
	}
}

void ASOTMIsabelBossAIController::OnUnPossess()
{
	if (USOTMPlayerStateSubsystem* PS = BoundPlayerState.Get())
	{
		PS->OnPlayerDeathStarted.RemoveDynamic(this, &ThisClass::HandlePlayerDeathStarted);
		PS->OnPlayerRespawned.RemoveDynamic(this, &ThisClass::HandlePlayerRespawned);
		PS->OnGameOver.RemoveDynamic(this, &ThisClass::HandleGameOver);
	}
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
	if (!BB || !PerceptionComp || bPlayerDead)
	{
		return; // player is dead: do not re-acquire a target
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

void ASOTMIsabelBossAIController::HandlePlayerDeathStarted(AActor* /*PlayerActor*/)
{
	SetPlayerDead(true);
}

void ASOTMIsabelBossAIController::HandlePlayerRespawned(AActor* /*PlayerActor*/)
{
	SetPlayerDead(false);
}

void ASOTMIsabelBossAIController::HandleGameOver()
{
	SetPlayerDead(true);
}

void ASOTMIsabelBossAIController::SetPlayerDead(const bool bDead)
{
	bPlayerDead = bDead;
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		return;
	}
	BB->SetValueAsBool(IsPlayerDeadKey, bDead);
	if (bDead)
	{
		// Forget the player so the tree stops fighting and can send her home.
		BB->ClearValue(TargetActorKey);
		BB->SetValueAsBool(CanSeeTargetKey, false);
		StopMovement();
		if (PerceptionComp)
		{
			PerceptionComp->ForgetAll();
		}
	}
}
