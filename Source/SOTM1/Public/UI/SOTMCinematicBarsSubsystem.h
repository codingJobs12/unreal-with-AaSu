#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SOTMPlayerStateSubsystem.h"
#include "SOTMCinematicBarsSubsystem.generated.h"

class SWidget;

/**
 * Shows black cinematic bars (top and bottom) for EVERY sequence in the game automatically:
 * whenever any system holds the Cinematic input lock the bars slide in, and when the last Cinematic
 * lock is released they slide out. Real-time animation, so it is frame-rate independent.
 */
UCLASS()
class SOTM1_API USOTMCinematicBarsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Bar height in pixels at the top and at the bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic Bars")
	float BarHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic Bars")
	float SlideInSeconds = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic Bars")
	float SlideOutSeconds = 0.6f;

private:
	UFUNCTION()
	void HandleInputLocksChanged(bool bInputLocked, TArray<ESOTMInputLockReason> ActiveReasons);

	void SetBarsVisible(bool bVisible);
	bool Tick(float DeltaTime);
	void AddWidget();
	void RemoveWidget();

	/** Cine/other cameras with a constrained aspect ratio make black bars at the LEFT and RIGHT (pillarbox).
	 *  During any cinematic every camera is switched to fill the whole screen. */
	void FillScreenWithCameras();
	bool SweepTick(float DeltaTime);
	FTSTicker::FDelegateHandle SweepHandle;

	TWeakObjectPtr<USOTMPlayerStateSubsystem> BoundPlayerState;
	TSharedPtr<SWidget> Root;
	TSharedPtr<float> Amount;
	FTSTicker::FDelegateHandle TickHandle;
	float Target = 0.0f;
	bool bWanted = false;
};
