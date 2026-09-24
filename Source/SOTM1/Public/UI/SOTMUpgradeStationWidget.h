#pragma once

#include "CoreMinimal.h"
#include "Ability/SOTMSpeedBoostTypes.h"
#include "Ability/SOTMLightningThrowTypes.h"
#include "Blueprint/UserWidget.h"
#include "Objective/SOTMObjectiveSubsystem.h"
#include "SOTMUpgradeStationWidget.generated.h"

class UButton;
class UTextBlock;
class USOTMObjectiveSubsystem;
class USOTMPlayerStateSubsystem;

/**
 * Event-driven presentation adapter for Timmy's Upgrade Station screen.
 *
 * Visuals are built in the Designer (WBP_UpgradeStation, a Blueprint subclass of this
 * class) instead of hand-built Slate in C++. This base class only drives state and
 * reacts to purchase/close input. Same BindWidgetOptional pattern as SOTMIngameUIWidget:
 * build a widget of the matching type and EXACT name below inside WBP_UpgradeStation's
 * Designer canvas and UMG wires the pointer up for you on compile.
 *
 * PRIORITY LAYOUT (same idea as SOTMIngameUIWidget's current-vs-future objective text):
 * the big widget group (AbilityNameText/OwnershipText/DescriptionText/LevelText/
 * SpeedIncreaseText/DurationText/CooldownText/AvailableCoinsText/CostText/
 * RequirementText/UnlockButton) always shows whichever ability the player should buy
 * next - Speed Boost until it's owned, then Lightning Throw. The small "Lightning..."-
 * named group is the minimized slot for whichever ability is NOT currently the
 * priority (so it holds Lightning Throw's info before Speed Boost is owned, and Speed
 * Boost's own OWNED status afterward). Which ability each group's text/button actually
 * represents is decided per-frame in RefreshPresentation() - the widget names in the
 * Designer don't need to change.
 */
UCLASS(Abstract, Blueprintable)
class SOTM1_API USOTMUpgradeStationWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleCoinsChanged(int32 AvailableCoins, int32 LifetimeCoinsCollected);

	UFUNCTION()
	void HandleOwnershipChanged(bool bUnlocked, int32 Level);

	UFUNCTION()
	void HandleObjectiveChanged(FSOTMObjectiveData Objective);

	UFUNCTION()
	void HandleLightningOwnershipChanged(bool bUnlocked);

	UFUNCTION()
	void HandleUnlockClicked();

	UFUNCTION()
	void HandleLightningUnlockClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void RefreshPresentation();
	bool CanPurchase() const;
	bool CanPurchaseLightning() const;
	FText GetRequirementText() const;
	FText GetLightningRequirementText() const;

	UPROPERTY(Transient)
	TObjectPtr<USOTMPlayerStateSubsystem> PlayerState;

	UPROPERTY(Transient)
	TObjectPtr<USOTMObjectiveSubsystem> ObjectiveState;

	// --- Designer-bound widgets ---------------------------------------------
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AbilityNameText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> OwnershipText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	// Stat rows under StatsBorder/VerticalBox_Stats. Reused for whichever ability is the
	// current priority: Speed Boost fills them with SPEED INCREASE/DURATION/COOLDOWN,
	// Lightning Throw fills them with RANGE/STUN DURATION/COOLDOWN.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeedIncreaseText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DurationText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CooldownText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AvailableCoinsText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RequirementText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> UnlockButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> UnlockButtonText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LightningOwnershipText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LightningRequirementText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> LightningUnlockButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LightningUnlockButtonText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	ESOTMSpeedBoostPurchaseResult LastPurchaseResult = ESOTMSpeedBoostPurchaseResult::ObjectiveIncomplete;
	ESOTMLightningThrowPurchaseResult LastLightningResult = ESOTMLightningThrowPurchaseResult::ObjectiveIncomplete;
	bool bHasAttemptedPurchase = false;
	bool bHasAttemptedLightningPurchase = false;
};
