#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class ABodycamCharacter;

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Tout ce avec quoi le joueur peut interagir (objets, portes, interrupteurs...).
 * Le joueur vise l'objet, le texte s'affiche au centre de l'écran, et il appuie sur la touche d'interaction.
 */
class LASTSIGNAL_API IInteractable
{
	GENERATED_BODY()

public:
	/** Texte affiché quand le joueur vise l'objet (ex: "Ramasser Jerrican"). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText() const;

	/** Appelé quand le joueur appuie sur la touche d'interaction. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(ABodycamCharacter* User);
};
