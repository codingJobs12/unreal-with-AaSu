#include "Demo/SOTMDemoPhase4WorldSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "AI/SOTMCousinCharacter.h"
#include "Demo/SOTMChestActor.h"
#include "Gate/SOTMKeyGateActor.h"
#include "Demo/SOTMDemoPhase2WorldSubsystem.h"
#include "Demo/SOTMPhase4Interactable.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Objective/SOTMObjectiveSubsystem.h"
#include "SOTMPlayerStateSubsystem.h"
#include "UI/SOTMSubtitleStyle.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UI/SOTMDemoCompleteWidget.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMPhase4, Log, All);

namespace SOTMPhase4Private
{
	const FName ForestMap(TEXT("/Game/MenuSystemPro/ExampleContent/Designs/Design_Silence/Levels/CH1"));
	const TCHAR* InteractActionPath = TEXT("/Game/MenuSystemPro/Blueprints/Input/CharacterOnFoot/IA_Interact.IA_Interact");
	const TCHAR* GateMeshPath = TEXT("/Game/Fab/Main_gate_entrance/main_gate_entrance/StaticMeshes/main_gate_entrance.main_gate_entrance");
	const TCHAR* KeyMeshPath = TEXT("/Game/Chest_Keys/GateKeys.GateKeys");
	const TCHAR* ChestOpenSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_ChestOpen.SFX_TEMP_ChestOpen");
	const TCHAR* KeyAcquiredSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_KeyAcquired.SFX_TEMP_KeyAcquired");
	const TCHAR* GateLockedSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_GateLocked.SFX_TEMP_GateLocked");
	const TCHAR* GateOpenSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_GateOpen.SFX_TEMP_GateOpen");
	const TCHAR* ChestSequencePath = TEXT("/Game/Sequence/Chest_Sequence.Chest_Sequence");
	const TCHAR* IsabelIntroSequencePath = TEXT("/Game/Sequence/IsabelaIntro_sequence.IsabelaIntro_sequence");
	const TCHAR* IsabelIntroVO[3] =
	{
		TEXT("/Game/Audio/Dialogue/Chapter1/Temporary/VO_TEMP_ISABELLA_INTRO1.VO_TEMP_ISABELLA_INTRO1"),
		TEXT("/Game/Audio/Dialogue/Chapter1/Temporary/VO_TEMP_ISABELLA_INTRO2.VO_TEMP_ISABELLA_INTRO2"),
		TEXT("/Game/Audio/Dialogue/Chapter1/Temporary/VO_TEMP_ISABELLA_INTRO3.VO_TEMP_ISABELLA_INTRO3"),
	};
	constexpr float IsabelDialogueDelaySeconds = 4.5f;
	const TCHAR* TimmyChestVO = TEXT("/Game/Audio/Dialogue/Chapter1/Temporary/VO_TEMP_Timmy_Chest_001.VO_TEMP_Timmy_Chest_001");
	const TCHAR* DemoCompleteSound = TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_DemoComplete.SFX_TEMP_DemoComplete");

	FName NormalizeMapPackageName(const UWorld* World)
	{
		return World ? FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())) : NAME_None;
	}
}

bool USOTMDemoPhase4WorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void USOTMDemoPhase4WorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (SOTMPhase4Private::NormalizeMapPackageName(&InWorld) != SOTMPhase4Private::ForestMap)
	{
		return;
	}
	UGameInstance* GameInstance = InWorld.GetGameInstance();
	PlayerState = GameInstance ? GameInstance->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	Objectives = GameInstance ? GameInstance->GetSubsystem<USOTMObjectiveSubsystem>() : nullptr;
	if (PlayerState)
	{
		PlayerState->OnPlayerDeathStarted.AddUniqueDynamic(this, &ThisClass::HandlePlayerUnavailable);
		PlayerState->OnGameOver.AddUniqueDynamic(this, &ThisClass::HideDemoComplete);
		PlayerState->OnPlayerRespawned.AddUniqueDynamic(this, &ThisClass::HandlePlayerRespawned);
	}
	InWorld.GetTimerManager().SetTimer(InitializeTimer, this, &ThisClass::InitializePhase4, 1.1f, false);
}

void USOTMDemoPhase4WorldSubsystem::Deinitialize()
{
	EndIsabelCutscene(true);
	EndChestDialogue();
	if (Objectives)
	{
		Objectives->OnObjectiveChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveChanged);
	}
	HideDemoComplete();
	UnbindProductionInput();
	if (PlayerState)
	{
		PlayerState->OnPlayerDeathStarted.RemoveDynamic(this, &ThisClass::HandlePlayerUnavailable);
		PlayerState->OnGameOver.RemoveDynamic(this, &ThisClass::HideDemoComplete);
		PlayerState->OnPlayerRespawned.RemoveDynamic(this, &ThisClass::HandlePlayerRespawned);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (ChestAnchor) ChestAnchor->Destroy();
	if (GateAnchor) GateAnchor->Destroy();
	if (KeyAnchor) KeyAnchor->Destroy();
	if (KeyPresentation) KeyPresentation->Destroy();
	if (ChestArt) ChestArt->Destroy();
	ChestAnchor = nullptr;
	GateAnchor = nullptr;
	KeyAnchor = nullptr;
	KeyPresentation = nullptr;
	ChestArt = nullptr;
	GateArt = nullptr;
	Objectives = nullptr;
	PlayerState = nullptr;
	Super::Deinitialize();
}

void USOTMDemoPhase4WorldSubsystem::InitializePhase4()
{
	if (!PlayerState || !Objectives)
	{
		UE_LOG(LogSOTMPhase4, Error, TEXT("Phase 4 initialization failed: required state subsystem missing."));
		return;
	}
	FindProductionArtAndCreateAnchors();
	BindProductionInput();
	Objectives->OnObjectiveChanged.AddUniqueDynamic(this, &ThisClass::HandleObjectiveChanged);
	RefreshChestMarker();
	if (PlayerState->IsPhase4ChestOpened())
	{
		BeginChestPresentation(true);
	}
	if (PlayerState->IsPhase4GateUnlocked())
	{
		BeginGatePresentation(true);
	}
	if (PlayerState->IsPhase4DemoCompleted())
	{
		GetWorld()->GetTimerManager().SetTimer(DemoCompleteTimer, this, &ThisClass::ShowDemoComplete, 0.6f, false);
	}
	UE_LOG(LogSOTMPhase4, Display,
		TEXT("Phase 4 initialized chest=%s gate=%s opened=%d key=%d gateUnlocked=%d demoComplete=%d"),
		*GetNameSafe(ChestArt), *GetNameSafe(GateArt), PlayerState->IsPhase4ChestOpened(),
		PlayerState->HasPhase4GateKey(), PlayerState->IsPhase4GateUnlocked(), PlayerState->IsPhase4DemoCompleted());

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("SOTMPhase4DeathAfterKeyAcceptance")))
	{
		BeginDevelopmentDeathAfterKeyAcceptance();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("SOTMPhase4Acceptance")))
	{
		BeginDevelopmentAcceptanceRoute();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("SOTMPhase4DataAcceptance")))
	{
		RunDevelopmentDataAcceptance();
	}
#endif
}

void USOTMDemoPhase4WorldSubsystem::FindProductionArtAndCreateAnchors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// BP_Chest (ASOTMChestActor) is hand-placed in the level now, same as GateArt below -
	// found here rather than spawned, so wherever it's placed in the editor is where it
	// plays.
	for (TActorIterator<ASOTMChestActor> It(World); It; ++It)
	{
		ChestArt = *It;
		break;
	}
	if (ChestArt)
	{
		ChestClosedTransform = ChestArt->GetActorTransform();
		ChestMarker = ChestArt->FindComponentByClass<UWidgetComponent>();
		float NearestGateSq = TNumericLimits<float>::Max();
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
			if (!Mesh || Mesh->GetPathName() != SOTMPhase4Private::GateMeshPath)
			{
				continue;
			}
			const float DistanceSq = FVector::DistSquared(ChestClosedTransform.GetLocation(), It->GetActorLocation());
			if (DistanceSq < NearestGateSq)
			{
				NearestGateSq = DistanceSq;
				GateArt = *It;
			}
		}
	}
	if (GateArt)
	{
		GateClosedTransform = GateArt->GetActorTransform();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	if (ChestArt)
	{
		ChestAnchor = World->SpawnActor<ASOTMPhase4Interactable>(
			ASOTMPhase4Interactable::StaticClass(), ChestArt->GetActorLocation(), FRotator::ZeroRotator, Params);
		if (ChestAnchor)
		{
			ChestAnchor->Configure(ESOTMPhase4InteractableKind::Chest, 250.0f);
			ChestAnchor->OnPlayerEntered.AddUObject(this, &ThisClass::HandleEntered);
			ChestAnchor->OnPlayerExited.AddUObject(this, &ThisClass::HandleExited);
		}
	}
	if (GateArt)
	{
		GateAnchor = World->SpawnActor<ASOTMPhase4Interactable>(
			ASOTMPhase4Interactable::StaticClass(), GateArt->GetActorLocation(), FRotator::ZeroRotator, Params);
		if (GateAnchor)
		{
			GateAnchor->Configure(ESOTMPhase4InteractableKind::Gate, 1200.0f);
			GateAnchor->OnPlayerEntered.AddUObject(this, &ThisClass::HandleEntered);
			GateAnchor->OnPlayerExited.AddUObject(this, &ThisClass::HandleExited);
		}
	}
	if (!ChestArt || !GateArt)
	{
		UE_LOG(LogSOTMPhase4, Error, TEXT("Production Phase 4 art missing chest=%s gate=%s"),
			*GetNameSafe(ChestArt), *GetNameSafe(GateArt));
	}
}

void USOTMDemoPhase4WorldSubsystem::BindProductionInput()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	if (!Input)
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
	InteractInputAction = LoadObject<UInputAction>(nullptr, SOTMPhase4Private::InteractActionPath);
	if (!InteractInputAction)
	{
		UE_LOG(LogSOTMPhase4, Error, TEXT("Production IA_Interact asset failed to load."));
		return;
	}
	FEnhancedInputActionEventBinding& Binding = Input->BindAction(
		InteractInputAction, ETriggerEvent::Started, this, &ThisClass::HandleInteractInput);
	InteractBindingHandle = Binding.GetHandle();
	BoundEnhancedInput = Input;
}

void USOTMDemoPhase4WorldSubsystem::UnbindProductionInput()
{
	if (UEnhancedInputComponent* Input = BoundEnhancedInput.Get(); Input && InteractBindingHandle != 0)
	{
		Input->RemoveBindingByHandle(InteractBindingHandle);
	}
	BoundEnhancedInput.Reset();
	InteractBindingHandle = 0;
}

void USOTMDemoPhase4WorldSubsystem::HandleInteractInput()
{
	UE_LOG(LogSOTMPhase4, Verbose, TEXT("Interact pressed: nearChest=%d nearGate=%d chestOpen=%d hasKey=%d dialogue=%d"),
		bNearChest, bNearGate, PlayerState && PlayerState->IsPhase4ChestOpened(),
		PlayerState && PlayerState->HasPhase4GateKey(), bChestDialogueActive);
	if (!PlayerState || PlayerState->IsPlayerDead() || PlayerState->IsGameOver() || DemoCompleteWidget || bChestDialogueActive || bIsabelCutsceneActive)
	{
		return;
	}
	// Collecting the key reuses the SAME chest trigger (bNearChest) rather than a
	// separate sphere spawned at interact time - that separate sphere had a
	// spawn-order bug (its first overlap could fire before anything was
	// listening) and was extra complexity for no benefit, since the player is
	// already standing in the chest's trigger by definition. Pressing E near
	// the chest opens it if it's still closed, or collects the key once it's
	// open (and not yet collected) - interaction with the key is only enabled
	// after the chest is actually open.
	// Safety net: chest is open, key not yet taken, and the player is standing at the
	// chest - never require leaving and re-entering the trigger to collect the key.
	if (!bNearChest && PlayerState->IsPhase4ChestOpened() && !PlayerState->HasPhase4GateKey()
		&& IsPlayerWithinChestRange())
	{
		bNearChest = true;
	}
	if (bNearChest)
	{
		if (!PlayerState->IsPhase4ChestOpened())
		{
			InteractWithChest();
		}
		else if (!PlayerState->HasPhase4GateKey())
		{
			InteractWithKey();
		}
	}
	else if (bNearGate)
	{
		InteractWithGate();
	}
}

void USOTMDemoPhase4WorldSubsystem::HandleEntered(const ESOTMPhase4InteractableKind Kind, AActor* Actor)
{
	if (!PlayerState || !PlayerState->IsBoundPlayerActor(Actor))
	{
		return;
	}
	if (Kind == ESOTMPhase4InteractableKind::Chest)
	{
		bNearChest = true;
		// Player reached the chest: the marker is removed for good.
		bChestMarkerDismissed = true;
	}
	else if (Kind == ESOTMPhase4InteractableKind::Key) bNearKey = true;
	else bNearGate = true;
	RefreshChestMarker();
	RefreshPrompt();
}

void USOTMDemoPhase4WorldSubsystem::HandleExited(const ESOTMPhase4InteractableKind Kind, AActor* Actor)
{
	if (!PlayerState || !PlayerState->IsBoundPlayerActor(Actor))
	{
		return;
	}
	if (Kind == ESOTMPhase4InteractableKind::Chest) bNearChest = false;
	else if (Kind == ESOTMPhase4InteractableKind::Key) bNearKey = false;
	else bNearGate = false;
	RefreshPrompt();
}

void USOTMDemoPhase4WorldSubsystem::RefreshPrompt()
{
	if (bChestDialogueActive || bIsabelCutsceneActive || !PlayerState || PlayerState->IsPlayerDead() || PlayerState->IsGameOver())
	{
		OnPromptChanged.Broadcast(false, FText::GetEmpty());
		return;
	}
	if (bNearChest)
	{
		if (!PlayerState->IsPhase4ChestOpened())
		{
			const FSOTMObjectiveData Active = Objectives ? Objectives->GetActiveChapterOneObjective() : FSOTMObjectiveData();
			OnPromptChanged.Broadcast(true,
				Active.ObjectiveId == USOTMObjectiveSubsystem::FindChestId
					? NSLOCTEXT("SOTM", "OpenChestPrompt", "[E]  OPEN CHEST")
					: NSLOCTEXT("SOTM", "ChestLockedPrompt", "COMPLETE PREVIOUS OBJECTIVES"));
		}
		else if (!PlayerState->HasPhase4GateKey())
		{
			// Toggles the instant the chest opens - same trigger as the chest
			// itself, no separate sphere/spawn-timing to worry about.
			OnPromptChanged.Broadcast(true, NSLOCTEXT("SOTM", "CollectKeyPrompt", "[E]  COLLECT KEY"));
		}
		else
		{
			OnPromptChanged.Broadcast(true, NSLOCTEXT("SOTM", "ChestAlreadyOpened", "CHEST OPENED"));
		}
		return;
	}
	// The legacy "GATE LOCKED" requirements panel is intentionally never shown;
	// the Mansion Gate (ASOTMKeyGateActor) owns its own prompt.
	if (false && bNearGate && !PlayerState->IsPhase4GateUnlocked())
	{
		const bool bReady = Objectives &&
			Objectives->GetActiveChapterOneObjective().ObjectiveId == USOTMObjectiveSubsystem::ReachGateId;
		OnPromptChanged.Broadcast(true, bReady
			? NSLOCTEXT("SOTM", "UnlockGatePrompt", "[E]  UNLOCK GATE")
			: (Objectives ? Objectives->GetGateRequirementFeedback() : FText::GetEmpty()));
		return;
	}
	OnPromptChanged.Broadcast(false, FText::GetEmpty());
}

void USOTMDemoPhase4WorldSubsystem::InteractWithChest()
{
	if (!Objectives || !PlayerState || PlayerState->IsPhase4ChestOpened())
	{
		return;
	}
	const ESOTMPhase4ActionResult Result = Objectives->TryOpenPhase4Chest();
	if (Result == ESOTMPhase4ActionResult::Success)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase4Private::ChestOpenSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, ChestArt ? ChestArt->GetActorLocation() : FVector::ZeroVector, 0.60f);
		}
		// Opening the chest only reveals the key now - it isn't collected (and the objective
		// doesn't update) until the player presses E again. See InteractWithKey. The chest's
		// own trigger (bNearChest) is reused for this, so the prompt below updates to
		// COLLECT KEY immediately - no separate key-specific trigger needed.
		BeginChestPresentation(false);
		// The "CHEST OPENED / TAKE THE KEY" popup is deliberately NOT shown here - it would
		// clash with Timmy's line. EndChestDialogue shows it once the dialogue is over.
		BeginChestDialogue();
	}
	else if (Result != ESOTMPhase4ActionResult::AlreadyCompleted)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase4Private::GateLockedSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, ChestArt ? ChestArt->GetActorLocation() : FVector::ZeroVector, 0.45f);
		}
		OnNotification.Broadcast(
			NSLOCTEXT("SOTM", "ChestLocked", "CHEST LOCKED"),
			NSLOCTEXT("SOTM", "CompletePreviousObjectives", "COMPLETE PREVIOUS OBJECTIVES"));
	}
	RefreshPrompt();
	UE_LOG(LogSOTMPhase4, Display, TEXT("Chest interaction result=%d"), static_cast<int32>(Result));
}

void USOTMDemoPhase4WorldSubsystem::PlayIsabelGateIntro()
{
	if (bIsabelCutsceneActive)
	{
		return;
	}
	if (!PlayerState)
	{
		UE_LOG(LogSOTMPhase4, Warning, TEXT("PlayIsabelGateIntro: Phase 4 subsystem has no PlayerState - cutscene skipped."));
		return;
	}
	UE_LOG(LogSOTMPhase4, Display, TEXT("Isabella gate intro cutscene requested by the Mansion Gate."));
	bIsabelFromKeyGate = true;
	BeginIsabelCutscene();
}

void USOTMDemoPhase4WorldSubsystem::HideAllUIWidgets(TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& OutHidden)
{
	OutHidden.Reset();
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundWidgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : FoundWidgets)
	{
		if (!Widget)
		{
			continue;
		}
		const ESlateVisibility Current = Widget->GetVisibility();
		if (Current == ESlateVisibility::Collapsed || Current == ESlateVisibility::Hidden)
		{
			continue;
		}
		OutHidden.Add(Widget, Current);
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USOTMDemoPhase4WorldSubsystem::RestoreHiddenUIWidgets(TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Hidden)
{
	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Pair : Hidden)
	{
		if (UUserWidget* Widget = Pair.Key.Get())
		{
			Widget->SetVisibility(Pair.Value);
		}
	}
	Hidden.Reset();
}

void USOTMDemoPhase4WorldSubsystem::BeginIsabelCutscene()
{
	UWorld* World = GetWorld();
	if (!World || bIsabelCutsceneActive)
	{
		return;
	}
	bIsabelCutsceneActive = true;
	bIsabelDialogueDone = false;
	IsabelLineIndex = 0;

	// Nothing else may talk or be on screen.
	if (USOTMDemoPhase2WorldSubsystem* Phase2 = World->GetSubsystem<USOTMDemoPhase2WorldSubsystem>())
	{
		Phase2->SetDialogueSuppressed(true);
	}
	OnPromptChanged.Broadcast(false, FText::GetEmpty());
	HideAllUIWidgets(IsabelHiddenWidgets);
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		bPreviousMouseCursorIsabel = PC->bShowMouseCursor;
		PC->bShowMouseCursor = false;
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
	if (PlayerState)
	{
		PlayerState->AcquireInputLock(ESOTMInputLockReason::Cinematic);
		bIsabelInputLockHeld = true;
	}

	// IsabelaIntro_sequence is intentionally NOT played - the gate actor drives the whole cinematic.
	bIsabelSequencePlaying = false;

	// Mansion Gate flow: gate swings open, the player walks in, the gate closes - THEN the dialogue.
	if (bIsabelFromKeyGate)
	{
		for (TActorIterator<ASOTMKeyGateActor> GateIt(World); GateIt; ++GateIt)
		{
			TWeakObjectPtr<USOTMDemoPhase4WorldSubsystem> WeakThis(this);
			GateIt->PlayIntroEntrance([WeakThis]()
			{
				if (USOTMDemoPhase4WorldSubsystem* Self = WeakThis.Get())
				{
					Self->StartIsabelDialogue();
				}
			});
			return;
		}
	}
	// Otherwise: dialogue starts a few seconds after the sequence starts.
	World->GetTimerManager().SetTimer(
		IsabelDialogueStartTimer, this, &ThisClass::StartIsabelDialogue,
		SOTMPhase4Private::IsabelDialogueDelaySeconds, false);
}

void USOTMDemoPhase4WorldSubsystem::HandleIsabelSequenceFinished()
{
	if (!bIsabelCutsceneActive || !bIsabelSequencePlaying)
	{
		return;
	}
	bIsabelSequencePlaying = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IsabelSequenceTimeoutTimer);
	}
	if (IsabelSequencePlayer)
	{
		IsabelSequencePlayer->OnFinished.RemoveAll(this);
	}
	if (bIsabelDialogueDone)
	{
		EndIsabelCutscene(false);
	}
}

void USOTMDemoPhase4WorldSubsystem::StartIsabelDialogue()
{
	if (!bIsabelCutsceneActive)
	{
		return;
	}
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		FinishIsabelDialogue();
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
					SNew(STextBlock)
					.Font(SOTMSubtitle::Font())
					.Text(NSLOCTEXT("SOTM", "IsabelName", "ISABELLA"))
					.ColorAndOpacity(FLinearColor(0.85f, 0.10f, 0.12f, 1.0f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SAssignNew(IsabelSubtitleLine, STextBlock)
					.Font(SOTMSubtitle::Font())
					.Text(FText::GetEmpty())
					.ColorAndOpacity(FLinearColor::White)
					.WrapTextAt(900.0f)
					.Justification(ETextJustify::Center)
				]
			]
		];
	IsabelSubtitleRoot = Content;
	Viewport->AddViewportWidgetContent(IsabelSubtitleRoot.ToSharedRef(), 950);
	PlayIsabelLine(0);
}

void USOTMDemoPhase4WorldSubsystem::PlayIsabelLine(const int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !bIsabelCutsceneActive || !IsabelSubtitleLine.IsValid())
	{
		return;
	}
	IsabelLineIndex = Index;
	static const FText Lines[3] =
	{
		NSLOCTEXT("SOTM", "IsabelIntroLine1", "You stole my gifts.."),
		NSLOCTEXT("SOTM", "IsabelIntroLine2", "You are not supposed to have lightning.."),
		NSLOCTEXT("SOTM", "IsabelIntroLine3", "Give it back.."),
	};
	IsabelFullLine = Lines[Index].ToString();
	IsabelRevealedChars = 0;
	IsabelSubtitleLine->SetText(FText::GetEmpty());

	float VODuration = 0.0f;
	if (USoundBase* VO = LoadObject<USoundBase>(nullptr, SOTMPhase4Private::IsabelIntroVO[Index]))
	{
		VODuration = VO->GetDuration();
		IsabelVoice = UGameplayStatics::SpawnSound2D(this, VO, 1.0f, 1.0f, 0.0f, nullptr, false, true);
	}
	float TypeTarget = VODuration;
	if (IsabelVoice)
	{
		IsabelVoice->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleIsabelLineAudioFinished);
		World->GetTimerManager().SetTimer(
			IsabelLineTimeoutTimer, this, &ThisClass::FinishIsabelLine, VODuration + 1.0f, false);
		UE_LOG(LogSOTMPhase4, Display, TEXT("Isabella intro VO %d: %s duration=%.2fs"),
			Index + 1, SOTMPhase4Private::IsabelIntroVO[Index], VODuration);
	}
	else
	{
		UE_LOG(LogSOTMPhase4, Warning, TEXT("Isabella intro VO %d unavailable (%s) - timed subtitle."),
			Index + 1, SOTMPhase4Private::IsabelIntroVO[Index]);
		TypeTarget = 2.4f;
		World->GetTimerManager().SetTimer(IsabelLineTimeoutTimer, this, &ThisClass::FinishIsabelLine, 3.0f, false);
	}
	const int32 CharCount = IsabelFullLine.Len();
	const float PacedInterval = TypeTarget > 0.0f ? (TypeTarget * 0.85f) / FMath::Max(CharCount, 1) : 0.045f;
	World->GetTimerManager().SetTimer(
		IsabelTypewriterTimer, this, &ThisClass::TickIsabelTypewriter, FMath::Clamp(PacedInterval, 0.015f, 0.06f), true);
}

void USOTMDemoPhase4WorldSubsystem::TickIsabelTypewriter()
{
	if (!IsabelSubtitleLine.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(IsabelTypewriterTimer);
		}
		return;
	}
	++IsabelRevealedChars;
	IsabelSubtitleLine->SetText(FText::FromString(IsabelFullLine.Left(IsabelRevealedChars)));
	if (IsabelRevealedChars >= IsabelFullLine.Len())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(IsabelTypewriterTimer);
		}
	}
}

void USOTMDemoPhase4WorldSubsystem::HandleIsabelLineAudioFinished()
{
	if (IsabelVoice)
	{
		IsabelVoice->OnAudioFinished.RemoveAll(this);
	}
	IsabelVoice = nullptr;
	FinishIsabelLine();
}

void USOTMDemoPhase4WorldSubsystem::FinishIsabelLine()
{
	UWorld* World = GetWorld();
	if (!World || !bIsabelCutsceneActive || bIsabelDialogueDone)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(IsabelLineTimeoutTimer);
	World->GetTimerManager().ClearTimer(IsabelTypewriterTimer);
	if (IsabelVoice)
	{
		IsabelVoice->OnAudioFinished.RemoveAll(this);
		IsabelVoice->Stop();
		IsabelVoice = nullptr;
	}
	if (IsabelSubtitleLine.IsValid())
	{
		IsabelSubtitleLine->SetText(FText::FromString(IsabelFullLine));
	}
	if (IsabelLineIndex + 1 < 3)
	{
		World->GetTimerManager().SetTimer(IsabelLineGapTimer, this, &ThisClass::PlayNextIsabelLine, 0.3f, false);
	}
	else
	{
		FinishIsabelDialogue();
	}
}

void USOTMDemoPhase4WorldSubsystem::PlayNextIsabelLine()
{
	PlayIsabelLine(IsabelLineIndex + 1);
}

void USOTMDemoPhase4WorldSubsystem::FinishIsabelDialogue()
{
	if (!bIsabelCutsceneActive || bIsabelDialogueDone)
	{
		return;
	}
	bIsabelDialogueDone = true;
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(IsabelLineTimeoutTimer);
		World->GetTimerManager().ClearTimer(IsabelTypewriterTimer);
		World->GetTimerManager().ClearTimer(IsabelLineGapTimer);
	}
	if (IsabelSubtitleRoot.IsValid())
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(IsabelSubtitleRoot.ToSharedRef());
		}
	}
	IsabelSubtitleRoot.Reset();
	IsabelSubtitleLine.Reset();
	if (bIsabelFromKeyGate && World)
	{
		// Dialogue over: Isabella (already standing there) wakes up. Nobody is moved or dropped.
		for (TActorIterator<ASOTMKeyGateActor> GateIt(World); GateIt; ++GateIt)
		{
			GateIt->OpenGateAndSpawnIsabel();
			break;
		}
	}
	// Dialogue is done; the cutscene ends once the sequence has finished too.
	if (!bIsabelSequencePlaying)
	{
		EndIsabelCutscene(false);
	}
}

void USOTMDemoPhase4WorldSubsystem::EndIsabelCutscene(const bool bAborted)
{
	if (!bIsabelCutsceneActive)
	{
		return;
	}
	bIsabelCutsceneActive = false;
	bIsabelSequencePlaying = false;
	bIsabelDialogueDone = false;

	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<ASOTMKeyGateActor> CinGate(World); CinGate; ++CinGate)
		{
			CinGate->EndIntroCinematic();
		}
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(IsabelDialogueStartTimer);
		Timers.ClearTimer(IsabelLineTimeoutTimer);
		Timers.ClearTimer(IsabelTypewriterTimer);
		Timers.ClearTimer(IsabelLineGapTimer);
		Timers.ClearTimer(IsabelSequenceTimeoutTimer);
	}
	if (IsabelVoice)
	{
		IsabelVoice->OnAudioFinished.RemoveAll(this);
		IsabelVoice->Stop();
		IsabelVoice = nullptr;
	}
	if (IsabelSubtitleRoot.IsValid())
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(IsabelSubtitleRoot.ToSharedRef());
		}
	}
	IsabelSubtitleRoot.Reset();
	IsabelSubtitleLine.Reset();
	if (IsabelSequencePlayer)
	{
		IsabelSequencePlayer->OnFinished.RemoveAll(this);
		IsabelSequencePlayer->Stop();
		IsabelSequencePlayer = nullptr;
	}
	if (IsabelSequenceActor)
	{
		IsabelSequenceActor->Destroy();
		IsabelSequenceActor = nullptr;
	}

	RestoreHiddenUIWidgets(IsabelHiddenWidgets);
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		PC->bShowMouseCursor = bPreviousMouseCursorIsabel;
		if (bPreviousMouseCursorIsabel)
		{
			FInputModeGameAndUI RestoredInputMode;
			RestoredInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			RestoredInputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(RestoredInputMode);
		}
		else
		{
			PC->SetInputMode(FInputModeGameOnly());
		}
	}
	if (PlayerState && bIsabelInputLockHeld)
	{
		PlayerState->ReleaseInputLock(ESOTMInputLockReason::Cinematic);
	}
	bIsabelInputLockHeld = false;
	if (World)
	{
		if (USOTMDemoPhase2WorldSubsystem* Phase2 = World->GetSubsystem<USOTMDemoPhase2WorldSubsystem>())
		{
			Phase2->SetDialogueSuppressed(false);
		}
	}
	if (bIsabelFromKeyGate)
	{
		// Mansion Gate flow: the Isabel encounter continues from here.
		bIsabelFromKeyGate = false;
		return;
	}
	if (bAborted || !World || !PlayerState)
	{
		return;
	}
	OnNotification.Broadcast(
		NSLOCTEXT("SOTM", "GateUnlocked", "GATE UNLOCKED"),
		NSLOCTEXT("SOTM", "DemoGoalAchieved", "FINAL DEMO GOAL ACHIEVED"));
	// Hand over to the existing ending: if the gate has finished opening, Demo Complete
	// follows shortly; otherwise UpdateGatePresentation schedules it when the gate is open.
	if (!bGateAnimationRunning && !PlayerState->IsPhase4DemoCompleted()
		&& !PlayerState->IsPlayerDead() && !PlayerState->IsGameOver())
	{
		World->GetTimerManager().SetTimer(DemoCompleteTimer, this, &ThisClass::FinishGatePresentation, 0.8f, false);
	}
}

void USOTMDemoPhase4WorldSubsystem::BeginChestDialogue()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	EndChestDialogue();
	bChestDialogueActive = true;

	// 1. Nothing else may talk: stop Cousin voices/subtitles and block new ones.
	if (USOTMDemoPhase2WorldSubsystem* Phase2 = World->GetSubsystem<USOTMDemoPhase2WorldSubsystem>())
	{
		Phase2->SetDialogueSuppressed(true);
	}

	// 2. Clear the interaction prompt, then hide every remaining UI widget (in-game HUD,
	//    ability icons, crosshair, minimap, popups...). The subtitle is raw Slate and is
	//    unaffected by this sweep.
	OnPromptChanged.Broadcast(false, FText::GetEmpty());
	ChestDialogueHiddenWidgets.Reset();
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundWidgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : FoundWidgets)
	{
		if (!Widget)
		{
			continue;
		}
		const ESlateVisibility Current = Widget->GetVisibility();
		if (Current == ESlateVisibility::Collapsed || Current == ESlateVisibility::Hidden)
		{
			continue;
		}
		ChestDialogueHiddenWidgets.Add(Widget, Current);
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 3. Lock all interaction: Cinematic lock (also makes the HUD widgets self-collapse)
	//    plus UI-only input so no game action (including E) can reach the player.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		bPreviousMouseCursorChestDialogue = PC->bShowMouseCursor;
		PC->bShowMouseCursor = false;
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
	if (PlayerState)
	{
		PlayerState->AcquireInputLock(ESOTMInputLockReason::Cinematic);
		bChestDialogueInputLockHeld = true;
	}

	// Play Chest_Sequence first; the dialogue starts when it finishes.
	if (ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, SOTMPhase4Private::ChestSequencePath))
	{
		FMovieSceneSequencePlaybackSettings Settings;
		Settings.bPauseAtEnd = false;
		Settings.bDisableCameraCuts = false;
		ALevelSequenceActor* CreatedActor = nullptr;
		ChestSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, Settings, CreatedActor);
		ChestSequenceActor = CreatedActor;
		if (ChestSequencePlayer)
		{
			bChestSequencePlaying = true;
			ChestSequencePlayer->OnFinished.AddUniqueDynamic(this, &ThisClass::HandleChestSequenceFinished);
			// Safety net in case OnFinished never fires.
			const float SequenceSeconds = ChestSequencePlayer->GetDuration().AsSeconds();
			World->GetTimerManager().SetTimer(
				ChestSequenceTimeoutTimer, this, &ThisClass::HandleChestSequenceFinished,
				FMath::Max(SequenceSeconds, 0.1f) + 2.0f, false);
			ChestSequencePlayer->Play();
			UE_LOG(LogSOTMPhase4, Display, TEXT("Chest_Sequence playing duration=%.2fs"), SequenceSeconds);
			// Dialogue starts together with the sequence (the key pop-up animation was already
			// started by BeginChestPresentation when the chest opened).
			PlayChestDialogueLine();
			return;
		}
	}
	UE_LOG(LogSOTMPhase4, Warning, TEXT("Chest_Sequence unavailable (%s) - starting dialogue directly."),
		SOTMPhase4Private::ChestSequencePath);

	// Short beat so the UI has visibly cleared before the line starts.
	World->GetTimerManager().SetTimer(
		ChestDialoguePreDelayTimer, this, &ThisClass::PlayChestDialogueLine, 0.3f, false);
}

void USOTMDemoPhase4WorldSubsystem::HandleChestSequenceFinished()
{
	if (!bChestDialogueActive || !bChestSequencePlaying)
	{
		return;
	}
	bChestSequencePlaying = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChestSequenceTimeoutTimer);
	}
	if (ChestSequencePlayer)
	{
		ChestSequencePlayer->OnFinished.RemoveAll(this);
	}
	// The dialogue runs alongside the sequence: finish the whole cutscene state only
	// once the spoken line is done too.
	if (bChestLineDone)
	{
		EndChestDialogue();
	}
}

void USOTMDemoPhase4WorldSubsystem::RefreshChestMarker()
{
	if (!ChestMarker)
	{
		return;
	}
	const bool bFindChestActive = Objectives &&
		Objectives->GetActiveChapterOneObjective().ObjectiveId == USOTMObjectiveSubsystem::FindChestId;
	const bool bShow = bFindChestActive && !bChestMarkerDismissed && !bNearChest && !bChestDialogueActive
		&& PlayerState && !PlayerState->IsPhase4ChestOpened()
		&& !PlayerState->IsPlayerDead() && !PlayerState->IsGameOver();
	ChestMarker->SetHiddenInGame(!bShow);
	ChestMarker->SetVisibility(bShow);
}

void USOTMDemoPhase4WorldSubsystem::HandleObjectiveChanged(FSOTMObjectiveData Objective)
{
	(void)Objective;
	RefreshChestMarker();
}

void USOTMDemoPhase4WorldSubsystem::PlayChestDialogueLine()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		EndChestDialogue();
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
					SNew(STextBlock)
					.Font(SOTMSubtitle::Font())
					.Text(NSLOCTEXT("SOTM", "TimmyName", "TIMMY"))
					.ColorAndOpacity(FLinearColor(0.72f, 0.16f, 0.88f, 1.0f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SAssignNew(ChestDialogueLineText, STextBlock)
					.Font(SOTMSubtitle::Font())
					.Text(FText::GetEmpty())
					.ColorAndOpacity(FLinearColor::White)
					.WrapTextAt(900.0f)
					.Justification(ETextJustify::Center)
				]
			]
		];
	ChestDialogueSubtitleRoot = Content;
	Viewport->AddViewportWidgetContent(ChestDialogueSubtitleRoot.ToSharedRef(), 950);

	float VODuration = 0.0f;
	if (USoundBase* VO = LoadObject<USoundBase>(nullptr, SOTMPhase4Private::TimmyChestVO))
	{
		VODuration = VO->GetDuration();
		ChestDialogueAudio = UGameplayStatics::SpawnSound2D(this, VO, 1.0f, 1.0f, 0.0f, nullptr, false, true);
	}
	if (ChestDialogueAudio)
	{
		ChestDialogueAudio->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleChestDialogueAudioFinished);
		UE_LOG(LogSOTMPhase4, Display, TEXT("Chest dialogue VO: playing %s duration=%.2fs"),
			SOTMPhase4Private::TimmyChestVO, VODuration);
		// Safety net in case OnAudioFinished never fires.
		World->GetTimerManager().SetTimer(
			ChestDialogueTimeoutTimer, this, &ThisClass::FinishChestDialogueLine, VODuration + 1.0f, false);
		StartChestDialogueTypewriter(VODuration);
		return;
	}

	UE_LOG(LogSOTMPhase4, Warning, TEXT("Chest dialogue VO unavailable (%s) - falling back to timed subtitle."),
		SOTMPhase4Private::TimmyChestVO);
	World->GetTimerManager().SetTimer(
		ChestDialogueTimeoutTimer, this, &ThisClass::FinishChestDialogueLine, 6.5f, false);
	StartChestDialogueTypewriter(5.5f);
}

void USOTMDemoPhase4WorldSubsystem::StartChestDialogueTypewriter(const float TargetDuration)
{
	UWorld* World = GetWorld();
	if (!World || !ChestDialogueLineText.IsValid())
	{
		return;
	}
	ChestDialogueFullLine = NSLOCTEXT("SOTM", "TimmyChestLine",
		"That key leads to her true form.\nYou\u2019ll need both abilities to survive.").ToString();
	ChestDialogueRevealedChars = 0;
	ChestDialogueLineText->SetText(FText::GetEmpty());

	// Same pacing as the other Timmy dialogues: finish just before the VO ends.
	const int32 CharCount = ChestDialogueFullLine.Len();
	const float PacedInterval = TargetDuration > 0.0f ? (TargetDuration * 0.85f) / FMath::Max(CharCount, 1) : 0.045f;
	World->GetTimerManager().SetTimer(
		ChestDialogueTypewriterTimer, this, &ThisClass::TickChestDialogueTypewriter,
		FMath::Clamp(PacedInterval, 0.015f, 0.06f), true);
}

void USOTMDemoPhase4WorldSubsystem::TickChestDialogueTypewriter()
{
	if (!ChestDialogueLineText.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChestDialogueTypewriterTimer);
		}
		return;
	}
	++ChestDialogueRevealedChars;
	ChestDialogueLineText->SetText(FText::FromString(ChestDialogueFullLine.Left(ChestDialogueRevealedChars)));
	if (ChestDialogueRevealedChars >= ChestDialogueFullLine.Len())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChestDialogueTypewriterTimer);
		}
	}
}

void USOTMDemoPhase4WorldSubsystem::HandleChestDialogueAudioFinished()
{
	if (ChestDialogueAudio)
	{
		ChestDialogueAudio->OnAudioFinished.RemoveAll(this);
	}
	ChestDialogueAudio = nullptr;
	FinishChestDialogueLine();
}

void USOTMDemoPhase4WorldSubsystem::FinishChestDialogueLine()
{
	if (!bChestDialogueActive || bChestLineDone)
	{
		return;
	}
	bChestLineDone = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChestDialogueTimeoutTimer);
		World->GetTimerManager().ClearTimer(ChestDialogueTypewriterTimer);
		World->GetTimerManager().ClearTimer(ChestDialoguePreDelayTimer);
	}
	if (ChestDialogueAudio)
	{
		ChestDialogueAudio->OnAudioFinished.RemoveAll(this);
		ChestDialogueAudio->Stop();
		ChestDialogueAudio = nullptr;
	}
	if (ChestDialogueSubtitleRoot.IsValid())
	{
		UWorld* World = GetWorld();
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(ChestDialogueSubtitleRoot.ToSharedRef());
		}
	}
	ChestDialogueSubtitleRoot.Reset();
	ChestDialogueLineText.Reset();
	ChestDialogueFullLine.Empty();
	ChestDialogueRevealedChars = 0;

	// If the sequence is still running, keep the cutscene state (UI hidden, input locked)
	// until it finishes; HandleChestSequenceFinished will end it.
	if (!bChestSequencePlaying)
	{
		EndChestDialogue();
	}
}

bool USOTMDemoPhase4WorldSubsystem::IsPlayerWithinChestRange() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !ChestArt || !PlayerState || !PlayerState->IsBoundPlayerActor(Pawn))
	{
		return false;
	}
	// Chest trigger radius is 250 - small margin for the player capsule.
	return FVector::Dist2D(Pawn->GetActorLocation(), ChestClosedTransform.GetLocation()) <= 250.0f + 90.0f
		&& FMath::Abs(Pawn->GetActorLocation().Z - ChestClosedTransform.GetLocation().Z) <= 250.0f + 150.0f;
}

void USOTMDemoPhase4WorldSubsystem::ResyncChestProximity()
{
	// Overlap begin/end events can be missed or flipped while the sequence/cutscene lock
	// is active, which used to force the player to leave and re-enter the chest trigger
	// before the key could be collected. Re-derive the state from the actual position.
	bNearChest = IsPlayerWithinChestRange();
}

void USOTMDemoPhase4WorldSubsystem::EndChestDialogue()
{
	if (!bChestDialogueActive)
	{
		return;
	}
	bChestDialogueActive = false;
	bChestSequencePlaying = false;
	bChestLineDone = false;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ChestSequenceTimeoutTimer);
	}
	if (ChestSequencePlayer)
	{
		ChestSequencePlayer->OnFinished.RemoveAll(this);
		ChestSequencePlayer->Stop();
		ChestSequencePlayer = nullptr;
	}
	if (ChestSequenceActor)
	{
		ChestSequenceActor->Destroy();
		ChestSequenceActor = nullptr;
	}
	if (World)
	{
		World->GetTimerManager().ClearTimer(ChestDialoguePreDelayTimer);
		World->GetTimerManager().ClearTimer(ChestDialogueTimeoutTimer);
		World->GetTimerManager().ClearTimer(ChestDialogueTypewriterTimer);
	}
	ChestDialogueFullLine.Empty();
	ChestDialogueRevealedChars = 0;

	if (ChestDialogueAudio)
	{
		ChestDialogueAudio->OnAudioFinished.RemoveAll(this);
		ChestDialogueAudio->Stop();
		ChestDialogueAudio = nullptr;
	}
	if (ChestDialogueSubtitleRoot.IsValid())
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(ChestDialogueSubtitleRoot.ToSharedRef());
		}
	}
	ChestDialogueSubtitleRoot.Reset();
	ChestDialogueLineText.Reset();

	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Pair : ChestDialogueHiddenWidgets)
	{
		if (UUserWidget* Widget = Pair.Key.Get())
		{
			Widget->SetVisibility(Pair.Value);
		}
	}
	ChestDialogueHiddenWidgets.Reset();

	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		PC->bShowMouseCursor = bPreviousMouseCursorChestDialogue;
		if (bPreviousMouseCursorChestDialogue)
		{
			FInputModeGameAndUI RestoredInputMode;
			RestoredInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			RestoredInputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(RestoredInputMode);
		}
		else
		{
			PC->SetInputMode(FInputModeGameOnly());
		}
		// The cutscene ran in UI-only mode: hand keyboard focus back to the game viewport and
		// drop any stale key state, otherwise the FIRST E press afterwards is swallowed.
		PC->FlushPressedKeys();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetUserFocusToGameViewport(0);
		}
	}
	if (PlayerState && bChestDialogueInputLockHeld)
	{
		PlayerState->ReleaseInputLock(ESOTMInputLockReason::Cinematic);
	}
	bChestDialogueInputLockHeld = false;

	// Cousins may speak again.
	if (World)
	{
		if (USOTMDemoPhase2WorldSubsystem* Phase2 = World->GetSubsystem<USOTMDemoPhase2WorldSubsystem>())
		{
			Phase2->SetDialogueSuppressed(false);
		}
	}

	ResyncChestProximity();

	// Dialogue over: bring back the normal "chest opened" feedback and the key prompt,
	// unless the player died / the game ended in the meantime.
	if (PlayerState && !PlayerState->IsPlayerDead() && !PlayerState->IsGameOver())
	{
		OnNotification.Broadcast(
			NSLOCTEXT("SOTM", "ChestOpened", "CHEST OPENED"),
			NSLOCTEXT("SOTM", "TakeTheKey", "TAKE THE KEY"));
		RefreshPrompt();
	}
}

void USOTMDemoPhase4WorldSubsystem::InteractWithKey()
{
	if (!Objectives || !PlayerState || PlayerState->HasPhase4GateKey())
	{
		return;
	}
	const ESOTMPhase4ActionResult Result = Objectives->TryCollectPhase4GateKey();
	if (Result == ESOTMPhase4ActionResult::Success)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase4Private::KeyAcquiredSound))
		{
			UGameplayStatics::PlaySound2D(this, Sound, 0.58f);
		}
		// The key has been picked up - clear its visual so it can't be "collected" again
		// (HasPhase4GateKey() is the actual guard; this just removes the floating mesh).
		if (KeyPresentation)
		{
			KeyPresentation->Destroy();
			KeyPresentation = nullptr;
		}
		OnNotification.Broadcast(
			NSLOCTEXT("SOTM", "GateKeyAcquired", "GATE KEY ACQUIRED"),
			NSLOCTEXT("SOTM", "ReachGateUpdated", "OBJECTIVE UPDATED  -  REACH THE GATE"));
	}
	RefreshPrompt();
	UE_LOG(LogSOTMPhase4, Display, TEXT("Key interaction result=%d"), static_cast<int32>(Result));
}

void USOTMDemoPhase4WorldSubsystem::InteractWithGate()
{
	// Legacy gate path disabled: the Mansion Gate (ASOTMKeyGateActor) handles unlocking.
	if (true)
	{
		return;
	}
	if (!Objectives || !PlayerState || PlayerState->IsPhase4GateUnlocked())
	{
		return;
	}
	const ESOTMPhase4ActionResult Result = Objectives->TryUnlockPhase4Gate();
	if (Result == ESOTMPhase4ActionResult::Success)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase4Private::GateOpenSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GateArt ? GateArt->GetActorLocation() : FVector::ZeroVector, 0.62f);
		}
		// The "GATE UNLOCKED" popup is shown when the Isabella cutscene ends (see
		// EndIsabelCutscene), and the Demo Complete screen waits for it as well.
		BeginIsabelCutscene();
		BeginGatePresentation(false);
	}
	else if (Result != ESOTMPhase4ActionResult::AlreadyCompleted)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase4Private::GateLockedSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GateArt ? GateArt->GetActorLocation() : FVector::ZeroVector, 0.48f);
		}
		OnNotification.Broadcast(
			NSLOCTEXT("SOTM", "GateLocked", "GATE LOCKED"),
			Objectives->GetGateRequirementFeedback());
	}
	RefreshPrompt();
	UE_LOG(LogSOTMPhase4, Display, TEXT("Gate interaction result=%d"), static_cast<int32>(Result));
}

void USOTMDemoPhase4WorldSubsystem::BeginChestPresentation(const bool bRestoreImmediately)
{
	if (!ChestArt || !GetWorld())
	{
		return;
	}
	ChestAnimationAlpha = bRestoreImmediately ? 1.0f : 0.0f;
	if (UStaticMeshComponent* ChestComponent = ChestArt->GetChestMesh())
	{
		ChestComponent->SetMobility(EComponentMobility::Movable);
	}
	// If the key was already collected (e.g. loading a save from after this point), there's
	// nothing left to reveal - the chest just shows as opened and empty.
	if (!KeyPresentation && PlayerState && !PlayerState->HasPhase4GateKey())
	{
		if (UStaticMesh* KeyMesh = LoadObject<UStaticMesh>(nullptr, SOTMPhase4Private::KeyMeshPath))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			KeyPresentation = GetWorld()->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(), ChestClosedTransform.GetLocation() + FVector(0, 0, 125),
				ChestClosedTransform.Rotator(), Params);
			if (KeyPresentation)
			{
				UStaticMeshComponent* KeyComponent = KeyPresentation->GetStaticMeshComponent();
				KeyComponent->SetMobility(EComponentMobility::Movable);
				KeyComponent->SetStaticMesh(KeyMesh);
				KeyComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				KeyPresentation->SetActorScale3D(FVector(2.2f));
			}
			// The key mesh itself has no collision (it's purely visual, floating and
			// spinning) - collecting it is driven entirely by the chest's own trigger
			// (bNearChest), not a separate anchor/sphere.
		}
	}
	if (bRestoreImmediately)
	{
		UpdateChestPresentation();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(
		ChestAnimationTimer, this, &ThisClass::UpdateChestPresentation, 0.03f, true);
}

void USOTMDemoPhase4WorldSubsystem::UpdateChestPresentation()
{
	if (!ChestArt || !GetWorld())
	{
		return;
	}
	ChestAnimationAlpha = FMath::Min(1.0f, ChestAnimationAlpha + 0.04f);
	const float Ease = FMath::InterpEaseOut(0.0f, 1.0f, ChestAnimationAlpha, 2.0f);
	FTransform OpenTransform = ChestClosedTransform;
	OpenTransform.SetLocation(ChestClosedTransform.GetLocation() + FVector(0, 0, 10.0f * Ease));
	FRotator Rotation = ChestClosedTransform.Rotator();
	Rotation.Roll += 5.0f * Ease;
	OpenTransform.SetRotation(Rotation.Quaternion());
	ChestArt->SetActorTransform(OpenTransform);
	if (KeyPresentation)
	{
		KeyPresentation->SetActorLocation(ChestClosedTransform.GetLocation() + FVector(0, 0, 125.0f + 65.0f * Ease));
		KeyPresentation->SetActorRotation(Rotation + FRotator(0, 120.0f * Ease, 0));
	}
	if (ChestAnimationAlpha >= 1.0f)
	{
		GetWorld()->GetTimerManager().ClearTimer(ChestAnimationTimer);
	}
}

void USOTMDemoPhase4WorldSubsystem::BeginGatePresentation(const bool bRestoreImmediately)
{
	if (!GateArt || !GetWorld())
	{
		return;
	}
	if (UStaticMeshComponent* Component = GateArt->GetStaticMeshComponent())
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	bNearGate = false;
	OnPromptChanged.Broadcast(false, FText::GetEmpty());
	GateAnimationAlpha = bRestoreImmediately ? 1.0f : 0.0f;
	bGateAnimationRunning = !bRestoreImmediately;
	if (bRestoreImmediately)
	{
		UpdateGatePresentation();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(
		GateAnimationTimer, this, &ThisClass::UpdateGatePresentation, 0.03f, true);
}

void USOTMDemoPhase4WorldSubsystem::UpdateGatePresentation()
{
	if (!GateArt || !GetWorld())
	{
		return;
	}
	GateAnimationAlpha = FMath::Min(1.0f, GateAnimationAlpha + 0.025f);
	const float Ease = FMath::InterpEaseInOut(0.0f, 1.0f, GateAnimationAlpha, 2.0f);
	GateArt->SetActorLocation(GateClosedTransform.GetLocation() + FVector(0, 0, 520.0f * Ease));
	if (GateAnimationAlpha >= 1.0f)
	{
		GetWorld()->GetTimerManager().ClearTimer(GateAnimationTimer);
		bGateAnimationRunning = false;
		if ((!PlayerState || !PlayerState->IsPhase4DemoCompleted()) && !bIsabelCutsceneActive)
		{
			GetWorld()->GetTimerManager().SetTimer(
				DemoCompleteTimer, this, &ThisClass::FinishGatePresentation, 0.8f, false);
		}
	}
}

void USOTMDemoPhase4WorldSubsystem::FinishGatePresentation()
{
	if (Objectives)
	{
		const ESOTMPhase4ActionResult Result = Objectives->TryCompletePhase4Demo();
		if (Result != ESOTMPhase4ActionResult::Success && Result != ESOTMPhase4ActionResult::AlreadyCompleted)
		{
			UE_LOG(LogSOTMPhase4, Error, TEXT("Demo completion persistence failed result=%d"), static_cast<int32>(Result));
			return;
		}
	}
	ShowDemoComplete();
}

void USOTMDemoPhase4WorldSubsystem::ShowDemoComplete()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || DemoCompleteWidget)
	{
		return;
	}
	DemoCompleteWidget = CreateWidget<USOTMDemoCompleteWidget>(PC, USOTMDemoCompleteWidget::StaticClass());
	if (!DemoCompleteWidget)
	{
		return;
	}
	DemoCompleteWidget->AddToViewport(1000);
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, SOTMPhase4Private::DemoCompleteSound))
	{
		UGameplayStatics::PlaySound2D(this, Sound, 0.58f);
	}
	PC->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	PC->SetInputMode(InputMode);
	if (PlayerState)
	{
		PlayerState->AcquireInputLock(ESOTMInputLockReason::Custom);
		bDemoInputLockHeld = true;
	}
	UE_LOG(LogSOTMPhase4, Display, TEXT("DEMO COMPLETE presentation shown; unfinished boss content not loaded."));
}

void USOTMDemoPhase4WorldSubsystem::HideDemoComplete()
{
	if (DemoCompleteWidget)
	{
		DemoCompleteWidget->RemoveFromParent();
		DemoCompleteWidget = nullptr;
	}
	if (PlayerState && bDemoInputLockHeld)
	{
		PlayerState->ReleaseInputLock(ESOTMInputLockReason::Custom);
	}
	bDemoInputLockHeld = false;
}

void USOTMDemoPhase4WorldSubsystem::HandlePlayerUnavailable(AActor* PlayerActor)
{
	(void)PlayerActor;
	EndChestDialogue();
	EndIsabelCutscene(true);
	bNearChest = false;
	bNearGate = false;
	RefreshChestMarker();
	OnPromptChanged.Broadcast(false, FText::GetEmpty());
}

void USOTMDemoPhase4WorldSubsystem::HandlePlayerRespawned(AActor* PlayerActor)
{
	(void)PlayerActor;
	RefreshChestMarker();
	RefreshPrompt();
}

#if !UE_BUILD_SHIPPING
void USOTMDemoPhase4WorldSubsystem::BeginDevelopmentDeathAfterKeyAcceptance()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	USOTMDemoPhase2WorldSubsystem* Phase2 = World
		? World->GetSubsystem<USOTMDemoPhase2WorldSubsystem>() : nullptr;
	ASOTMCousinCharacter* Cousin = nullptr;
	if (World)
	{
		for (TActorIterator<ASOTMCousinCharacter> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Cousin = *It;
				break;
			}
		}
	}
	if (!PlayerState || !Objectives || !World || !Pawn || !ChestArt || !Phase2 || !Cousin)
	{
		UE_LOG(LogSOTMPhase4, Error, TEXT("[Phase4DeathAfterKey] setup failed state=%d objectives=%d world=%d pawn=%d chest=%d phase2=%d cousin=%d"),
			PlayerState != nullptr, Objectives != nullptr, World != nullptr, Pawn != nullptr,
			ChestArt != nullptr, Phase2 != nullptr, Cousin != nullptr);
		return;
	}

	PlayerState->SetPhase3ProgressForDebug(80, 330, true, 1);
	PlayerState->SetPhase4ProgressForDebug(false, false, false, false);
	Pawn->SetActorLocation(ChestArt->GetActorLocation() + FVector(180.0f, 0.0f, 90.0f),
		false, nullptr, ETeleportType::TeleportPhysics);
	bNearChest = true;
	RefreshPrompt();
	InteractWithChest();
	// Chest-open no longer auto-grants the key - collect it immediately too so this dev route
	// still ends up in the same state it did before that was split apart.
	bNearKey = true;
	InteractWithKey();

	const int32 LivesBeforeCatch = PlayerState->GetCurrentLives();
	const int32 AvailableBeforeCatch = PlayerState->GetAvailableCoins();
	const int32 LifetimeBeforeCatch = PlayerState->GetLifetimeCoinsCollected();
	const bool bPreconditions = PlayerState->IsPhase4ChestOpened() && PlayerState->HasPhase4GateKey() &&
		PlayerState->IsSpeedBoostUnlocked() &&
		Objectives->GetActiveChapterOneObjective().ObjectiveId == USOTMObjectiveSubsystem::ReachGateId;
	UE_LOG(LogSOTMPhase4, Display,
		TEXT("[Phase4DeathAfterKey] PRE catch preconditions=%d lives=%d coins=%d lifetime=%d chest=%d key=%d boost=%d active=%s"),
		bPreconditions, LivesBeforeCatch, AvailableBeforeCatch, LifetimeBeforeCatch,
		PlayerState->IsPhase4ChestOpened(), PlayerState->HasPhase4GateKey(),
		PlayerState->IsSpeedBoostUnlocked(), *Objectives->GetActiveChapterOneObjective().ObjectiveId.ToString());

	const FVector CatchLocation = Cousin->GetActorLocation() + Cousin->GetActorForwardVector() * 145.0f;
	Pawn->SetActorLocation(CatchLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Pawn->SetActorRotation((Cousin->GetActorLocation() - CatchLocation).Rotation());
	const bool bCatchStarted = Phase2->TryStartCousinCatch(Cousin, Pawn);
	UE_LOG(LogSOTMPhase4, Display, TEXT("[Phase4DeathAfterKey] production Cousin catch started=%d cousin=%s"),
		bCatchStarted, *GetNameSafe(Cousin));

	FTimerHandle VerifyTimer;
	World->GetTimerManager().SetTimer(VerifyTimer, FTimerDelegate::CreateWeakLambda(this,
		[this, LivesBeforeCatch, AvailableBeforeCatch, LifetimeBeforeCatch]
		{
			const bool bPass = PlayerState && Objectives && !PlayerState->IsPlayerDead() &&
				PlayerState->GetCurrentLives() == LivesBeforeCatch - 1 &&
				PlayerState->IsPhase4ChestOpened() && PlayerState->HasPhase4GateKey() &&
				PlayerState->IsSpeedBoostUnlocked() &&
				PlayerState->GetAvailableCoins() == AvailableBeforeCatch &&
				PlayerState->GetLifetimeCoinsCollected() == LifetimeBeforeCatch &&
				Objectives->GetActiveChapterOneObjective().ObjectiveId == USOTMObjectiveSubsystem::ReachGateId;
			UE_LOG(LogSOTMPhase4, Display,
				TEXT("[Phase4DeathAfterKey] PASS=%d lives=%d dead=%d chest=%d key=%d boost=%d coins=%d lifetime=%d active=%s"),
				bPass, PlayerState ? PlayerState->GetCurrentLives() : -1,
				PlayerState && PlayerState->IsPlayerDead(),
				PlayerState && PlayerState->IsPhase4ChestOpened(),
				PlayerState && PlayerState->HasPhase4GateKey(),
				PlayerState && PlayerState->IsSpeedBoostUnlocked(),
				PlayerState ? PlayerState->GetAvailableCoins() : -1,
				PlayerState ? PlayerState->GetLifetimeCoinsCollected() : -1,
				Objectives ? *Objectives->GetActiveChapterOneObjective().ObjectiveId.ToString() : TEXT("None"));
			if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				Controller->ConsoleCommand(TEXT("quit"), true);
			}
		}), 8.0f, false);
}

void USOTMDemoPhase4WorldSubsystem::BeginDevelopmentAcceptanceRoute()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerState || !Objectives || !World || !Pawn || !ChestArt || !GateArt)
	{
		UE_LOG(LogSOTMPhase4, Error, TEXT("[Phase4Acceptance] setup failed."));
		return;
	}
	PlayerState->SetPhase3ProgressForDebug(330, 330, true, 1);
	PlayerState->SetPhase4ProgressForDebug(false, false, false, false);
	Pawn->SetActorLocation(ChestArt->GetActorLocation() + FVector(180.0f, 0.0f, 90.0f), false, nullptr, ETeleportType::TeleportPhysics);
	bNearChest = true;
	RefreshPrompt();
	FScreenshotRequest::RequestScreenshot(
		FPaths::ProjectSavedDir() / TEXT("Screenshots/WindowsEditor/Phase4/01_FindChest_Prompt.png"), true, false);
	UE_LOG(LogSOTMPhase4, Display, TEXT("[Phase4Acceptance] START coins=%d lifetime=%d boost=%d active=%s"),
		PlayerState->GetAvailableCoins(), PlayerState->GetLifetimeCoinsCollected(),
		PlayerState->IsSpeedBoostUnlocked(), *Objectives->GetActiveChapterOneObjective().ObjectiveId.ToString());

	FTimerHandle OpenChestTimer;
	World->GetTimerManager().SetTimer(OpenChestTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		InteractWithChest();
		// See BeginDevelopmentDeathAfterKeyAcceptance - collecting is a separate step from opening now.
		bNearKey = true;
		InteractWithKey();
		const bool bPass = PlayerState && PlayerState->IsPhase4ChestOpened() && PlayerState->HasPhase4GateKey() &&
			Objectives && Objectives->GetActiveChapterOneObjective().ObjectiveId == USOTMObjectiveSubsystem::ReachGateId;
		UE_LOG(LogSOTMPhase4, Display, TEXT("[Phase4Acceptance] CHEST pass=%d opened=%d key=%d active=%s"),
			bPass, PlayerState && PlayerState->IsPhase4ChestOpened(), PlayerState && PlayerState->HasPhase4GateKey(),
			Objectives ? *Objectives->GetActiveChapterOneObjective().ObjectiveId.ToString() : TEXT("None"));
		FScreenshotRequest::RequestScreenshot(
			FPaths::ProjectSavedDir() / TEXT("Screenshots/WindowsEditor/Phase4/02_GateKey_Acquired.png"), true, false);
	}), 0.8f, false);

	FTimerHandle ReachGateTimer;
	World->GetTimerManager().SetTimer(ReachGateTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		UWorld* CurrentWorld = GetWorld();
		APlayerController* CurrentPC = CurrentWorld ? CurrentWorld->GetFirstPlayerController() : nullptr;
		APawn* CurrentPawn = CurrentPC ? CurrentPC->GetPawn() : nullptr;
		if (!CurrentPawn || !GateArt)
		{
			return;
		}
		CurrentPawn->SetActorLocation(GateArt->GetActorLocation() + FVector(220.0f, 0.0f, 90.0f), false, nullptr, ETeleportType::TeleportPhysics);
		bNearChest = false;
		bNearGate = true;
		RefreshPrompt();
		FScreenshotRequest::RequestScreenshot(
			FPaths::ProjectSavedDir() / TEXT("Screenshots/WindowsEditor/Phase4/03_ReachGate_Prompt.png"), true, false);
		InteractWithGate();
	}), 2.0f, false);

	FTimerHandle CompleteTimer;
	World->GetTimerManager().SetTimer(CompleteTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		const bool bPass = PlayerState && PlayerState->IsPhase4GateUnlocked() && PlayerState->IsPhase4DemoCompleted() && DemoCompleteWidget;
		UE_LOG(LogSOTMPhase4, Display, TEXT("[Phase4Acceptance] COMPLETE pass=%d gate=%d demo=%d widget=%d"),
			bPass, PlayerState && PlayerState->IsPhase4GateUnlocked(),
			PlayerState && PlayerState->IsPhase4DemoCompleted(), DemoCompleteWidget != nullptr);
		FScreenshotRequest::RequestScreenshot(
			FPaths::ProjectSavedDir() / TEXT("Screenshots/WindowsEditor/Phase4/04_DemoComplete.png"), true, false);
	}), 5.2f, false);

	FTimerHandle ReturnTimer;
	World->GetTimerManager().SetTimer(ReturnTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		UE_LOG(LogSOTMPhase4, Display, TEXT("[Phase4Acceptance] RETURN_TO_MAIN_MENU requested through production Player State route."));
		if (PlayerState)
		{
			PlayerState->ReturnToMainMenu();
		}
	}), 7.0f, false);
}

void USOTMDemoPhase4WorldSubsystem::RunDevelopmentDataAcceptance()
{
	if (!PlayerState || !Objectives)
	{
		return;
	}

	PlayerState->SetPhase3ProgressForDebug(0, 0, false, 0);
	PlayerState->SetPhase4ProgressForDebug(false, false, false, false);
	const bool bIncompleteChestRejected = Objectives->TryOpenPhase4Chest() ==
		ESOTMPhase4ActionResult::PreviousObjectivesIncomplete;
	const bool bIncompleteGateRejected = Objectives->TryUnlockPhase4Gate() ==
		ESOTMPhase4ActionResult::PreviousObjectivesIncomplete;

	PlayerState->SetPhase3ProgressForDebug(330, 330, false, 0);
	const bool bNoBoostChestRejected = Objectives->TryOpenPhase4Chest() ==
		ESOTMPhase4ActionResult::MissingSpeedBoost;
	const bool bNoBoostGateRejected = Objectives->TryUnlockPhase4Gate() ==
		ESOTMPhase4ActionResult::MissingSpeedBoost;

	PlayerState->SetPhase3ProgressForDebug(80, 330, true, 1);
	const bool bNoKeyGateRejected = Objectives->TryUnlockPhase4Gate() ==
		ESOTMPhase4ActionResult::MissingGateKey;
	const bool bChestAccepted = Objectives->TryOpenPhase4Chest() == ESOTMPhase4ActionResult::Success;
	const bool bDuplicateChestRejected = Objectives->TryOpenPhase4Chest() ==
		ESOTMPhase4ActionResult::AlreadyCompleted;
	const bool bGateAccepted = Objectives->TryUnlockPhase4Gate() == ESOTMPhase4ActionResult::Success;
	const bool bDuplicateGateRejected = Objectives->TryUnlockPhase4Gate() ==
		ESOTMPhase4ActionResult::AlreadyCompleted;
	const bool bDemoAccepted = Objectives->TryCompletePhase4Demo() == ESOTMPhase4ActionResult::Success;
	const bool bDuplicateDemoRejected = Objectives->TryCompletePhase4Demo() ==
		ESOTMPhase4ActionResult::AlreadyCompleted;
	const bool bRequirementMatrix = bIncompleteChestRejected && bIncompleteGateRejected &&
		bNoBoostChestRejected && bNoBoostGateRejected && bNoKeyGateRejected;
	const bool bOneShotFlow = bChestAccepted && bDuplicateChestRejected && bGateAccepted &&
		bDuplicateGateRejected && bDemoAccepted && bDuplicateDemoRejected;

	const FString SlotA(TEXT("SOTM_Phase4_Acceptance_SlotA"));
	const FString SlotB(TEXT("SOTM_Phase4_Acceptance_SlotB"));
	PlayerState->SetPhase3ProgressForDebug(80, 330, true, 1);
	PlayerState->SetPhase4ProgressForDebug(true, true, true, true);
	const bool bSavedA = PlayerState->SavePlayerStateToSlot(SlotA);
	PlayerState->SetPhase3ProgressForDebug(0, 0, false, 0);
	PlayerState->SetPhase4ProgressForDebug(false, false, false, false);
	const bool bSavedB = PlayerState->SavePlayerStateToSlot(SlotB);
	const bool bLoadedA = PlayerState->LoadPlayerStateFromSlot(SlotA, false);
	const bool bSlotAState = bLoadedA && PlayerState->IsPhase4ChestOpened() && PlayerState->HasPhase4GateKey() &&
		PlayerState->IsPhase4GateUnlocked() && PlayerState->IsPhase4DemoCompleted() &&
		PlayerState->IsSpeedBoostUnlocked() && PlayerState->GetLifetimeCoinsCollected() == 330 &&
		PlayerState->GetAvailableCoins() == 80;
	const bool bLoadedB = PlayerState->LoadPlayerStateFromSlot(SlotB, false);
	const bool bSlotBState = bLoadedB && !PlayerState->IsPhase4ChestOpened() && !PlayerState->HasPhase4GateKey() &&
		!PlayerState->IsPhase4GateUnlocked() && !PlayerState->IsPhase4DemoCompleted() &&
		!PlayerState->IsSpeedBoostUnlocked() && PlayerState->GetLifetimeCoinsCollected() == 0;
	PlayerState->ResetRuntimeStateForNewGame();
	const bool bNewGameReset = !PlayerState->IsPhase4ChestOpened() && !PlayerState->HasPhase4GateKey() &&
		!PlayerState->IsPhase4GateUnlocked() && !PlayerState->IsPhase4DemoCompleted();
	UE_LOG(LogSOTMPhase4, Display,
		TEXT("[Phase4DataAcceptance] PASS=%d requirements=%d oneShot=%d saveA=%d loadA=%d stateA=%d saveB=%d loadB=%d stateB=%d newGameReset=%d"),
		bRequirementMatrix && bOneShotFlow && bSavedA && bSavedB && bSlotAState && bSlotBState && bNewGameReset,
		bRequirementMatrix, bOneShotFlow, bSavedA, bLoadedA, bSlotAState, bSavedB, bLoadedB, bSlotBState, bNewGameReset);
}
#endif
