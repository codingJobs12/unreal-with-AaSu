#include "Gate/SOTMKeyGateActor.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Demo/SOTMDemoPhase4WorldSubsystem.h"
#include "Demo/SOTMPhase4Types.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "AI/SOTMBossVitalComponent.h"
#include "EngineUtils.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Objective/SOTMObjectiveSubsystem.h"
#include "SOTMPlayerStateSubsystem.h"
#include "TimerManager.h"
#include "Camera/CameraActor.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Sound/SoundBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMKeyGate, Log, All);

namespace SOTMKeyGatePrivate
{
	// Reuses the same production Interact input action every other CH1 interactable
	// (chest/key/gate, upgrade station) already binds, so pressing E behaves
	// identically everywhere in the level.
	const TCHAR* InteractActionPath = TEXT("/Game/MenuSystemPro/Blueprints/Input/CharacterOnFoot/IA_Interact.IA_Interact");
}

ASOTMKeyGateActor::ASOTMKeyGateActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	GateRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GateRoot"));
	SetRootComponent(GateRoot);

	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(GateRoot);
	GateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GateMesh->SetCollisionObjectType(ECC_WorldStatic);
	GateMesh->SetMobility(EComponentMobility::Movable);

	OverlapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapBox"));
	OverlapBox->SetupAttachment(GateRoot);
	OverlapBox->SetBoxExtent(FVector(250.0f, 250.0f, 150.0f));
	OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OverlapBox->SetCollisionObjectType(ECC_WorldDynamic);
	OverlapBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	OverlapBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	OverlapBox->SetGenerateOverlapEvents(true);
	OverlapBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleOverlapBegin);
	OverlapBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleOverlapEnd);

	// Placeholder state indicator until a real open animation/material exists -
	// red while locked, green once unlocked. Mobility must be Movable to allow
	// changing its color/intensity at runtime.
	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(GateRoot);
	StatusLight->SetMobility(EComponentMobility::Movable);
	StatusLight->SetAttenuationRadius(400.0f);
	StatusLight->SetCastShadows(false);
}

void ASOTMKeyGateActor::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	PlayerState = GameInstance ? GameInstance->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	Objectives = GameInstance ? GameInstance->GetSubsystem<USOTMObjectiveSubsystem>() : nullptr;

	// If this save already had the gate unlocked (checkpoint reload, Continue, etc.)
	// reflect that immediately instead of starting locked/red.
	bUnlocked = PlayerState.IsValid() && PlayerState->IsIsabelGateUnlocked();
	RefreshStatusLight();
	FindGateMarker();
	RefreshGateMarker();
	if (bUnlocked)
	{
		// Covers reloading into a save where the gate was already unlocked -
		// she should still be there, not only on the exact unlock moment.
		SpawnIsabelBossIfNeeded();
	}

	// Listen for the shared progress delegate too, not just the one-time check
	// above - this is what makes the "SOTM.ResetIsabelGate" testing console
	// command (and any other live change to this state) actually reflected on
	// an already-placed, already-playing gate instance instead of only taking
	// effect after a full level reload.
	if (PlayerState.IsValid())
	{
		PlayerState->OnIsabelGateProgressChanged.RemoveDynamic(this, &ThisClass::HandlePlayerStateGateProgressChanged);
		PlayerState->OnIsabelGateProgressChanged.AddDynamic(this, &ThisClass::HandlePlayerStateGateProgressChanged);

		// If the player collects the chest key WHILE already standing in the
		// overlap box (walked to the gate first, then went and got the key
		// without ever leaving/re-entering the trigger), this is what retries
		// "Reach the Gate" at that moment instead of requiring them to step out
		// and back in.
		PlayerState->OnPhase4ProgressChanged.RemoveDynamic(this, &ThisClass::HandlePhase4KeyProgressChanged);
		PlayerState->OnPhase4ProgressChanged.AddDynamic(this, &ThisClass::HandlePhase4KeyProgressChanged);
	}
	if (Objectives.IsValid())
	{
		Objectives->OnObjectiveChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveChangedForMarker);
		Objectives->OnObjectiveChanged.AddDynamic(this, &ThisClass::HandleObjectiveChangedForMarker);
	}

	BindInteractInput();
}

void ASOTMKeyGateActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindInputRetryTimer);
	}
	if (PlayerState.IsValid())
	{
		PlayerState->OnIsabelGateProgressChanged.RemoveDynamic(this, &ThisClass::HandlePlayerStateGateProgressChanged);
		PlayerState->OnPhase4ProgressChanged.RemoveDynamic(this, &ThisClass::HandlePhase4KeyProgressChanged);
	}
	if (Objectives.IsValid())
	{
		Objectives->OnObjectiveChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveChangedForMarker);
	}
	UnbindInteractInput();
	EndIntroCinematic();
	HideLetterbox();
	HideFlash();
	Super::EndPlay(EndPlayReason);
}

void ASOTMKeyGateActor::HandlePlayerStateGateProgressChanged(
	const bool bReached, const bool bHasKey, const bool bUnlockedParam)
{
	(void)bHasKey;
	if (bReached)
	{
		bMarkerDismissed = true;
		RefreshGateMarker();
	}
	if (bUnlocked != bUnlockedParam)
	{
		bUnlocked = bUnlockedParam;
		RefreshStatusLight();
		RefreshGateMarker();
		// Isabel is spawned (and the gate opened) after the intro dialogue - see
		// OpenGateAndSpawnIsabel().
	}
	RefreshPrompt();
}

void ASOTMKeyGateActor::HandlePhase4KeyProgressChanged(
	const bool bChestOpened, const bool bHasGateKey, const bool bGateUnlocked, const bool bDemoCompleted)
{
	(void)bChestOpened;
	(void)bGateUnlocked;
	(void)bDemoCompleted;
	if (bHasGateKey && bPlayerInRange && !bUnlocked && Objectives.IsValid())
	{
		Objectives->TryReachIsabelGate();
	}
	RefreshGateMarker();
	RefreshPrompt();
}

void ASOTMKeyGateActor::HandleOverlapBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!PlayerState.IsValid() || !PlayerState->IsBoundPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInRange = true;
	bMarkerDismissed = true; // player reached the gate: marker off for good

	if (!bUnlocked && Objectives.IsValid())
	{
		Objectives->TryReachIsabelGate();
	}
	RefreshGateMarker();
	RefreshPrompt();
}

void ASOTMKeyGateActor::HandleOverlapEnd(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;

	if (!PlayerState.IsValid() || !PlayerState->IsBoundPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInRange = false;
	RefreshPrompt();
}

void ASOTMKeyGateActor::BindInteractInput()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	if (!Input)
	{
		// The player's input component may not exist yet this early (e.g. right at
		// level start) - retry shortly instead of giving up, mirroring the same
		// pattern the Phase 4 interactables use for their own Interact binding.
		if (World)
		{
			World->GetTimerManager().SetTimer(
				BindInputRetryTimer, this, &ThisClass::BindInteractInput, 0.5f, false);
		}
		return;
	}
	if (BoundEnhancedInput.IsValid())
	{
		return;
	}
	InteractInputAction = LoadObject<UInputAction>(nullptr, SOTMKeyGatePrivate::InteractActionPath);
	if (!InteractInputAction)
	{
		UE_LOG(LogSOTMKeyGate, Error, TEXT("SOTMKeyGateActor: IA_Interact asset failed to load."));
		return;
	}
	FEnhancedInputActionEventBinding& Binding = Input->BindAction(
		InteractInputAction, ETriggerEvent::Started, this, &ThisClass::HandleInteractInput);
	InteractBindingHandle = Binding.GetHandle();
	BoundEnhancedInput = Input;

#if !UE_BUILD_SHIPPING
	// Testing only - see header comment. UEnhancedInputComponent explicitly
	// deletes the legacy BindKey() overload to discourage mixing the two input
	// systems, but the underlying legacy key-binding support it inherits from
	// UInputComponent still works fine at runtime - calling through a plain
	// UInputComponent* pointer (instead of the UEnhancedInputComponent* one)
	// resolves to the real, usable base-class overload instead of the deleted
	// derived-class one, so no new Input Action asset is needed for these
	// throwaway testing keys.
	UInputComponent* LegacyInput = Input;
	// U / X are now the player's attack keys (USOTMPlayerCombatWorldSubsystem), so the old
	// debug-damage bindings are intentionally not registered any more.
	(void)LegacyInput;
#endif
}

void ASOTMKeyGateActor::UnbindInteractInput()
{
	if (UEnhancedInputComponent* Input = BoundEnhancedInput.Get(); Input && InteractBindingHandle != 0)
	{
		Input->RemoveBindingByHandle(InteractBindingHandle);
	}
	BoundEnhancedInput.Reset();
	InteractBindingHandle = 0;
}

void ASOTMKeyGateActor::HandleInteractInput()
{
	if (bUnlocked || !bPlayerInRange || !PlayerState.IsValid() || !Objectives.IsValid())
	{
		return;
	}
	if (PlayerState->IsPlayerDead() || PlayerState->IsGameOver())
	{
		return;
	}

	const ESOTMPhase4ActionResult Result = Objectives->TryUnlockIsabelGate();
	switch (Result)
	{
	case ESOTMPhase4ActionResult::Success:
		bUnlocked = true;
		RefreshStatusLight();
		RefreshPrompt();
		UE_LOG(LogSOTMKeyGate, Display, TEXT("SOTMKeyGateActor: unlocked."));
		// IsabelaIntro_sequence + Isabella's dialogue.
		if (UWorld* GateWorld = GetWorld())
		{
			if (USOTMDemoPhase4WorldSubsystem* Phase4 = GateWorld->GetSubsystem<USOTMDemoPhase4WorldSubsystem>())
			{
				Phase4->PlayIsabelGateIntro();
			}
			else
			{
				UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: Phase 4 subsystem not available - Isabella intro skipped."));
			}
		}
		break;
	case ESOTMPhase4ActionResult::MissingGateKey:
		UE_LOG(LogSOTMKeyGate, Display, TEXT("SOTMKeyGateActor: interact blocked, no key yet."));
		break;
	case ESOTMPhase4ActionResult::AlreadyCompleted:
		bUnlocked = true;
		RefreshStatusLight();
		RefreshPrompt();
		break;
	default:
		UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: unlock attempt failed (result=%d)."),
			static_cast<int32>(Result));
		break;
	}
}

namespace
{
	/** Finds the floor under a point. Returns the Z of the floor surface. */
	bool FindFloorZ(UWorld* World, const FVector& From, const AActor* IgnoreA, const AActor* IgnoreB, float& OutZ)
	{
		if (!World)
		{
			return false;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FightStartFloor), false);
		Params.AddIgnoredActor(IgnoreA);
		Params.AddIgnoredActor(IgnoreB);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, From + FVector(0.0f, 0.0f, 150.0f), From - FVector(0.0f, 0.0f, 5000.0f), ECC_Visibility, Params))
		{
			OutZ = Hit.ImpactPoint.Z;
			return true;
		}
		return false;
	}

	float CapsuleHalfHeightOf(const AActor* Actor)
	{
		if (const ACharacter* Character = Cast<ACharacter>(Actor))
		{
			return Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
		}
		if (const UCapsuleComponent* Capsule = Actor ? Actor->FindComponentByClass<UCapsuleComponent>() : nullptr)
		{
			return Capsule->GetScaledCapsuleHalfHeight();
		}
		return 88.0f;
	}
}

void ASOTMKeyGateActor::OpenGateAndSpawnIsabel()
{
	// The intro entrance (gate swing + walk-in) already put the player in place - he is NOT moved now.
	// Isabella has been standing in the arena since the intro started: just wake her up and face the player.
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	EndIntroCinematic();
	if (SpawnedIsabelBoss)
	{
		// Same frame as the camera cut: the doll disappears and the real Isabella is simply standing there.
		SpawnedIsabelBoss->SetActorHiddenInGame(false);
	}
	// Already spawned for the reveal (or spawn now), standing on the floor, facing the player, AI held back...
	SpawnIsabelBossIfNeeded();
	if (SpawnedIsabelBoss && Pawn)
	{
		FVector ToPlayer = Pawn->GetActorLocation() - SpawnedIsabelBoss->GetActorLocation();
		ToPlayer.Z = 0.0f;
		if (!ToPlayer.IsNearlyZero())
		{
			SpawnedIsabelBoss->SetActorRotation(FRotator(0.0f, ToPlayer.Rotation().Yaw, 0.0f), ETeleportType::TeleportPhysics);
		}
	}
	SetIsabelAIPaused(true);

	// ...and only released once she is really ready (standing on the ground, one short beat to settle),
	// so the fight never starts with her mid-air or half set up.
	if (World)
	{
		ReadyLastTime = -1.0;
		IsabelReadyTimeLeft = 3.0f;
		IsabelReadyStableTime = 0.0f;
		World->GetTimerManager().SetTimer(IsabelReadyTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const float Step = ConsumeDelta(ReadyLastTime);
			IsabelReadyTimeLeft -= Step;
			bool bGrounded = true;
			if (const ACharacter* BossCharacter = Cast<ACharacter>(SpawnedIsabelBoss.Get()))
			{
				bGrounded = BossCharacter->GetCharacterMovement() && BossCharacter->GetCharacterMovement()->IsMovingOnGround();
			}
			IsabelReadyStableTime = bGrounded ? IsabelReadyStableTime + Step : 0.0f;
			if (!SpawnedIsabelBoss || IsabelReadyStableTime >= 0.4f || IsabelReadyTimeLeft <= 0.0f)
			{
				if (UWorld* W = GetWorld())
				{
					W->GetTimerManager().ClearTimer(IsabelReadyTimer);
				}
				SetIsabelAIPaused(false);
				UE_LOG(LogSOTMKeyGate, Display, TEXT("SOTMKeyGateActor: Isabella is ready - the fight can start."));
			}
		}), 0.1f, true);
	}
}

void ASOTMKeyGateActor::RefreshPrompt()
{
	if (bUnlocked || !bPlayerInRange)
	{
		OnGatePromptChanged.Broadcast(false, FText::GetEmpty());
		return;
	}

	const bool bHasKey = PlayerState.IsValid() && PlayerState->HasPhase4GateKey();
	OnGatePromptChanged.Broadcast(true, bHasKey ? LockedHasKeyPrompt : LockedNoKeyPrompt);
}

void ASOTMKeyGateActor::FindGateMarker()
{
	GateMarker = FindComponentByClass<UWidgetComponent>();
	if (GateMarker || !GetWorld())
	{
		return;
	}
	// BP_Gate may be a separate actor that holds the marker: look for it by name/label.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == this)
		{
			continue;
		}
		const FString Name = Candidate->GetName();
		if (Name.StartsWith(TEXT("BP_Gate")) && !Name.StartsWith(TEXT("BP_Gate_Key")))
		{
			if (UWidgetComponent* Widget = Candidate->FindComponentByClass<UWidgetComponent>())
			{
				GateMarker = Widget;
				return;
			}
		}
	}
}

void ASOTMKeyGateActor::RefreshGateMarker()
{
	if (!GateMarker)
	{
		return;
	}
	bool bMissionActive = false;
	if (Objectives.IsValid())
	{
		bMissionActive = Objectives->GetActiveChapterOneObjective().ObjectiveId == USOTMObjectiveSubsystem::ReachGateId;
	}
	if (!bMissionActive && PlayerState.IsValid())
	{
		// Isabel-gate chain: reach-the-gate is the mission once the player carries the gate key.
		bMissionActive = PlayerState->HasPhase4GateKey() && !PlayerState->IsIsabelGateReached() && !bUnlocked;
	}
	const bool bShow = bMissionActive && !bMarkerDismissed && !bPlayerInRange && !bUnlocked;
	GateMarker->SetHiddenInGame(!bShow);
	GateMarker->SetVisibility(bShow);
}

void ASOTMKeyGateActor::HandleObjectiveChangedForMarker(FSOTMObjectiveData Objective)
{
	(void)Objective;
	RefreshGateMarker();
}

void ASOTMKeyGateActor::RefreshStatusLight()
{
	if (!bToggleStatusLight || !StatusLight)
	{
		return;
	}
	StatusLight->SetLightColor(bUnlocked ? UnlockedLightColor : LockedLightColor);
	StatusLight->SetIntensity(StatusLightIntensity);
}

void ASOTMKeyGateActor::SpawnIsabelBossIfNeeded()
{
	if (SpawnedIsabelBoss || !IsabelBossClass)
	{
		if (!IsabelBossClass)
		{
			UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: gate unlocked but IsabelBossClass is not assigned - set it in the Details panel to BP_Isabel."));
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector SpawnLocation = GetActorLocation();
	FRotator SpawnRotation = FRotator::ZeroRotator;

	AActor* SpawnPoint = IsabelSpawnPoint.Get();
	if (!SpawnPoint)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == TEXT("PointLight2") || It->GetActorLabel() == TEXT("PointLight2"))
			{
				SpawnPoint = *It;
				break;
			}
		}
	}
	if (SpawnPoint)
	{
		SpawnLocation = SpawnPoint->GetActorLocation();
	}
	else if (IsabelArenaActor)
	{
		if (const UBoxComponent* ArenaBox = IsabelArenaActor->FindComponentByClass<UBoxComponent>())
		{
			SpawnLocation = ArenaBox->GetComponentLocation();
			SpawnRotation = ArenaBox->GetComponentRotation();
		}
		else
		{
			SpawnLocation = IsabelArenaActor->GetActorLocation();
			SpawnRotation = IsabelArenaActor->GetActorRotation();
			UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: IsabelArenaActor has no Box component - spawning at its actor location instead."));
		}
	}
	else
	{
		UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: IsabelArenaActor is not assigned - spawning Isabel at the gate's own location instead."));
	}

	// Spawn standing on the floor (never in mid-air): drop the spawn point onto the ground.
	{
		float FloorZ = 0.0f;
		APlayerController* FloorPC = World->GetFirstPlayerController();
		if (FindFloorZ(World, SpawnLocation, FloorPC ? FloorPC->GetPawn() : nullptr, nullptr, FloorZ))
		{
			const ACharacter* BossCDO = Cast<ACharacter>(IsabelBossClass->GetDefaultObject());
			SpawnLocation.Z = FloorZ + CapsuleHalfHeightOf(BossCDO) + 2.0f;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	SpawnedIsabelBoss = World->SpawnActor<APawn>(IsabelBossClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (SpawnedIsabelBoss)
	{
		if (USkeletalMesh* Doll = IsabelMesh.LoadSynchronous())
		{
			if (ACharacter* BossChar = Cast<ACharacter>(SpawnedIsabelBoss))
			{
				if (USkeletalMeshComponent* BossMesh = BossChar->GetMesh())
				{
					if (BossMesh->GetSkeletalMeshAsset() != Doll)
					{
						BossMesh->SetSkeletalMesh(Doll);
					}
				}
			}
		}
		UE_LOG(LogSOTMKeyGate, Display, TEXT("SOTMKeyGateActor: spawned Isabel boss at (%s)."), *SpawnLocation.ToString());
		OnIsabelBossSpawned.Broadcast(SpawnedIsabelBoss);
	}
	else
	{
		UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: failed to spawn Isabel boss."));
	}
}

#if !UE_BUILD_SHIPPING
void ASOTMKeyGateActor::HandleDebugDamage20()
{
	UE_LOG(LogSOTMKeyGate, Display, TEXT("Debug damage: U pressed."));
	ApplyDebugDamageToIsabel(20.0f);
}

void ASOTMKeyGateActor::HandleDebugDamage50()
{
	UE_LOG(LogSOTMKeyGate, Display, TEXT("Debug damage: X pressed."));
	ApplyDebugDamageToIsabel(50.0f);
}

AActor* ASOTMKeyGateActor::FindIsabelBossActor() const
{
	if (SpawnedIsabelBoss)
	{
		return SpawnedIsabelBoss;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Prefer matching by her actual pawn class (already assigned in the Details
	// panel for spawning) - this finds her whether she was spawned by this gate,
	// placed by hand in the level, or reached via a fast-travel/skip command,
	// and works even if her health is tracked purely in her own Blueprint rather
	// than through USOTMBossVitalComponent.
	if (IsabelBossClass)
	{
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (It->IsA(IsabelBossClass))
			{
				return *It;
			}
		}
	}

	// Fallback: any actor carrying the C++ boss health component, in case
	// IsabelBossClass was never assigned on this gate.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<USOTMBossVitalComponent>())
		{
			return *It;
		}
	}
	return nullptr;
}

void ASOTMKeyGateActor::ApplyDebugDamageToIsabel(const float Damage)
{
	AActor* Target = FindIsabelBossActor();
	if (!Target)
	{
		UE_LOG(LogSOTMKeyGate, Warning, TEXT("Debug damage: could not find Isabel in the level - has she spawned yet, and is IsabelBossClass assigned on this gate?"));
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;

	if (!Target->FindComponentByClass<USOTMBossVitalComponent>())
	{
		UE_LOG(LogSOTMKeyGate, Warning, TEXT("Debug damage: found %s but it has no SOTMBossVitalComponent - damage will still fire an AnyDamage event, but the BossProgressBar needs either that component or ISOTMBossHealthBridgeInterface implemented on it to track health."), *Target->GetName());
	}

	const float Applied = UGameplayStatics::ApplyDamage(Target, Damage, PC, this, nullptr);
	UE_LOG(LogSOTMKeyGate, Display, TEXT("Debug damage: applied %.1f to %s (requested %.1f)."),
		Applied, *Target->GetName(), Damage);
}
#endif

void ASOTMKeyGateActor::SetIsabelAIPaused(const bool bPaused)
{
	if (!SpawnedIsabelBoss)
	{
		return;
	}
	if (AAIController* AI = Cast<AAIController>(SpawnedIsabelBoss->GetController()))
	{
		AI->StopMovement();
		if (AI->BrainComponent)
		{
			if (bPaused)
			{
				AI->BrainComponent->StopLogic(TEXT("Intro"));
			}
			else
			{
				AI->BrainComponent->RestartLogic();
			}
		}
	}
}

void ASOTMKeyGateActor::PlayIntroEntrance(TFunction<void()> OnComplete)
{
	UWorld* World = GetWorld();
	if (!World || EntrancePhase != EEntrancePhase::None)
	{
		if (OnComplete)
		{
			OnComplete();
		}
		return;
	}
	EntranceComplete = MoveTemp(OnComplete);

	// Isabella is NOT spawned yet - she appears after the dialogue (see OpenGateAndSpawnIsabel).

	// The swinging part of the gate.
	USceneComponent* Door = nullptr;
	TInlineComponentArray<USceneComponent*> SceneComps(this);
	for (USceneComponent* Comp : SceneComps)
	{
		if (Comp && Comp->GetFName() == GateDoorComponentName)
		{
			Door = Comp;
			break;
		}
	}
	if (!Door)
	{
		Door = GateMesh;
		UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: door component '%s' not found - rotating GateMesh."), *GateDoorComponentName.ToString());
	}
	DoorComponent = Door;
	DoorYawClosed = FMath::RoundToFloat(GateClosedYaw);
	DoorYawCurrent = DoorYawClosed;
	{
		FRotator Start = Door->GetRelativeRotation();
		Start.Yaw = DoorYawClosed;
		Door->SetRelativeRotation(Start);
	}
	DoorYawAccumulator = 0.0f;

	// The normal player camera is used for the whole sequence (bars on, nothing else changes).
	ShowLetterbox();
	EntrancePhase = EEntrancePhase::Opening;
	EntranceLastTime = -1.0;
	World->GetTimerManager().SetTimer(EntranceTimer, this, &ThisClass::EntranceTick, 0.016f, true);
}

float ASOTMKeyGateActor::ConsumeDelta(double& LastTime) const
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float Delta = LastTime < 0.0 ? 0.0f : static_cast<float>(FMath::Clamp(Now - LastTime, 0.0, 0.1));
	LastTime = Now;
	return Delta;
}

void ASOTMKeyGateActor::PlayGateSound()
{
	if (USoundBase* Sound = GateSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void ASOTMKeyGateActor::EntranceTick()
{
	UWorld* World = GetWorld();
	USceneComponent* Door = DoorComponent.Get();
	if (!World || !Door)
	{
		FinishEntrance();
		return;
	}
	const float Delta = ConsumeDelta(EntranceLastTime);

	// Smooth swing: eased in/out over a fixed time (slow start, gentle settle).
	auto Swing = [this, Door, Delta](const float TargetYaw) -> bool
	{
		const bool bFirst = DoorYawAccumulator <= 0.0f;
		if (bFirst)
		{
			DoorSwingFrom = DoorYawCurrent;
			DoorSwingDuration = (TargetYaw == DoorYawClosed) ? GateCloseSeconds : GateOpenSeconds;
			PlayGateSound();
		}
		DoorYawAccumulator += Delta;
		const float Alpha = FMath::Clamp(DoorYawAccumulator / DoorSwingDuration, 0.0f, 1.0f);
		const float Smooth = Alpha * Alpha * Alpha * (Alpha * (Alpha * 6.0f - 15.0f) + 10.0f); // smootherstep
		DoorYawCurrent = FMath::Lerp(DoorSwingFrom, TargetYaw, Smooth);
		FRotator Rot = Door->GetRelativeRotation();
		Rot.Yaw = DoorYawCurrent;
		Door->SetRelativeRotation(Rot);
		// Status light flickers while the gate moves.
		if (bToggleStatusLight && StatusLight)
		{
			StatusLight->SetIntensity(StatusLightIntensity * (0.6f + 0.4f * FMath::Sin(DoorYawAccumulator * 23.0f) * FMath::Sin(DoorYawAccumulator * 7.0f)));
		}
		if (Alpha >= 1.0f)
		{
			DoorYawCurrent = TargetYaw;
			if (bToggleStatusLight && StatusLight)
			{
				StatusLight->SetIntensity(StatusLightIntensity);
			}
			return true;
		}
		return false;
	};

	switch (EntrancePhase)
	{
	case EEntrancePhase::Opening:
		if (Swing(GateOpenYaw))
		{
			// The player walks in to the InsideArrow.
			USceneComponent* InsideArrow = nullptr;
			TInlineComponentArray<USceneComponent*> SceneComps(this);
			for (USceneComponent* Comp : SceneComps)
			{
				if (Comp && Comp->GetName().StartsWith(TEXT("InsideArrow")))
				{
					InsideArrow = Comp;
					break;
				}
			}
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!InsideArrow || !Pawn)
			{
				UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: no InsideArrow / player pawn - skipping the walk-in."));
				DoorYawAccumulator = 0.0f;
				EntrancePhase = EEntrancePhase::Closing;
				break;
			}
			WalkingPawn = Pawn;
			WalkTarget = InsideArrow->GetComponentLocation();
			FVector Flat = WalkTarget - Pawn->GetActorLocation();
			Flat.Z = 0.0f;
			WalkTimeLeft = Flat.Size() / FMath::Max(PlayerWalkSpeed, 50.0f) * 2.0f + 3.0f;
			if (!Flat.IsNearlyZero())
			{
				PC->SetControlRotation(FRotator(0.0f, Flat.Rotation().Yaw, 0.0f));
			}
			EntrancePhase = EEntrancePhase::Walking;
		}
		break;

	case EEntrancePhase::Walking:
	{
		APawn* Pawn = WalkingPawn.Get();
		ACharacter* Character = Cast<ACharacter>(Pawn);
		WalkTimeLeft -= Delta;
		FVector To = WalkTarget - (Pawn ? Pawn->GetActorLocation() : WalkTarget);
		To.Z = 0.0f;
		const float Dist = To.Size();
		if (!Pawn || Dist <= 45.0f || WalkTimeLeft <= 0.0f)
		{
			if (Character && Character->GetCharacterMovement())
			{
				Character->GetCharacterMovement()->StopMovementImmediately();
			}
			DoorYawAccumulator = 0.0f;
			EntrancePhase = EEntrancePhase::Closing;
			break;
		}
		const FVector Dir = To / Dist;
		if (Character && Character->GetCharacterMovement())
		{
			// Drives the normal walk animation; works even while player input is locked.
			Character->GetCharacterMovement()->RequestDirectMove(Dir * PlayerWalkSpeed, false);
		}
		else
		{
			Pawn->AddActorWorldOffset(Dir * FMath::Min(Dist, PlayerWalkSpeed * Delta));
		}
		const FRotator Target(0.0f, Dir.Rotation().Yaw, 0.0f);
		Pawn->SetActorRotation(FMath::RInterpTo(Pawn->GetActorRotation(), Target, Delta, 8.0f));
		break;
	}

	case EEntrancePhase::Closing:
		if (Swing(DoorYawClosed))
		{
			FinishEntrance();
		}
		break;

	default:
		FinishEntrance();
		break;
	}
}

void ASOTMKeyGateActor::FinishEntrance()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EntranceTimer);
	}
	EntrancePhase = EEntrancePhase::None;
	// Face Isabella, standing still where he stopped (not moved any further).
	if (APawn* Pawn = WalkingPawn.Get())
	{
		if (SpawnedIsabelBoss)
		{
			FVector ToIsabel = SpawnedIsabelBoss->GetActorLocation() - Pawn->GetActorLocation();
			ToIsabel.Z = 0.0f;
			if (!ToIsabel.IsNearlyZero())
			{
				const FRotator Face(0.0f, ToIsabel.Rotation().Yaw, 0.0f);
				Pawn->SetActorRotation(Face);
				if (AController* Controller = Pawn->GetController())
				{
					Controller->SetControlRotation(Face);
				}
			}
		}
	}
	TFunction<void()> Done = MoveTemp(EntranceComplete);
	EntranceComplete = nullptr;
	// Gate is closed: spawn Isabella at PointLight2, swing the camera to her, THEN the dialogue.
	BeginIsabelReveal(MoveTemp(Done));
}

AActor* ASOTMKeyGateActor::ResolveEntranceCamera()
{
	if (EntranceCamera)
	{
		return EntranceCamera;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ACameraActor> It(World); It; ++It)
		{
			if (It->GetName() == TEXT("CineCameraActor2") || It->GetActorLabel() == TEXT("CineCameraActor2"))
			{
				EntranceCamera = *It;
				return *It;
			}
		}
	}
	return nullptr;
}

void ASOTMKeyGateActor::SpawnCinematicDoll()
{
	UWorld* World = GetWorld();
	if (!World || CinematicDoll)
	{
		return;
	}
	AActor* SpawnPoint = IsabelSpawnPoint.Get();
	if (!SpawnPoint)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == TEXT("PointLight2") || It->GetActorLabel() == TEXT("PointLight2"))
			{
				SpawnPoint = *It;
				break;
			}
		}
	}
	UClass* DemoClass = DemoIsabellaClass.LoadSynchronous();
	if (!DemoClass)
	{
		UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: BP_DemoIsabella could not be loaded - set DemoIsabellaClass on the gate."));
		return;
	}
	FVector Loc = SpawnPoint ? SpawnPoint->GetActorLocation() : (IsabelArenaActor ? IsabelArenaActor->GetActorLocation() : GetActorLocation());
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(IntroDollFloor), false);
		Params.AddIgnoredActor(WalkingPawn.Get());
		Params.AddIgnoredActor(this);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Loc, Loc - FVector(0.0f, 0.0f, 6000.0f), ECC_Visibility, Params))
		{
			Loc.Z = Hit.ImpactPoint.Z;
		}
	}
	// Character: lift by capsule half height so she stands exactly on the floor.
	const ACharacter* DemoCDO = Cast<ACharacter>(DemoClass->GetDefaultObject());
	Loc.Z += CapsuleHalfHeightOf(DemoCDO) + 2.0f + DollZOffset;
	DollFloorLocation = Loc;

	float Yaw = 0.0f;
	if (APawn* Player = WalkingPawn.Get())
	{
		FVector ToPlayer = Player->GetActorLocation() - Loc;
		ToPlayer.Z = 0.0f;
		if (!ToPlayer.IsNearlyZero())
		{
			Yaw = ToPlayer.Rotation().Yaw;
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CinematicDoll = World->SpawnActor<AActor>(DemoClass, Loc, FRotator(0.0f, Yaw + DollYawOffset, 0.0f), Params);
	if (!CinematicDoll)
	{
		UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: BP_DemoIsabella failed to spawn."));
		return;
	}
	// Stays exactly where she was placed: no falling, no pushing, no AI wandering.
	CinematicDoll->SetActorEnableCollision(false);
	if (ACharacter* DemoChar = Cast<ACharacter>(CinematicDoll))
	{
		if (UCharacterMovementComponent* Move = DemoChar->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->GravityScale = 0.0f;
			Move->DisableMovement();
		}
	}
	UE_LOG(LogSOTMKeyGate, Display, TEXT("SOTMKeyGateActor: BP_DemoIsabella spawned at %s (spawn point: %s)."), *Loc.ToString(), SpawnPoint ? *SpawnPoint->GetName() : TEXT("NONE - fallback"));
}

void ASOTMKeyGateActor::BeginIsabelReveal(TFunction<void()> OnComplete)
{
	UWorld* World = GetWorld();
	SpawnCinematicDoll();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && CinematicDoll)
	{
		ShotRestoreTarget = PC->GetViewTarget();
		// Switch to BP_DemoIsabella's own cine camera (no blend) just before her dialogue.
		if (UCameraComponent* DemoCam = CinematicDoll->FindComponentByClass<UCameraComponent>())
		{
			ShotCameraComp = DemoCam;
			ShotCamBaseLoc = DemoCam->GetRelativeLocation();
			ShotCamBaseRot = DemoCam->GetRelativeRotation();
			ShotCamBaseFov = DemoCam->FieldOfView;
			PC->SetViewTarget(CinematicDoll);
		}
		else
		{
			UE_LOG(LogSOTMKeyGate, Warning, TEXT("SOTMKeyGateActor: BP_DemoIsabella has no camera component - staying on the player camera."));
		}
		// BP_Isabel is created NOW (view is already on the other camera, so nobody sees it pop in),
		// grounded, hidden and frozen. It is only revealed on the cut back after the dialogue.
		SpawnIsabelBossIfNeeded();
		if (SpawnedIsabelBoss)
		{
			SpawnedIsabelBoss->SetActorHiddenInGame(true);
			SetIsabelAIPaused(true);
			if (APawn* Player = WalkingPawn.Get())
			{
				FVector ToPlayer = Player->GetActorLocation() - SpawnedIsabelBoss->GetActorLocation();
				ToPlayer.Z = 0.0f;
				if (!ToPlayer.IsNearlyZero())
				{
					SpawnedIsabelBoss->SetActorRotation(FRotator(0.0f, ToPlayer.Rotation().Yaw, 0.0f), ETeleportType::TeleportPhysics);
				}
			}
		}
	}
	if (OnComplete)
	{
		OnComplete();
	}
}

void ASOTMKeyGateActor::RevealTick()
{
	// Intentionally empty: the dialogue camera stays completely still.
}

void ASOTMKeyGateActor::EndIntroCinematic()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimer);
	}
	RevealComplete = nullptr;
	// Camera goes straight back to the player (no blend), then the demo doll goes away.
	if (CinematicDoll)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				AActor* Back = ShotRestoreTarget.IsValid() ? ShotRestoreTarget.Get() : static_cast<AActor*>(PC->GetPawn());
				if (Back)
				{
					PC->SetViewTarget(Back);
				}
			}
		}
		CinematicDoll->Destroy();
		CinematicDoll = nullptr;
		ShowFlash(); // flash on the cut back, fight begins
	}
	AnimateLetterbox(0.0f, 0.6f);
}

void ASOTMKeyGateActor::ShowLetterbox()
{
	if (!bLetterbox || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (!LetterboxWidget.IsValid())
	{
		LetterboxAmount = MakeShared<float>(0.0f);
		TSharedPtr<float> Amount = LetterboxAmount;
		auto Bar = [Amount]()
		{
			return SNew(SBox).HeightOverride_Lambda([Amount]() { return FOptionalSize(*Amount * 90.0f); })
				[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)];
		};
		LetterboxWidget = SNew(SOverlay)
			+ SOverlay::Slot().VAlign(VAlign_Top)[Bar()]
			+ SOverlay::Slot().VAlign(VAlign_Bottom)[Bar()];
		GEngine->GameViewport->AddViewportWidgetContent(LetterboxWidget.ToSharedRef(), 5);
	}
	AnimateLetterbox(1.0f, 0.9f);
}

void ASOTMKeyGateActor::AnimateLetterbox(const float Target, const float Seconds)
{
	UWorld* World = GetWorld();
	if (!World || !LetterboxAmount.IsValid())
	{
		return;
	}
	LetterboxFrom = *LetterboxAmount;
	LetterboxTo = Target;
	LetterboxElapsed = 0.0f;
	LetterboxDuration = FMath::Max(Seconds, 0.01f);
	LetterboxLastTime = -1.0;
	World->GetTimerManager().SetTimer(LetterboxTimer, this, &ThisClass::LetterboxTick, 0.016f, true);
}

void ASOTMKeyGateActor::LetterboxTick()
{
	LetterboxElapsed += ConsumeDelta(LetterboxLastTime);
	const float Alpha = FMath::Clamp(LetterboxElapsed / LetterboxDuration, 0.0f, 1.0f);
	if (LetterboxAmount.IsValid())
	{
		*LetterboxAmount = FMath::InterpEaseInOut(LetterboxFrom, LetterboxTo, Alpha, 2.0f);
	}
	if (Alpha >= 1.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(LetterboxTimer);
		}
		if (LetterboxTo <= 0.0f)
		{
			HideLetterbox();
		}
	}
}

void ASOTMKeyGateActor::ShowFlash()
{
	UWorld* World = GetWorld();
	if (!World || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	HideFlash();
	FlashAmount = MakeShared<float>(1.0f);
	FlashElapsed = 0.0f;
	TSharedPtr<float> Amount = FlashAmount;
	FlashWidget = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor_Lambda([Amount]() { return FLinearColor(1.0f, 1.0f, 1.0f, *Amount); })
		.Visibility(EVisibility::HitTestInvisible);
	GEngine->GameViewport->AddViewportWidgetContent(FlashWidget.ToSharedRef(), 20);
	FlashLastTime = -1.0;
	World->GetTimerManager().SetTimer(FlashTimer, this, &ThisClass::FlashTick, 0.016f, true);
}

void ASOTMKeyGateActor::FlashTick()
{
	FlashElapsed += ConsumeDelta(FlashLastTime);
	const float Alpha = FMath::Clamp(FlashElapsed / FlashSeconds, 0.0f, 1.0f);
	if (FlashAmount.IsValid())
	{
		*FlashAmount = 1.0f - FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);
	}
	if (Alpha >= 1.0f)
	{
		HideFlash();
	}
}

void ASOTMKeyGateActor::HideFlash()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlashTimer);
	}
	if (FlashWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(FlashWidget.ToSharedRef());
	}
	FlashWidget.Reset();
	FlashAmount.Reset();
}

void ASOTMKeyGateActor::HideLetterbox()
{
	if (LetterboxWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(LetterboxWidget.ToSharedRef());
	}
	LetterboxWidget.Reset();
	LetterboxAmount.Reset();
}
