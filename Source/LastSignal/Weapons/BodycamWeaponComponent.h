#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodycamWeaponComponent.generated.h"

class ABodycamCharacter;

/**
 * Arme à feu réaliste : tir semi-automatique, chargeur, munitions de réserve rares,
 * rechargement qui prend du temps, dispersion plus grande sans viser ou en bougeant.
 * Le tir est "hitscan" (la balle touche instantanément ce qui est visé).
 */
UCLASS(ClassGroup = (LastSignal), meta = (BlueprintSpawnableComponent))
class LASTSIGNAL_API UBodycamWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodycamWeaponComponent();

	/** Tente de tirer. Renvoie true si une balle est partie. */
	UFUNCTION(BlueprintCallable, Category = "Arme")
	bool TryFire();

	UFUNCTION(BlueprintCallable, Category = "Arme")
	void StartReload();

	/** Ajoute des munitions de réserve. Renvoie le nombre réellement ajouté. */
	UFUNCTION(BlueprintCallable, Category = "Arme")
	int32 AddReserveAmmo(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Arme")
	bool IsReloading() const { return bReloading; }

	/** 0 -> 1 pendant le rechargement. */
	UFUNCTION(BlueprintPure, Category = "Arme")
	float GetReloadProgress() const;

	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	int32 GetMagazineSize() const { return MagazineSize; }
	bool IsReserveFull() const { return ReserveAmmo >= MaxReserveAmmo; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme")
	FText WeaponName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Munitions")
	int32 MagazineSize = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Munitions")
	int32 AmmoInMagazine = 8;

	/** Munitions de départ en réserve (rares !). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Munitions")
	int32 ReserveAmmo = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Munitions")
	int32 MaxReserveAmmo = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Tir")
	float Damage = 34.f;

	/** Portée en cm (60 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Tir")
	float Range = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Tir")
	float TimeBetweenShots = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Tir")
	float ReloadDuration = 2.3f;

	/** Dispersion (degrés) sans viser. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Tir")
	float HipSpread = 3.5f;

	/** Dispersion (degrés) en visant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Tir")
	float AimSpread = 0.6f;

	/** Force du recul. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Tir")
	float RecoilKick = 0.9f;

	/** Poussée appliquée aux objets physiques touchés. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme|Tir")
	float ImpactImpulse = 25000.f;

private:
	ABodycamCharacter* GetCharacter() const;
	void FinishReload();

	bool bReloading = false;
	float LastFireTime = -100.f;
	float ReloadStartTime = 0.f;
	FTimerHandle ReloadTimer;
};
