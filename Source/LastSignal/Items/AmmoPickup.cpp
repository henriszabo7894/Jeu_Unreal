#include "Items/AmmoPickup.h"

#include "Player/BodycamCharacter.h"
#include "Weapons/BodycamWeaponComponent.h"

AAmmoPickup::AAmmoPickup()
{
	ItemId = TEXT("Ammo9mm");
	DisplayName = NSLOCTEXT("LastSignal", "AmmoBox", "Munitions 9mm");
	Weight = 0.3f;
	Color = FLinearColor(0.12f, 0.14f, 0.07f);
	MeshScale = FVector(0.16f, 0.1f, 0.07f);
}

void AAmmoPickup::Interact_Implementation(ABodycamCharacter* User)
{
	if (!User || !User->GetWeapon() || IsActorBeingDestroyed())
	{
		return;
	}

	UBodycamWeaponComponent* Weapon = User->GetWeapon();
	if (Weapon->IsReserveFull())
	{
		User->ShowMessage(NSLOCTEXT("LastSignal", "AmmoFull", "Je ne peux pas porter plus de munitions"), 2.f);
		return;
	}

	const int32 Added = Weapon->AddReserveAmmo(AmmoAmount);
	User->ShowMessage(FText::Format(NSLOCTEXT("LastSignal", "AmmoAdded", "+{0} balles 9mm"), FText::AsNumber(Added)), 2.5f);
	Destroy();
}
