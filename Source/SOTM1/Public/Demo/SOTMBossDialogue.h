#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "SOTMBossDialogue.generated.h"

class SWidget;
struct FStreamableHandle;
class STextBlock;
class UAudioComponent;
class USoundBase;

/**
 * One row of the boss dialogue DataTable (Row Name = the id you call from Blueprint).
 * Create the table in the editor: Content Browser > Miscellaneous > Data Table > pick
 * "SOTMBossDialogueLine" as the row structure. Default path: /Game/Dialogue/DT_BossDialogue.
 */
USTRUCT(BlueprintType)
struct SOTM1_API FSOTMBossDialogueLine : public FTableRowBase
{
	GENERATED_BODY()

	/** Name shown above the line (ISABELLA, TIMMY...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Speaker;

	/** Subtitle text (typed out with the typewriter effect). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;

	/** Optional voice-over. When empty the line is shown for a time based on its length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundBase> Voice;

	/** Optional group name (e.g. "Attack", "Stunned"). PlayBossDialogueFromGroup picks a random row of the group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Group;

	/** Optional row to play right after this one finishes (e.g. "AAAH you dare?!" -> "I GAVE YOU THAT POWER!"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NextRow;

	/** Higher priority interrupts a playing line; equal/lower is skipped while another line plays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Priority = 0;

	/** Seconds before THIS row may play again (stops fight spam). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Cooldown = 6.0f;

	/** Minimum time the subtitle stays up (also used when there is no voice). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinDisplayTime = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor SpeakerColor = FLinearColor(0.85f, 0.10f, 0.12f, 1.0f);
};

/**
 * In-fight dialogue: a non-blocking subtitle + voice line. Unlike the cutscene dialogues it
 * does NOT lock input or hide the HUD, so it can play while the fight is happening.
 */
UCLASS()
class SOTM1_API USOTMBossDialogueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Returns true if the line started. */
	/** bForce (cutscenes): ignore cooldown, priority and the short gap after the previous line. */
	bool PlayLine(FName RowName, UDataTable* TableOverride = nullptr, bool bForce = false);
	bool PlayRandomFromGroup(FName Group, UDataTable* TableOverride = nullptr);

	/** If the default table has no row with this name, adds it in memory (not saved to disk) so cutscene lines always play. */
	void EnsureRow(FName RowName, const FSOTMBossDialogueLine& Row);
	void StopDialogue();
	void PreloadTable(UDataTable* Table) { ResolveTable(Table); }
	bool IsPlaying() const { return bPlaying; }

	/** Fired when a line finished naturally (not when interrupted). Argument = row name. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLineFinished, FName);
	FOnLineFinished OnLineFinished;

private:
	UDataTable* ResolveTable(UDataTable* TableOverride);
	void PreloadVoicesAsync(UDataTable* Table);
	void Present(FName RowName, const FSOTMBossDialogueLine& Row);
	void TickTypewriter();
	void HandleVoiceFinished();
	void FinishLine();
	void RemoveOverlay();

	UPROPERTY(Transient) TObjectPtr<UDataTable> CachedTable;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Voice;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> PreloadedVoices;

	TSharedPtr<FStreamableHandle> VoiceLoadHandle;
	TSharedPtr<FStreamableHandle> PreloadHandle;
	TSharedPtr<SWidget> OverlayRoot;
	TSharedPtr<STextBlock> SpeakerText;
	TSharedPtr<STextBlock> LineText;
	TMap<FName, double> LastPlayedTime;
	FName CurrentRow;
	FName PendingNextRow;
	FString FullLine;
	int32 RevealedChars = 0;
	int32 CurrentPriority = 0;
	bool bPlaying = false;
	bool bInChain = false;
	double LastLineEndTime = -100.0;
	bool bVoiceDone = true;
	bool bMinTimeDone = true;
	FName LastGroupRow;
	FTimerHandle TypewriterTimer;
	FTimerHandle MinTimeTimer;
	FTimerHandle SafetyTimer;
};

/** Blueprint nodes - call these from BP_Isabel (or anything else). */
UCLASS()
class SOTM1_API USOTMBossDialogueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Plays one row of the boss dialogue table. Leave Table empty to use /Game/Dialogue/DT_BossDialogue. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Boss Dialogue", meta=(WorldContext="WorldContextObject"))
	static bool PlayBossDialogue(UObject* WorldContextObject, FName RowName, UDataTable* Table = nullptr);

	/** Plays a random row (never the same one twice in a row) whose Group matches, e.g. "Attack". */
	UFUNCTION(BlueprintCallable, Category="SOTM|Boss Dialogue", meta=(WorldContext="WorldContextObject"))
	static bool PlayBossDialogueFromGroup(UObject* WorldContextObject, FName Group, UDataTable* Table = nullptr);

	/** Call once at BeginPlay (or when Isabella spawns) with your table so no line ever loads mid-fight. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Boss Dialogue", meta=(WorldContext="WorldContextObject"))
	static void PreloadBossDialogue(UObject* WorldContextObject, UDataTable* Table = nullptr);

	UFUNCTION(BlueprintCallable, Category="SOTM|Boss Dialogue", meta=(WorldContext="WorldContextObject"))
	static void StopBossDialogue(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="SOTM|Boss Dialogue", meta=(WorldContext="WorldContextObject"))
	static bool IsBossDialoguePlaying(UObject* WorldContextObject);
};
