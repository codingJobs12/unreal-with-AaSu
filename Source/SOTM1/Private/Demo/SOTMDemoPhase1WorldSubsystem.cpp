#include "Demo/SOTMDemoPhase1WorldSubsystem.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "SOTMPlayerStateSubsystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace SOTMDemoPhase1Private
{
	const FName MainMenuMap(TEXT("/Game/Main_Menu_Map"));
	const FName MansionMap(TEXT("/Game/Mansion_GameStart"));
	const FName ForestMap(TEXT("/Game/MenuSystemPro/ExampleContent/Designs/Design_Silence/Levels/CH1"));
	const TCHAR* IntroSequence = TEXT("/Game/Cinematics/LS_SOTM_MansionIntro.LS_SOTM_MansionIntro");
	const TCHAR* IsabelMontage = TEXT("/Game/AI/AM_Isabel_JumpScare_Phase3.AM_Isabel_JumpScare_Phase3");
	const TCHAR* IsabelScream = TEXT("/Game/AI/Nightmare_scream_jumpscare_SFX.Nightmare_scream_jumpscare_SFX");

	AActor* FindActor(UWorld* World, const FString& Needle)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (IsValid(Actor) && (Actor->GetActorNameOrLabel().Contains(Needle, ESearchCase::IgnoreCase) ||
				Actor->GetName().Contains(Needle, ESearchCase::IgnoreCase)))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	AActor* FindTimmyActor(UWorld* World)
	{
		if (AActor* NamedActor = FindActor(World, TEXT("HorrorBear"))) return NamedActor;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor)) continue;
			if (USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
					Asset && Asset->GetPathName().Contains(TEXT("HorrorBear"), ESearchCase::IgnoreCase))
				{
					return Actor;
				}
			}
		}
		return nullptr;
	}
}

bool USOTMDemoPhase1WorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void USOTMDemoPhase1WorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FName Map = GetMapPackageName(&InWorld);
	if (Map == SOTMDemoPhase1Private::MainMenuMap)
	{
		InWorld.GetTimerManager().SetTimer(SaveSlotNormalizationTimer, this,
			&ThisClass::NormalizeSaveSlotPresentation, 0.25f, true, 0.25f);
	}
	else if (Map == SOTMDemoPhase1Private::MansionMap)
	{
		InWorld.GetTimerManager().SetTimer(IntroStartTimer, this,
			&ThisClass::TryBeginMansionIntro, 0.10f, true, 0.10f);
	}
}

void USOTMDemoPhase1WorldSubsystem::Deinitialize()
{
	CleanupIntro(true);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::Deinitialize();
}

FName USOTMDemoPhase1WorldSubsystem::GetMapPackageName(const UWorld* World)
{
	if (!World) return NAME_None;
	return FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
}

void USOTMDemoPhase1WorldSubsystem::NormalizeSaveSlotPresentation()
{
	UWorld* World = GetWorld();
	UClass* SlotClass = LoadClass<UUserWidget>(nullptr,
		TEXT("/Game/MenuSystemPro/ExampleContent/Designs/Design_Silence/Widgets/SaveGame/WBP_SaveSlot.WBP_SaveSlot_C"));
	if (!World || !SlotClass) return;

	TArray<UUserWidget*> Slots;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Slots, SlotClass, false);
	if (Slots.IsEmpty()) return;

	int32 Normalized = 0;
	for (UUserWidget* Slot : Slots)
	{
		if (!IsValid(Slot) || !Slot->WidgetTree) continue;
		FString InternalName;
		if (const FStrProperty* Property = FindFProperty<FStrProperty>(Slot->GetClass(), TEXT("SlotName")))
		{
			InternalName = Property->GetPropertyValue_InContainer(Slot);
		}
		int32 Number = INDEX_NONE;
		for (int32 Index = InternalName.Len() - 1; Index >= 0; --Index)
		{
			if (!FChar::IsDigit(InternalName[Index]))
			{
				const FString Digits = InternalName.Mid(Index + 1);
				Number = Digits.IsEmpty() ? INDEX_NONE : FCString::Atoi(*Digits);
				break;
			}
		}
		if (Number >= 1 && Number <= 4)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Slot->WidgetTree->FindWidget(TEXT("SaveSlotNameText"))))
			{
				Label->SetText(FText::Format(NSLOCTEXT("SOTM", "StorySaveSlot", "SAVE SLOT {0}"), Number));
				++Normalized;
			}
		}
	}
	if (Slots.Num() == 4 && Normalized == 4)
	{
		World->GetTimerManager().ClearTimer(SaveSlotNormalizationTimer);
		UE_LOG(LogTemp, Display, TEXT("SOTM Demo Phase 1: exactly four story save slots normalized."));
	}
}

void USOTMDemoPhase1WorldSubsystem::TryBeginMansionIntro()
{
	UWorld* World = GetWorld();
	if (!World || bIntroRequested) return;
	USOTMPlayerStateSubsystem* State = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!State || !PC || !PC->GetPawn()) return;

	World->GetTimerManager().ClearTimer(IntroStartTimer);
	if (!State->ConsumePendingMansionIntro())
	{
		UE_LOG(LogTemp, Display, TEXT("SOTM Demo Phase 1: Continue/direct Mansion entry; intro not replayed."));
		return;
	}
	bIntroRequested = true;
	BeginMansionIntro(PC, State);
}

void USOTMDemoPhase1WorldSubsystem::BeginMansionIntro(APlayerController* PC, USOTMPlayerStateSubsystem* State)
{
	UWorld* World = GetWorld();
	if (!World || !PC || !State) return;
	IntroPlayerController = PC;
	IntroPlayerState = State;
	IntroGameViewport = World->GetGameInstance() ? World->GetGameInstance()->GetGameViewportClient() : nullptr;
	State->AcquireInputLock(ESOTMInputLockReason::Cinematic);
	bCinematicLockHeld = true;

	TimmyActor = SOTMDemoPhase1Private::FindTimmyActor(World);
	IsabelActor = SOTMDemoPhase1Private::FindActor(World, TEXT("Isabel_Phase1"));
	CinematicCameraActor = SOTMDemoPhase1Private::FindActor(World, TEXT("SequenceCamera"));
	if (AActor* Timmy = TimmyActor.Get()) Timmy->SetActorHiddenInGame(false);
	if (AActor* Isabel = IsabelActor.Get())
	{
		StopIsabelGameplayLogic(Isabel);
		Isabel->SetActorHiddenInGame(true);
		Isabel->SetActorEnableCollision(false);
	}
	CreateSubtitleOverlay();
	if (ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, SOTMDemoPhase1Private::IntroSequence))
	{
		FMovieSceneSequencePlaybackSettings Settings;
		Settings.bPauseAtEnd = true;
		Settings.bDisableCameraCuts = true;
		ALevelSequenceActor* CreatedSequenceActor = nullptr;
		IntroSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
			World, Sequence, Settings, CreatedSequenceActor);
		IntroSequenceActor = CreatedSequenceActor;
		if (IntroSequencePlayer) IntroSequencePlayer->Play();
	}

	PresentTimmyOpening();
	FrameActorWithCinematicCamera(TimmyActor.Get());
	World->GetTimerManager().SetTimer(TimmyWarningTimer, this, &ThisClass::PresentTimmyWarning, 5.0f, false);
	World->GetTimerManager().SetTimer(IsabelArrivalTimer, this, &ThisClass::PresentIsabelArrival, 9.0f, false);
	World->GetTimerManager().SetTimer(KnockoutTimer, this, &ThisClass::PresentKnockout, 12.0f, false);
	World->GetTimerManager().SetTimer(DraggingTimer, this, &ThisClass::PresentDragging, 14.0f, false);
	World->GetTimerManager().SetTimer(FinishTimer, this, &ThisClass::FinishMansionIntro, 21.5f, false);
	UE_LOG(LogTemp, Display, TEXT("SOTM Demo Phase 1: Mansion intro started; cinematic lock held."));
}

void USOTMDemoPhase1WorldSubsystem::PresentTimmyOpening()
{
	SetSubtitle(NSLOCTEXT("SOTM", "TimmyName", "TIMMY"),
		NSLOCTEXT("SOTM", "TimmyIntro1", "Hello. I'm Timmy Bottom Smith. You need to listen carefully."));
}

void USOTMDemoPhase1WorldSubsystem::PresentTimmyWarning()
{
	SetSubtitle(NSLOCTEXT("SOTM", "TimmyName", "TIMMY"),
		NSLOCTEXT("SOTM", "TimmyIntro2", "Your powers were stolen... but you can get them back. Just not in here."));
}

void USOTMDemoPhase1WorldSubsystem::PresentIsabelArrival()
{
	SetSubtitle(NSLOCTEXT("SOTM", "TimmyName", "TIMMY"), NSLOCTEXT("SOTM", "TimmyWarning", "Oh no... she's coming—"));
	AActor* Isabel = IsabelActor.Get();
	if (!Isabel) return;
	Isabel->SetActorHiddenInGame(false);
	FrameActorWithCinematicCamera(Isabel);
	if (USkeletalMeshComponent* Mesh = Isabel->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, SOTMDemoPhase1Private::IsabelMontage))
			{
				Anim->Montage_Play(Montage);
			}
		}
	}
}

void USOTMDemoPhase1WorldSubsystem::PresentKnockout()
{
	SetSubtitle(NSLOCTEXT("SOTM", "IsabelName", "ISABELLA"),
		NSLOCTEXT("SOTM", "IsabelKnockout", "You were never meant to keep those gifts."));
	if (USoundBase* Scream = LoadObject<USoundBase>(nullptr, SOTMDemoPhase1Private::IsabelScream))
	{
		UGameplayStatics::PlaySound2D(this, Scream, 0.75f);
	}
	FrameActorWithCinematicCamera(IsabelActor.Get());
	if (APlayerController* PC = IntroPlayerController.Get(); PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, 0.65f, FLinearColor::Black, false, true);
	}
	UE_LOG(LogTemp, Display, TEXT("SOTM Demo Phase 1: scripted zero-damage knockout presented."));
}

void USOTMDemoPhase1WorldSubsystem::PresentDragging()
{
	SetSubtitle(FText::GetEmpty(), NSLOCTEXT("SOTM", "Dragging", "[Footsteps drag you through the darkness...]"));
	if (APlayerController* PC = IntroPlayerController.Get(); PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 1.0f, FLinearColor::Black, false, true);
	}
}

void USOTMDemoPhase1WorldSubsystem::FinishMansionIntro()
{
	SetSubtitle(FText::GetEmpty(), NSLOCTEXT("SOTM", "ForestArrival", "The forest waits."));
	if (APlayerController* PC = IntroPlayerController.Get(); PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, 0.55f, FLinearColor::Black, false, true);
	}
	if (USOTMPlayerStateSubsystem* State = IntroPlayerState.Get(); State && bCinematicLockHeld)
	{
		State->ReleaseInputLock(ESOTMInputLockReason::Cinematic);
		bCinematicLockHeld = false;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TravelTimer, this, &ThisClass::TravelToForest, 0.60f, false);
	}
}

void USOTMDemoPhase1WorldSubsystem::TravelToForest()
{
	RemoveSubtitleOverlay();
	UE_LOG(LogTemp, Display, TEXT("SOTM Demo Phase 1: Mansion intro complete; travelling to production CH1."));
	UGameplayStatics::OpenLevel(this, SOTMDemoPhase1Private::ForestMap);
}

void USOTMDemoPhase1WorldSubsystem::SetSubtitle(const FText& Speaker, const FText& Line)
{
	if (SubtitleSpeakerText) SubtitleSpeakerText->SetText(Speaker);
	if (SubtitleLineText) SubtitleLineText->SetText(Line);
}

void USOTMDemoPhase1WorldSubsystem::FrameActorWithCinematicCamera(AActor* Subject)
{
	AActor* Camera = CinematicCameraActor.Get();
	APlayerController* PC = IntroPlayerController.Get();
	if (!Camera || !Subject || !PC) return;

	const FVector SubjectFocus = Subject->GetActorLocation() + FVector(0.0, 0.0, 75.0);
	FVector Direction = PC->GetPawn()
		? (PC->GetPawn()->GetActorLocation() - Subject->GetActorLocation()).GetSafeNormal2D()
		: Subject->GetActorForwardVector();
	if (Direction.IsNearlyZero()) Direction = FVector::ForwardVector;
	const FVector CameraLocation = SubjectFocus + Direction * 260.0f + FVector(0.0, 0.0, 35.0);
	Camera->SetActorLocationAndRotation(CameraLocation, (SubjectFocus - CameraLocation).Rotation(), false, nullptr,
		ETeleportType::TeleportPhysics);
	PC->SetViewTarget(Camera);
}

void USOTMDemoPhase1WorldSubsystem::CreateSubtitleOverlay()
{
	UGameViewportClient* Viewport = IntroGameViewport.Get();
	if (SubtitleViewportRoot.IsValid() || !Viewport) return;
	TSharedRef<SWidget> Content =
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
		.Padding(FMargin(80.0f, 40.0f, 80.0f, 80.0f))
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.01f, 0.01f, 0.015f, 0.82f))
			.Padding(FMargin(28.0f, 16.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SAssignNew(SubtitleSpeakerText, STextBlock)
					.ColorAndOpacity(FLinearColor(0.72f, 0.16f, 0.88f, 1.0f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SAssignNew(SubtitleLineText, STextBlock)
					.ColorAndOpacity(FLinearColor::White)
					.WrapTextAt(900.0f)
					.Justification(ETextJustify::Center)
				]
			]
		];
	SubtitleViewportRoot = Content;
	Viewport->AddViewportWidgetContent(SubtitleViewportRoot.ToSharedRef(), 950);
}

void USOTMDemoPhase1WorldSubsystem::RemoveSubtitleOverlay()
{
	if (SubtitleViewportRoot.IsValid())
	{
		if (UGameViewportClient* Viewport = IntroGameViewport.Get())
		{
			Viewport->RemoveViewportWidgetContent(SubtitleViewportRoot.ToSharedRef());
		}
	}
	SubtitleViewportRoot.Reset();
	SubtitleSpeakerText.Reset();
	SubtitleLineText.Reset();
}

void USOTMDemoPhase1WorldSubsystem::StopIsabelGameplayLogic(AActor* Isabel) const
{
	// The production BP_AI still contains legacy timer-driven Blackboard writes.
	// Keep its Blackboard alive, but suspend visible gameplay and clear the timers
	// owned by this temporary Mansion instance. The Mansion transitions to CH1 at
	// the end of the intro, so none of these timers need to be restored.
	if (UWorld* World = Isabel->GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(Isabel);
	}
	Isabel->SetActorTickEnabled(false);
	if (APawn* Pawn = Cast<APawn>(Isabel))
	{
		if (AAIController* Controller = Cast<AAIController>(Pawn->GetController()))
		{
			if (UWorld* World = Controller->GetWorld())
			{
				World->GetTimerManager().ClearAllTimersForObject(Controller);
			}
			Controller->StopMovement();
			Controller->SetActorTickEnabled(false);
		}
	}
}

void USOTMDemoPhase1WorldSubsystem::CleanupIntro(const bool bRestoreCameraFade)
{
	if (IntroSequencePlayer) IntroSequencePlayer->Stop();
	if (USOTMPlayerStateSubsystem* State = IntroPlayerState.Get(); State && bCinematicLockHeld)
	{
		State->ReleaseInputLock(ESOTMInputLockReason::Cinematic);
	}
	bCinematicLockHeld = false;
	if (bRestoreCameraFade)
	{
		if (APlayerController* PC = IntroPlayerController.Get(); PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StopCameraFade();
		}
	}
	RemoveSubtitleOverlay();
	IntroSequencePlayer = nullptr;
	IntroSequenceActor = nullptr;
	IntroPlayerController.Reset();
	CinematicCameraActor.Reset();
	IntroGameViewport.Reset();
	IntroPlayerState.Reset();
}
