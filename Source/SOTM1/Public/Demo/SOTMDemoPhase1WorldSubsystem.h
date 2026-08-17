#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SOTMDemoPhase1WorldSubsystem.generated.h"

class AActor;
class ALevelSequenceActor;
class APlayerController;
class UGameViewportClient;
class ULevelSequencePlayer;
class USOTMPlayerStateSubsystem;
class UTextBlock;

/**
 * Demo Phase 1 bridge for the production menu and New Game Mansion introduction.
 * It deliberately owns presentation/orchestration only; save, player, coin and AI
 * gameplay logic remain in their existing authoritative systems.
 */
UCLASS()
class SOTM1_API USOTMDemoPhase1WorldSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	static FName GetMapPackageName(const UWorld* World);
	void NormalizeSaveSlotPresentation();
	void TryBeginMansionIntro();
	void BeginMansionIntro(APlayerController* PlayerController, USOTMPlayerStateSubsystem* PlayerState);
	void PresentTimmyOpening();
	void PresentTimmyWarning();
	void PresentIsabelArrival();
	void PresentKnockout();
	void PresentDragging();
	void FinishMansionIntro();
	void TravelToForest();
	void SetSubtitle(const FText& Speaker, const FText& Line);
	void FrameActorWithCinematicCamera(AActor* Subject);
	void CreateSubtitleOverlay();
	void RemoveSubtitleOverlay();
	void StopIsabelGameplayLogic(AActor* IsabelActor) const;
	void CleanupIntro(bool bRestoreCameraFade);

	FTimerHandle SaveSlotNormalizationTimer;
	FTimerHandle IntroStartTimer;
	FTimerHandle TimmyOpeningTimer;
	FTimerHandle TimmyWarningTimer;
	FTimerHandle IsabelArrivalTimer;
	FTimerHandle KnockoutTimer;
	FTimerHandle DraggingTimer;
	FTimerHandle FinishTimer;
	FTimerHandle TravelTimer;

	TWeakObjectPtr<APlayerController> IntroPlayerController;
	TWeakObjectPtr<AActor> TimmyActor;
	TWeakObjectPtr<AActor> IsabelActor;
	TWeakObjectPtr<AActor> CinematicCameraActor;
	TWeakObjectPtr<UGameViewportClient> IntroGameViewport;
	TWeakObjectPtr<USOTMPlayerStateSubsystem> IntroPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> IntroSequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> IntroSequenceActor;

	TSharedPtr<class SWidget> SubtitleViewportRoot;
	TSharedPtr<class STextBlock> SubtitleSpeakerText;
	TSharedPtr<class STextBlock> SubtitleLineText;
	bool bIntroRequested = false;
	bool bCinematicLockHeld = false;
};
