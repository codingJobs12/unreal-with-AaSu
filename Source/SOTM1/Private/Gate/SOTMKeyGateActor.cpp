#include "Gate/SOTMKeyGateActor.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Demo/SOTMPhase4Types.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
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
