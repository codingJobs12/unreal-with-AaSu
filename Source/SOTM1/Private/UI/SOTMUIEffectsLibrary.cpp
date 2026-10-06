#include "UI/SOTMUIEffectsLibrary.h"

#include "Components/Widget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "UI/SOTMIngameUIWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMUIEffects, Log, All);

namespace SOTMUIEffectsPrivate
{
	struct FUIShakeState
	{
		TWeakObjectPtr<UWidget> Widget;
		FTimerHandle TimerHandle;
		float Elapsed = 0.0f;
		double LastSeconds = -1.0;
		float Duration = 0.22f;
		FVector2D PeakOffset = FVector2D(14.0f, 14.0f);
	};

	// Keeps each in-flight shake alive for its duration, keyed by the widget it is
	// playing on - re-triggering the same widget restarts it instead of stacking timers.
	static TMap<TWeakObjectPtr<UWidget>, TSharedPtr<FUIShakeState>> ActiveShakes;

	void TickShake(const TSharedPtr<FUIShakeState>& State)
	{
		UWidget* Widget = State->Widget.Get();
		if (!Widget)
		{
			ActiveShakes.Remove(State->Widget);
			return;
		}

		// Real elapsed time (frame-rate independent).
		const double NowSeconds = FPlatformTime::Seconds();
		State->Elapsed += State->LastSeconds < 0.0 ? 0.0f : static_cast<float>(FMath::Min(NowSeconds - State->LastSeconds, 0.1));
		State->LastSeconds = NowSeconds;
		const float Alpha = FMath::Clamp(State->Elapsed / State->Duration, 0.0f, 1.0f);

		// Out-and-back: peak displacement (down-right) at the midpoint, eased back to zero by the end.
		const float Curve = FMath::Sin(Alpha * PI);
		const FVector2D CurrentOffset = State->PeakOffset * Curve;

		FWidgetTransform Transform = Widget->GetRenderTransform();
		Transform.Translation = CurrentOffset;
		Widget->SetRenderTransform(Transform);

		if (Alpha >= 1.0f)
		{
			Transform.Translation = FVector2D::ZeroVector;
			Widget->SetRenderTransform(Transform);

			if (const UWorld* World = Widget->GetWorld())
			{
				World->GetTimerManager().ClearTimer(State->TimerHandle);
			}
			ActiveShakes.Remove(State->Widget);
		}
	}
}

void USOTMUIEffectsLibrary::PlayUIUnlockShake(
	UObject* WorldContextObject, UWidget* TargetWidget, const float ShakeOffset, const float ShakeDuration)
{
	using namespace SOTMUIEffectsPrivate;

	if (!TargetWidget)
	{
		return;
	}

	UWorld* World = TargetWidget->GetWorld();
	if (!World && WorldContextObject)
	{
		World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	}
	if (!World)
	{
		return;
	}

	const TWeakObjectPtr<UWidget> WeakWidget(TargetWidget);

	if (const TSharedPtr<FUIShakeState>* Existing = ActiveShakes.Find(WeakWidget))
	{
		World->GetTimerManager().ClearTimer((*Existing)->TimerHandle);
		ActiveShakes.Remove(WeakWidget);
	}

	const TSharedPtr<FUIShakeState> State = MakeShared<FUIShakeState>();
	State->Widget = WeakWidget;
	State->Duration = FMath::Max(ShakeDuration, 0.01f);
	State->PeakOffset = FVector2D(ShakeOffset, ShakeOffset);
	ActiveShakes.Add(WeakWidget, State);

	World->GetTimerManager().SetTimer(
		State->TimerHandle,
		FTimerDelegate::CreateLambda([State]()
		{
			TickShake(State);
		}),
		1.0f / 60.0f,
		true);
}

void USOTMUIEffectsLibrary::ReportIsabelBossHealth(UObject* WorldContextObject, const float CurrentHealth, const float MaxHealth)
{
	UE_LOG(LogSOTMUIEffects, Display, TEXT("ReportIsabelBossHealth: called with Current=%.1f Max=%.1f, WorldContextObject=%s"),
		CurrentHealth, MaxHealth, WorldContextObject ? *WorldContextObject->GetName() : TEXT("(null)"));

	if (!WorldContextObject)
	{
		UE_LOG(LogSOTMUIEffects, Warning, TEXT("ReportIsabelBossHealth: WorldContextObject is null - check the node's World Context pin is connected (usually Self)."));
		return;
	}

	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContextObject, FoundWidgets, USOTMIngameUIWidget::StaticClass(), false);
	UE_LOG(LogSOTMUIEffects, Display, TEXT("ReportIsabelBossHealth: found %d widget(s) of class SOTMIngameUIWidget (or subclass)."), FoundWidgets.Num());

	// Update EVERY matching instance, not just the first - GetAllWidgetsOfClass's order
	// is not guaranteed to put the currently-visible HUD first, so if a stale/leftover
	// instance is still alive in the viewport (e.g. from a level transition that never
	// called RemoveFromParent on the old one), only updating "the first one found" can
	// silently update the wrong, invisible instance while the real one never moves.
	int32 UpdatedCount = 0;
	for (UUserWidget* Widget : FoundWidgets)
	{
		USOTMIngameUIWidget* HUD = Cast<USOTMIngameUIWidget>(Widget);
		if (!HUD)
		{
			continue;
		}

		++UpdatedCount;
		if (CurrentHealth <= 0.0f)
		{
			UE_LOG(LogSOTMUIEffects, Display, TEXT("ReportIsabelBossHealth: health <= 0, clearing boss progress panel on %s."), *HUD->GetName());
			HUD->ClearBossProgress();
		}
		else
		{
			const float Normalized = MaxHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;
			UE_LOG(LogSOTMUIEffects, Display, TEXT("ReportIsabelBossHealth: setting boss progress to %.2f normalized on %s."), Normalized, *HUD->GetName());
			HUD->SetBossProgress(NSLOCTEXT("SOTM", "IsabelBossName", "ISABELLA"), Normalized);
		}
	}

	if (UpdatedCount == 0)
	{
		UE_LOG(LogSOTMUIEffects, Warning, TEXT("ReportIsabelBossHealth: no SOTMIngameUIWidget instance found in viewport - is the in-game HUD actually added to the viewport right now?"));
	}
	else if (FoundWidgets.Num() > 1)
	{
		UE_LOG(LogSOTMUIEffects, Warning, TEXT("ReportIsabelBossHealth: updated %d HUD instances (more than 1 found) - you likely have a stale/leftover HUD widget still in the viewport from an earlier level load/restart that was never removed."), UpdatedCount);
	}
}
