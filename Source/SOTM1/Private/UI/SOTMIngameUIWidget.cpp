#include "UI/SOTMIngameUIWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "SOTMPlayerBlueprintLibrary.h"
#include "SOTMPlayerStateSubsystem.h"
#include "TimerManager.h"

void USOTMIngameUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureCoinCounter();

	BoundCoinState = USOTMPlayerBlueprintLibrary::GetPlayerStateSubsystem(this);
	if (BoundCoinState)
	{
		BoundCoinState->OnCoinsChanged.RemoveDynamic(this, &USOTMIngameUIWidget::HandleCoinsChanged);
		BoundCoinState->OnCoinsChanged.AddDynamic(this, &USOTMIngameUIWidget::HandleCoinsChanged);
		RefreshCoinCounter(BoundCoinState->GetLifetimeCoinsCollected(), false);
	}
	else
	{
		RefreshCoinCounter(0, false);
	}
}

void USOTMIngameUIWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CoinPulseTimerHandle);
	}

	if (BoundCoinState)
	{
		BoundCoinState->OnCoinsChanged.RemoveDynamic(this, &USOTMIngameUIWidget::HandleCoinsChanged);
	}
	BoundCoinState = nullptr;
	Super::NativeDestruct();
}

void USOTMIngameUIWidget::HandleCoinsChanged(
	const int32 AvailableCoins,
	const int32 LifetimeCoinsCollected)
{
	(void)AvailableCoins;
	RefreshCoinCounter(LifetimeCoinsCollected, true);
}

void USOTMIngameUIWidget::EnsureCoinCounter()
{
	if (CoinCounterText || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		UE_LOG(LogTemp, Error, TEXT("SOTM Coin HUD: WBP_IngameUI root is not a CanvasPanel."));
		return;
	}

	CoinCounterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SOTM_CoinCounterText"));
	CoinCounterText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.18f, 1.0f)));
	CoinCounterText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	CoinCounterText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	CoinCounterText->SetJustification(ETextJustify::Left);
	FSlateFontInfo Font = CoinCounterText->GetFont();
	Font.Size = 22;
	CoinCounterText->SetFont(Font);

	if (UCanvasPanelSlot* CoinCanvasSlot = RootCanvas->AddChildToCanvas(CoinCounterText))
	{
		CoinCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CoinCanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CoinCanvasSlot->SetPosition(FVector2D(36.0f, 36.0f));
		CoinCanvasSlot->SetSize(FVector2D(360.0f, 52.0f));
		CoinCanvasSlot->SetZOrder(100);
	}
}

void USOTMIngameUIWidget::RefreshCoinCounter(
	const int32 LifetimeCoinsCollected,
	const bool bPlayFeedback)
{
	if (!CoinCounterText)
	{
		return;
	}

	const int32 SafeLifetimeCoins = FMath::Max(0, LifetimeCoinsCollected);
	const bool bCountIncreased = DisplayedLifetimeCoins != INDEX_NONE && SafeLifetimeCoins > DisplayedLifetimeCoins;
	DisplayedLifetimeCoins = SafeLifetimeCoins;
	CoinCounterText->SetText(FText::Format(
		NSLOCTEXT("SOTM", "CoinCounterFormat", "COINS COLLECTED   {0}"),
		FText::AsNumber(SafeLifetimeCoins)));

	if (bPlayFeedback && bCountIncreased)
	{
		FWidgetTransform PulseTransform;
		PulseTransform.Scale = FVector2D(1.12f, 1.12f);
		CoinCounterText->SetRenderTransform(PulseTransform);
		CoinCounterText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.45f, 1.0f)));

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CoinPulseTimerHandle);
			World->GetTimerManager().SetTimer(
				CoinPulseTimerHandle,
				this,
				&USOTMIngameUIWidget::FinishCoinPulse,
				0.16f,
				false);
		}
	}
}

void USOTMIngameUIWidget::FinishCoinPulse()
{
	if (CoinCounterText)
	{
		CoinCounterText->SetRenderTransform(FWidgetTransform());
		CoinCounterText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.18f, 1.0f)));
	}
}
