#include "SOTMGameOverWidget.h"

#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "SOTMPlayerStateSubsystem.h"

void USOTMGameOverWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Buttons are built in a WBP subclass's Designer canvas (BindWidgetOptional in the
	// header), so their click handling is wired here instead of hand-built Slate.
	if (RetryButton)
	{
		RetryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
	}
	if (MainMenuButton)
	{
		MainMenuButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMainMenuClicked);
	}
}

void USOTMGameOverWidget::NativeDestruct()
{
	if (RetryButton)
	{
		RetryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRetryClicked);
	}
	if (MainMenuButton)
	{
		MainMenuButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleMainMenuClicked);
	}

	Super::NativeDestruct();
}

void USOTMGameOverWidget::HandleRetryClicked()
{
	// If this never logs when you click the button you expect to be Retry, the click
	// isn't reaching RetryButton's OnClicked at all - most likely RetryButton in this
	// WBP's Designer isn't named EXACTLY "RetryButton" (check the Details panel's
	// Variable Name with "Is Variable" ticked), or another widget is overlapping it.
	UE_LOG(LogTemp, Display, TEXT("SOTM Game Over UI: RetryButton clicked."));
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USOTMPlayerStateSubsystem* State = GameInstance->GetSubsystem<USOTMPlayerStateSubsystem>())
		{
			State->RetryFromGameOver();
		}
	}
}

void USOTMGameOverWidget::HandleMainMenuClicked()
{
	// Same check as HandleRetryClicked above, but for MainMenuButton.
	UE_LOG(LogTemp, Display, TEXT("SOTM Game Over UI: MainMenuButton clicked."));
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USOTMPlayerStateSubsystem* State = GameInstance->GetSubsystem<USOTMPlayerStateSubsystem>())
		{
			State->ReturnToMainMenu();
		}
	}
}
