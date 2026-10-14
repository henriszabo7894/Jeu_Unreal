#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BodycamCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UInventoryComponent;
class UBodycamWeaponComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Le joueur. Toute la sensation "bodycam" se trouve ici :
 *  - caméra à hauteur de poitrine, grand angle, qui suit le corps avec un peu d'inertie
 *  - balancement de la marche, respiration (plus forte quand on est essoufflé)
 *  - se pencher à gauche / à droite, s'accroupir, sprinter avec endurance
 *  - lampe torche avec un léger retard et des grésillements de temps en temps
 */
UCLASS()
class LASTSIGNAL_API ABodycamCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABodycamCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UCameraComponent* GetCameraComponent() const { return Camera; }
	UInventoryComponent* GetInventory() const { return Inventory; }
	UBodycamWeaponComponent* GetWeapon() const { return Weapon; }

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetStamina() const { return Stamina; }
	float GetMaxStamina() const { return MaxStamina; }
	bool IsAiming() const { return bIsAiming; }
	bool IsSprinting() const { return bIsSprinting; }
	bool IsDead() const { return bIsDead; }
	bool IsFlashlightOn() const { return bFlashlightOn; }
	float GetTimeSinceLastDamage() const;

	/** Le joueur peut-il tirer en ce moment ? (pas en sprint, pas mort) */
	bool CanUseWeapon() const;

	bool HasInteractionFocus() const { return FocusedActor.IsValid(); }
	FText GetFocusedInteractionText() const;

	/** Affiche un message à l'écran (via le HUD). */
	void ShowMessage(const FText& Message, float Duration = 3.f);

	/** Appelé par l'arme à chaque tir : recul, flash, secousse. */
	void OnWeaponFired(float Kick);

	/** Active / coupe les contrôles (fin de partie). */
	void SetGameplayInputEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	// ---------- Composants ----------

	/** Point d'ancrage de la caméra (poitrine). Se décale pour se pencher et s'accroupir. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<USceneComponent> CameraRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<USpotLightComponent> Flashlight;

	/** Point qui bouge avec l'arme (balancement, visée, recul). Les modèles 3D y sont accrochés. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<USceneComponent> WeaponPivot;

	/** Arme provisoire (un simple bloc), ou ton modèle statique si WeaponModel est rempli. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** Utilisé si WeaponSkeletalModel est rempli (arme animée / bras). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<USkeletalMeshComponent> WeaponSkeletalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UBodycamWeaponComponent> Weapon;

	// ---------- Arme 3D (à remplir dans BP_Player) ----------

	/** Modèle 3D statique de l'arme (ex : pistolet téléchargé sur Fab). Remplace le bloc gris. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Arme 3D")
	TObjectPtr<UStaticMesh> WeaponModel;

	/** Modèle 3D animé (Skeletal Mesh) : arme ou bras + arme. Prioritaire sur WeaponModel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Arme 3D")
	TObjectPtr<USkeletalMesh> WeaponSkeletalModel;

	/** Position / rotation / taille du modèle 3D par rapport au point de l'arme. À ajuster jusqu'à ce que ce soit joli. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Arme 3D")
	FTransform WeaponModelOffset;

	/** Position du bout du canon (flash du tir) par rapport au point de l'arme. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Arme 3D")
	FVector MuzzleOffset = FVector(16.f, 0.f, 2.f);

	// ---------- Réglages : contrôles ----------

	/** true = clavier AZERTY (ZQSD), false = QWERTY (WASD). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Controles")
	bool bAzertyLayout = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Controles")
	float LookSensitivity = 1.f;

	// ---------- Réglages : mouvement ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float WalkSpeed = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float SprintSpeed = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float CrouchSpeed = 140.f;

	/** Multiplicateur de vitesse en visant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float AimSpeedMultiplier = 0.6f;

	/** Multiplicateur de vitesse en reculant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float BackwardSpeedMultiplier = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float MaxStamina = 100.f;

	/** Endurance perdue par seconde en sprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float StaminaDrain = 16.f;

	/** Endurance récupérée par seconde. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float StaminaRegen = 11.f;

	/** Délai avant de récupérer après un effort. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float StaminaRegenDelay = 1.2f;

	/** Endurance à retrouver après avoir été à bout de souffle pour pouvoir re-sprinter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float ExhaustionRecoverThreshold = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float JumpStaminaCost = 12.f;

	/** Vitesse de chute (cm/s) à partir de laquelle on se blesse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Mouvement")
	float FallDamageSpeedThreshold = 900.f;

	// ---------- Réglages : caméra ----------

	/** Hauteur de la caméra (par rapport au centre de la capsule) debout. ~1m50 du sol. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Camera")
	float StandingCameraHeight = 58.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Camera")
	float CrouchedCameraHeight = 40.f;

	/** Champ de vision normal (grand angle façon bodycam). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Camera")
	float HipFOV = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Camera")
	float AimFOV = 78.f;

	/** Force du balancement quand on marche (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Camera")
	float BobAmplitude = 1.6f;

	/** Distance quand on se penche (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Camera")
	float LeanDistance = 34.f;

	/** Inclinaison de la caméra quand on se penche (degrés). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Camera")
	float LeanRoll = 11.f;

	// ---------- Réglages : lampe / vie ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Lampe")
	float FlashlightIntensity = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Vie")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Interaction")
	float InteractDistance = 230.f;

private:
	// ---------- Entrées (créées par le code, pas besoin d'assets) ----------
	void CreateInputActions();

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LeanLeftAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LeanRightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AimAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FlashlightAction;

	void Move(const FInputActionValue& Value);
	void MoveCompleted();
	void Look(const FInputActionValue& Value);
	void StartJump();
	void SprintPressed();
	void SprintReleased();
	void ToggleCrouch();
	void LeanLeftPressed();
	void LeanLeftReleased();
	void LeanRightPressed();
	void LeanRightReleased();
	void FirePressed();
	void AimPressed();
	void AimReleased();
	void ReloadPressed();
	void InteractPressed();
	void ToggleFlashlight();

	// ---------- Mises à jour par frame ----------
	void UpdateStamina(float DeltaSeconds);
	void UpdateMovementSpeed();
	void UpdateCamera(float DeltaSeconds);
	void UpdateWeaponPose(float DeltaSeconds);
	void UpdateFlashlight(float DeltaSeconds);
	void UpdateInteractionFocus();
	void UpdateDeathCamera(float DeltaSeconds);
	void Die();

	// ---------- État ----------
	float Health = 100.f;
	float Stamina = 100.f;
	float StaminaRegenDelayLeft = 0.f;
	bool bExhausted = false;
	bool bWantsToSprint = false;
	bool bIsSprinting = false;
	bool bIsAiming = false;
	bool bIsDead = false;
	bool bFlashlightOn = true;
	bool bLeanLeftHeld = false;
	bool bLeanRightHeld = false;
	FVector2D MoveInput = FVector2D::ZeroVector;
	float LastDamageTime = -1000.f;
	float MaxFallSpeed = 0.f;

	// Caméra procédurale
	float BobPhase = 0.f;
	float BobAmp = 0.f;
	float BreathPhase = 0.f;
	float CurrentLean = 0.f;
	float CurrentCamZ = 0.f;
	float LandDip = 0.f;
	FVector2D LookSway = FVector2D::ZeroVector;
	FRotator CameraKick = FRotator::ZeroRotator;
	float DeathAlpha = 0.f;

	// Arme
	FVector CurrentWeaponLocation = FVector::ZeroVector;
	FRotator CurrentWeaponRotation = FRotator::ZeroRotator;
	float WeaponKickBack = 0.f;
	FTimerHandle MuzzleFlashTimer;

	// Lampe
	FRotator FlashlightRotation = FRotator::ZeroRotator;
	float FlickerCooldown = 30.f;
	float FlickerTimeLeft = 0.f;

	TWeakObjectPtr<AActor> FocusedActor;
};
