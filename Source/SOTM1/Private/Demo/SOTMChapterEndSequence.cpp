#include "Demo/SOTMChapterEndSequence.h"

#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "AI/SOTMCousinCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "BrainComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Ability/SOTMLightningStunComponent.h"
#include "Demo/SOTMBossDialogue.h"
#include "Demo/SOTMDemoPhase2WorldSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "SOTMPlayerStateSubsystem.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMChapterEnd, Log, All);

ASOTMChapterEndSequence::ASOTMChapterEndSequence()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

bool ASOTMChapterEndSequence::TriggerChapterEnd(UObject* WorldContextObject, AActor* IsabellaActor)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ASOTMChapterEndSequence> It(World); It; ++It)
	{
		It->PlayChapterEnd(IsabellaActor);
		return true;
	}
	UE_LOG(LogSOTMChapterEnd, Warning, TEXT("No Chapter End actor placed in the level - add BP_ChapterEnd (class SOTMChapterEndSequence)."));
	return false;
}

void ASOTMChapterEndSequence::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveBars();
	ClearAllTimers();
	RestoreTimeDilation();
	if (UWorld* World = GetWorld())
	{
		if (USOTMBossDialogueSubsystem* Dialogue = World->GetSubsystem<USOTMBossDialogueSubsystem>())
		{
			Dialogue->OnLineFinished.Remove(LineFinishedHandle);
		}
	}
	if (EndCardRoot.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UGameViewportClient* Viewport = GI->GetGameViewportClient())
				{
					Viewport->RemoveViewportWidgetContent(EndCardRoot.ToSharedRef());
				}
			}
		}
		EndCardRoot.Reset();
	}
	if (bInputLocked)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (USOTMPlayerStateSubsystem* PS = GI->GetSubsystem<USOTMPlayerStateSubsystem>())
				{
					PS->ReleaseInputLock(ESOTMInputLockReason::Cinematic);
				}
			}
		}
		bInputLocked = false;
	}
	Super::EndPlay(EndPlayReason);
}

void ASOTMChapterEndSequence::AnimateBars(const float Target, const float Seconds)
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UGameViewportClient* Viewport = GI ? GI->GetGameViewportClient() : nullptr;
	if (!bLetterbox || !Viewport)
	{
		return;
	}
	if (!BarsRoot.IsValid())
	{
		if (Target <= 0.0f)
		{
			return;
		}
		BarsAmount = MakeShared<float>(0.0f);
		TSharedPtr<float> Amount = BarsAmount;
		const float Height = LetterboxHeight;
		auto Bar = [Amount, Height]()
		{
			return SNew(SBox).HeightOverride_Lambda([Amount, Height]() { return FOptionalSize(*Amount * Height); })
				[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)];
		};
		BarsRoot = SNew(SOverlay)
			+ SOverlay::Slot().VAlign(VAlign_Top)[Bar()]
			+ SOverlay::Slot().VAlign(VAlign_Bottom)[Bar()];
		Viewport->AddViewportWidgetContent(BarsRoot.ToSharedRef(), 5);
	}
	FTSTicker::GetCoreTicker().RemoveTicker(BarsTickerHandle);
	const float From = BarsAmount.IsValid() ? *BarsAmount : 0.0f;
	const float Duration = FMath::Max(Seconds, 0.01f);
	TWeakObjectPtr<ASOTMChapterEndSequence> WeakThis(this);
	float Elapsed = 0.0f;
	BarsTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, From, Target, Duration, Elapsed](float Dt) mutable
	{
		ASOTMChapterEndSequence* Self = WeakThis.Get();
		if (!Self || !Self->BarsAmount.IsValid())
		{
			return false;
		}
		Elapsed += Dt;
		const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
		*Self->BarsAmount = FMath::InterpEaseInOut(From, Target, Alpha, 2.0f);
		if (Alpha >= 1.0f)
		{
			if (Target <= 0.0f)
			{
				Self->RemoveBars();
			}
			return false;
		}
		return true;
	}));
}

void ASOTMChapterEndSequence::RemoveBars()
{
	FTSTicker::GetCoreTicker().RemoveTicker(BarsTickerHandle);
	if (BarsRoot.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UGameViewportClient* Viewport = GI->GetGameViewportClient())
				{
					Viewport->RemoveViewportWidgetContent(BarsRoot.ToSharedRef());
				}
			}
		}
	}
	BarsRoot.Reset();
	BarsAmount.Reset();
}

void ASOTMChapterEndSequence::ClearAllTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : ActiveTimers)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	ActiveTimers.Reset();
}

void ASOTMChapterEndSequence::After(const float Seconds, TFunction<void()> Next)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Next]() { Next(); }),
		FMath::Max(Seconds, 0.02f), false);
	ActiveTimers.Add(Handle);
}

void ASOTMChapterEndSequence::WhenDialogueFinishes(const FName Row, TFunction<void()> Next)
{
	UWorld* World = GetWorld();
	USOTMBossDialogueSubsystem* Dialogue = World ? World->GetSubsystem<USOTMBossDialogueSubsystem>() : nullptr;
	if (!Dialogue || Row.IsNone())
	{
		After(1.5f, Next);
		return;
	}
	TSharedRef<bool> Done = MakeShared<bool>(false);
	auto Finish = [this, Done, Next]()
	{
		if (*Done)
		{
			return;
		}
		*Done = true;
		if (UWorld* W = GetWorld())
		{
			if (USOTMBossDialogueSubsystem* D = W->GetSubsystem<USOTMBossDialogueSubsystem>())
			{
				D->OnLineFinished.Remove(LineFinishedHandle);
			}
		}
		Next();
	};
	LineFinishedHandle = Dialogue->OnLineFinished.AddWeakLambda(this, [Row, Finish](FName Finished)
	{
		if (Finished == Row)
		{
			Finish();
		}
	});
	if (!Dialogue->PlayLine(Row, nullptr, true))
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Dialogue row '%s' did not play - continuing after a short pause."), *Row.ToString());
		After(2.0f, Finish);
	}
	else
	{
		After(30.0f, Finish); // safety net
	}
}

namespace
{
	AActor* FindActorByLabelOrClass(UWorld* World, const TCHAR* Label)
	{
		if (!World)
		{
			return nullptr;
		}
		const FString ClassPrefix = FString(Label) + TEXT("_C");
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetActorLabel() == Label || It->GetName().StartsWith(ClassPrefix))
			{
				return *It;
			}
		}
		return nullptr;
	}

	FTransform ArrowTransformOf(const AActor* Actor)
	{
		if (const UArrowComponent* Arrow = Actor->FindComponentByClass<UArrowComponent>())
		{
			return Arrow->GetComponentTransform();
		}
		return Actor->GetActorTransform();
	}
}

UCameraComponent* ASOTMChapterEndSequence::PickCamera(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	TArray<UCameraComponent*> Cameras;
	Actor->GetComponents<UCameraComponent>(Cameras);
	for (UCameraComponent* Camera : Cameras)
	{
		if (Camera && Camera->GetName().Contains(TEXT("Cine")))
		{
			return Camera;
		}
	}
	return Cameras.Num() > 0 ? Cameras[0] : nullptr;
}

bool ASOTMChapterEndSequence::CutToActorCamera(AActor* Actor, const bool bDetachCamera)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UCameraComponent* Chosen = PickCamera(Actor);
	if (!PC || !Actor || !Chosen)
	{
		return false;
	}
	TArray<UCameraComponent*> Cameras;
	Actor->GetComponents<UCameraComponent>(Cameras);
	for (UCameraComponent* Camera : Cameras)
	{
		if (Camera && Camera != Chosen)
		{
			Camera->Deactivate();
		}
	}
	Chosen->Activate(true);
	if (bDetachCamera)
	{
		// Keeps its authored place in the world, so the actor can be flipped / moved without turning the shot.
		Chosen->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	PC->SetViewTarget(Actor); // instant cut - hidden by the flash
	return true;
}

void ASOTMChapterEndSequence::FlashCut(TFunction<void()> AtPeak)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager : nullptr;
	if (!bFlashOnCameraCut || !CameraManager)
	{
		if (AtPeak)
		{
			AtPeak();
		}
		return;
	}
	constexpr float InSeconds = 0.07f;
	CameraManager->StartCameraFade(0.0f, 1.0f, InSeconds, FLinearColor::White, false, true);
	After(InSeconds + 0.02f, [this, AtPeak]()
	{
		if (AtPeak)
		{
			AtPeak();
		}
		if (UWorld* W = GetWorld())
		{
			if (APlayerController* P = W->GetFirstPlayerController())
			{
				if (P->PlayerCameraManager)
				{
					P->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.25f, FLinearColor::White, false, false);
				}
			}
		}
	});
}

FVector ASOTMChapterEndSequence::ResolvePoint(const AActor* Point, const FVector& Fallback) const
{
	return Point ? Point->GetActorLocation() : Fallback;
}

void ASOTMChapterEndSequence::HideHud()
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		// Hidden (not Collapsed): no layout recompute of heavy widgets, which hitched the game.
		if (Widget && Widget->IsVisible())
		{
			Widget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void ASOTMChapterEndSequence::LockPlayer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (USOTMPlayerStateSubsystem* PS = GI->GetSubsystem<USOTMPlayerStateSubsystem>())
		{
			PS->AcquireInputLock(ESOTMInputLockReason::Cinematic);
			bInputLocked = true;
		}
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->bShowMouseCursor = false;
		FInputModeUIOnly Mode;
		PC->SetInputMode(Mode);
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetCanBeDamaged(false);
		}
	}
}

void ASOTMChapterEndSequence::PlayChapterEnd(AActor* IsabellaActor)
{
	UWorld* World = GetWorld();
	if (bStarted || !World)
	{
		return;
	}
	bStarted = true;
	AnimateBars(1.0f, 0.9f);

	Isabella = IsabellaActor;
	if (!Isabella)
	{
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("Isabel")))
			{
				Isabella = *It;
				break;
			}
		}
	}
	if (!Isabella)
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Chapter End: Isabella not found - pass her into Play Chapter End."));
	}

	LockPlayer();
	HideHud();

	// The player is no longer in the scene: invisible and non-colliding (view target is Isabella's camera).
	if (APlayerController* PlayerPC = World->GetFirstPlayerController())
	{
		if (APawn* PlayerPawn = PlayerPC->GetPawn())
		{
			PlayerPawn->SetActorHiddenInGame(true);
			PlayerPawn->SetActorEnableCollision(false);
		}
	}

	// Nothing else may talk or attack during the ending.
	if (USOTMDemoPhase2WorldSubsystem* Phase2 = World->GetSubsystem<USOTMDemoPhase2WorldSubsystem>())
	{
		Phase2->SetDialogueSuppressed(true);
	}
	for (TActorIterator<ASOTMCousinCharacter> It(World); It; ++It)
	{
		if (AAIController* AI = Cast<AAIController>(It->GetController()))
		{
			AI->StopMovement();
			if (AI->GetBrainComponent())
			{
				AI->GetBrainComponent()->StopLogic(TEXT("ChapterEnd"));
			}
		}
	}

	// Camera: an explicitly assigned camera actor wins; otherwise use the CineCamera that lives
	// inside BP_Isabel (she spawns later in the level, so it is looked up here at trigger time).
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (SequenceCamera)
		{
			PC->SetViewTargetWithBlend(SequenceCamera, CameraBlendTime);
			if (bDynamicCamera)
			{
				StartCameraDirector(SequenceCamera->GetCameraComponent());
			}
		}
		else if (Isabella)
		{
			TArray<UCameraComponent*> Cameras;
			Isabella->GetComponents<UCameraComponent>(Cameras);
			UCameraComponent* Chosen = nullptr;
			for (UCameraComponent* Camera : Cameras)
			{
				if (Camera && Camera->GetName().Contains(TEXT("Cine")))
				{
					Chosen = Camera;
					break;
				}
			}
			if (!Chosen && Cameras.Num() > 0)
			{
				Chosen = Cameras[0];
			}
			if (Chosen)
			{
				// The view target uses its first ACTIVE camera, so make sure this one is it.
				for (UCameraComponent* Camera : Cameras)
				{
					if (Camera && Camera != Chosen)
					{
						Camera->Deactivate();
					}
				}
				Chosen->Activate(true);
				PC->SetViewTargetWithBlend(Isabella, CameraBlendTime);
				UE_LOG(LogSOTMChapterEnd, Display, TEXT("Chapter End: viewing through %s."), *Chosen->GetName());
				if (bDynamicCamera)
				{
					StartCameraDirector(Chosen);
				}
			}
			else
			{
				UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Chapter End: no camera component found on Isabella."));
			}
		}
	}

	UE_LOG(LogSOTMChapterEnd, Display, TEXT("Chapter End sequence started."));
	// Spread the work over two frames: the setup above first, the montage + first line right after,
	// so the two costs never land in the same frame.
	After(0.1f, [this]() { StepIsabelDies(); });
}

void ASOTMChapterEndSequence::StepIsabelDies()
{
	bMontageDone = false;
	bLowHealthDone = false;
	bSpiderDialogueStarted = false;
	bTimmyArrived = false;
	bTimmyDialogueStarted = false;

	// Remember where ArrowTimmy is NOW, before the death montage moves her mesh/root.
	bHasArrowTransform = false;
	bHasSpiderArrow = false;
	if (Isabella)
	{
		TInlineComponentArray<USceneComponent*> ArrowComps(Isabella);
		for (USceneComponent* Comp : ArrowComps)
		{
			if (Comp && !bHasArrowTransform && Comp->GetName().StartsWith(TEXT("ArrowTimmy")))
			{
				TimmyArrowTransform = Comp->GetComponentTransform();
				bHasArrowTransform = true;
			}
			else if (Comp && !bHasSpiderArrow && Comp->GetName().StartsWith(TEXT("ArrowSpider")))
			{
				SpiderArrowTransform = Comp->GetComponentTransform();
				bHasSpiderArrow = true;
			}
		}
	}

	// Pre-placed Timmy / spider stay unseen until their moment (no spawning out of thin air).
	bTimmyWalking = false;
	for (AActor* Hidden : { TimmyActor.Get(), SpiderActor.Get() })
	{
		if (Hidden)
		{
			Hidden->SetActorHiddenInGame(true);
			Hidden->SetActorEnableCollision(false);
		}
	}

	// Timmy is not shown yet: he appears after Isabella's death zoom (see StepFreezeIsabel).

	float MontageTime = 0.1f;
	if (APawn* IsabelPawn = Cast<APawn>(Isabella))
	{
		IsabelPawn->SetCanBeDamaged(false);
		if (USOTMLightningStunComponent* StunComp = IsabelPawn->FindComponentByClass<USOTMLightningStunComponent>())
		{
			StunComp->DisableStunPermanently();
		}
		if (AAIController* AI = Cast<AAIController>(IsabelPawn->GetController()))
		{
			AI->StopMovement();
			if (AI->GetBrainComponent())
			{
				AI->GetBrainComponent()->StopLogic(TEXT("ChapterEnd"));
			}
		}
		if (ACharacter* IsabelCharacter = Cast<ACharacter>(IsabelPawn))
		{
			IsabelCharacter->GetCharacterMovement()->DisableMovement();
		}
	}
	if (USkeletalMeshComponent* Mesh = Isabella ? Isabella->FindComponentByClass<USkeletalMeshComponent>() : nullptr)
	{
		if (IsabelDeathMontage && Mesh->GetAnimInstance())
		{
			// Montage_Play returns the montage LENGTH, not a duration - work the duration out ourselves.
			const float Length = FMath::Max(IsabelDeathMontage->GetPlayLength(), 0.1f);
			const float Rate = Length / (Length + FMath::Max(DeathExtraSeconds, 0.0f));
			const float Played = Mesh->GetAnimInstance()->Montage_Play(IsabelDeathMontage, Rate);
			if (Played > 0.0f)
			{
				MontageTime = FMath::Max((Length + FMath::Max(DeathExtraSeconds, 0.0f)) / FMath::Max(IsabelDeathMontage->RateScale, 0.01f) - 0.05f, 0.1f);
			}
		}
		else if (!IsabelDeathMontage)
		{
			UE_LOG(LogSOTMChapterEnd, Warning, TEXT("IsabelDeathMontage is not set."));
		}
	}

	// Her mesh disappears 0.3s before the montage finishes (actor stays - the camera lives in it).
	After(FMath::Max(MontageTime - 0.3f, 0.02f), [this]()
	{
		if (USkeletalMeshComponent* Mesh = Isabella ? Isabella->FindComponentByClass<USkeletalMeshComponent>() : nullptr)
		{
			Mesh->SetHiddenInGame(true, true);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (Isabella)
		{
			Isabella->SetActorEnableCollision(false);
		}
	});

	// When the montage ends, hold the pose (hidden by now) and move on.
	After(MontageTime, [this]()
	{
		StepFreezeIsabel();
	});

	// Her dying line plays alongside the montage.
	WhenDialogueFinishes(LowHealthDialogueRow, [this]()
	{
		bLowHealthDone = true;
		TryStartTimmyDialogue();
	});
}

void ASOTMChapterEndSequence::StepFreezeIsabel()
{
	if (USkeletalMeshComponent* Mesh = Isabella ? Isabella->FindComponentByClass<USkeletalMeshComponent>() : nullptr)
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (IsabelDeathMontage)
			{
				Anim->Montage_Pause(IsabelDeathMontage);
			}
		}
		Mesh->bPauseAnims = true; // keep the last pose
		// Isabella is gone now: invisible and non-colliding, but the ACTOR stays (the camera lives in it).
		Mesh->SetHiddenInGame(true, true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Isabella)
	{
		Isabella->SetActorEnableCollision(false);
	}
	bMontageDone = true;

	// Isabella's death animation is over: swing the camera straight to Timmy's spot, then he appears.
	{
		FVector FocusSpot = GetActorLocation();
		if (TimmySpawnPoint)
		{
			FocusSpot = TimmySpawnPoint->GetActorLocation();
		}
		else if (bHasArrowTransform)
		{
			FocusSpot = TimmyArrowTransform.GetLocation();
		}
		else if (Isabella)
		{
			FocusSpot = Isabella->GetActorLocation() + Isabella->GetActorRightVector() * TimmyDistanceFromIsabella;
		}
		TimmyFocusPoint = FocusSpot + FVector(0.0f, 0.0f, 70.0f);
		CamPhase = ECamPhase::Timmy;
		TimmyZoomElapsed = 0.0f;
	}
	// The death zoom is over: Timmy appears now and reaches his spot within TimmyWalkInSeconds.
	StepTimmyAppears();
	// The camera swings to Timmy; his dialogue starts once he stands on his mark and her line is over.
	TryStartTimmyDialogue();
}

void ASOTMChapterEndSequence::StepTimmyAppears()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const FVector PlayerLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : GetActorLocation();
	const FVector Forward = PlayerPawn ? PlayerPawn->GetActorForwardVector() : FVector::ForwardVector;
	// Default: on Isabella's right side, facing the same way she does.
	FVector CameraLocation = PlayerLocation;
	FRotator CameraRotation = FRotator::ZeroRotator;
	if (PC && PC->PlayerCameraManager)
	{
		CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
		CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	}
	const FVector IsabelLoc = Isabella ? Isabella->GetActorLocation() : PlayerLocation + Forward * 350.0f;
	const FRotator FlatCamera(0.0f, CameraRotation.Yaw, 0.0f);
	const FVector CamRight = FRotationMatrix(FlatCamera).GetUnitAxis(EAxis::Y);
	const FVector CamBack = -FRotationMatrix(FlatCamera).GetUnitAxis(EAxis::X);
	(void)CamRight;
	(void)CamBack;
	// Right side of Isabella (her own right, not the camera's).
	const FVector SideSpot = Isabella
		? IsabelLoc + Isabella->GetActorRightVector() * TimmyDistanceFromIsabella
		: IsabelLoc + CamRight * TimmyDistanceFromIsabella;
	FVector Location = ResolvePoint(TimmySpawnPoint, SideSpot);
	// Preferred: the exact ArrowTimmy transform, captured before the death montage moved her.
	const bool bUseArrow = !TimmySpawnPoint && bHasArrowTransform;
	if (bUseArrow)
	{
		Location = TimmyArrowTransform.GetLocation();
	}
	else if (!TimmySpawnPoint)
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("No 'ArrowTimmy' component found on Isabella - using the fallback spot."));
	}
	// Face the same direction as Isabella.
	FRotator Rotation = FRotator::ZeroRotator;
	if (Isabella)
	{
		Rotation = FRotator(0.0f, Isabella->GetActorRotation().Yaw, 0.0f);
	}
	if (bUseArrow)
	{
		Rotation = TimmyArrowTransform.Rotator();
	}

	// Timmy WALKS to his mark (Location) from somewhere out of shot, instead of popping in.
	FVector Mark = Location;
	const FRotator MarkRot = Rotation;
	FVector Start = Mark;
	{
		FVector Away = (Mark - IsabelLoc);
		Away.Z = 0.0f;
		Away = Away.GetSafeNormal();
		if (Away.IsNearlyZero())
		{
			Away = Isabella ? Isabella->GetActorRightVector() : FVector::RightVector;
		}
		FVector Offset = Away * TimmyWalkInDistance;
		// He walks in from the FAR side of the shot (top of the screen), never from the camera's side (bottom).
		if (World)
		{
			if (APlayerController* ViewPC = World->GetFirstPlayerController())
			{
				if (ViewPC->PlayerCameraManager)
				{
					FVector CamForward = FRotator(0.0f, ViewPC->PlayerCameraManager->GetCameraRotation().Yaw, 0.0f).Vector();
					const float AlongView = FVector::DotProduct(Offset, CamForward);
					if (AlongView < 0.0f)
					{
						Offset -= 2.0f * AlongView * CamForward; // mirror to the far side, keep the sideways part
					}
					else if (FMath::Abs(AlongView) < TimmyWalkInDistance * 0.3f)
					{
						Offset = CamForward * TimmyWalkInDistance; // purely sideways: use straight "up" the screen instead
					}
				}
			}
		}
		const FVector Computed = Mark + Offset;
		if (TimmyWalkStartPoint)
		{
			Start = TimmyWalkStartPoint->GetActorLocation();
		}
		else if (TimmyActor && FVector::DistXY(TimmyActor->GetActorLocation(), Mark) > 150.0f)
		{
			Start = TimmyActor->GetActorLocation();
		}
		else
		{
			Start = Computed;
		}
	}
	// Both the start and the mark sit exactly on the floor (never in the air).
	if (World)
	{
		float HalfHeight = 88.0f;
		if (const ACharacter* TimmyChar = TimmyActor ? Cast<ACharacter>(TimmyActor.Get())
			: (TimmyClass ? Cast<ACharacter>(TimmyClass->GetDefaultObject()) : nullptr))
		{
			if (TimmyChar->GetCapsuleComponent())
			{
				HalfHeight = TimmyChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			}
		}
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(TimmyWalkGround), false);
		TraceParams.AddIgnoredActor(Isabella);
		TraceParams.AddIgnoredActor(TimmyActor);
		TraceParams.AddIgnoredActor(PlayerPawn);
		for (FVector* Point : { &Mark, &Start })
		{
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, *Point + FVector(0.0f, 0.0f, 200.0f), *Point - FVector(0.0f, 0.0f, 3000.0f), ECC_Visibility, TraceParams))
			{
				Point->Z = Hit.ImpactPoint.Z + HalfHeight + 2.0f;
			}
		}
	}
	FVector ToMark = Mark - Start;
	ToMark.Z = 0.0f;
	const FRotator WalkRot = ToMark.IsNearlyZero() ? MarkRot : FRotator(0.0f, ToMark.Rotation().Yaw, 0.0f);

	Timmy = TimmyActor;
	if (Timmy)
	{
		Timmy->SetActorHiddenInGame(false);
		Timmy->SetActorEnableCollision(true);
		Timmy->SetActorLocationAndRotation(Start, WalkRot, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (World && TimmyClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Timmy = World->SpawnActor<AActor>(TimmyClass, Start, WalkRot, Params);
	}
	Location = Start;
	Rotation = WalkRot;
	if (!Timmy)
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Timmy could not appear - assign TimmyClass or TimmyActor."));
	}
	if (!Timmy)
	{
		StepTimmyArrived();
		return;
	}
	if (ACharacter* TimmyCharacter = Cast<ACharacter>(Timmy))
	{
		// The walk is driven by code (the walk animation is rooted), so nothing else may move him.
		if (AAIController* TimmyAI = Cast<AAIController>(TimmyCharacter->GetController()))
		{
			TimmyAI->StopMovement();
			if (TimmyAI->BrainComponent)
			{
				TimmyAI->BrainComponent->StopLogic(TEXT("Entrance"));
			}
		}
		if (UCharacterMovementComponent* Move = TimmyCharacter->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
	}
	TimmyWalkTarget = Mark;
	TimmyWalkFinalRot = MarkRot;
	TimmyWalkStartZ = Timmy->GetActorLocation().Z;
	TimmyWalkTotalDist = FMath::Max(FVector::Dist2D(Timmy->GetActorLocation(), Mark), 1.0f);
	TimmyWalkCurSpeed = TimmyWalkInSeconds > 0.05f ? TimmyWalkTotalDist / TimmyWalkInSeconds : FMath::Max(TimmyWalkSpeed, 50.0f);
	TimmyWalkTimeLeft = TimmyWalkTotalDist / TimmyWalkCurSpeed * 2.0f + 1.5f;

	// Walk cycle (looping) on his mesh.
	UAnimSequenceBase* WalkSequence = TimmyWalkAnimation;
	if (!WalkSequence)
	{
		WalkSequence = LoadObject<UAnimSequenceBase>(nullptr,
			TEXT("/Game/HorrorBear/Animations/Anim_HorrorBear_Walk.Anim_HorrorBear_Walk"));
	}
	if (!WalkSequence || PlayTimmyAnimation(WalkSequence, true, TimmyWalkAnimRate * FMath::Clamp(TimmyWalkCurSpeed / FMath::Max(TimmyWalkSpeed, 50.0f), 1.0f, 2.5f)) == 0)
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Timmy walk-in: walk animation could not be played (assign TimmyWalkAnimation)."));
	}
	bTimmyWalking = true;
	SetActorTickEnabled(true);
}

int32 ASOTMChapterEndSequence::PlayTimmyAnimation(UAnimSequenceBase* Sequence, const bool bLoop, const float Rate)
{
	if (!Timmy || !Sequence)
	{
		return 0;
	}
	// BP_Timmy can have several skeletal mesh components: play on every real (non-follower) one.
	TArray<USkeletalMeshComponent*> TimmyMeshes;
	Timmy->GetComponents<USkeletalMeshComponent>(TimmyMeshes);
	int32 Played = 0;
	for (USkeletalMeshComponent* M : TimmyMeshes)
	{
		if (!M || !M->GetSkeletalMeshAsset() || M->LeaderPoseComponent.IsValid())
		{
			continue;
		}
		M->bPauseAnims = false;
		M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		M->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		M->PlayAnimation(Sequence, bLoop);
		M->SetPlayRate(Rate);
		++Played;
	}
	return Played;
}

void ASOTMChapterEndSequence::RestoreTimmyAnimBlueprint()
{
	if (!Timmy)
	{
		return;
	}
	TArray<USkeletalMeshComponent*> TimmyMeshes;
	Timmy->GetComponents<USkeletalMeshComponent>(TimmyMeshes);
	for (USkeletalMeshComponent* M : TimmyMeshes)
	{
		if (!M || !M->GetSkeletalMeshAsset() || M->LeaderPoseComponent.IsValid() || !M->GetAnimClass())
		{
			continue;
		}
		// Back to his Animation Blueprint (idle pose) once the walk is over.
		M->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		M->SetAnimInstanceClass(M->GetAnimClass());
	}
}

void ASOTMChapterEndSequence::StepTimmyArrived()
{
	bTimmyWalking = false;
	if (Timmy)
	{
		Timmy->SetActorLocationAndRotation(TimmyWalkTarget, TimmyWalkFinalRot, false, nullptr, ETeleportType::TeleportPhysics);
		RestoreTimmyAnimBlueprint(); // idle again
		if (ACharacter* TimmyCharacter = Cast<ACharacter>(Timmy))
		{
			if (UCharacterMovementComponent* Move = TimmyCharacter->GetCharacterMovement())
			{
				Move->StopMovementImmediately();
				Move->SetMovementMode(MOVE_Walking);
			}
		}
		if (UWorld* World = GetWorld())
		{
			if (TimmyAppearEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, TimmyAppearEffect, TimmyWalkTarget, TimmyWalkFinalRot);
			}
		}
		if (USkeletalMeshComponent* Mesh = Timmy->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (TimmyAppearMontage && Mesh->GetAnimInstance())
			{
				Mesh->GetAnimInstance()->Montage_Play(TimmyAppearMontage);
			}
		}
	}
	bTimmyArrived = true;
	TryStartTimmyDialogue();
}

void ASOTMChapterEndSequence::TryStartTimmyDialogue()
{
	// Timmy's line needs: Isabella's death finished + her line over + Timmy standing on his mark.
	if (bTimmyDialogueStarted || !bMontageDone || !bLowHealthDone || !bTimmyArrived)
	{
		return;
	}
	bTimmyDialogueStarted = true;
	After(DelayBeforeTimmyDialogue, [this]() { StepTimmyDialogue(); });
}

void ASOTMChapterEndSequence::StepTimmyDialogue()
{
	// Timmy has his own cine camera: cut to it (flash) for his line.
	if (Timmy && PickCamera(Timmy))
	{
		CutToActorCamera(Timmy, false); // direct cut, no flash
	}
	WhenDialogueFinishes(DefeatedDialogueRow, [this]()
	{
		After(DelayBeforeSpider, [this]() { StepSpiderSpawns(); });
	});
}

void ASOTMChapterEndSequence::StepSpiderSpawns()
{
	if (!GetWorld())
	{
		return;
	}
	CamPhase = ECamPhase::Spider;
	SetActorTickEnabled(true);
	// 1) The spider is put at BP_Arrow first (hidden, upside down) - well before the camera moves to it.
	SpawnSpiderNow();
	// 2) Flash cut: at the white peak the view switches to the spider's camera and it starts coming down.
	FlashCut([this]() { RevealSpiderShot(); });
}

void ASOTMChapterEndSequence::RevealSpiderShot()
{
	if (!bSpiderHangPrepared || !Spider)
	{
		return; // old single-step path already ran
	}
	bSpiderHangPrepared = false;
	Spider->SetActorHiddenInGame(false);
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (SpiderShotCam.IsValid())
			{
				PC->SetViewTarget(Spider);
			}
		}
	}
	SpiderShotElapsed = 0.0f;
	SpiderShot = ESpiderShot::Descend;
}

void ASOTMChapterEndSequence::SpawnSpiderNow()
{
	UWorld* World = GetWorld();
	const FVector IsabelLocation = Isabella ? Isabella->GetActorLocation() : GetActorLocation();
	FVector SpiderDir = FVector::ForwardVector;
	if (APlayerController* CamPC = World ? World->GetFirstPlayerController() : nullptr)
	{
		if (CamPC->PlayerCameraManager)
		{
			SpiderDir = FRotationMatrix(FRotator(0.0f, CamPC->PlayerCameraManager->GetCameraRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::X);
		}
	}
	// Away from Timmy's side by default (never behind / next to him).
	FVector TimmySideDir = (TimmyFocusPoint - IsabelLocation);
	TimmySideDir.Z = 0.0f;
	TimmySideDir = TimmySideDir.GetSafeNormal();
	if (!TimmySideDir.IsNearlyZero())
	{
		SpiderDir = -TimmySideDir;
	}
	FVector Location = ResolvePoint(SpiderSpawnPoint, IsabelLocation + SpiderDir * SpiderDistanceBehindIsabella + FVector(0.0f, 0.0f, 600.0f));
	const bool bUseSpiderArrow = !SpiderSpawnPoint && bHasSpiderArrow;
	FRotator Rotation = SpiderSpawnPoint ? SpiderSpawnPoint->GetActorRotation()
		: (Timmy ? FRotator(0.0f, (Timmy->GetActorLocation() - Location).Rotation().Yaw, 0.0f) : FRotator::ZeroRotator);
	if (bUseSpiderArrow)
	{
		// Exactly the ArrowSpider component's transform from BP_Isabel (captured when the sequence began).
		Location = SpiderArrowTransform.GetLocation();
		Rotation = SpiderArrowTransform.Rotator();
		if (bSpiderOppositeTimmy && !TimmySideDir.IsNearlyZero())
		{
			FVector Offset = Location - IsabelLocation;
			Offset.Z = 0.0f;
			if (FVector::DotProduct(Offset, TimmySideDir) > 0.0f)
			{
				// Same side as Timmy: mirror around Isabella (half-turn) to the opposite side.
				Location.X = IsabelLocation.X - Offset.X;
				Location.Y = IsabelLocation.Y - Offset.Y;
				Rotation.Yaw += 180.0f;
			}
		}
	}
	else
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("No 'ArrowSpider' component found on Isabella - using a fallback spot/rotation."));
	}
	// BP_Arrow / BP_Arrow2: the spider hangs upside down at BP_Arrow and is lowered to BP_Arrow2.
	AActor* HangStart = SpiderHangStart ? SpiderHangStart.Get() : FindActorByLabelOrClass(World, TEXT("BP_Arrow"));
	AActor* HangEnd = SpiderHangEnd ? SpiderHangEnd.Get() : FindActorByLabelOrClass(World, TEXT("BP_Arrow2"));
	const bool bHangSequence = HangStart && HangEnd && HangStart != HangEnd;
	if (HangStart)
	{
		const FTransform StartTransform = ArrowTransformOf(HangStart);
		Location = StartTransform.GetLocation();
		Rotation = StartTransform.Rotator();
	}
	else
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Spider: 'BP_Arrow' not found in the level - using the old spawn spot."));
	}
	const FVector FinalLocation = Location;

	if (SpiderActor)
	{
		// Already in the level: reveal it up in the air at the arrow, the slam brings it down.
		Spider = SpiderActor;
		Spider->SetActorHiddenInGame(false);
		Spider->SetActorEnableCollision(true);
		Spider->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (World && SpiderClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spider = World->SpawnActor<AActor>(SpiderClass, Location, Rotation, Params);
	}
	else
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("SpiderClass is not set - no spider spawned."));
	}

	if (Spider)
	{
		// Cinematic prop: keep the ArrowSpider rotation for the rest of the ending (nothing may turn it).
		SpiderFinalRot = Rotation;
		bSpiderRotationLocked = true;
		if (APawn* SpiderPawn = Cast<APawn>(Spider))
		{
			if (AAIController* SpiderAI = Cast<AAIController>(SpiderPawn->GetController()))
			{
				SpiderAI->StopMovement();
				if (SpiderAI->GetBrainComponent())
				{
					SpiderAI->GetBrainComponent()->StopLogic(TEXT("ChapterEnd"));
				}
				SpiderAI->ClearFocus(EAIFocusPriority::Gameplay);
			}
			if (ACharacter* SpiderCharacter = Cast<ACharacter>(SpiderPawn))
			{
				SpiderCharacter->bUseControllerRotationYaw = false;
				SpiderCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
				SpiderCharacter->GetCharacterMovement()->bUseControllerDesiredRotation = false;
			}
			SpiderPawn->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
		}
	}

	SpiderShot = ESpiderShot::None;
	bSpiderCamTrack = false;
	bSlamCamMode = false;
	bSlamCamRising = false;
	ActiveSlamCam = nullptr;
	bSpiderHangPrepared = false;
	SpiderShotCam = nullptr;
	if (Spider && bHangSequence)
	{
		// Spider keeps BP_Arrow's rotation exactly (your upside-down Roll=180) for the whole hang.
		if (UCameraComponent* SpiderCamera = PickCamera(Spider))
		{
			// Camera is made ready and detached now; the view only switches to it at the flash cut.
			TArray<UCameraComponent*> AllCameras;
			Spider->GetComponents<UCameraComponent>(AllCameras);
			for (UCameraComponent* Other : AllCameras)
			{
				if (Other && Other != SpiderCamera)
				{
					Other->Deactivate();
				}
			}
			SpiderCamera->Activate(true);
			// Kept exactly where your flipped BP camera puts it (spider is already at BP_Arrow), but fixed in the world
			// so the camera stays STATIC and you SEE the spider being lowered.
			SpiderCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			SpiderShotCam = SpiderCamera;
			SpiderCamAuthoredQuat = SpiderCamera->GetComponentQuat();
			SpiderCamFromLoc = SpiderCamera->GetComponentLocation();
			SpiderCamRestLoc = SpiderCamFromLoc;
			bSpiderCamTrack = false;
		}
		const FTransform EndTransform = ArrowTransformOf(HangEnd);
		SpiderHangStartLoc = Location;
		SpiderHangEndLoc = EndTransform.GetLocation(); // BP_Arrow2 (used for the slam height)
		{
			// While hanging it only comes down a short way toward BP_Arrow2 (not all of it).
			const FVector ToEnd = SpiderHangEndLoc - Location;
			const float Distance = ToEnd.Size();
			SpiderHangDropLoc = Distance > KINDA_SMALL_NUMBER ? Location + ToEnd * (FMath::Min(Distance, SpiderHangDropDistance) / Distance) : Location;
		}
		const FQuat ArrowQuat = Rotation.Quaternion();
		SpiderHangStartQuat = bSpiderHangUpsideDown ? (ArrowQuat * FQuat(FVector::ForwardVector, PI)).GetNormalized() : ArrowQuat;
		SpiderHangEndQuat = EndTransform.GetRotation();
		if (ACharacter* SpiderCharacter = Cast<ACharacter>(Spider))
		{
			SpiderCharacter->GetCharacterMovement()->StopMovementImmediately();
			SpiderCharacter->GetCharacterMovement()->SetMovementMode(MOVE_None);
		}
		Spider->SetActorLocationAndRotation(Location, SpiderHangStartQuat, false, nullptr, ETeleportType::TeleportPhysics);
		SpiderFinalRot = SpiderHangStartQuat.Rotator();
		SpiderShotElapsed = 0.0f;
		SpiderShot = ESpiderShot::None;
		// Waits at BP_Arrow, hidden, until the camera cut - then it is simply THERE (nothing pops in).
		Spider->SetActorHiddenInGame(true);
		bSpiderHangPrepared = true;
		SetActorTickEnabled(true);
	}

	if (SpiderSpawnSound && World)
	{
		UGameplayStatics::PlaySoundAtLocation(World, SpiderSpawnSound, FinalLocation);
	}
	if (SpiderSpawnEffect && World)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SpiderSpawnEffect, FinalLocation, FRotator::ZeroRotator);
	}
	if (bSpiderHangPrepared)
	{
		return; // revealed + lowered + slammed from RevealSpiderShot / Tick
	}
	StartSpiderReveal(FinalLocation);
	if (!bSpiderSlam)
	{
		// Nothing to slam down (spawned on the ground / no ground found): speak right away.
		StepSpiderDialogue();
	}
}

void ASOTMChapterEndSequence::StepSpiderDialogue()
{
	if (bSpiderDialogueStarted)
	{
		return;
	}
	bSpiderDialogueStarted = true;
	// Make sure both rows exist even if they were not added to the DataTable.
	if (UWorld* World = GetWorld())
	{
		if (USOTMBossDialogueSubsystem* Dialogue = World->GetSubsystem<USOTMBossDialogueSubsystem>())
		{
			FSOTMBossDialogueLine SpiderLine;
			SpiderLine.Speaker = FText::FromString(TEXT("SPIDER"));
			SpiderLine.Text = FText::FromString(TEXT("My daughter\u2026 and now\u2026 my lightning thief."));
			SpiderLine.Voice = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Dialogue/Chapter1/Temporary/VO_TEMP_SPIDER.VO_TEMP_SPIDER")));
			SpiderLine.Priority = 10;
			SpiderLine.Cooldown = 0.0f;
			SpiderLine.MinDisplayTime = 2.5f;
			SpiderLine.SpeakerColor = FLinearColor(0.60f, 0.10f, 0.70f, 1.0f);
			Dialogue->EnsureRow(SpiderDialogueRow, SpiderLine);

			FSOTMBossDialogueLine OutroLine;
			OutroLine.Speaker = FText::FromString(TEXT("TIMMY"));
			OutroLine.Text = FText::FromString(TEXT("No\u2026 no\u2026 She wasn't supposed to wake up yet\u2014!"));
			OutroLine.Voice = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Dialogue/Chapter1/Temporary/VO_TEMP_TIMMY_OUTRO.VO_TEMP_TIMMY_OUTRO")));
			OutroLine.Priority = 10;
			OutroLine.Cooldown = 0.0f;
			OutroLine.MinDisplayTime = 2.5f;
			OutroLine.SpeakerColor = FLinearColor(0.35f, 0.75f, 1.0f, 1.0f);
			Dialogue->EnsureRow(TimmyOutroDialogueRow, OutroLine);
		}
	}
	// Spider speaks, then Timmy answers, then Timmy collapses.
	WhenDialogueFinishes(SpiderDialogueRow, [this]()
	{
		// Back to Timmy's own camera (flash cut) for his answer and the collapse.
		bSpiderCamTrack = false;
		if (Timmy && PickCamera(Timmy))
		{
			FlashCut([this]() { CutToActorCamera(Timmy, false); });
		}
		WhenDialogueFinishes(TimmyOutroDialogueRow, [this]()
		{
			StepTimmyCollapses();
		});
	});
}

void ASOTMChapterEndSequence::StepTimmyCollapses()
{
	CamPhase = ECamPhase::Collapse;
	float Hold = DelayBeforeFade;
	bTimmyWalking = false;
	if (!Timmy)
	{
		UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Collapse: Timmy does not exist."));
	}
	else
	{
		USkeletalMeshComponent* Mesh = nullptr;
		if (ACharacter* TimmyCharacter = Cast<ACharacter>(Timmy))
		{
			Mesh = TimmyCharacter->GetMesh();
			// Nothing may drive him any more (AI / movement would fight the collapse).
			if (AAIController* AI = Cast<AAIController>(TimmyCharacter->GetController()))
			{
				AI->StopMovement();
				if (AI->BrainComponent)
				{
					AI->BrainComponent->StopLogic(TEXT("Collapse"));
				}
			}
			if (UCharacterMovementComponent* Move = TimmyCharacter->GetCharacterMovement())
			{
				Move->StopMovementImmediately();
				Move->DisableMovement();
			}
		}
		if (!Mesh)
		{
			Mesh = Timmy->FindComponentByClass<USkeletalMeshComponent>();
		}
		if (!Mesh)
		{
			UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Collapse: Timmy has no skeletal mesh."));
		}
		else
		{
			float AnimLength = 0.0f;
			// Anim_HorrorBear_Death is played DIRECTLY on his mesh (no montage / slot needed).
			UAnimSequenceBase* Sequence = TimmyCollapseAnimation;
			if (!Sequence)
			{
				Sequence = LoadObject<UAnimSequenceBase>(nullptr,
					TEXT("/Game/HorrorBear/Animations/Anim_HorrorBear_Death.Anim_HorrorBear_Death"));
			}
			if (Sequence)
			{
				// BP_Timmy may have several skeletal mesh components (the character's default one can be an
				// unused leftover): play it on EVERY mesh that actually has a mesh asset and is not a follower.
				TArray<USkeletalMeshComponent*> TimmyMeshes;
				Timmy->GetComponents<USkeletalMeshComponent>(TimmyMeshes);
				int32 Played = 0;
				for (USkeletalMeshComponent* M : TimmyMeshes)
				{
					if (!M || !M->GetSkeletalMeshAsset() || M->LeaderPoseComponent.IsValid())
					{
						continue;
					}
					M->bPauseAnims = false;
					M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					M->SetAnimationMode(EAnimationMode::AnimationSingleNode);
					M->PlayAnimation(Sequence, false);
					++Played;
					UE_LOG(LogSOTMChapterEnd, Display, TEXT("Collapse: playing '%s' on mesh component '%s' (mesh %s, skeleton %s; animation skeleton %s)."),
						*Sequence->GetName(), *M->GetName(), *M->GetSkeletalMeshAsset()->GetName(),
						M->GetSkeletalMeshAsset()->GetSkeleton() ? *M->GetSkeletalMeshAsset()->GetSkeleton()->GetName() : TEXT("none"),
						Sequence->GetSkeleton() ? *Sequence->GetSkeleton()->GetName() : TEXT("none"));
				}
				if (Played > 0)
				{
					AnimLength = Sequence->GetPlayLength();
				}
				else
				{
					UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Collapse: Timmy has no skeletal mesh component with a mesh asset."));
				}
			}
			else if (TimmyCollapseMontage && Mesh->GetAnimInstance())
			{
				const float Played = Mesh->GetAnimInstance()->Montage_Play(TimmyCollapseMontage);
				if (Played > 0.0f)
				{
					AnimLength = TimmyCollapseMontage->GetPlayLength() / FMath::Max(TimmyCollapseMontage->RateScale, 0.01f);
				}
			}
			else
			{
				UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Collapse: no animation found (assign TimmyCollapseAnimation)."));
			}
			if (AnimLength <= 0.0f && bRagdollOnCollapse)
			{
				Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
				Mesh->SetSimulatePhysics(true);
			}
			Hold = FMath::Max(DelayBeforeFade, AnimLength);
		}
	}
	After(Hold, [this]() { StepFadeOut(); });
}

void ASOTMChapterEndSequence::StepFadeOut()
{
	AnimateBars(0.0f, FadeDuration);
	UWorld* World = GetWorld();
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeDuration, FLinearColor::Black, true, true);
		}
	}
	After(FadeDuration + 0.3f, [this]() { StepEndCard(); });
}

void ASOTMChapterEndSequence::StepEndCard()
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UGameViewportClient* Viewport = GI ? GI->GetGameViewportClient() : nullptr;
	if (Viewport)
	{
		TSharedRef<SWidget> Content =
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(EndCardText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 40))
				.ColorAndOpacity(FLinearColor::White)
			];
		EndCardRoot = Content;
		Viewport->AddViewportWidgetContent(EndCardRoot.ToSharedRef(), 2000);
	}
	After(EndCardDuration, [this]() { StepLeave(); });
}

void ASOTMChapterEndSequence::StepLeave()
{
	OnChapterEndFinished.Broadcast();
	if (!NextLevel.IsNone())
	{
		UGameplayStatics::OpenLevel(this, NextLevel);
	}
}


void ASOTMChapterEndSequence::StartCameraDirector(USceneComponent* Camera)
{
	if (!Camera)
	{
		return;
	}
	DirCamera = Camera;
	CamBaseLoc = Camera->GetComponentLocation();
	CamBaseRot = Camera->GetComponentRotation();
	CamCurLoc = CamBaseLoc;
	CamCurRot = CamBaseRot.Quaternion();
	CamElapsed = 0.0f;
	PullBackCur = 0.0f;
	RollCur = 0.0f;
	CamPhase = ECamPhase::Death;
	bCamDirecting = true;
	SetActorTickEnabled(true);
}

void ASOTMChapterEndSequence::StartSpiderReveal(const FVector& FinalLocation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	SpiderFinalLoc = FinalLocation;
	SetActorTickEnabled(true);

	// Work out the exact ground level under the spider (trace straight down); it slams down to it in Tick.
	if (Spider)
	{
		float FeetOffset = 0.0f;
		if (const ACharacter* SpiderCharacter = Cast<ACharacter>(Spider))
		{
			FeetOffset = SpiderCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		else
		{
			FVector Origin, Extent;
			Spider->GetActorBounds(false, Origin, Extent);
			FeetOffset = Spider->GetActorLocation().Z - (Origin.Z - Extent.Z);
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(SpiderGroundTrace), false, Spider);
		Params.AddIgnoredActor(Isabella);
		Params.AddIgnoredActor(Timmy);
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			Params.AddIgnoredActor(PC->GetPawn());
		}
		const FVector Start = Spider->GetActorLocation();
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0f, 0.0f, 50000.0f), ECC_Visibility, Params))
		{
			SpiderImpactPoint = Hit.ImpactPoint;
			SpiderSlamTargetZ = Hit.ImpactPoint.Z + FeetOffset;
			SpiderSlamHangLeft = SpiderSlamHangTime;
			SpiderSlamVelocity = 0.0f;
			bSpiderSlam = Start.Z > SpiderSlamTargetZ + 1.0f;
			if (bSpiderSlam)
			{
				if (ACharacter* SpiderCharacter = Cast<ACharacter>(Spider))
				{
					SpiderCharacter->GetCharacterMovement()->SetMovementMode(MOVE_None); // we place it, no gravity fight
				}
			}
		}
		else
		{
			UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Spider slam: no ground found below the spider - it stays where it spawned."));
		}
	}
	if (bSpiderSlowMotion)
	{
		bSlowMoActive = true;
		SlowMoLeft = SpiderSlowMotionSeconds;
		UGameplayStatics::SetGlobalTimeDilation(World, SpiderSlowMotionScale);
	}
}

void ASOTMChapterEndSequence::RestoreTimeDilation()
{
	if (bSlowMoActive)
	{
		bSlowMoActive = false;
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
		}
	}
}

void ASOTMChapterEndSequence::BeginSpiderCameraDown()
{
	if (!SpiderShotCam.IsValid() || !Spider)
	{
		BeginSpiderSlam();
		return;
	}
	// Hang is over: flash, and under the flash the spider takes BP_Arrow2's pose and the camera is freed.
	SpiderShot = ESpiderShot::Flash;
	FlipAndDropSpiderCamera(); // direct cut, no flash
}

void ASOTMChapterEndSequence::FlipAndDropSpiderCamera()
{
	UCameraComponent* Cam = SpiderShotCam.Get();
	UWorld* World = GetWorld();
	if (!Cam || !Spider || !World)
	{
		BeginSpiderSlam();
		return;
	}
	// Slam setup: BP_Arrow3 = where the spider slams (location + rotation), CineCameraActor5 = the slam camera.
	AActor* Arrow3 = SpiderSlamPoint ? SpiderSlamPoint.Get() : FindActorByLabelOrClass(World, TEXT("BP_Arrow3"));
	AActor* SlamCam = SpiderSlamCamera ? Cast<AActor>(SpiderSlamCamera.Get()) : nullptr;
	if (!SlamCam)
	{
		for (TActorIterator<ACameraActor> CamIt(World); CamIt; ++CamIt)
		{
			if (CamIt->GetActorLabel() == TEXT("CineCameraActor5") || CamIt->GetName() == TEXT("CineCameraActor5"))
			{
				SlamCam = *CamIt;
				break;
			}
		}
	}
	if (SlamCam)
	{
		FVector SlamLoc = SpiderHangEndLoc;
		FQuat SlamQuat = SpiderHangEndQuat;
		if (Arrow3)
		{
			const FTransform Arrow3Transform = ArrowTransformOf(Arrow3);
			SlamLoc = Arrow3Transform.GetLocation();
			SlamQuat = Arrow3Transform.GetRotation();
			// Comes down onto BP_Arrow3 from the height of the hang.
			SlamLoc.Z = FMath::Max(SpiderHangEndLoc.Z, Arrow3Transform.GetLocation().Z + 300.0f);
		}
		else
		{
			UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Spider slam: 'BP_Arrow3' not found - slamming straight down from BP_Arrow2."));
		}
		SlamLoc.Z += SpiderSlamStartExtraHeight; // waits a little higher before the slam
		Spider->SetActorLocationAndRotation(SlamLoc, SlamQuat, false, nullptr, ETeleportType::TeleportPhysics);
		SpiderFinalRot = SlamQuat.Rotator();
		if (APlayerController* SlamPC = World->GetFirstPlayerController())
		{
			SlamPC->SetViewTarget(SlamCam); // already placed correctly - not moved before the landing
		}
		ActiveSlamCam = SlamCam;
		SlamCamBaseZ = bSlamCameraAbsoluteZ ? SlamCameraStartZ : SlamCam->GetActorLocation().Z;
		SlamCamRiseTotal = bSlamCameraAbsoluteZ ? (SlamCameraEndZ - SlamCameraStartZ) : SlamCameraRise;
		SlamCamBaseXY = FVector2D(SlamCam->GetActorLocation().X, SlamCam->GetActorLocation().Y);
		SlamCamBaseRot = SlamCam->GetActorRotation();
		bSlamCamShaking = false;
		bSlamCamRising = false;
		bSlamCamMode = true;
		bSpiderCamTrack = false;
		SpiderShotElapsed = 0.0f;
		SpiderShot = ESpiderShot::CamDown; // only a short beat before the slam in this mode
		return;
	}
	UE_LOG(LogSOTMChapterEnd, Warning, TEXT("Spider slam: 'CineCameraActor5' not found - using BP_Spider's camera dropped to the ground."));

	// The spider now takes BP_Arrow2's location and rotation (hidden by the flash).
	Spider->SetActorLocationAndRotation(SpiderHangEndLoc, SpiderHangEndQuat, false, nullptr, ETeleportType::TeleportPhysics);
	SpiderFinalRot = SpiderHangEndQuat.Rotator();

	// Free the camera from the spider and put it upright, looking at the spider.
	Cam->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	SpiderCamFromLoc = Cam->GetComponentLocation();
	const FQuat Look = (Spider->GetActorLocation() - SpiderCamFromLoc).Rotation().Quaternion();
	SpiderCamAuthoredQuat = Look;
	Cam->SetWorldLocationAndRotation(SpiderCamFromLoc, Look);
	SpiderCamRestLoc = SpiderCamFromLoc;

	// Floor under the spider -> fast move of the camera down to ground level.
	float FloorZ = Spider->GetActorLocation().Z - 300.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpiderCamFloor), false, Spider);
	Params.AddIgnoredActor(Isabella);
	Params.AddIgnoredActor(Timmy);
	FHitResult Hit;
	const FVector Start = Spider->GetActorLocation();
	if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0f, 0.0f, 50000.0f), ECC_Visibility, Params))
	{
		FloorZ = Hit.ImpactPoint.Z;
	}
	SpiderCamToLoc = SpiderCamFromLoc;
	SpiderCamToLoc.Z = FMath::Min(SpiderCamFromLoc.Z, FloorZ + SpiderCameraGroundHeight);
	SpiderShotElapsed = 0.0f;
	bSpiderCamTrack = true;
	SpiderShot = ESpiderShot::CamDown;
}

void ASOTMChapterEndSequence::BeginSpiderSlam()
{
	SpiderShot = ESpiderShot::None;
	if (Spider)
	{
		StartSpiderReveal(Spider->GetActorLocation());
		if (!bSpiderSlam)
		{
			StepSpiderDialogue();
		}
	}
}

void ASOTMChapterEndSequence::UpdateSpiderShot(const float RealDelta)
{
	auto Smooth = [](const float A) { const float T = FMath::Clamp(A, 0.0f, 1.0f); return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f); };
	UCameraComponent* Cam = SpiderShotCam.Get();

	if (SpiderShot == ESpiderShot::Descend && Spider)
	{
		SpiderShotElapsed += RealDelta;
		const float A = SpiderShotElapsed / FMath::Max(SpiderDescendSeconds, 0.1f);
		const float S = Smooth(A);
		FVector Loc = FMath::Lerp(SpiderHangStartLoc, SpiderHangDropLoc, S);
		Loc.Z += FMath::Sin(SpiderShotElapsed * 2.3f) * 5.0f * (1.0f - S); // slight bob on its thread
		// Pendulum swing on its thread (position + a little tilt), so it really looks like it hangs.
		const float SwingAmp = 1.0f - 0.4f * S;
		const float SwingA = FMath::Sin(SpiderShotElapsed * SpiderSwingSpeed) * SwingAmp;
		const float SwingB = FMath::Sin(SpiderShotElapsed * SpiderSwingSpeed * 0.73f + 1.1f) * SwingAmp;
		Loc += FVector(SwingA * SpiderSwingDistance, SwingB * SpiderSwingDistance * 0.5f, 0.0f);
		const FQuat SwingQuat = FQuat(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(SwingA * SpiderSwingDegrees))
			* FQuat(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(-SwingB * SpiderSwingDegrees * 0.5f));
		const FQuat Rot = (SwingQuat * SpiderHangStartQuat).GetNormalized(); // BP_Arrow rotation + swing
		Spider->SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
		SpiderFinalRot = Rot.Rotator();
		if (A >= 1.0f)
		{
			BeginSpiderCameraDown();
		}
	}
	else if (SpiderShot == ESpiderShot::CamDown && Spider)
	{
		SpiderShotElapsed += RealDelta;
		const float A = SpiderShotElapsed / FMath::Max(SpiderCameraDownSeconds, 0.1f);
		if (Cam)
		{
			const float Fast = FMath::Clamp(A, 0.0f, 1.0f);
			SpiderCamRestLoc = FMath::Lerp(SpiderCamFromLoc, SpiderCamToLoc, 1.0f - FMath::Square(1.0f - Fast)); // sudden drop, soft stop
		}
		if (bSlamCamMode ? SpiderShotElapsed >= 0.4f :  // 0.4s hold after the direct cut on CineCameraActor5
			 A >= 1.0f + 0.2f) // tiny beat, then the slam
		{
			BeginSpiderSlam();
		}
	}

	// CineCameraActor5: rises smoothly after the landing (while the spider speaks) and shakes on impact.
	if (AActor* SlamCamActor = ActiveSlamCam.Get())
	{
		float Rise = 0.0f;
		FVector2D ZoomXY(0.0, 0.0);
		if (bSlamCamRising)
		{
			SlamCamElapsed += RealDelta;
			Rise = SlamCamRiseTotal * Smooth(SlamCamElapsed / FMath::Max(SlamCameraRiseSeconds, 0.1f));
			{ const FVector Fwd = SlamCamBaseRot.Vector().GetSafeNormal2D(); SlamZoomDir = FVector2D(Fwd.X, Fwd.Y); }
			ZoomXY = SlamZoomDir * (SlamZoomDistance * Smooth(SlamCamElapsed / FMath::Max(SlamZoomSeconds, 0.1f)));
			if (SlamCamElapsed >= SlamCameraRiseSeconds)
			{
				bSlamCamRising = false;
				SlamCamBaseZ += SlamCamRiseTotal; // stays at the top of the move
				SlamCamBaseXY += SlamZoomDir * SlamZoomDistance; // keeps the push-in
				Rise = 0.0f;
				StepSpiderDialogue(); // camera move finished -> the spider speaks now
			}
		}
		FVector ShakeOffset = FVector::ZeroVector;
		FRotator ShakeRot = FRotator::ZeroRotator;
		const bool bWasShaking = SpiderCamShakeLeft > 0.0f;
		if (bWasShaking)
		{
			SpiderCamShakeLeft -= RealDelta;
			const float Life = FMath::Clamp(SpiderCamShakeLeft / FMath::Max(SlamShakeSeconds, 0.01f), 0.0f, 1.0f);
			const float Strength = FMath::Square(Life);
			const float Time = GetWorld()->GetRealTimeSeconds();
			ShakeOffset = FVector(FMath::Sin(Time * 47.0f), FMath::Sin(Time * 39.0f + 1.3f), FMath::Sin(Time * 57.0f + 2.1f)) * SlamShakeAmplitude * Strength;
			ShakeRot = FRotator(FMath::Sin(Time * 43.0f), FMath::Sin(Time * 37.0f + 0.7f), FMath::Sin(Time * 29.0f + 1.9f)) * SlamShakeRotation * Strength;
			bSlamCamShaking = true;
		}
		if (bSlamCamRising || bWasShaking || bSlamCamShaking)
		{
			SlamCamActor->SetActorLocationAndRotation(
				FVector(SlamCamBaseXY.X + ZoomXY.X + ShakeOffset.X, SlamCamBaseXY.Y + ZoomXY.Y + ShakeOffset.Y, SlamCamBaseZ + Rise + ShakeOffset.Z),
				SlamCamBaseRot + ShakeRot);
			if (!bWasShaking)
			{
				bSlamCamShaking = false; // shake over: camera is back exactly on its authored rotation
			}
		}
	}

	// The camera keeps watching the spider (descending, then slamming), and shakes on the landing.
	if (bSpiderCamTrack && Cam && Spider)
	{
		const FVector Target = Spider->GetActorLocation();
		const FQuat LookQuat = (Target - SpiderCamRestLoc).Rotation().Quaternion();
		const FQuat Wanted = FQuat::Slerp(SpiderCamAuthoredQuat, LookQuat, 0.85f).GetNormalized();
		FVector Offset = FVector::ZeroVector;
		if (SpiderCamShakeLeft > 0.0f)
		{
			SpiderCamShakeLeft -= RealDelta;
			const float Strength = FMath::Square(FMath::Clamp(SpiderCamShakeLeft / FMath::Max(SpiderLandShakeSeconds, 0.01f), 0.0f, 1.0f)) * SpiderShakeStrength;
			const float Time = GetWorld()->GetRealTimeSeconds();
			Offset = FVector(FMath::Sin(Time * 43.0f) * 9.0f, FMath::Sin(Time * 37.0f + 1.0f) * 9.0f, FMath::Sin(Time * 51.0f + 2.0f) * 7.0f - 12.0f) * Strength;
		}
		const FQuat Current = Cam->GetComponentQuat();
		Cam->SetWorldLocationAndRotation(SpiderCamRestLoc + Offset, FQuat::Slerp(Current, Wanted, FMath::Clamp(RealDelta * 6.0f, 0.0f, 1.0f)).GetNormalized());
	}
}

void ASOTMChapterEndSequence::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Work in real time so slow motion doesn't stretch the camera moves.
	const float Dilation = FMath::Max(UGameplayStatics::GetGlobalTimeDilation(World), 0.01f);
	const float RealDelta = DeltaSeconds / Dilation;

	// Slow motion: hold, then ease back to normal speed over the last 40%.
	if (bSlowMoActive)
	{
		SlowMoLeft -= RealDelta;
		if (SlowMoLeft <= 0.0f)
		{
			RestoreTimeDilation();
		}
		else
		{
			const float T = 1.0f - SlowMoLeft / FMath::Max(SpiderSlowMotionSeconds, 0.01f);
			const float Back = FMath::Clamp((T - 0.6f) / 0.4f, 0.0f, 1.0f);
			UGameplayStatics::SetGlobalTimeDilation(World, FMath::Lerp(SpiderSlowMotionScale, 1.0f, Back));
		}
	}

	// The spider appears once the flash is over.
	if (bSpiderPending)
	{
		SpiderPendingLeft -= RealDelta;
		if (SpiderPendingLeft <= 0.0f)
		{
			bSpiderPending = false;
			SpawnSpiderNow();
		}
	}
	UpdateSpiderShot(RealDelta);
	if (bSpiderRotationLocked && Spider)
	{
		Spider->SetActorRotation(SpiderFinalRot, ETeleportType::TeleportPhysics);
	}

	// Spider slam: hangs briefly, then is pulled down hard to the exact ground level (real time).
	if (Spider && bSpiderSlam)
	{
		if (SpiderSlamHangLeft > 0.0f)
		{
			SpiderSlamHangLeft -= RealDelta;
		}
		else
		{
			SpiderSlamVelocity += SpiderSlamAcceleration * RealDelta;
			FVector Location = Spider->GetActorLocation();
			Location.Z -= SpiderSlamVelocity * RealDelta;
			const bool bLanded = Location.Z <= SpiderSlamTargetZ;
			if (bLanded)
			{
				Location.Z = SpiderSlamTargetZ;
			}
			Spider->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
			if (bLanded)
			{
				bSpiderSlam = false;
				if (ACharacter* SpiderCharacter = Cast<ACharacter>(Spider))
				{
					SpiderCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				}
				if (SpiderLandShakeSeconds > 0.0f && SpiderShakeStrength > 0.0f)
				{
					ShakeLeft = SpiderLandShakeSeconds;
					SpiderCamShakeLeft = SpiderLandShakeSeconds;
				}
				if (ActiveSlamCam.IsValid())
				{
					SpiderCamShakeLeft = SlamShakeSeconds; // the hit: strong shake on the slam camera
					if (APlayerController* FlashPC = World->GetFirstPlayerController())
					{
						if (FlashPC->PlayerCameraManager)
						{
							FlashPC->PlayerCameraManager->StartCameraFade(0.85f, 0.0f, 0.25f, FLinearColor::White, false, true);
						}
					}
					bSlamCamRising = true;
					SlamCamElapsed = 0.0f;
				}
				if (SpiderLandEffect)
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SpiderLandEffect, SpiderImpactPoint, FRotator::ZeroRotator);
				}
				if (!ActiveSlamCam.IsValid() || FMath::IsNearlyZero(SlamCamRiseTotal))
				{
					StepSpiderDialogue(); // no camera move: speak right away
				}
				// otherwise the dialogue starts when the camera has finished its move (UpdateSpiderShot)
			}
		}
	}

	// Timmy walking to his mark.
	if (bTimmyWalking && Timmy)
	{
		TimmyWalkTimeLeft -= DeltaSeconds;
		FVector To = TimmyWalkTarget - Timmy->GetActorLocation();
		To.Z = 0.0f;
		const float Dist = To.Size();
		if (Dist <= 4.0f || TimmyWalkTimeLeft <= 0.0f)
		{
			StepTimmyArrived();
		}
		else
		{
			// The walk animation is rooted: move Timmy along with it at a steady speed.
			const FVector Dir = To / Dist;
			const float Step = FMath::Min(Dist, TimmyWalkCurSpeed * DeltaSeconds);
			FVector NewLocation = Timmy->GetActorLocation() + Dir * Step;
			const float Alpha = FMath::Clamp(1.0f - (Dist - Step) / TimmyWalkTotalDist, 0.0f, 1.0f);
			NewLocation.Z = FMath::Lerp(TimmyWalkStartZ, TimmyWalkTarget.Z, Alpha); // fallback line
			// Follow the REAL floor under him every frame (uneven ground / slopes) so he never sinks while walking.
			{
				float HalfHeight = 88.0f;
				if (const ACharacter* TimmyChar = Cast<ACharacter>(Timmy))
				{
					if (TimmyChar->GetCapsuleComponent())
					{
						HalfHeight = TimmyChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					}
				}
				FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(TimmyWalkFloor), false);
				FloorParams.AddIgnoredActor(Timmy);
				FHitResult FloorHit;
				const FVector TraceStart(NewLocation.X, NewLocation.Y, NewLocation.Z + 150.0f);
				if (GetWorld()->LineTraceSingleByChannel(FloorHit, TraceStart, TraceStart - FVector(0, 0, 600.0f), ECC_Visibility, FloorParams))
				{
					const float WantedZ = FloorHit.ImpactPoint.Z + HalfHeight + 1.0f;
					NewLocation.Z = FMath::FInterpTo(Timmy->GetActorLocation().Z, WantedZ, DeltaSeconds, 18.0f);
					NewLocation.Z = FMath::Max(NewLocation.Z, WantedZ - 0.5f); // never below the floor
				}
			}
			const FRotator Face = FMath::RInterpTo(Timmy->GetActorRotation(), FRotator(0.0f, Dir.Rotation().Yaw, 0.0f), DeltaSeconds, 10.0f);
			Timmy->SetActorLocationAndRotation(NewLocation, Face, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// Dynamic camera.
	USceneComponent* Camera = DirCamera.Get();
	if (bCamDirecting && Camera)
	{
		CamElapsed += RealDelta;
		const FVector Forward = CamBaseRot.Vector();
		const float Push = FMath::Min(CameraDollyDistance, CameraDollySpeed * CamElapsed);
		const float PullTarget = CamPhase == ECamPhase::Spider ? -SpiderCameraPullBack : 0.0f;
		PullBackCur = FMath::FInterpTo(PullBackCur, PullTarget, RealDelta, CamPhase == ECamPhase::Spider ? 9.0f : 2.0f);
		FVector DesiredLoc = CamBaseLoc + Forward * (Push + PullBackCur);

		FQuat DesiredRot = CamBaseRot.Quaternion();
		FVector LookPoint = FVector::ZeroVector;
		float Blend = 0.0f;
		if (CamPhase == ECamPhase::Timmy)
		{
			// Focused on Timmy's spot (before he appears) and then on Timmy himself.
			// While he walks in the camera holds his mark, so he walks INTO the shot.
			LookPoint = (Timmy && !bTimmyWalking) ? Timmy->GetActorLocation() + FVector(0, 0, 70.0f) : TimmyFocusPoint;
			Blend = TimmyFocusBlend;
		}
		else if (Timmy && CamPhase != ECamPhase::Death)
		{
			if (CamPhase == ECamPhase::Spider && Spider)
			{
				LookPoint = (Timmy->GetActorLocation() + Spider->GetActorLocation()) * 0.5f + FVector(0, 0, 60.0f);
				Blend = 0.65f;
			}
			else
			{
				LookPoint = Timmy->GetActorLocation() + FVector(0, 0, 70.0f);
				Blend = 0.5f;
			}
		}
		if (CamPhase == ECamPhase::Timmy)
		{
			// Timmy is small: walk the camera in until it stands TimmyCameraDistance from him,
			// and lower it toward his eye level.
			const FVector ToTarget = LookPoint - CamBaseLoc;
			if (ToTarget.Size() > TimmyCameraDistance)
			{
				DesiredLoc = LookPoint - ToTarget.GetSafeNormal() * TimmyCameraDistance;
			}
			// Camera height is left alone (it is not moved up or down); it is only tilted, see below.
			// After the camera has moved to Timmy: slow push-in (zoom) toward him.
			TimmyZoomElapsed += RealDelta;
			const float ZoomA = FMath::Clamp(TimmyZoomElapsed / FMath::Max(TimmyZoomSeconds, 0.1f), 0.0f, 1.0f);
			const float ZoomS = ZoomA * ZoomA * (3.0f - 2.0f * ZoomA);
			const FVector ZoomDir = (LookPoint - DesiredLoc).GetSafeNormal2D();
			DesiredLoc += ZoomDir * (TimmyZoomDistance * ZoomS);
		}
		if (Blend > 0.0f)
		{
			const FQuat LookQuat = (LookPoint - CamCurLoc).Rotation().Quaternion();
			DesiredRot = FQuat::Slerp(DesiredRot, LookQuat, Blend);
		}
		if (CamPhase == ECamPhase::Timmy && TimmyCameraPitchDown != 0.0f)
		{
			// Rotate (not move) the camera down by a few degrees.
			FRotator TiltedRot = DesiredRot.Rotator();
			TiltedRot.Pitch -= TimmyCameraPitchDown;
			DesiredRot = TiltedRot.Quaternion();
		}
		RollCur = FMath::FInterpTo(RollCur, CamPhase == ECamPhase::Spider ? SpiderDutchRoll : 0.0f, RealDelta, 1.2f);
		DesiredRot = DesiredRot * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(RollCur));

		CamCurLoc = FMath::VInterpTo(CamCurLoc, DesiredLoc, RealDelta, CamPhase == ECamPhase::Timmy ? FMath::Max(TimmyCameraMoveSpeed, 0.05f) : 5.0f);
		CamCurRot = FQuat::Slerp(CamCurRot, DesiredRot, FMath::Clamp(RealDelta * (CamPhase == ECamPhase::Timmy ? 8.0f : 3.0f), 0.0f, 1.0f)).GetNormalized();

		FVector ShakeOffset = FVector::ZeroVector;
		FQuat ShakeRot = FQuat::Identity;
		if (ShakeLeft > 0.0f)
		{
			ShakeLeft -= RealDelta;
			const float Strength = FMath::Square(FMath::Clamp(ShakeLeft / FMath::Max(SpiderShakeSeconds, 0.01f), 0.0f, 1.0f))
				* SpiderShakeStrength;
			const float Time = World->GetRealTimeSeconds();
			ShakeOffset = FVector(FMath::Sin(Time * 43.0f) * 4.0f, FMath::Sin(Time * 37.0f + 1.0f) * 4.0f,
				FMath::Sin(Time * 51.0f + 2.0f) * 3.0f) * Strength;
			ShakeRot = FRotator(FMath::Sin(Time * 47.0f) * 1.0f, FMath::Sin(Time * 41.0f + 0.5f) * 1.0f,
				FMath::Sin(Time * 29.0f) * 0.8f).Quaternion();
			ShakeRot = FQuat::Slerp(FQuat::Identity, ShakeRot, Strength);
		}
		Camera->SetWorldLocationAndRotation(CamCurLoc + ShakeOffset, (CamCurRot * ShakeRot).Rotator());
	}
}
