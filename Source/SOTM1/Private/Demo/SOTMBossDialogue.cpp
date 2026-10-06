#include "Demo/SOTMBossDialogue.h"

#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UI/SOTMSubtitleStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMBossDialogue, Log, All);

namespace
{
	const TCHAR* DefaultTablePath = TEXT("/Game/Dialogue/DT_BossDialogue.DT_BossDialogue");
}

bool USOTMBossDialogueSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void USOTMBossDialogueSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Load the table and its voice lines now, so no disk hitch happens mid-fight on the first line.
	if (UDataTable* Table = ResolveTable(nullptr))
	{
		Table->ForeachRow<FSOTMBossDialogueLine>(TEXT("BossDialoguePreload"),
			[this](const FName&, const FSOTMBossDialogueLine& Row)
			{
				if (USoundBase* Sound = Row.Voice.LoadSynchronous())
				{
					PreloadedVoices.AddUnique(Sound);
					// Decode/prime the audio now so the first play does not hitch.
					UGameplayStatics::PrimeSound(Sound);
				}
			});
	}
}

void USOTMBossDialogueSubsystem::Deinitialize()
{
	StopDialogue();
	Super::Deinitialize();
}

void USOTMBossDialogueSubsystem::PreloadVoicesAsync(UDataTable* Table)
{
	if (!Table)
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	Table->ForeachRow<FSOTMBossDialogueLine>(TEXT("BossDialoguePreloadAsync"),
		[&Paths](const FName&, const FSOTMBossDialogueLine& Row)
		{
			if (!Row.Voice.IsNull() && !Row.Voice.IsValid())
			{
				Paths.AddUnique(Row.Voice.ToSoftObjectPath());
			}
		});
	if (Paths.Num() > 0)
	{
		PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}
}

UDataTable* USOTMBossDialogueSubsystem::ResolveTable(UDataTable* TableOverride)
{
	if (TableOverride)
	{
		if (CachedTable != TableOverride)
		{
			CachedTable = TableOverride;
			PreloadVoicesAsync(TableOverride);
		}
		return TableOverride;
	}
	if (!CachedTable)
	{
		CachedTable = LoadObject<UDataTable>(nullptr, DefaultTablePath);
		if (!CachedTable)
		{
			UE_LOG(LogSOTMBossDialogue, Warning, TEXT("Boss dialogue table not found at %s - create it (row struct SOTMBossDialogueLine) or pass a Table."), DefaultTablePath);
		}
	}
	return CachedTable;
}

bool USOTMBossDialogueSubsystem::PlayLine(const FName RowName, UDataTable* TableOverride, const bool bForce)
{
	UDataTable* Table = ResolveTable(TableOverride);
	const FSOTMBossDialogueLine* Row = Table ? Table->FindRow<FSOTMBossDialogueLine>(RowName, TEXT("BossDialogue"), false) : nullptr;
	UWorld* World = GetWorld();
	if (!Row || !World)
	{
		UE_LOG(LogSOTMBossDialogue, Warning, TEXT("Row '%s' not found."), *RowName.ToString());
		return false;
	}
	const double Now = World->GetTimeSeconds();
	if (const double* Last = bForce ? nullptr : LastPlayedTime.Find(RowName))
	{
		if (Now - *Last < Row->Cooldown)
		{
			return false;
		}
	}
	// One trigger = one line (plus its chained NextRow). While a line/chain plays, or right
	// after one ended, ordinary triggers are ignored so lines never pile up.
	if (!bForce && bPlaying && (bInChain || Row->Priority <= CurrentPriority))
	{
		return false;
	}
	if (!bForce && !bPlaying && Row->Priority < 2 && Now - LastLineEndTime < 3.0)
	{
		return false;
	}
	bInChain = false;
	StopDialogue();
	LastPlayedTime.Add(RowName, Now);
	Present(RowName, *Row);
	return true;
}

void USOTMBossDialogueSubsystem::EnsureRow(const FName RowName, const FSOTMBossDialogueLine& Row)
{
	UDataTable* Table = ResolveTable(nullptr);
	if (!Table)
	{
		return;
	}
	if (Table->FindRow<FSOTMBossDialogueLine>(RowName, TEXT("BossDialogueEnsure"), false))
	{
		return;
	}
	UE_LOG(LogSOTMBossDialogue, Warning, TEXT("Row '%s' was missing from the table - added it in memory (add it to the DataTable to edit it)."), *RowName.ToString());
	Table->AddRow(RowName, Row);
	if (!Row.Voice.IsNull())
	{
		if (USoundBase* Sound = Row.Voice.LoadSynchronous())
		{
			PreloadedVoices.AddUnique(Sound);
		}
	}
}

bool USOTMBossDialogueSubsystem::PlayRandomFromGroup(const FName Group, UDataTable* TableOverride)
{
	UDataTable* Table = ResolveTable(TableOverride);
	if (!Table)
	{
		return false;
	}
	TArray<FName> Candidates;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FSOTMBossDialogueLine* Row = reinterpret_cast<const FSOTMBossDialogueLine*>(Pair.Value);
		if (Row && Row->Group == Group)
		{
			Candidates.Add(Pair.Key);
		}
	}
	if (Candidates.Num() > 1)
	{
		Candidates.Remove(LastGroupRow);
	}
	// Try random picks until one is not on cooldown.
	while (Candidates.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, Candidates.Num() - 1);
		const FName Pick = Candidates[Index];
		if (PlayLine(Pick, Table))
		{
			LastGroupRow = Pick;
			return true;
		}
		Candidates.RemoveAtSwap(Index);
	}
	return false;
}

void USOTMBossDialogueSubsystem::Present(const FName RowName, const FSOTMBossDialogueLine& Row)
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!World || !Viewport)
	{
		return;
	}
	bPlaying = true;
	CurrentRow = RowName;
	CurrentPriority = Row.Priority;
	PendingNextRow = Row.NextRow;
	FullLine = Row.Text.ToString();
	RevealedChars = 0;

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
					SAssignNew(SpeakerText, STextBlock)
					.Font(SOTMSubtitle::Font())
					.Text(Row.Speaker)
					.ColorAndOpacity(Row.SpeakerColor)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SAssignNew(LineText, STextBlock)
					.Font(SOTMSubtitle::Font())
					.Text(FText::GetEmpty())
					.ColorAndOpacity(FLinearColor::White)
					.WrapTextAt(900.0f)
					.Justification(ETextJustify::Center)
				]
			]
		];
	OverlayRoot = Content;
	Viewport->AddViewportWidgetContent(OverlayRoot.ToSharedRef(), 940);

	if (Row.Voice.IsNull())
	{
		UE_LOG(LogSOTMBossDialogue, Warning, TEXT("Row '%s' has NO Voice assigned - it will play silently (subtitle only)."), *RowName.ToString());
	}
	float VODuration = 0.0f;
	bVoiceDone = true;
	bool bVoiceLoadPending = false;
	auto StartVoice = [this](USoundBase* Sound)
	{
		Voice = UGameplayStatics::SpawnSound2D(this, Sound, 1.0f, 1.0f, 0.0f, nullptr, false, true);
		if (Voice)
		{
			bVoiceDone = false;
			Voice->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleVoiceFinished);
		}
	};
	if (USoundBase* Sound = Row.Voice.Get())
	{
		// Already in memory (preloaded) - no disk access, no hitch.
		VODuration = Sound->GetDuration();
		StartVoice(Sound);
	}
	else if (!Row.Voice.IsNull())
	{
		// Not loaded yet: NEVER block the game thread for it. The subtitle starts now and the
		// voice joins as soon as the asset has streamed in.
		bVoiceLoadPending = true;
		bVoiceDone = false;
		const FName ForRow = RowName;
		const TSoftObjectPtr<USoundBase> SoftVoice = Row.Voice;
		VoiceLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			SoftVoice.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(this, [this, ForRow, SoftVoice, StartVoice]()
			{
				USoundBase* Loaded = SoftVoice.Get();
				if (!Loaded)
				{
					UE_LOG(LogSOTMBossDialogue, Warning, TEXT("Row '%s': Voice asset '%s' failed to load (wrong path / not a Sound asset)."),
						*ForRow.ToString(), *SoftVoice.ToSoftObjectPath().ToString());
					bVoiceDone = true;
					return;
				}
				PreloadedVoices.AddUnique(Loaded);
				if (bPlaying && CurrentRow == ForRow)
				{
					StartVoice(Loaded);
				}
			}));
	}
	const float TextTime = FMath::Max(Row.MinDisplayTime, FullLine.Len() * 0.06f + 0.6f);
	const float HoldTime = VODuration > 0.0f ? VODuration : TextTime;
	bMinTimeDone = false;
	World->GetTimerManager().SetTimer(MinTimeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bMinTimeDone = true;
		if (bVoiceDone)
		{
			FinishLine();
		}
	}), FMath::Max(HoldTime, Row.MinDisplayTime), false);
	// Never hang if the audio callback is missed.
	World->GetTimerManager().SetTimer(SafetyTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		FinishLine();
	}), FMath::Max(HoldTime, Row.MinDisplayTime) + (bVoiceLoadPending ? 8.0f : 2.0f), false);

	const float Target = VODuration > 0.0f ? VODuration : TextTime;
	const float Paced = (Target * 0.85f) / FMath::Max(FullLine.Len(), 1);
	World->GetTimerManager().SetTimer(
		TypewriterTimer, this, &ThisClass::TickTypewriter, FMath::Clamp(Paced, 0.015f, 0.06f), true);
}

void USOTMBossDialogueSubsystem::TickTypewriter()
{
	if (!LineText.IsValid())
	{
		return;
	}
	++RevealedChars;
	LineText->SetText(FText::FromString(FullLine.Left(RevealedChars)));
	if (RevealedChars >= FullLine.Len())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TypewriterTimer);
		}
	}
}

void USOTMBossDialogueSubsystem::HandleVoiceFinished()
{
	bVoiceDone = true;
	if (Voice)
	{
		Voice->OnAudioFinished.RemoveAll(this);
		Voice = nullptr;
	}
	if (bMinTimeDone)
	{
		FinishLine();
	}
}

void USOTMBossDialogueSubsystem::FinishLine()
{
	if (!bPlaying)
	{
		return;
	}
	const FName Next = PendingNextRow;
	const FName Finished = CurrentRow;
	UDataTable* Table = CachedTable;
	StopDialogue();
	OnLineFinished.Broadcast(Finished);
	if (!Next.IsNone())
	{
		// Chained line (bypasses its cooldown so the pair always plays together).
		LastPlayedTime.Remove(Next);
		LastLineEndTime = -100.0;
		PlayLine(Next, Table);
		bInChain = bPlaying;
	}
}

void USOTMBossDialogueSubsystem::StopDialogue()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(TypewriterTimer);
		TM.ClearTimer(MinTimeTimer);
		TM.ClearTimer(SafetyTimer);
	}
	if (Voice)
	{
		Voice->OnAudioFinished.RemoveAll(this);
		Voice->Stop();
		Voice = nullptr;
	}
	RemoveOverlay();
	if (bPlaying)
	{
		if (UWorld* EndWorld = GetWorld())
		{
			LastLineEndTime = EndWorld->GetTimeSeconds();
		}
	}
	bPlaying = false;
	bInChain = false;
	CurrentPriority = 0;
	PendingNextRow = NAME_None;
}

void USOTMBossDialogueSubsystem::RemoveOverlay()
{
	if (OverlayRoot.IsValid())
	{
		UWorld* World = GetWorld();
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(OverlayRoot.ToSharedRef());
		}
	}
	OverlayRoot.Reset();
	SpeakerText.Reset();
	LineText.Reset();
}

namespace
{
	USOTMBossDialogueSubsystem* GetSubsystem(UObject* Context)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		return World ? World->GetSubsystem<USOTMBossDialogueSubsystem>() : nullptr;
	}
}

bool USOTMBossDialogueLibrary::PlayBossDialogue(UObject* WorldContextObject, const FName RowName, UDataTable* Table)
{
	USOTMBossDialogueSubsystem* Sub = GetSubsystem(WorldContextObject);
	return Sub && Sub->PlayLine(RowName, Table);
}

bool USOTMBossDialogueLibrary::PlayBossDialogueFromGroup(UObject* WorldContextObject, const FName Group, UDataTable* Table)
{
	USOTMBossDialogueSubsystem* Sub = GetSubsystem(WorldContextObject);
	return Sub && Sub->PlayRandomFromGroup(Group, Table);
}

void USOTMBossDialogueLibrary::StopBossDialogue(UObject* WorldContextObject)
{
	if (USOTMBossDialogueSubsystem* Sub = GetSubsystem(WorldContextObject))
	{
		Sub->StopDialogue();
	}
}

bool USOTMBossDialogueLibrary::IsBossDialoguePlaying(UObject* WorldContextObject)
{
	const USOTMBossDialogueSubsystem* Sub = GetSubsystem(WorldContextObject);
	return Sub && Sub->IsPlaying();
}

void USOTMBossDialogueLibrary::PreloadBossDialogue(UObject* WorldContextObject, UDataTable* Table)
{
	if (USOTMBossDialogueSubsystem* Sub = GetSubsystem(WorldContextObject))
	{
		Sub->PreloadTable(Table);
	}
}
