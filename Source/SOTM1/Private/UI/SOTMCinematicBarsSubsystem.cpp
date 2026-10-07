#include "UI/SOTMCinematicBarsSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

void USOTMCinematicBarsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<USOTMPlayerStateSubsystem>();
	Super::Initialize(Collection);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USOTMPlayerStateSubsystem* PS = GI->GetSubsystem<USOTMPlayerStateSubsystem>())
		{
			BoundPlayerState = PS;
			PS->OnInputLocksChanged.AddDynamic(this, &ThisClass::HandleInputLocksChanged);
			// In case a cinematic lock is already held.
			HandleInputLocksChanged(PS->HasAnyInputLock(), PS->GetActiveInputLockReasons());
		}
	}
}

void USOTMCinematicBarsSubsystem::Deinitialize()
{
	if (USOTMPlayerStateSubsystem* PS = BoundPlayerState.Get())
	{
		PS->OnInputLocksChanged.RemoveDynamic(this, &ThisClass::HandleInputLocksChanged);
	}
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(SweepHandle);
	RemoveWidget();
	Super::Deinitialize();
}

void USOTMCinematicBarsSubsystem::HandleInputLocksChanged(const bool /*bInputLocked*/, TArray<ESOTMInputLockReason> ActiveReasons)
{
	SetBarsVisible(ActiveReasons.Contains(ESOTMInputLockReason::Cinematic));
}

void USOTMCinematicBarsSubsystem::SetBarsVisible(const bool bVisible)
{
	if (bVisible == bWanted)
	{
		return;
	}
	bWanted = bVisible;
	Target = bVisible ? 1.0f : 0.0f;
	if (bVisible)
	{
		AddWidget();
		FillScreenWithCameras();
		// Cameras can be spawned or switched during the sequence: keep sweeping while it lasts.
		if (!SweepHandle.IsValid())
		{
			SweepHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &USOTMCinematicBarsSubsystem::SweepTick), 0.2f);
		}
	}
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &USOTMCinematicBarsSubsystem::Tick));
	}
}

bool USOTMCinematicBarsSubsystem::Tick(const float DeltaTime)
{
	if (!Amount.IsValid())
	{
		TickHandle.Reset();
		return false;
	}
	// Eased approach to the target, in real time: constant speed in "amount per second" then smoothed by the widget.
	const float Seconds = Target > *Amount ? FMath::Max(SlideInSeconds, 0.05f) : FMath::Max(SlideOutSeconds, 0.05f);
	const float Step = FMath::Min(DeltaTime, 0.1f) / Seconds;
	*Amount = Target > *Amount ? FMath::Min(Target, *Amount + Step) : FMath::Max(Target, *Amount - Step);
	if (FMath::IsNearlyEqual(*Amount, Target))
	{
		*Amount = Target;
		if (Target <= 0.0f)
		{
			RemoveWidget();
		}
		TickHandle.Reset();
		return false;
	}
	return true;
}

void USOTMCinematicBarsSubsystem::AddWidget()
{
	if (Root.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	Amount = MakeShared<float>(0.0f);
	TSharedPtr<float> Shared = Amount;
	const float Height = BarHeight;
	auto Bar = [Shared, Height]()
	{
		return SNew(SBox).HeightOverride_Lambda([Shared, Height]()
			{
				const float A = FMath::Clamp(*Shared, 0.0f, 1.0f);
				return FOptionalSize(FMath::InterpEaseInOut(0.0f, 1.0f, A, 2.0f) * Height);
			})
			[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)];
	};
	Root = SNew(SOverlay)
		+ SOverlay::Slot().VAlign(VAlign_Top)[Bar()]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)[Bar()];
	GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 5);
}

void USOTMCinematicBarsSubsystem::RemoveWidget()
{
	if (Root.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef());
	}
	Root.Reset();
	Amount.Reset();
}

void USOTMCinematicBarsSubsystem::FillScreenWithCameras()
{
	const UWorld* GameWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!GameWorld)
	{
		return;
	}
	for (TObjectIterator<UCameraComponent> It; It; ++It)
	{
		UCameraComponent* Camera = *It;
		if (Camera && !Camera->HasAnyFlags(RF_ClassDefaultObject) && Camera->GetWorld() == GameWorld
			&& Camera->bConstrainAspectRatio)
		{
			Camera->bConstrainAspectRatio = false; // no side bars: fill the whole screen
		}
	}
}

bool USOTMCinematicBarsSubsystem::SweepTick(float /*DeltaTime*/)
{
	if (!bWanted)
	{
		SweepHandle.Reset();
		return false;
	}
	FillScreenWithCameras();
	return true;
}
