#include "Demo/SOTMDemoPhase3WorldSubsystem.h"

#include "Ability/SOTMPhase3Settings.h"
#include "Ability/SOTMSpeedBoostComponent.h"
#include "Ability/SOTMLightningThrowSettings.h"
#include "Ability/SOTMTimmyUpgradeStation.h"
#include "AI/SOTMCousinAIController.h"
#include "AI/SOTMCousinCharacter.h"
#include "Components/AudioComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Objective/SOTMObjectiveSubsystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "SOTMPlayerBlueprintLibrary.h"
#include "SOTMPlayerStateSubsystem.h"
#include "SOTMPlayerVitalComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/SOTMUpgradeStationWidget.h"
#include "UI/SOTMSkillTreeWidget.h"
#include "UI/SOTMIngameUIWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMPhase3, Log, All);

namespace SOTMPhase3Private
{
	const FName ForestMap(TEXT("/Game/MenuSystemPro/ExampleContent/Designs/Design_Silence/Levels/CH1"));
	const TCHAR* CharacterOnFootContextPath = TEXT("/Game/MenuSystemPro/Blueprints/Input/CharacterOnFoot/IMC_CharacterOnFoot.IMC_CharacterOnFoot");
	const TCHAR* SprintActionPath = TEXT("/Game/MenuSystemPro/Blueprints/Input/CharacterOnFoot/IA_Sprint.IA_Sprint");
	const TCHAR* InteractActionPath = TEXT("/Game/MenuSystemPro/Blueprints/Input/CharacterOnFoot/IA_Interact.IA_Interact");
	const TCHAR* StationOpenSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_StationOpen.SFX_TEMP_StationOpen");
	const TCHAR* DeniedSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_Denied.SFX_TEMP_Denied");
	const TCHAR* UpgradeSuccessSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_UpgradeSuccess.SFX_TEMP_UpgradeSuccess");
	const TCHAR* BoostSound = TEXT("/Game/SuperPowers/Powers/Speedster/SFX/Cue/WindGust_Cue.WindGust_Cue");
	const TCHAR* TimmySpeedBoostUnlockVO = TEXT("/Game/Audio/Dialogue/Chapter1/Temporary/VO_TEMP_Timmy_Upgrade_001.VO_TEMP_Timmy_Upgrade_001");
	const TCHAR* TimmyLightningThrowUnlockVO = TEXT("/Game/Audio/Dialogue/Chapter1/Temporary/VO_TEMP_Timmy_Upgrade_002.VO_TEMP_Timmy_Upgrade_002");

	FName NormalizeMapPackageName(const UWorld* World)
	{
		if (!World)
		{
			return NAME_None;
		}
		return FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
	}
}

bool USOTMDemoPhase3WorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void USOTMDemoPhase3WorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (SOTMPhase3Private::NormalizeMapPackageName(&InWorld) != SOTMPhase3Private::ForestMap)
	{
		return;
	}

	PlayerState = InWorld.GetGameInstance()
		? InWorld.GetGameInstance()->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	if (PlayerState)
	{
		PlayerState->OnSpeedBoostOwnershipChanged.AddUniqueDynamic(this, &ThisClass::HandleOwnershipChanged);
		PlayerState->OnPlayerDeathStarted.AddUniqueDynamic(this, &ThisClass::HandlePlayerDeathStarted);
		PlayerState->OnPlayerRespawned.AddUniqueDynamic(this, &ThisClass::HandlePlayerRespawned);
		PlayerState->OnGameOver.AddUniqueDynamic(this, &ThisClass::HandleGameOver);
	}
	InWorld.GetTimerManager().SetTimer(InitializeTimer, this, &ThisClass::InitializePhase3, 0.9f, false);
}

void USOTMDemoPhase3WorldSubsystem::Deinitialize()
{
	EndUpgradeUnlockDialogue();
	if (ActiveBoostAudio)
	{
		ActiveBoostAudio->Stop();
		ActiveBoostAudio = nullptr;
	}
	CloseUpgradeUI();
	CloseSkillTreeUI();
	RestoreMovementSpeed();
	UnbindProductionInput();
	if (PlayerState)
	{
		PlayerState->OnSpeedBoostOwnershipChanged.RemoveDynamic(this, &ThisClass::HandleOwnershipChanged);
		PlayerState->OnPlayerDeathStarted.RemoveDynamic(this, &ThisClass::HandlePlayerDeathStarted);
		PlayerState->OnPlayerRespawned.RemoveDynamic(this, &ThisClass::HandlePlayerRespawned);
		PlayerState->OnGameOver.RemoveDynamic(this, &ThisClass::HandleGameOver);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	// StationActor is a level-placed actor now (not spawned by this subsystem), so it
	// is not ours to Destroy() - just drop our delegate bindings and the reference.
	if (StationActor)
	{
		StationActor->OnPlayerEntered.RemoveAll(this);
		StationActor->OnPlayerExited.RemoveAll(this);
	}
	StationActor = nullptr;
	PlayerState = nullptr;
	Super::Deinitialize();
}

void USOTMDemoPhase3WorldSubsystem::InitializePhase3()
{
	if (!PlayerState)
	{
		return;
	}
	BindToPlacedTimmyStation();
	BindProductionInput();
	SetRuntimeState(PlayerState->IsSpeedBoostUnlocked()
		? ESOTMSpeedBoostRuntimeState::Ready
		: ESOTMSpeedBoostRuntimeState::Locked);
	UE_LOG(LogSOTMPhase3, Display,
		TEXT("Phase 3 initialized station=%s interact=IA_Interact(E) boost=Q unlocked=%d level=%d available=%d lifetime=%d"),
		*GetNameSafe(StationActor), PlayerState->IsSpeedBoostUnlocked(), PlayerState->GetSpeedBoostLevel(),
		PlayerState->GetAvailableCoins(), PlayerState->GetLifetimeCoinsCollected());

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("SOTMPhase3Persistence")))
	{
		BeginDevelopmentPersistenceCheck();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("SOTMPhase3Acceptance")))
	{
		BeginDevelopmentAcceptanceRoute();
	}
#endif
}

#if !UE_BUILD_SHIPPING
void USOTMDemoPhase3WorldSubsystem::BeginDevelopmentPersistenceCheck()
{
	if (!PlayerState || !GetWorld())
	{
		return;
	}
	const bool bLoaded = PlayerState->LoadPlayerStateFromSlot(TEXT("SOTM_Phase3_Acceptance_Slot1"), false);
	SetRuntimeState(PlayerState->IsSpeedBoostUnlocked()
		? ESOTMSpeedBoostRuntimeState::Ready : ESOTMSpeedBoostRuntimeState::Locked);
	UE_LOG(LogSOTMPhase3, Display, TEXT("[Phase3Persistence] explicit relaunch load success=%d"), bLoaded);
	LogDevelopmentAcceptanceState(TEXT("RELAUNCH_CONTINUE_LOAD"));
	CaptureDevelopmentEvidence(TEXT("14_1920x1080_Continue_READY"));
	FTimerHandle ExitTimer;
	GetWorld()->GetTimerManager().SetTimer(ExitTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->ConsoleCommand(TEXT("quit"), true);
		}
	}), 3.0f, false);
}

void USOTMDemoPhase3WorldSubsystem::BeginDevelopmentAcceptanceRoute()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (!World || !Player || !PlayerState || !StationActor)
	{
		UE_LOG(LogSOTMPhase3, Error, TEXT("[Phase3Acceptance] Cannot start: world/player/state/station unavailable."));
		return;
	}

	// Preview state: incomplete objective and insufficient currency. This is a
	// development-only route; production state changes still use normal APIs.
	PlayerState->SetPhase3ProgressForDebug(120, 120, false, 0);
	const FVector PreviewLocation = StationActor->GetActorLocation() + StationActor->GetActorForwardVector() * 260.0f;
	Player->SetActorLocation(PreviewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Player->SetActorRotation((StationActor->GetActorLocation() - PreviewLocation).Rotation());
	HandleStationEntered(Player);
	LogDevelopmentAcceptanceState(TEXT("PREVIEW_INCOMPLETE_OBJECTIVE"));
	CaptureDevelopmentEvidence(TEXT("01_1280x720_Timmy_Prompt"));

	FTimerHandle LockedTimer;
	World->GetTimerManager().SetTimer(LockedTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		OpenUpgradeUI();
		LogDevelopmentAcceptanceState(TEXT("LOCKED_COLLECT_ALL_FIRST"));
		CaptureDevelopmentEvidence(TEXT("02_Locked_Objective_Requirement"));
	}), 1.0f, false);

	FTimerHandle InsufficientTimer;
	World->GetTimerManager().SetTimer(InsufficientTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		CloseUpgradeUI();
		PlayerState->SetPhase3ProgressForDebug(100, 330, false, 0);
		OpenUpgradeUI();
		const ESOTMSpeedBoostPurchaseResult Rejected = TryPurchaseSpeedBoost();
		UE_LOG(LogSOTMPhase3, Display, TEXT("[Phase3Acceptance] insufficient purchase result=%d expected=%d"),
			static_cast<int32>(Rejected), static_cast<int32>(ESOTMSpeedBoostPurchaseResult::NotEnoughCoins));
		LogDevelopmentAcceptanceState(TEXT("OBJECTIVE_COMPLETE_NOT_ENOUGH_COINS"));
		CaptureDevelopmentEvidence(TEXT("03_Not_Enough_Coins"));
	}), 2.2f, false);

	FTimerHandle ReadyTimer;
	World->GetTimerManager().SetTimer(ReadyTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		CloseUpgradeUI();
		PlayerState->SetPhase3ProgressForDebug(330, 330, false, 0);
		OpenUpgradeUI();
		LogDevelopmentAcceptanceState(TEXT("PURCHASE_READY"));
		CaptureDevelopmentEvidence(TEXT("04_Purchase_Ready"));
	}), 3.4f, false);

	FTimerHandle PurchaseTimer;
	World->GetTimerManager().SetTimer(PurchaseTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		const int32 BeforeAvailable = PlayerState->GetAvailableCoins();
		const int32 BeforeLifetime = PlayerState->GetLifetimeCoinsCollected();
		const ESOTMSpeedBoostPurchaseResult First = TryPurchaseSpeedBoost();
		const ESOTMSpeedBoostPurchaseResult Duplicate = TryPurchaseSpeedBoost();
		UE_LOG(LogSOTMPhase3, Display,
			TEXT("[Phase3Acceptance] purchase first=%d duplicate=%d available %d->%d lifetime %d->%d"),
			static_cast<int32>(First), static_cast<int32>(Duplicate), BeforeAvailable,
			PlayerState->GetAvailableCoins(), BeforeLifetime, PlayerState->GetLifetimeCoinsCollected());
		LogDevelopmentAcceptanceState(TEXT("PURCHASED_OWNED"));
		CaptureDevelopmentEvidence(TEXT("05_Purchased_OWNED"));
	}), 4.6f, false);

	FTimerHandle ActiveStateTimer;
	World->GetTimerManager().SetTimer(ActiveStateTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		CloseUpgradeUI();
		const bool bFirst = TryActivateSpeedBoost();
		const bool bStacked = TryActivateSpeedBoost();
		UE_LOG(LogSOTMPhase3, Display, TEXT("[Phase3Acceptance] activation first=%d stacked=%d expected=1,0"), bFirst, bStacked);
		LogDevelopmentAcceptanceState(TEXT("BOOST_ACTIVE"));
		CaptureDevelopmentEvidence(TEXT("06_SpeedBoost_ACTIVE"));
	}), 5.8f, false);

	FTimerHandle CooldownStateTimer;
	World->GetTimerManager().SetTimer(CooldownStateTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		const bool bReactivated = TryActivateSpeedBoost();
		UE_LOG(LogSOTMPhase3, Display, TEXT("[Phase3Acceptance] cooldown reactivation=%d expected=0"), bReactivated);
		LogDevelopmentAcceptanceState(TEXT("BOOST_COOLDOWN"));
		CaptureDevelopmentEvidence(TEXT("07_SpeedBoost_COOLDOWN"));
	}), 9.2f, false);

	FTimerHandle ReadyAgainTimer;
	World->GetTimerManager().SetTimer(ReadyAgainTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		LogDevelopmentAcceptanceState(TEXT("BOOST_READY_AGAIN"));
		CaptureDevelopmentEvidence(TEXT("08_SpeedBoost_READY"));
		PositionForDevelopmentCousinChase();
	}), 19.7f, false);

	FTimerHandle SlotTimer;
	World->GetTimerManager().SetTimer(SlotTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		// Two explicit test slots prove that ownership and balances are serialized
		// per slot without changing the production menu's selected slot.
		PlayerState->SetPhase3ProgressForDebug(80, 330, true, 1);
		const bool bSlot1Saved = PlayerState->SavePlayerStateToSlot(TEXT("SOTM_Phase3_Acceptance_Slot1"));
		PlayerState->ResetRuntimeStateForNewGame();
		const bool bSlot2Saved = PlayerState->SavePlayerStateToSlot(TEXT("SOTM_Phase3_Acceptance_Slot2"));
		const bool bSlot1Loaded = PlayerState->LoadPlayerStateFromSlot(TEXT("SOTM_Phase3_Acceptance_Slot1"), false);
		const bool bSlot1Unlocked = PlayerState->IsSpeedBoostUnlocked();
		const int32 Slot1Available = PlayerState->GetAvailableCoins();
		const bool bSlot2Loaded = PlayerState->LoadPlayerStateFromSlot(TEXT("SOTM_Phase3_Acceptance_Slot2"), false);
		const bool bSlot2Unlocked = PlayerState->IsSpeedBoostUnlocked();
		const int32 Slot2Available = PlayerState->GetAvailableCoins();
		PlayerState->LoadPlayerStateFromSlot(TEXT("SOTM_Phase3_Acceptance_Slot1"), false);
		ResetRuntimeAfterDeath();
		UE_LOG(LogSOTMPhase3, Display,
			TEXT("[Phase3Acceptance] slots save1=%d save2=%d load1=%d unlocked1=%d available1=%d load2=%d unlocked2=%d available2=%d"),
			bSlot1Saved, bSlot2Saved, bSlot1Loaded, bSlot1Unlocked, Slot1Available,
			bSlot2Loaded, bSlot2Unlocked, Slot2Available);
		LogDevelopmentAcceptanceState(TEXT("SLOT1_RESTORED_READY"));
		CaptureDevelopmentEvidence(TEXT("10_Slot1_Restored_READY"));
	}), 22.2f, false);

	FTimerHandle DeathTimer;
	World->GetTimerManager().SetTimer(DeathTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		AActor* Pawn = Controller ? Controller->GetPawn() : nullptr;
		const bool bActivated = TryActivateSpeedBoost();
		const float Damage = USOTMPlayerBlueprintLibrary::ApplyPlayerDamage(this, Pawn, 10000.0f, nullptr, StationActor);
		UE_LOG(LogSOTMPhase3, Display, TEXT("[Phase3Acceptance] caught-during-boost surrogate activated=%d damage=%.1f"), bActivated, Damage);
		LogDevelopmentAcceptanceState(TEXT("DEATH_DURING_BOOST"));
	}), 23.5f, false);

	FTimerHandle RespawnTimer;
	World->GetTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		LogDevelopmentAcceptanceState(TEXT("POST_RESPAWN_READY"));
		CaptureDevelopmentEvidence(TEXT("12_Post_Respawn_READY"));
	}), 29.5f, false);

	FTimerHandle ExitTimer;
	World->GetTimerManager().SetTimer(ExitTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		LogDevelopmentAcceptanceState(TEXT("FINAL"));
		if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			Controller->ConsoleCommand(TEXT("quit"), true);
		}
	}), 32.0f, false);
}

void USOTMDemoPhase3WorldSubsystem::PositionForDevelopmentCousinChase()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Player = PC ? PC->GetPawn() : nullptr;
	ASOTMCousinCharacter* Cousin = nullptr;
	for (TActorIterator<ASOTMCousinCharacter> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Cousin = *It;
			break;
		}
	}
	if (!Player || !Cousin)
	{
		UE_LOG(LogSOTMPhase3, Warning, TEXT("[Phase3Acceptance] Natural cousin chase evidence unavailable."));
		return;
	}
	const FVector ChaseLocation = Cousin->GetActorLocation() + Cousin->GetActorForwardVector() * 700.0f;
	Player->SetActorLocation(ChaseLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Player->SetActorRotation((Cousin->GetActorLocation() - ChaseLocation).Rotation());
	if (ASOTMCousinAIController* AI = Cast<ASOTMCousinAIController>(Cousin->GetController()))
	{
		if (UAIPerceptionComponent* Perception = AI->GetPerceptionComponent())
		{
			Perception->RequestStimuliListenerUpdate();
		}
	}
	const bool bActivated = TryActivateSpeedBoost();
	UE_LOG(LogSOTMPhase3, Display, TEXT("[Phase3Acceptance] natural cousin chase positioned; boost activated=%d"), bActivated);
	CaptureDevelopmentEvidence(TEXT("09_Natural_Cousin_Chase_Boost"));
}

void USOTMDemoPhase3WorldSubsystem::LogDevelopmentAcceptanceState(const TCHAR* Label) const
{
	const UWorld* World = GetWorld();
	const USOTMObjectiveSubsystem* Objectives = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<USOTMObjectiveSubsystem>() : nullptr;
	const FSOTMObjectiveData Objective = Objectives
		? Objectives->GetCollectAllForestCoinsObjective() : FSOTMObjectiveData();
	const UCharacterMovementComponent* Movement = ResolveMovementComponent();
	UE_LOG(LogSOTMPhase3, Display,
		TEXT("[Phase3Acceptance] %s available=%d lifetime=%d objective=%d/%d objectiveState=%d unlocked=%d level=%d runtime=%d speed=%.1f lives=%d dead=%d gameOver=%d"),
		Label, PlayerState ? PlayerState->GetAvailableCoins() : -1,
		PlayerState ? PlayerState->GetLifetimeCoinsCollected() : -1,
		Objective.CurrentProgress, Objective.RequiredProgress, static_cast<int32>(Objective.State),
		PlayerState && PlayerState->IsSpeedBoostUnlocked(), PlayerState ? PlayerState->GetSpeedBoostLevel() : -1,
		static_cast<int32>(RuntimeState), Movement ? Movement->MaxWalkSpeed : -1.0f,
		PlayerState ? PlayerState->GetCurrentLives() : -1, PlayerState && PlayerState->IsPlayerDead(),
		PlayerState && PlayerState->IsGameOver());
}

void USOTMDemoPhase3WorldSubsystem::CaptureDevelopmentEvidence(const FString& Label) const
{
	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (const UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->GetViewportSize(ViewportSize);
		}
	}
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(),
		TEXT("Screenshots/WindowsEditor/Phase3"), Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogSOTMPhase3, Display, TEXT("[Phase3Acceptance] screenshot viewport=%.0fx%.0f path=%s"),
		ViewportSize.X, ViewportSize.Y, *Path);
}
#endif

void USOTMDemoPhase3WorldSubsystem::BindToPlacedTimmyStation()
{
	UWorld* World = GetWorld();
	if (!World || StationActor)
	{
		return;
	}

	// The station is now a Blueprint-placeable actor (BP subclass of
	// ASOTMTimmyUpgradeStation) that a level designer drags directly into the level at
	// Timmy's location, instead of being found-and-spawned by C++ at runtime. Find the
	// instance placed in this level and bind to it.
	ASOTMTimmyUpgradeStation* PlacedStation = nullptr;
	for (TActorIterator<ASOTMTimmyUpgradeStation> It(World); It; ++It)
	{
		PlacedStation = *It;
		break;
	}
	if (!PlacedStation)
	{
		UE_LOG(LogSOTMPhase3, Error,
			TEXT("No ASOTMTimmyUpgradeStation (e.g. BP_TimmyUpgradeStation) is placed in this level; station prompt/UI will not work."));
		return;
	}

	StationActor = PlacedStation;
	StationActor->OnPlayerEntered.AddUObject(this, &ThisClass::HandleStationEntered);
	StationActor->OnPlayerExited.AddUObject(this, &ThisClass::HandleStationExited);
	// This actor's BeginPlay() already ran as part of normal level startup, well before
	// this binding happens (~0.9s into InitializePhase3) - so if the player is already
	// standing inside its interaction radius, the real begin-overlap event already fired
	// with nobody listening. Catch that case explicitly now that we are listening.
	StationActor->NotifyBoundListenersOfExistingOverlaps();

	UE_LOG(LogSOTMPhase3, Display, TEXT("Bound to placed Timmy station %s at %s"),
		*StationActor->GetName(), *StationActor->GetActorLocation().ToCompactString());
}

void USOTMDemoPhase3WorldSubsystem::BindProductionInput()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UEnhancedInputComponent* EnhancedInput = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!EnhancedInput || !InputSubsystem)
	{
		if (World)
		{
			World->GetTimerManager().SetTimer(InitializeTimer, this, &ThisClass::BindProductionInput, 0.5f, false);
		}
		return;
	}
	if (BoundEnhancedInput.IsValid())
	{
		return;
	}

	InteractInputAction = LoadObject<UInputAction>(nullptr, SOTMPhase3Private::InteractActionPath);
	if (InteractInputAction)
	{
		FEnhancedInputActionEventBinding& Binding = EnhancedInput->BindAction(
			InteractInputAction, ETriggerEvent::Started, this, &ThisClass::HandleInteractInput);
		InteractBindingHandle = Binding.GetHandle();
	}

	SpeedBoostInputAction = NewObject<UInputAction>(this, TEXT("IA_SOTM_SpeedBoost"));
	SpeedBoostInputAction->ValueType = EInputActionValueType::Boolean;
	SkillTreeInputAction = NewObject<UInputAction>(this, TEXT("IA_SOTM_SkillTree"));
	SkillTreeInputAction->ValueType = EInputActionValueType::Boolean;
	Phase3InputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_SOTM_Phase3"));
	Phase3InputContext->MapKey(SpeedBoostInputAction, EKeys::Q);
	Phase3InputContext->MapKey(SkillTreeInputAction, EKeys::T);
	InputSubsystem->AddMappingContext(Phase3InputContext, 50);
	if (GetDefault<USOTMPhase3Settings>()->bDisableShiftTestSprint)
	{
		BlockShiftTestSprint(InputSubsystem);
	}
	FEnhancedInputActionEventBinding& BoostBinding = EnhancedInput->BindAction(
		SpeedBoostInputAction, ETriggerEvent::Started, this, &ThisClass::HandleSpeedBoostInput);
	SpeedBoostBindingHandle = BoostBinding.GetHandle();
	FEnhancedInputActionEventBinding& SkillTreeBinding = EnhancedInput->BindAction(
		SkillTreeInputAction, ETriggerEvent::Started, this, &ThisClass::HandleSkillTreeInput);
	SkillTreeBindingHandle = SkillTreeBinding.GetHandle();
	BoundEnhancedInput = EnhancedInput;
}

// Disables the Shift test sprint without touching the character Blueprint or the input
// assets: every key IMC_CharacterOnFoot maps to IA_Sprint is also mapped, in a much
// higher-priority context, to an empty action that consumes the input. Enhanced Input then
// never delivers those keys to IA_Sprint, so the Blueprint's sprint (12000 speed + stamina
// drain) simply never fires. Removing this context turns Shift back on.
void USOTMDemoPhase3WorldSubsystem::BlockShiftTestSprint(UEnhancedInputLocalPlayerSubsystem* InputSubsystem)
{
	if (!InputSubsystem || SprintBlockContext)
	{
		return;
	}

	TArray<FKey> SprintKeys;
	const UInputMappingContext* OnFootContext = LoadObject<UInputMappingContext>(nullptr, SOTMPhase3Private::CharacterOnFootContextPath);
	const UInputAction* SprintAction = LoadObject<UInputAction>(nullptr, SOTMPhase3Private::SprintActionPath);
	if (OnFootContext && SprintAction)
	{
		for (const FEnhancedActionKeyMapping& Mapping : OnFootContext->GetMappings())
		{
			if (Mapping.Action == SprintAction)
			{
				SprintKeys.AddUnique(Mapping.Key);
			}
		}
	}
	if (SprintKeys.IsEmpty())
	{
		SprintKeys.Add(EKeys::LeftShift); // fallback if the assets could not be read
	}

	SprintBlockAction = NewObject<UInputAction>(this, TEXT("IA_SOTM_BlockShiftSprint"));
	SprintBlockAction->ValueType = EInputActionValueType::Boolean;
	SprintBlockAction->bConsumeInput = true;
	SprintBlockContext = NewObject<UInputMappingContext>(this, TEXT("IMC_SOTM_BlockShiftSprint"));
	FString KeyNames;
	for (const FKey& Key : SprintKeys)
	{
		SprintBlockContext->MapKey(SprintBlockAction, Key);
		KeyNames += Key.ToString() + TEXT(" ");
	}
	InputSubsystem->AddMappingContext(SprintBlockContext, 1000);
	UE_LOG(LogSOTMPhase3, Display, TEXT("Shift test sprint disabled (blocked keys: %s)"), *KeyNames);
}

void USOTMDemoPhase3WorldSubsystem::UnbindProductionInput()
{
	if (UEnhancedInputComponent* Input = BoundEnhancedInput.Get())
	{
		if (InteractBindingHandle != 0)
		{
			Input->RemoveBindingByHandle(InteractBindingHandle);
		}
		if (SpeedBoostBindingHandle != 0)
		{
			Input->RemoveBindingByHandle(SpeedBoostBindingHandle);
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
					LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					if (Phase3InputContext)
					{
						InputSubsystem->RemoveMappingContext(Phase3InputContext);
					}
					if (SprintBlockContext)
					{
						InputSubsystem->RemoveMappingContext(SprintBlockContext);
					}
				}
			}
		}
	}
	BoundEnhancedInput.Reset();
	SprintBlockContext = nullptr;
	SprintBlockAction = nullptr;
	InteractBindingHandle = 0;
	SpeedBoostBindingHandle = 0;
}

void USOTMDemoPhase3WorldSubsystem::HandleInteractInput()
{
	if (bPlayerInStationRange && !UpgradeWidget)
	{
		OpenUpgradeUI();
	}
}

void USOTMDemoPhase3WorldSubsystem::HandleSpeedBoostInput()
{
	TryActivateSpeedBoost();
}

void USOTMDemoPhase3WorldSubsystem::HandleSkillTreeInput()
{
	UE_LOG(LogSOTMPhase3, Display, TEXT("T pressed: SkillTreeOpen=%d PlayerState=%s Dead=%d GameOver=%d"),
		IsSkillTreeUIOpen(), *GetNameSafe(PlayerState),
		PlayerState ? PlayerState->IsPlayerDead() : -1, PlayerState ? PlayerState->IsGameOver() : -1);
	if (IsSkillTreeUIOpen())
	{
		CloseSkillTreeUI();
	}
	else
	{
		OpenSkillTreeUI();
	}
}

// StationActor->OnPlayerEntered -> here. ASOTMTimmyUpgradeStation already filters for
// the player (BP_MenuSystemCharacter0) before broadcasting, so this is just: overlap
// started -> show the prompt panel. Nothing else.
void USOTMDemoPhase3WorldSubsystem::HandleStationEntered(AActor* Actor)
{
	(void)Actor;
	bPlayerInStationRange = true;
	OnStationPromptChanged.Broadcast(true); // -> SOTMIngameUIWidget::HandleStationPromptChanged shows StationPromptPanel
}

// StationActor->OnPlayerExited -> here. Overlap ended -> hide the prompt panel. Nothing else.
void USOTMDemoPhase3WorldSubsystem::HandleStationExited(AActor* Actor)
{
	(void)Actor;
	bPlayerInStationRange = false;
	OnStationPromptChanged.Broadcast(false); // -> SOTMIngameUIWidget::HandleStationPromptChanged hides StationPromptPanel
}

void USOTMDemoPhase3WorldSubsystem::OpenUpgradeUI()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!bPlayerInStationRange || UpgradeWidget || !PC || !PlayerState ||
		PlayerState->IsPlayerDead() || PlayerState->IsGameOver())
	{
		return;
	}

	// UI is now built in the Designer (WBP_UpgradeStation, a Blueprint subclass of
	// USOTMUpgradeStationWidget) instead of the old hand-built Slate widget, so load
	// that class instead of using the raw C++ StaticClass() - same pattern as
	// USOTMDemoPhase2WorldSubsystem::EnsureProductionHUD loading WBP_InGameMain.
	UClass* UpgradeWidgetClass = LoadClass<USOTMUpgradeStationWidget>(nullptr,
		TEXT("/Game/UI/Horror/WBP_UpgradeStation.WBP_UpgradeStation_C"));
	if (!UpgradeWidgetClass)
	{
		// USOTMUpgradeStationWidget is Abstract now (visuals live only in the WBP
		// subclass), so there is no usable fallback - just bail out loudly.
		UE_LOG(LogSOTMPhase3, Error, TEXT("Could not load WBP_UpgradeStation; the upgrade station UI will not open."));
		return;
	}
	UpgradeWidget = CreateWidget<USOTMUpgradeStationWidget>(PC, UpgradeWidgetClass);
	if (!UpgradeWidget)
	{
		return;
	}
	UpgradeWidget->AddToViewport(600);
	bPreviousMouseCursor = PC->bShowMouseCursor;
	PC->bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(UpgradeWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PlayerState->AcquireInputLock(ESOTMInputLockReason::Custom);
	bUpgradeInputLockHeld = true;
	OnStationPromptChanged.Broadcast(false);
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase3Private::StationOpenSound))
	{
		UGameplayStatics::PlaySound2D(this, Sound, 0.50f);
	}
	UE_LOG(LogSOTMPhase3, Display, TEXT("Timmy Upgrade Station UI opened."));
}

void USOTMDemoPhase3WorldSubsystem::CloseUpgradeUI()
{
	// Only restore the PC's pre-upgrade-UI cursor/input mode if the upgrade UI was
	// actually open. CloseUpgradeUI() is also called defensively (Deinitialize, various
	// dev-acceptance timers, and via ResetRuntimeAfterDeath on every death/Game Over)
	// regardless of whether the upgrade station was ever open - doing this reset
	// unconditionally was clobbering whatever cursor/input mode another UI (e.g. the
	// Game Over screen, opened moments earlier in the same call chain) had just set,
	// which is why the mouse cursor never stayed visible on Game Over.
	const bool bWasOpen = UpgradeWidget != nullptr;
	if (UpgradeWidget)
	{
		UpgradeWidget->RemoveFromParent();
		UpgradeWidget = nullptr;
	}
	if (PlayerState && bUpgradeInputLockHeld)
	{
		PlayerState->ReleaseInputLock(ESOTMInputLockReason::Custom);
	}
	bUpgradeInputLockHeld = false;
	if (bWasOpen)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->bShowMouseCursor = bPreviousMouseCursor;
				FInputModeGameOnly InputMode;
				PC->SetInputMode(InputMode);
			}
		}
	}
	if (bPlayerInStationRange)
	{
		OnStationPromptChanged.Broadcast(true);
	}
}

void USOTMDemoPhase3WorldSubsystem::OpenSkillTreeUI()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (SkillTreeWidget || !PC || !PlayerState || PlayerState->IsPlayerDead() || PlayerState->IsGameOver())
	{
		UE_LOG(LogSOTMPhase3, Warning,
			TEXT("OpenSkillTreeUI blocked: WidgetAlreadyOpen=%d PC=%s PlayerState=%s"),
			SkillTreeWidget != nullptr, *GetNameSafe(PC), *GetNameSafe(PlayerState));
		return;
	}

	TSubclassOf<USOTMSkillTreeWidget> SkillTreeWidgetClass = GetDefault<USOTMPhase3Settings>()->SkillTreeWidgetClass;
	if (!SkillTreeWidgetClass)
	{
		SkillTreeWidgetClass = USOTMSkillTreeWidget::StaticClass();
	}
	SkillTreeWidget = CreateWidget<USOTMSkillTreeWidget>(PC, SkillTreeWidgetClass);
	if (!SkillTreeWidget)
	{
		UE_LOG(LogSOTMPhase3, Error, TEXT("OpenSkillTreeUI: CreateWidget<USOTMSkillTreeWidget> failed."));
		return;
	}
	SkillTreeWidget->AddToViewport(50000);
	bPreviousMouseCursorSkillTree = PC->bShowMouseCursor;
	PC->bShowMouseCursor = true;
	// UIOnly (not GameAndUI): the skill tree is a full modal screen, and GameAndUI lets
	// gameplay input (e.g. mouse-wheel weapon switching) fire at the same time as the UI,
	// which was leaking scroll input from this panel straight into weapon cycling.
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(SkillTreeWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PlayerState->AcquireInputLock(ESOTMInputLockReason::Custom);
	bSkillTreeInputLockHeld = true;
	UE_LOG(LogSOTMPhase3, Display, TEXT("Skill tree UI opened."));
}

void USOTMDemoPhase3WorldSubsystem::CloseSkillTreeUI()
{
	// Same reasoning as CloseUpgradeUI() above: only reset the PC's cursor/input mode
	// if the skill tree was actually open, so a defensive/unrelated call to this
	// function never clobbers another UI's cursor/input mode state.
	const bool bWasOpen = SkillTreeWidget != nullptr;
	if (SkillTreeWidget)
	{
		SkillTreeWidget->RemoveFromParent();
		SkillTreeWidget = nullptr;
	}
	if (PlayerState && bSkillTreeInputLockHeld)
	{
		PlayerState->ReleaseInputLock(ESOTMInputLockReason::Custom);
	}
	bSkillTreeInputLockHeld = false;
	if (bWasOpen)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->bShowMouseCursor = bPreviousMouseCursorSkillTree;
				FInputModeGameOnly InputMode;
				PC->SetInputMode(InputMode);
			}
		}
	}
}

ESOTMSpeedBoostPurchaseResult USOTMDemoPhase3WorldSubsystem::TryPurchaseSpeedBoost()
{
	if (!PlayerState)
	{
		return ESOTMSpeedBoostPurchaseResult::NoActiveSave;
	}
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	USOTMObjectiveSubsystem* Objectives = GameInstance
		? GameInstance->GetSubsystem<USOTMObjectiveSubsystem>() : nullptr;
	const bool bObjectiveComplete = Objectives &&
		Objectives->GetCollectAllForestCoinsObjective().State == ESOTMObjectiveState::Completed;
	const ESOTMSpeedBoostPurchaseResult Result = PlayerState->TryPurchaseSpeedBoost(
		GetDefault<USOTMPhase3Settings>()->SpeedBoostUnlockCost,
		bObjectiveComplete);
	if (Result == ESOTMSpeedBoostPurchaseResult::Success)
	{
		SetRuntimeState(ESOTMSpeedBoostRuntimeState::Ready);
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase3Private::UpgradeSuccessSound))
		{
			UGameplayStatics::PlaySound2D(this, Sound, 0.62f);
		}
		BeginUpgradeUnlockDialogue(
			SOTMPhase3Private::TimmySpeedBoostUnlockVO,
			NSLOCTEXT("SOTM", "TimmyName", "TIMMY"),
			NSLOCTEXT("SOTM", "TimmySpeedBoostUnlocked",
				"Okay… I’ve restored your speed boost. But use it wisely. You get tired after a while."));
	}
	else if (Result != ESOTMSpeedBoostPurchaseResult::AlreadyOwned)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase3Private::DeniedSound))
		{
			UGameplayStatics::PlaySound2D(this, Sound, 0.48f);
		}
	}
	UE_LOG(LogSOTMPhase3, Display, TEXT("Speed Boost purchase result=%d"), static_cast<int32>(Result));
	return Result;
}

void USOTMDemoPhase3WorldSubsystem::BeginUpgradeUnlockDialogue(
	const TCHAR* VOPath, const FText& Speaker, const FText& Line)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Re-triggering while a previous line is still showing just restarts it clean
	// instead of stacking overlays/locks.
	EndUpgradeUnlockDialogue();

	PendingDialogueVOPath = VOPath;
	PendingDialogueSpeaker = Speaker;
	PendingDialogueLine = Line;

	HideGameplayUIForDialogue();

	// Small beat so the HUD has visibly cleared before the line starts, rather
	// than the subtitle popping in on the exact same frame as the HUD vanishing.
	World->GetTimerManager().SetTimer(
		UpgradeDialoguePreDelayTimer, this, &ThisClass::PlayPendingUpgradeDialogueLine, 0.3f, false);
}

void USOTMDemoPhase3WorldSubsystem::HideGameplayUIForDialogue()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	// Hide every gameplay HUD instance currently in the viewport (there can be more
	// than one live at once - see ReportIsabelBossHealth's note on that), remembering
	// each one's actual visibility so it comes back exactly as it was.
	HiddenGameplayUIWidgets.Reset();
	TArray<UUserWidget*> FoundWidgets;
	// Cast the net over every UMG widget in the viewport, not just the known HUD
	// class - the project has more than one HUD Blueprint alive at once (see
	// ReportIsabelBossHealth's note on that), plus separate ability-icon / crosshair
	// / minimap widgets that are not USOTMIngameUIWidget subclasses at all. The
	// subtitle itself is raw Slate added straight to the viewport (not a UUserWidget),
	// so sweeping every UUserWidget here cannot accidentally hide it.
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundWidgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : FoundWidgets)
	{
		if (!Widget)
		{
			continue;
		}
		const ESlateVisibility CurrentVisibility = Widget->GetVisibility();
		if (CurrentVisibility == ESlateVisibility::Collapsed || CurrentVisibility == ESlateVisibility::Hidden)
		{
			continue;
		}
		HiddenGameplayUIWidgets.Add(Widget, CurrentVisibility);
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}

	bPreviousMouseCursorUpgradeDialogue = PC->bShowMouseCursor;
	PC->bShowMouseCursor = false;
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	if (PlayerState)
	{
		// Cinematic (not Custom) - USOTMIngameUIWidget::HandleInputLocksChanged and
		// USOTMPlayerHealthBarWidget::HandleInputLocksChanged both already auto-collapse
		// themselves whenever this reason is active (same as the Mansion intro cutscene),
		// which is a far more reliable way to hide the HUD than fighting their own
		// per-tick RefreshPresentationVisibility() with an external SetVisibility call.
		PlayerState->AcquireInputLock(ESOTMInputLockReason::Cinematic);
		bUpgradeDialogueInputLockHeld = true;
	}
}

void USOTMDemoPhase3WorldSubsystem::PlayPendingUpgradeDialogueLine()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		EndUpgradeUnlockDialogue();
		return;
	}

	TSharedRef<SWidget> Content =
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
		.Padding(FMargin(80.0f, 40.0f, 80.0f, 160.0f))
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.01f, 0.01f, 0.015f, 0.82f))
			.Padding(FMargin(28.0f, 16.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SAssignNew(UpgradeDialogueSpeakerText, STextBlock)
					.Text(PendingDialogueSpeaker)
					.ColorAndOpacity(FLinearColor(0.72f, 0.16f, 0.88f, 1.0f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SAssignNew(UpgradeDialogueLineText, STextBlock)
					.Text(FText::GetEmpty())
					.ColorAndOpacity(FLinearColor::White)
					.WrapTextAt(900.0f)
					.Justification(ETextJustify::Center)
				]
			]
		];

	UpgradeDialogueSubtitleRoot = Content;
	Viewport->AddViewportWidgetContent(UpgradeDialogueSubtitleRoot.ToSharedRef(), 950);

	// Subtitle is paced off the VO line's own audio duration (same pattern as
	// USOTMDemoPhase1WorldSubsystem::PlayTemporaryDialogue) - falls back to a fixed
	// reading-time estimate only if the VO asset is missing, so it's never stuck up forever.
	float VODuration = 0.0f;
	if (USoundBase* VO = LoadObject<USoundBase>(nullptr, PendingDialogueVOPath))
	{
		VODuration = VO->GetDuration();
		UpgradeDialogueAudio = UGameplayStatics::SpawnSound2D(this, VO, 1.0f, 1.0f, 0.0f, nullptr, false, true);
	}
	if (UpgradeDialogueAudio)
	{
		UpgradeDialogueAudio->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleUpgradeDialogueAudioFinished);
		UE_LOG(LogSOTMPhase3, Display, TEXT("Upgrade unlock VO: playing %s duration=%.2fs"),
			PendingDialogueVOPath, VODuration);
		// Safety net in case OnAudioFinished never fires (e.g. audio device issue).
		World->GetTimerManager().SetTimer(
			UpgradeDialogueTimeoutTimer, this, &ThisClass::EndUpgradeUnlockDialogue, VODuration + 1.0f, false);
		StartUpgradeDialogueTypewriter(VODuration);
		return;
	}

	UE_LOG(LogSOTMPhase3, Warning, TEXT("Upgrade unlock VO unavailable (%s) - falling back to timed subtitle."),
		PendingDialogueVOPath);
	World->GetTimerManager().SetTimer(
		UpgradeDialogueTimeoutTimer, this, &ThisClass::EndUpgradeUnlockDialogue, 4.5f, false);
	StartUpgradeDialogueTypewriter(4.0f);
}

void USOTMDemoPhase3WorldSubsystem::StartUpgradeDialogueTypewriter(const float TargetDuration)
{
	UWorld* World = GetWorld();
	if (!World || !UpgradeDialogueLineText.IsValid())
	{
		return;
	}

	UpgradeDialogueFullLine = PendingDialogueLine.ToString();
	UpgradeDialogueRevealedChars = 0;
	UpgradeDialogueLineText->SetText(FText::GetEmpty());

	if (UpgradeDialogueFullLine.IsEmpty())
	{
		return;
	}

	// Pace the reveal so the full line finishes roughly alongside the VO (leaving a
	// little breathing room at the end), falling back to a snappy fixed cadence if the
	// line is long enough that pacing it to a very short clip would look instant.
	const int32 CharCount = UpgradeDialogueFullLine.Len();
	const float PacedInterval = TargetDuration > 0.0f ? (TargetDuration * 0.85f) / FMath::Max(CharCount, 1) : 0.045f;
	UpgradeDialogueTypewriterInterval = FMath::Clamp(PacedInterval, 0.015f, 0.06f);

	World->GetTimerManager().SetTimer(
		UpgradeDialogueTypewriterTimer, this, &ThisClass::TickUpgradeDialogueTypewriter,
		UpgradeDialogueTypewriterInterval, true);
}

void USOTMDemoPhase3WorldSubsystem::TickUpgradeDialogueTypewriter()
{
	if (!UpgradeDialogueLineText.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(UpgradeDialogueTypewriterTimer);
		}
		return;
	}

	++UpgradeDialogueRevealedChars;
	UpgradeDialogueLineText->SetText(FText::FromString(UpgradeDialogueFullLine.Left(UpgradeDialogueRevealedChars)));

	if (UpgradeDialogueRevealedChars >= UpgradeDialogueFullLine.Len())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(UpgradeDialogueTypewriterTimer);
		}
	}
}

void USOTMDemoPhase3WorldSubsystem::HandleUpgradeDialogueAudioFinished()
{
	if (UpgradeDialogueAudio)
	{
		UpgradeDialogueAudio->OnAudioFinished.RemoveAll(this);
	}
	UpgradeDialogueAudio = nullptr;
	EndUpgradeUnlockDialogue();
}

void USOTMDemoPhase3WorldSubsystem::EndUpgradeUnlockDialogue()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(UpgradeDialoguePreDelayTimer);
		World->GetTimerManager().ClearTimer(UpgradeDialogueTimeoutTimer);
		World->GetTimerManager().ClearTimer(UpgradeDialogueTypewriterTimer);
	}
	UpgradeDialogueFullLine.Empty();
	UpgradeDialogueRevealedChars = 0;

	if (UpgradeDialogueAudio)
	{
		UpgradeDialogueAudio->OnAudioFinished.RemoveAll(this);
		UpgradeDialogueAudio->Stop();
		UpgradeDialogueAudio = nullptr;
	}

	if (UpgradeDialogueSubtitleRoot.IsValid())
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(UpgradeDialogueSubtitleRoot.ToSharedRef());
		}
	}
	UpgradeDialogueSubtitleRoot.Reset();
	UpgradeDialogueSpeakerText.Reset();
	UpgradeDialogueLineText.Reset();

	// Restore every gameplay HUD instance to exactly the visibility it had before.
	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Pair : HiddenGameplayUIWidgets)
	{
		if (UUserWidget* Widget = Pair.Key.Get())
		{
			Widget->SetVisibility(Pair.Value);
		}
	}
	HiddenGameplayUIWidgets.Reset();

	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC)
	{
		// Every caller that can trigger this dialogue (HandleUnlockClicked /
		// HandleLightningUnlockClicked on the Upgrade Station widget, and the matching
		// flow on the Skill Tree widget) fires while that shop/skill-tree panel is
		// still open - so hand control straight back to it: cursor visible, GameAndUI,
		// and explicitly focused on its root widget. FInputModeGameAndUI defaults
		// bHideCursorDuringCapture to true, and without an explicit focus target Slate
		// can leave the software cursor effectively invisible even with bShowMouseCursor
		// true - both are why just restoring the bool and GameAndUI wasn't enough.
		UUserWidget* FocusWidget = nullptr;
		if (UpgradeWidget) { FocusWidget = UpgradeWidget; }
		else if (SkillTreeWidget) { FocusWidget = SkillTreeWidget; }

		if (FocusWidget)
		{
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI RestoredInputMode;
			RestoredInputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
			RestoredInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			RestoredInputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(RestoredInputMode);
		}
		else
		{
			// Defensive fallback if this dialogue is ever triggered outside a shop UI
			// context in the future - restore exactly whatever cursor state existed
			// before the dialogue began.
			PC->bShowMouseCursor = bPreviousMouseCursorUpgradeDialogue;
			if (bPreviousMouseCursorUpgradeDialogue)
			{
				FInputModeGameAndUI RestoredInputMode;
				RestoredInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				RestoredInputMode.SetHideCursorDuringCapture(false);
				PC->SetInputMode(RestoredInputMode);
			}
			else
			{
				FInputModeGameOnly InputMode;
				PC->SetInputMode(InputMode);
			}
		}
	}
	if (PlayerState && bUpgradeDialogueInputLockHeld)
	{
		PlayerState->ReleaseInputLock(ESOTMInputLockReason::Cinematic);
	}
	bUpgradeDialogueInputLockHeld = false;
}

ESOTMLightningThrowPurchaseResult USOTMDemoPhase3WorldSubsystem::TryPurchaseLightningThrow()
{
	if (!PlayerState)
	{
		return ESOTMLightningThrowPurchaseResult::NoActiveSave;
	}
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	USOTMObjectiveSubsystem* Objectives = GameInstance
		? GameInstance->GetSubsystem<USOTMObjectiveSubsystem>() : nullptr;
	const bool bObjectiveComplete = Objectives &&
		Objectives->GetCollectAllForestCoinsObjective().State == ESOTMObjectiveState::Completed;
	const ESOTMLightningThrowPurchaseResult Result = PlayerState->TryPurchaseLightningThrow(
		GetDefault<USOTMLightningThrowSettings>()->LightningThrowUnlockCost,
		bObjectiveComplete);
	if (Result == ESOTMLightningThrowPurchaseResult::Success)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase3Private::UpgradeSuccessSound))
		{
			UGameplayStatics::PlaySound2D(this, Sound, 0.62f);
		}
		BeginUpgradeUnlockDialogue(
			SOTMPhase3Private::TimmyLightningThrowUnlockVO,
			NSLOCTEXT("SOTM", "TimmyName", "TIMMY"),
			NSLOCTEXT("SOTM", "TimmyLightningThrowUnlocked", "Run when they are close, or you die."));
	}
	else if (Result != ESOTMLightningThrowPurchaseResult::AlreadyOwned)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase3Private::DeniedSound))
		{
			UGameplayStatics::PlaySound2D(this, Sound, 0.48f);
		}
	}
	UE_LOG(LogSOTMPhase3, Display, TEXT("Lightning Throw purchase result=%d"), static_cast<int32>(Result));
	return Result;
}

bool USOTMDemoPhase3WorldSubsystem::TryActivateSpeedBoost()
{
	if (!PlayerState || RuntimeState != ESOTMSpeedBoostRuntimeState::Ready ||
		!PlayerState->IsSpeedBoostUnlocked() || PlayerState->IsPlayerDead() ||
		PlayerState->IsGameOver() || PlayerState->HasAnyInputLock())
	{
		return false;
	}
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	USOTMSpeedBoostComponent* Boost = USOTMSpeedBoostComponent::FindOrAddTo(PC ? PC->GetPawn() : nullptr);
	const USOTMPhase3Settings* Settings = GetDefault<USOTMPhase3Settings>();
	// Duration is the skill tree's effective value (base + any Level 2/3 upgrades bought),
	// not the raw settings default - this is what actually makes buying the upgrade matter.
	const float Duration = PlayerState->GetEffectiveSpeedBoostDuration();
	if (!World || !Boost || !Boost->StartBoost(Settings->SpeedBoostMultiplier))
	{
		return false;
	}
	ActiveBoostComponent = Boost;
	World->GetTimerManager().SetTimer(ActiveTimer, this, &ThisClass::FinishActiveSpeedBoost,
		Duration, false);
	World->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::UpdateRuntimePresentation,
		0.1f, true);
	SetRuntimeState(ESOTMSpeedBoostRuntimeState::Active, Duration);
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase3Private::BoostSound))
	{
		ActiveBoostAudio = UGameplayStatics::SpawnSound2D(
			this, Sound, 0.34f, 1.0f, 0.0f, nullptr, false, false);
	}
	UE_LOG(LogSOTMPhase3, Display, TEXT("Speed Boost ACTIVE multiplier=%.2f duration=%.1f"),
		Settings->SpeedBoostMultiplier, Duration);
	return true;
}

void USOTMDemoPhase3WorldSubsystem::FinishActiveSpeedBoost()
{
	UWorld* World = GetWorld();
	if (!World || RuntimeState != ESOTMSpeedBoostRuntimeState::Active)
	{
		return;
	}
	RestoreMovementSpeed();
	if (ActiveBoostAudio)
	{
		ActiveBoostAudio->FadeOut(0.18f, 0.0f);
		ActiveBoostAudio = nullptr;
	}
	const float Cooldown = PlayerState
		? PlayerState->GetEffectiveSpeedBoostCooldown()
		: GetDefault<USOTMPhase3Settings>()->SpeedBoostCooldown;
	World->GetTimerManager().SetTimer(CooldownTimer, this, &ThisClass::FinishCooldown, Cooldown, false);
	SetRuntimeState(ESOTMSpeedBoostRuntimeState::Cooldown, Cooldown);
}

void USOTMDemoPhase3WorldSubsystem::FinishCooldown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationTimer);
	}
	SetRuntimeState(PlayerState && PlayerState->IsSpeedBoostUnlocked()
		? ESOTMSpeedBoostRuntimeState::Ready
		: ESOTMSpeedBoostRuntimeState::Locked);
}

void USOTMDemoPhase3WorldSubsystem::UpdateRuntimePresentation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (RuntimeState == ESOTMSpeedBoostRuntimeState::Active)
	{
		// HUD countdown only - the movement effect is applied every frame by
		// USOTMSpeedBoostComponent on the pawn.
		const float Remaining = FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(ActiveTimer));
		SetRuntimeState(RuntimeState, Remaining);
	}
	else if (RuntimeState == ESOTMSpeedBoostRuntimeState::Cooldown)
	{
		const float Remaining = FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(CooldownTimer));
		SetRuntimeState(RuntimeState, Remaining);
	}
}

void USOTMDemoPhase3WorldSubsystem::SetRuntimeState(
	const ESOTMSpeedBoostRuntimeState NewState,
	const float RemainingSeconds)
{
	RuntimeState = NewState;
	float Total = 0.0f;
	if (NewState == ESOTMSpeedBoostRuntimeState::Active)
	{
		Total = PlayerState ? PlayerState->GetEffectiveSpeedBoostDuration() : GetDefault<USOTMPhase3Settings>()->SpeedBoostDuration;
	}
	else if (NewState == ESOTMSpeedBoostRuntimeState::Cooldown)
	{
		Total = PlayerState ? PlayerState->GetEffectiveSpeedBoostCooldown() : GetDefault<USOTMPhase3Settings>()->SpeedBoostCooldown;
	}
	const float Normalized = Total > 0.0f ? FMath::Clamp(RemainingSeconds / Total, 0.0f, 1.0f) : 0.0f;
	OnSpeedBoostStateChanged.Broadcast(NewState, RemainingSeconds, Normalized);
}

void USOTMDemoPhase3WorldSubsystem::RestoreMovementSpeed()
{
	if (USOTMSpeedBoostComponent* Boost = ActiveBoostComponent.Get())
	{
		Boost->StopBoost();
	}
	ActiveBoostComponent.Reset();
}

void USOTMDemoPhase3WorldSubsystem::ResetRuntimeAfterDeath()
{
	CloseUpgradeUI();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActiveTimer);
		World->GetTimerManager().ClearTimer(CooldownTimer);
		World->GetTimerManager().ClearTimer(PresentationTimer);
	}
	RestoreMovementSpeed();
	SetRuntimeState(PlayerState && PlayerState->IsSpeedBoostUnlocked()
		? ESOTMSpeedBoostRuntimeState::Ready
		: ESOTMSpeedBoostRuntimeState::Locked);
}

UCharacterMovementComponent* USOTMDemoPhase3WorldSubsystem::ResolveMovementComponent() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	return Character ? Character->GetCharacterMovement() : nullptr;
}

void USOTMDemoPhase3WorldSubsystem::HandleOwnershipChanged(bool bUnlocked, int32 Level)
{
	(void)Level;
	if (RuntimeState != ESOTMSpeedBoostRuntimeState::Active && RuntimeState != ESOTMSpeedBoostRuntimeState::Cooldown)
	{
		SetRuntimeState(bUnlocked ? ESOTMSpeedBoostRuntimeState::Ready : ESOTMSpeedBoostRuntimeState::Locked);
	}
}

void USOTMDemoPhase3WorldSubsystem::HandlePlayerDeathStarted(AActor* PlayerActor)
{
	(void)PlayerActor;
	bPlayerInStationRange = false;
	OnStationPromptChanged.Broadcast(false);
	ResetRuntimeAfterDeath();
}

void USOTMDemoPhase3WorldSubsystem::HandlePlayerRespawned(AActor* PlayerActor)
{
	(void)PlayerActor;
	ResetRuntimeAfterDeath();
}

void USOTMDemoPhase3WorldSubsystem::HandleGameOver()
{
	bPlayerInStationRange = false;
	OnStationPromptChanged.Broadcast(false);
	ResetRuntimeAfterDeath();
}
