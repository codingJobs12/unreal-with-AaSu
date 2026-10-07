#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SOTMIsabelBossAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTree;
class UBlackboardData;
class USOTMBossVitalComponent;
class USOTMPlayerStateSubsystem;

/**
 * New, separate AI Controller for a Behavior-Tree-driven Isabel boss fight.
 * This is intentionally NOT a subclass of, and does not modify, the existing
 * hand-rolled ASOTMIsabelAIController (used elsewhere) or ASOTMCousinAIController -
 * both keep working exactly as before. Possess Isabel's new boss Character
 * Blueprint with this controller, assign a Behavior Tree asset in the Details
 * panel, and build the tree yourself in the editor using the custom
 * BTService/BTTask nodes below plus any stock Behavior Tree nodes.
 *
 * Blackboard key names this controller and the matching BTService/BTTasks
 * expect (create keys of these exact names/types in your Blackboard asset):
 *   TargetActor       (Object, base class Actor)  - set automatically from AI Perception
 *   CanSeeTarget       (Bool)                       - set automatically from AI Perception
 *   DistanceToTarget   (Float)                       - kept updated by UBTService_UpdateIsabelBlackboard
 *   HealthPercent      (Float, 0-1)                    - kept updated by this controller + the service
 *   IsPlayerDead       (Bool)                          - TRUE while the player is dead / respawning / game over
 *                                                       (also clears TargetActor + CanSeeTarget and stops her).
 *                                                       Use it in the tree to send Isabella back home to stand.
 */
UCLASS(Blueprintable)
class SOTM1_API ASOTMIsabelBossAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASOTMIsabelBossAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	static const FName TargetActorKey;
	static const FName DistanceToTargetKey;
	static const FName CanSeeTargetKey;
	static const FName HealthPercentKey;
	static const FName IsPlayerDeadKey;

	UFUNCTION(BlueprintPure, Category="SOTM|Isabel|Boss AI")
	AActor* GetSensedTargetActor() const;

protected:
	/** Assign the Behavior Tree asset you build in the editor here. */
	UPROPERTY(EditDefaultsOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	/** Optional - if unset, the Behavior Tree asset's own default Blackboard is used. */
	UPROPERTY(EditDefaultsOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<UBlackboardData> BlackboardOverride;

	UPROPERTY(EditDefaultsOnly, Category="SOTM|Isabel|Boss AI", meta=(ClampMin="0.0"))
	float SightRadius = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category="SOTM|Isabel|Boss AI", meta=(ClampMin="0.0"))
	float LoseSightRadius = 2500.0f;

	UPROPERTY(EditDefaultsOnly, Category="SOTM|Isabel|Boss AI", meta=(ClampMin="0.0", ClampMax="360.0"))
	float PeripheralVisionAngleDegrees = 90.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	UPROPERTY(EditDefaultsOnly, Category="SOTM|Isabel|Boss AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

private:
	UFUNCTION()
	void HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	UFUNCTION()
	void HandleTargetHealthChanged(USOTMBossVitalComponent* VitalComponent, float PreviousHealth, float CurrentHealth, float MaximumHealth);

	void UpdateHealthPercentFromPossessedPawn();

	UFUNCTION()
	void HandlePlayerDeathStarted(AActor* PlayerActor);

	UFUNCTION()
	void HandlePlayerRespawned(AActor* PlayerActor);

	UFUNCTION()
	void HandleGameOver();

	void SetPlayerDead(bool bDead);

	TWeakObjectPtr<USOTMPlayerStateSubsystem> BoundPlayerState;
	bool bPlayerDead = false;
};
