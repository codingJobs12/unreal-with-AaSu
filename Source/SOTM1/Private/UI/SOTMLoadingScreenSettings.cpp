#include "UI/SOTMLoadingScreenSettings.h"

#include "Engine/Texture2D.h"

USOTMLoadingScreenSettings::USOTMLoadingScreenSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("SOTM Loading Screen");

	// Chapter 1 thumbnail used by the level's meta data (DA_CH1).
	BackgroundTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/3b6dc938-2b23-4871-a137-61944389d60c.3b6dc938-2b23-4871-a137-61944389d60c")));
	// The real CH1 map is /Game/MenuSystemPro/ExampleContent/Designs/Design_Silence/Levels/CH1.
	LevelPathContains = { TEXT("Design_Silence/Levels/CH1"), TEXT("Levels/CH1"), TEXT("Chapter1_Forest") };
	// The Chapter 1 artwork already carries its own title, so no extra title text by default.
	Title = FText::GetEmpty();
	SubTitle = FText::GetEmpty();
	Tips = {
		TEXT("Lightning only stuns them. It never kills."),
		TEXT("Stunning one cousin makes the others angry."),
		TEXT("Speed Boost has a cooldown. Use it wisely."),
		TEXT("When they get close... run."),
		TEXT("Not every sound in the forest is the wind."),
	};
}
