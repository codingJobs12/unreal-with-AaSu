#include "Gate/SOTMKeyGateActor.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Demo/SOTMPhase4Types.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "AI/SOTMBossVitalComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Objective/SOTMObjectiveSubsystem.h"
#include "SOTMPlayerStateSubsystem.h"
#include "TimerManager.h"

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
	UnbindInteractInput();
	Super::EndPlay(EndPlayReason);
}

void ASOTMKeyGateActor::HandlePlayerStateGateProgressChanged(
	const bool bReached, const bool bHasKey, const bool bUnlockedParam)
{
	(void)bReached;
	(void)bHasKey;
	if (bUnlocked != bUnlockedParam)
	{
		bUnlocked = bUnlockedParam;
		RefreshStatusLight();
		if (bUnlocked)
		{
			SpawnIsabelBossIfNeeded();
		}
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

	if (!bUnlocked && Objectives.IsValid())
	{
		Objectives->TryReachIsabelGate();
	}
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
	LegacyInput->BindKey(EKeys::U, IE_Pressed, this, &ThisClass::HandleDebugDamage20);
	LegacyInput->BindKey(EKeys::X, IE_Pressed, this, &ThisClass::HandleDebugDamage50);
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

void ASOTMKeyGateActor::RefreshStatusLight()
{
	if (!StatusLight)
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

	if (IsabelArenaActor)
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

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	SpawnedIsabelBoss = World->SpawnActor<APawn>(IsabelBossClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (SpawnedIsabelBoss)
	{
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
