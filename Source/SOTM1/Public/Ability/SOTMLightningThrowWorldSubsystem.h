#pragma once

#include "CoreMinimal.h"
#include "Ability/SOTMLightningThrowTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SOTMLightningThrowWorldSubsystem.generated.h"

class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class USOTMPlayerStateSubsystem;

/** CH1-only runtime for Chapter 1's second ability. Stuns Cousins and aggravates their siblings. */
UCLASS()
class SOTM1_API USOTMLightningThrowWorldSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Lightning Throw")
	FSOTMLightningThrowStateChangedSignature OnLightningThrowStateChanged;

	UFUNCTION(BlueprintPure, Category="SOTM|Lightning Throw")
	ESOTMLightningThrowRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category="SOTM|Lightning Throw")
	float GetCooldownRemaining() const;

	/** Returns the number of Cousins stunned by this throw. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Lightning Throw")
	int32 TryThrowLightning();

private:
	UFUNCTION()
	void HandleOwnershipChanged(bool bUnlocked);

	void BindProductionInput();
	void UnbindProductionInput();
	void HandleLightningInput();
	void SetRuntimeState(ESOTMLightningThrowRuntimeState NewState);
	void FinishCooldown();
	void RefreshStateFromOwnership();

	UPROPERTY(Transient)
	TObjectPtr<USOTMPlayerStateSubsystem> PlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LightningInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LightningInputContext;

	TWeakObjectPtr<UEnhancedInputComponent> BoundEnhancedInput;
	uint32 LightningBindingHandle = 0;
	ESOTMLightningThrowRuntimeState RuntimeState = ESOTMLightningThrowRuntimeState::Locked;
	FTimerHandle CooldownTimer;
	FTimerHandle InitializeTimer;
};
