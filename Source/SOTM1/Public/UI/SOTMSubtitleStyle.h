#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"

namespace SOTMSubtitle
{
	// Slate's default text size is 9; every subtitle (Mansion intro, Cousins, Timmy upgrade
	// lines, chest line) is scaled by this factor so they stay consistent.
	constexpr float FontScale = 1.2f;

	inline FSlateFontInfo Font()
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(9.0f * FontScale));
	}
}
