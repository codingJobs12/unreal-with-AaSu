#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SOTMLightningStunComponent.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSOTMStunStartedSignature, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSOTMStunEndedSignature);

/**
 * Add this component to BP_Isabel (or any pawn) to make the player's Lightning Throw stun it,
 * just like the cousins. While stunned the pawn stops moving and its Behavior Tree is paused;
 * when the stun ends the Behavior Tree resumes and she roams/attacks again.
 *
 * Blueprint hooks: On Stun Started / On Stun Ended (play a hit-react, change materials...).
 */
UCLASS(ClassGroup=(SOTM), meta=(BlueprintSpawnableComponent))
class SOTM1_API USOTMLightningStunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USOTMLightningStunComponent();

	/** Called by the Lightning Throw. Returns true if the stun started. */
	UFUNCTION(BlueprintCallable, Category="Stun")
	bool ApplyLightningStun(float DefaultDuration);

	UFUNCTION(BlueprintCallable, Category="Stun")
	bool CanBeStunned() const;

	UFUNCTION(BlueprintPure, Category="Stun")
	bool IsStunned() const { return bStunned; }

	/** Ends any stun and blocks all future stuns (used by the chapter ending). */
	UFUNCTION(BlueprintCallable, Category="Stun")
	void DisableStunPermanently();

	/** <= 0 uses the Lightning Throw's stun duration from the project settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stun")
	float StunDurationOverride = 0.0f;

	/** After a stun ends she cannot be stunned again for this long (stops stun-locking a boss). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stun")
	float ImmunityAfterStun = 3.0f;

	/** Optional hit-react / stunned animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stun")
	TObjectPtr<UAnimMontage> StunMontage;

	/** Row of the boss dialogue table played when stunned (empty = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stun")
	FName StunDialogueRow = TEXT("Stunned_Dare");

	UPROPERTY(BlueprintAssignable, Category="Stun")
	FSOTMStunStartedSignature OnStunStarted;

	UPROPERTY(BlueprintAssignable, Category="Stun")
	FSOTMStunEndedSignature OnStunEnded;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EndStun();

	bool bStunned = false;
	bool bPermanentlyDisabled = false;
	double ImmuneUntil = -1.0;
	float SavedMaxWalkSpeedMode = 0.0f;
	uint8 SavedMovementMode = 0;
	FTimerHandle StunTimer;
};
