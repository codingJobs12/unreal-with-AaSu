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
	if (RuntimeState == ESOTMLightningThrowRuntimeState::Cooldown)
	{
		RefreshStateFromOwnership();
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
	FVector ViewLocation = Pawn->GetActorLocation();
	FRotator ViewRotation = Pawn->GetActorRotation();
	if (PC)
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	const FVector ThrowDirection = ViewRotation.Vector();

	// The bolt travels until it meets world geometry; Cousins are caught by proximity to
	// that impact point so a throw does not need pixel-accurate aim at a moving target.
	const FVector TraceEnd = ViewLocation + ThrowDirection * Settings->LightningThrowRange;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(SOTMLightningThrow), false, Pawn);
	FHitResult Hit;
	const bool bHitWorld = World->LineTraceSingleByChannel(
		Hit, ViewLocation, TraceEnd, ECC_Visibility, TraceParams);
	const FVector ImpactPoint = bHitWorld ? Hit.ImpactPoint : TraceEnd;

	ASOTMCousinCharacter* StunnedCousin = nullptr;
	float NearestStunDistanceSq = TNumericLimits<float>::Max();
	for (TActorIterator<ASOTMCousinCharacter> It(World); It; ++It)
	{
		const float DistanceSq = FVector::DistSquared(It->GetActorLocation(), ImpactPoint);
		if (DistanceSq > FMath::Square(Settings->LightningThrowHitRadius))
		{
			continue;
		}
		if (DistanceSq < NearestStunDistanceSq)
		{
			NearestStunDistanceSq = DistanceSq;
			StunnedCousin = *It;
		}
	}

	int32 StunnedCount = 0;
	if (StunnedCousin)
	{
		if (ASOTMCousinAIController* Controller = Cast<ASOTMCousinAIController>(StunnedCousin->GetController()))
		{
			if (Controller->ApplyLightningStun(Settings->StunDuration))
			{
				++StunnedCount;
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
	SetRuntimeState(ESOTMLightningThrowRuntimeState::Cooldown);

	UE_LOG(LogSOTMLightning, Display,
		TEXT("Lightning Throw used: impact=%s stunned=%d"), *ImpactPoint.ToString(), StunnedCount);
	return StunnedCount;
}
