#pragma once

#include "CoreMinimal.h"
#include "Items/ItemPickup.h"
#include "AmmoPickup.generated.h"

/** Boîte de munitions : remplit directement la réserve de l'arme. */
UCLASS()
class LASTSIGNAL_API AAmmoPickup : public AItemPickup
{
	GENERATED_BODY()

public:
	AAmmoPickup();

	virtual void Interact_Implementation(ABodycamCharacter* User) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objet")
	int32 AmmoAmount = 6;
};
