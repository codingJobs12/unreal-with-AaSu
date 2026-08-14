#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SOTMIngameUIWidget.generated.h"

class UTextBlock;
class USOTMPlayerStateSubsystem;
struct FWidgetTransform;

/** Adds the event-driven production Coin counter to the existing HUD. */
UCLASS(Abstract)
class SOTM1_API USOTMIngameUIWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleCoinsChanged(int32 AvailableCoins, int32 LifetimeCoinsCollected);

	void EnsureCoinCounter();
	void RefreshCoinCounter(int32 LifetimeCoinsCollected, bool bPlayFeedback);
	void FinishCoinPulse();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CoinCounterText;

	UPROPERTY(Transient)
	TObjectPtr<USOTMPlayerStateSubsystem> BoundCoinState;

	FTimerHandle CoinPulseTimerHandle;
	int32 DisplayedLifetimeCoins = INDEX_NONE;
};
