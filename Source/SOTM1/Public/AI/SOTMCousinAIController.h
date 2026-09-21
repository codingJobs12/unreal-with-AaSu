#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "AI/SOTMCousinTypes.h"
#include "SOTMCousinAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/** One timer-driven, perception-based behavior shared by every Forest Cousin. */
UCLASS(Blueprintable)
class SOTM1_API ASOTMCousinAIController final : public AAIController
{
	GENERATED_BODY()

public:
	ASOTMCousinAIController();
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="SOTM|Cousin")
	ESOTMCousinAIState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category="SOTM|Cousin")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	void SuspendForPlayerDeath();
	void ResetAfterPlayerRespawn();
	void SetPresentationVariant(int32 VariantIndex);

	UFUNCTION(BlueprintPure, Category="SOTM|Cousin")
	bool IsStunned() const { return CurrentState == ESOTMCousinAIState::Disabled && bStunActive; }

	/** Lightning Throw stuns rather than kills. Returns false when already stunned or disabled. */
	bool ApplyLightningStun(float DurationSeconds);

	/** Forces this Cousin onto the player after a nearby sibling was stunned or screamed. */
	void AggravateTowards(AActor* PlayerActor);

private:
	void AlertPack(AActor* PlayerActor);

public:

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleMoveCompleted(FAIRequestID RequestId, EPathFollowingResult::Type Result);

	void EvaluateBehavior();
	void SetState(ESOTMCousinAIState NewState, const TCHAR* Reason);
	void BeginPatrol();
	void QueuePatrolMove(float DelaySeconds);
	void RequestPatrolMove();
	void EvaluateChase();
	void ClearTargetAndPatrol();
	bool IsValidLivingPlayer(const AActor* Actor) const;
	bool HasCatchLineOfSight(const AActor* Actor) const;
	float GetTargetDistance() const;

	UPROPERTY(VisibleAnywhere, Category="SOTM|Cousin|Perception")
	TObjectPtr<UAIPerceptionComponent> CousinPerception;

	UPROPERTY(VisibleAnywhere, Category="SOTM|Cousin|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Perception", meta=(ClampMin="200.0"))
	float SightRadius = 2300.0f;

	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Perception", meta=(ClampMin="200.0"))
	float LoseSightRadius = 2900.0f;

	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Perception", meta=(ClampMin="1.0", ClampMax="180.0"))
	float PeripheralVisionHalfAngle = 85.0f;

	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Movement", meta=(ClampMin="50.0"))
	float PatrolSpeed = 210.0f;

	/** Just above the player's 600 walk speed, but below the 840 Speed Boost, so the
	 *  boost stays the intended escape rather than walking away being enough. */
	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Movement", meta=(ClampMin="50.0"))
	float ChaseSpeed = 640.0f;

	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Movement", meta=(ClampMin="100.0"))
	float PatrolRadius = 1400.0f;

	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Catch", meta=(ClampMin="50.0"))
	float CatchRange = 220.0f;

	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Chase", meta=(ClampMin="0.2"))
	float LostTargetGraceSeconds = 7.0f;

	/** A Cousin that spots the player screams for its siblings, so they hunt as a pack. */
	UPROPERTY(EditAnywhere, Category="SOTM|Cousin|Chase", meta=(ClampMin="0.0"))
	float PackAlertRadius = 3000.0f;

	UPROPERTY(Transient)
	ESOTMCousinAIState CurrentState = ESOTMCousinAIState::Idle;

	TWeakObjectPtr<AActor> CurrentTarget;
	FVector HomeLocation = FVector::ZeroVector;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	float LastSeenTargetTime = -1.0f;
	float NextChaseMoveTime = 0.0f;
	int32 PresentationVariant = 0;
	bool bCanSeeTarget = false;
	bool bStunActive = false;
	FTimerHandle EvaluationTimer;
	FTimerHandle PatrolMoveTimer;
	FTimerHandle StunTimer;

	void EndLightningStun();
};
