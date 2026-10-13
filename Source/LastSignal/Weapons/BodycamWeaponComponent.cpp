#include "Weapons/BodycamWeaponComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Player/BodycamCharacter.h"

UBodycamWeaponComponent::UBodycamWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WeaponName = NSLOCTEXT("LastSignal", "Pistol", "Pistolet 9mm");
}

ABodycamCharacter* UBodycamWeaponComponent::GetCharacter() const
{
	return Cast<ABodycamCharacter>(GetOwner());
}

bool UBodycamWeaponComponent::TryFire()
{
	ABodycamCharacter* Character = GetCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || !Character->CanUseWeapon() || bReloading)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastFireTime < TimeBetweenShots)
	{
		return false;
	}
	LastFireTime = Now;

	if (AmmoInMagazine <= 0)
	{
		Character->ShowMessage(ReserveAmmo > 0
			? NSLOCTEXT("LastSignal", "EmptyMag", "*clic* Chargeur vide - [R] pour recharger")
			: NSLOCTEXT("LastSignal", "NoAmmo", "*clic* Plus de munitions"), 2.f);
		return false;
	}

	--AmmoInMagazine;

	UCameraComponent* Camera = Character->GetCameraComponent();
	const FVector Start = Camera->GetComponentLocation();
	const FVector Aim = Camera->GetForwardVector();

	// Dispersion : plus grande sans viser et en mouvement
	float Spread = Character->IsAiming() ? AimSpread : HipSpread;
	const float MoveSpeed = static_cast<float>(Character->GetVelocity().Size2D());
	Spread *= 1.f + FMath::Clamp(MoveSpeed / 300.f, 0.f, 1.5f);
	const FVector Direction = FMath::VRandCone(Aim, FMath::DegreesToRadians(Spread * 0.5f));

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamWeapon), true, Character);
	if (World->LineTraceSingleByChannel(Hit, Start, Start + Direction * Range, ECC_Visibility, Params))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			UGameplayStatics::ApplyPointDamage(HitActor, Damage, Direction, Hit, Character->GetController(), Character, UDamageType::StaticClass());
		}
		if (UPrimitiveComponent* HitComp = Hit.GetComponent())
		{
			if (HitComp->IsSimulatingPhysics())
			{
				HitComp->AddImpulseAtLocation(Direction * ImpactImpulse, Hit.ImpactPoint);
			}
		}
	}

	Character->OnWeaponFired(RecoilKick);
	return true;
}

void UBodycamWeaponComponent::StartReload()
{
	ABodycamCharacter* Character = GetCharacter();
	if (!Character || bReloading || AmmoInMagazine >= MagazineSize)
	{
		return;
	}
	if (ReserveAmmo <= 0)
	{
		Character->ShowMessage(NSLOCTEXT("LastSignal", "NoReserve", "Plus de chargeur de rechange"), 2.f);
		return;
	}

	bReloading = true;
	ReloadStartTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &UBodycamWeaponComponent::FinishReload, ReloadDuration, false);
}

void UBodycamWeaponComponent::FinishReload()
{
	const int32 Needed = MagazineSize - AmmoInMagazine;
	const int32 Taken = FMath::Min(Needed, ReserveAmmo);
	AmmoInMagazine += Taken;
	ReserveAmmo -= Taken;
	bReloading = false;
}

int32 UBodycamWeaponComponent::AddReserveAmmo(int32 Amount)
{
	const int32 Added = FMath::Clamp(Amount, 0, FMath::Max(0, MaxReserveAmmo - ReserveAmmo));
	ReserveAmmo += Added;
	return Added;
}

float UBodycamWeaponComponent::GetReloadProgress() const
{
	if (!bReloading || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadStartTime) / ReloadDuration, 0.f, 1.f);
}
