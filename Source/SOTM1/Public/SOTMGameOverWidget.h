#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SOTMGameOverWidget.generated.h"

class UButton;

/**
 * Game Over choice surface. Backend only - no layout is built in C++: this base class
 * just drives the retry/main-menu routing (delegated to the player-state subsystem)
 * and reacts to button clicks. Visuals are built in the Designer instead, same
 * BindWidgetOptional pattern as USOTMUpgradeStationWidget/USOTMSkillTreeWidget: make a
 * Blueprint subclass of this class (e.g. WBP_GameOver), lay out "GAME OVER" text plus
 * whatever else in its Designer canvas, and place a UButton with EXACTLY the name
 * RetryButton and/or MainMenuButton for NativeConstruct to wire up automatically on
 * compile. Either one left out (or the whole WBP not made yet) simply means that
 * button's click does nothing - nothing here is auto-generated to fill the gap.
 */
UCLASS(Blueprintable)
class SOTM1_API USOTMGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleMainMenuClicked();

	// --- Designer-bound widgets ---------------------------------------------
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;
};
