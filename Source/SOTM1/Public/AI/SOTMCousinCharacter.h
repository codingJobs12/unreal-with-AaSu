#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SOTMCousinCharacter.generated.h"

class UPointLightComponent;

/** Native reusable production Cousin pawn using the existing Cruel Doll assets. */
UCLASS(Blueprintable)
class SOTM1_API ASOTMCousinCharacter final : public ACharacter
{
	GENERATED_BODY()

public:
	ASOTMCousinCharacter();

	void SetPresentationVariant(int32 InVariantIndex);
	int32 GetPresentationVariant() const { return PresentationVariant; }

private:
	UPROPERTY(VisibleInstanceOnly, Category="SOTM|Cousin")
	int32 PresentationVariant = 0;

	// Small glowing marker light so the player can spot a Cousin from long range in the
	// dark forest, without needing to be lit by the flashlight first.
	UPROPERTY(VisibleAnywhere, Category="SOTM|Cousin")
	TObjectPtr<UPointLightComponent> AlertLight;
};

