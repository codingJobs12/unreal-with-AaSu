#include "Ability/SOTMLightningThrowWorldSubsystem.h"

#include "AI/SOTMCousinAIController.h"
#include "AI/SOTMCousinCharacter.h"
#include "Ability/SOTMLightningThrowSettings.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "SOTMPlayerStateSubsystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMLightning, Log, All);

namespace SOTMLightningPrivate
{
	const FName ForestMap(TEXT("/Game/MenuSystemPro/ExampleContent/Designs/Design_Silence/Levels/CH1"));
	const TCHAR* ThrowSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_UpgradeSuccess.SFX_TEMP_UpgradeSuccess");
	const TCHAR* DeniedSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_Denied.SFX_TEMP_Denied");
	// Self-contained VFX prefab from the Speedster asset pack: plays P_Sphere_of_Lightning
	// (plus a burst effect and sound) and cleans itself up after its own Duration.
	const TCHAR* LightningBurstActorPath = TEXT("/Game/SuperPowers/Powers/Speedster/Rays/BP_LightningBurst.BP_LightningBurst_C");

	FName NormalizeMapPackageName(const UWorld* World)
	{
		if (!World)
		{
			return NAME_None;
		}
		return FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
	}
}

bool USOTMLightningThrowWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void USOTMLightningThrowWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (SOTMLightningPrivate::NormalizeMapPackageName(&InWorld) != SOTMLightningPrivate::ForestMap)
	{
		return;
	}

	PlayerState = InWorld.GetGameInstance()
		? InWorld.GetGameInstance()->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	if (PlayerState)
	{
		PlayerState->OnLightningThrowOwnershipChanged.RemoveDynamic(this, &ThisClass::HandleOwnershipChanged);
		PlayerState->OnLightningThrowOwnershipChanged.AddDynamic(this, &ThisClass::HandleOwnershipChanged);
	}
	RefreshStateFromOwnership();
	BindProductionInput();
}

void USOTMLightningThrowWorldSubsystem::Deinitialize()
{
	if (PlayerState)
	{
		PlayerState->OnLightningThrowOwnershipChanged.RemoveDynamic(this, &ThisClass::HandleOwnershipChanged);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimer);
		World->GetTimerManager().ClearTimer(CooldownTickTimer);
		World->GetTimerManager().ClearTimer(InitializeTimer);
	}
	UnbindProductionInput();
	PlayerState = nullptr;
	Super::Deinitialize();
}

void USOTMLightningThrowWorldSubsystem::BindProductionInput()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UEnhancedInputComponent* EnhancedInput = PC
		? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!EnhancedInput || !InputSubsystem)
	{
		// PIE can create the pawn before its input component exists; retry rather than
		// silently leaving the ability unbound for the whole session.
		if (World)
		{
			World->GetTimerManager().SetTimer(
				InitializeTimer, this, &ThisClass::BindProductionInput, 0.5f, false);
		}
		return;
	}
	if (BoundEnhancedInput.IsValid())
	{
		return;
	}

	LightningInputAction = NewObject<UInputAction>(this, TEXT("IA_SOTM_LightningThrow"));
	LightningInputAction->ValueType = EInputActionValueType::Boolean;
	LightningInputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_SOTM_LightningThrow"));
	LightningInputContext->MapKey(LightningInputAction, EKeys::F);
	InputSubsystem->AddMappingContext(LightningInputContext, 50);
	FEnhancedInputActionEventBinding& Binding = EnhancedInput->BindAction(
		LightningInputAction, ETriggerEvent::Started, this, &ThisClass::HandleLightningInput);
	LightningBindingHandle = Binding.GetHandle();
	BoundEnhancedInput = EnhancedInput;

	UE_LOG(LogSOTMLightning, Display,
		TEXT("Lightning Throw input bound (F) unlocked=%d"),
		(PlayerState && PlayerState->IsLightningThrowUnlocked()) ? 1 : 0);
}

void USOTMLightningThrowWorldSubsystem::UnbindProductionInput()
{
	if (UEnhancedInputComponent* Input = BoundEnhancedInput.Get(); Input && LightningBindingHandle != 0)
	{
		Input->RemoveBindingByHandle(LightningBindingHandle);
	}
	BoundEnhancedInput.Reset();
	LightningBindingHandle = 0;
}

void USOTMLightningThrowWorldSubsystem::HandleOwnershipChanged(const bool bUnlocked)
{
	(void)bUnlocked;
	RefreshStateFromOwnership();
}

void USOTMLightningThrowWorldSubsystem::RefreshStateFromOwnership()
{
	const bool bUnlocked = PlayerState && PlayerState->IsLightningThrowUnlocked();
	SetRuntimeState(bUnlocked
		? ESOTMLightningThrowRuntimeState::Ready
		: ESOTMLightningThrowRuntimeState::Locked);
}

void USOTMLightningThrowWorldSubsystem::HandleLightningInput()
{
	TryThrowLightning();
}

float USOTMLightningThrowWorldSubsystem::GetCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World || RuntimeState != ESOTMLightningThrowRuntimeState::Cooldown)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(CooldownTimer));
}

void USOTMLightningThrowWorldSubsystem::SetRuntimeState(const ESOTMLightningThrowRuntimeState NewState)
{
	RuntimeState = NewState;
	const float Cooldown = FMath::Max(
		0.1f, GetDefault<USOTMLightningThrowSettings>()->LightningThrowCooldown);
	const float Remaining = GetCooldownRemaining();
	OnLightningThrowStateChanged.Broadcast(
		RuntimeState, Remaining, FMath::Clamp(Remaining / Cooldown, 0.0f, 1.0f));
}

void USOTMLightningThrowWorldSubsystem::FinishCooldown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTickTimer);
	}
	if (RuntimeState == ESOTMLightningThrowRuntimeState::Cooldown)
	{
		RefreshStateFromOwnership();
	}
}

void USOTMLightningThrowWorldSubsystem::TickCooldown()
{
	// Runs every 0.1s while on cooldown so the HUD's progress bar animates smoothly
	// and the countdown text steps down 4, 3, 2, 1 instead of only updating once at
	// the start and once at the end of the 4-second cooldown.
	if (RuntimeState != ESOTMLightningThrowRuntimeState::Cooldown)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CooldownTickTimer);
		}
		return;
	}

	const float Cooldown = FMath::Max(
		0.1f, GetDefault<USOTMLightningThrowSettings>()->LightningThrowCooldown);
	const float Remaining = GetCooldownRemaining();
	OnLightningThrowStateChanged.Broadcast(
		RuntimeState, Remaining, FMath::Clamp(Remaining / Cooldown, 0.0f, 1.0f));

	if (Remaining <= 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CooldownTickTimer);
		}
	}
}

int32 USOTMLightningThrowWorldSubsystem::TryThrowLightning()
{
	UWorld* World = GetWorld();
	if (!World || !PlayerState || !PlayerState->IsLightningThrowUnlocked() ||
		RuntimeState != ESOTMLightningThrowRuntimeState::Ready ||
		PlayerState->IsPlayerDead() || PlayerState->IsGameOver())
	{
		if (World && PlayerState && !PlayerState->IsLightningThrowUnlocked())
		{
			if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMLightningPrivate::DeniedSound))
			{
				UGameplayStatics::PlaySound2D(this, Sound, 0.5f);
			}
		}
		return 0;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return 0;
	}

	const USOTMLightningThrowSettings* Settings = GetDefault<USOTMLightningThrowSettings>();
	const FVector PlayerLocation = Pawn->GetActorLocation();

	// No aiming required: any Cousin within spotting range is a valid target, in any
	// direction around the player (not just in front, not just on-screen). With several
	// in range, only the nearest one is stunned - never more than one per throw.
	ASOTMCousinCharacter* StunnedCousin = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<ASOTMCousinCharacter> It(World); It; ++It)
	{
		const float Distance = FVector::Dist(It->GetActorLocation(), PlayerLocation);
		if (Distance > Settings->LightningThrowRange)
		{
			continue;
		}
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			StunnedCousin = *It;
		}
	}
	const FVector ImpactPoint = StunnedCousin ? StunnedCousin->GetActorLocation() : PlayerLocation;

	int32 StunnedCount = 0;
	if (StunnedCousin)
	{
		if (ASOTMCousinAIController* Controller = Cast<ASOTMCousinAIController>(StunnedCousin->GetController()))
		{
			if (Controller->ApplyLightningStun(Settings->StunDuration))
			{
				++StunnedCount;

				// Play the stun VFX on the cousin, exactly where they were hit.
				if (UClass* BurstClass = LoadClass<AActor>(nullptr, SOTMLightningPrivate::LightningBurstActorPath))
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					if (AActor* Burst = World->SpawnActor<AActor>(
						BurstClass, StunnedCousin->GetActorLocation(), FRotator::ZeroRotator, SpawnParams))
					{
						// Belt-and-suspenders: the prefab is expected to clean itself up via its
						// own Duration, but this guarantees it never outlives the stun regardless.
						Burst->SetLifeSpan(FMath::Max(0.5f, Settings->StunDuration));
					}
				}
				// "Stunning one cousin makes the others angry" - every sibling in range hunts the player.
				const FVector StunLocation = StunnedCousin->GetActorLocation();
				for (TActorIterator<ASOTMCousinCharacter> It(World); It; ++It)
				{
					if (*It == StunnedCousin)
					{
						continue;
					}
					if (FVector::DistSquared(It->GetActorLocation(), StunLocation) >
						FMath::Square(Settings->AggravationRadius))
					{
						continue;
					}
					if (ASOTMCousinAIController* Sibling = Cast<ASOTMCousinAIController>(It->GetController()))
					{
						Sibling->AggravateTowards(Pawn);
					}
				}
			}
		}
	}

	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMLightningPrivate::ThrowSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, ImpactPoint, 0.55f);
	}

	World->GetTimerManager().SetTimer(
		CooldownTimer, this, &ThisClass::FinishCooldown,
		FMath::Max(0.1f, Settings->LightningThrowCooldown), false);
	// Starts only here, i.e. only when the player presses F while the ability is
	// unlocked and Ready (guarded by the early-out above). Repeats every 0.1s so
	// the HUD progress bar and the 4/3/2/1 countdown update continuously instead
	// of jumping straight from full to empty.
	World->GetTimerManager().SetTimer(
		CooldownTickTimer, this, &ThisClass::TickCooldown, 0.1f, true);
	SetRuntimeState(ESOTMLightningThrowRuntimeState::Cooldown);

	if (StunnedCount == 0)
	{
		// No Cousin was within spotting range at all - log the closest one anyway so a
		// bad case (out of range) can be told apart from an actual bug at a glance.
		float NearestAnyDistance = -1.0f;
		for (TActorIterator<ASOTMCousinCharacter> It(World); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), PlayerLocation);
			if (NearestAnyDistance < 0.0f || Distance < NearestAnyDistance)
			{
				NearestAnyDistance = Distance;
			}
		}
		UE_LOG(LogSOTMLightning, Display,
			TEXT("Lightning Throw used: stunned=0 nearestCousinDistance=%.0f (range=%.0f)"),
			NearestAnyDistance, Settings->LightningThrowRange);
	}
	else
	{
		UE_LOG(LogSOTMLightning, Display,
			TEXT("Lightning Throw used: impact=%s stunned=%d"), *ImpactPoint.ToString(), StunnedCount);
	}
	return StunnedCount;
}
