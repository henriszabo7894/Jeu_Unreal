#include "Player/BodycamCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/ZoneGameMode.h"
#include "Interaction/Interactable.h"
#include "Items/InventoryComponent.h"
#include "UI/BodycamHUD.h"
#include "Weapons/BodycamWeaponComponent.h"

namespace BodycamPoses
{
	// Position de l'arme par rapport à la caméra (X = devant, Y = droite, Z = haut)
	const FVector HipLocation(38.f, 15.f, -21.f);
	const FRotator HipRotation(0.f, -4.f, 0.f);
	const FVector AimLocation(30.f, 0.f, -10.f);
	const FRotator AimRotation(0.f, 0.f, 0.f);
	const FVector SprintLocation(28.f, 20.f, -32.f);
	const FRotator SprintRotation(-32.f, -18.f, 12.f);
}

ABodycamCharacter::ABodycamCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);

	// Le corps tourne avec la souris (vue première personne)
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->MaxAcceleration = 1100.f;          // démarrage un peu lourd
	Move->BrakingDecelerationWalking = 1300.f;
	Move->GroundFriction = 7.f;
	Move->JumpZVelocity = 360.f;             // saut réaliste (pas de super-saut)
	Move->AirControl = 0.1f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(50.f);
	Move->bCanWalkOffLedgesWhenCrouching = true;

	// Pas de modèle de corps pour l'instant
	GetMesh()->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	CameraRoot->SetupAttachment(GetCapsuleComponent());
	CameraRoot->SetRelativeLocation(FVector(12.f, 0.f, StandingCameraHeight));
	CurrentCamZ = StandingCameraHeight;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("BodycamCamera"));
	Camera->SetupAttachment(CameraRoot);
	Camera->bUsePawnControlRotation = false; // on gère la rotation nous-mêmes (inertie, balancement)
	Camera->SetFieldOfView(HipFOV);

	// Effets d'objectif de bodycam : vignette, grain, aberration chromatique, flou de mouvement
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.85f;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 1.2f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.35f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.65f;
	PP.bOverride_MotionBlurMax = true;
	PP.MotionBlurMax = 8.f;
	PP.bOverride_LensFlareIntensity = true;
	PP.LensFlareIntensity = 0.35f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.9f;
	Camera->PostProcessBlendWeight = 1.f;

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(CameraRoot);
	Flashlight->SetRelativeLocation(FVector(4.f, -20.f, 6.f)); // fixée à l'épaule gauche (pas derrière l'arme)
	Flashlight->IntensityUnits = ELightUnits::Candelas;
	Flashlight->Intensity = FlashlightIntensity;
	Flashlight->AttenuationRadius = 3000.f;
	Flashlight->InnerConeAngle = 10.f;
	Flashlight->OuterConeAngle = 28.f;
	Flashlight->SourceRadius = 2.f;
	Flashlight->LightColor = FColor(255, 242, 222);
	Flashlight->VolumetricScatteringIntensity = 1.6f; // faisceau visible dans le brouillard
	Flashlight->SetMobility(EComponentMobility::Movable);

	WeaponPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponPivot"));
	WeaponPivot->SetupAttachment(Camera);
	WeaponPivot->SetRelativeLocation(BodycamPoses::HipLocation);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(WeaponPivot);
	if (CubeMesh.Succeeded())
	{
		WeaponMesh->SetStaticMesh(CubeMesh.Object);
	}
	WeaponMesh->SetRelativeScale3D(FVector(0.22f, 0.045f, 0.11f));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCastShadow(false);

	WeaponSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponSkeletalMesh"));
	WeaponSkeletalMesh->SetupAttachment(WeaponPivot);
	WeaponSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponSkeletalMesh->SetCastShadow(false);
	WeaponSkeletalMesh->SetVisibility(false);
	CurrentWeaponLocation = BodycamPoses::HipLocation;
	CurrentWeaponRotation = BodycamPoses::HipRotation;

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(WeaponPivot);
	MuzzleLight->SetRelativeLocation(MuzzleOffset);
	MuzzleLight->IntensityUnits = ELightUnits::Candelas;
	MuzzleLight->Intensity = 400.f;
	MuzzleLight->AttenuationRadius = 900.f;
	MuzzleLight->LightColor = FColor(255, 190, 110);
	MuzzleLight->SetVisibility(false);
	MuzzleLight->SetMobility(EComponentMobility::Movable);

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Weapon = CreateDefaultSubobject<UBodycamWeaponComponent>(TEXT("Weapon"));
}

void ABodycamCharacter::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	Stamina = MaxStamina;
	CurrentCamZ = StandingCameraHeight;
	FlashlightRotation = Camera->GetComponentRotation();
	Flashlight->SetIntensity(FlashlightIntensity);
	Flashlight->SetVisibility(bFlashlightOn);
	FlickerCooldown = FMath::FRandRange(20.f, 45.f);

	MuzzleLight->SetRelativeLocation(MuzzleOffset);

	if (WeaponSkeletalModel)
	{
		// Modèle animé fourni dans BP_Player
		WeaponSkeletalMesh->SetSkeletalMesh(WeaponSkeletalModel);
		WeaponSkeletalMesh->SetRelativeTransform(WeaponModelOffset);
		WeaponSkeletalMesh->SetVisibility(true);
		WeaponMesh->SetVisibility(false);
	}
	else if (WeaponModel)
	{
		// Modèle statique fourni dans BP_Player
		WeaponMesh->SetStaticMesh(WeaponModel);
		WeaponMesh->SetRelativeTransform(WeaponModelOffset);
	}
	// Sinon, arme provisoire : bloc gris foncé
	else if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.022f));
		WeaponMesh->SetMaterial(0, MID);
	}
}

// =====================================================================
//  Entrées
// =====================================================================

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	void AddSwizzle(UObject* Outer, FEnhancedActionKeyMapping& Mapping)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Outer);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}

	void AddNegate(UObject* Outer, FEnhancedActionKeyMapping& Mapping, bool bX, bool bY)
	{
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Outer);
		Negate->bX = bX;
		Negate->bY = bY;
		Negate->bZ = false;
		Mapping.Modifiers.Add(Negate);
	}
}

void ABodycamCharacter::CreateInputActions()
{
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Bodycam"));

	MoveAction = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(this, TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(this, TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	LeanLeftAction = MakeAction(this, TEXT("IA_LeanLeft"), EInputActionValueType::Boolean);
	LeanRightAction = MakeAction(this, TEXT("IA_LeanRight"), EInputActionValueType::Boolean);
	FireAction = MakeAction(this, TEXT("IA_Fire"), EInputActionValueType::Boolean);
	AimAction = MakeAction(this, TEXT("IA_Aim"), EInputActionValueType::Boolean);
	ReloadAction = MakeAction(this, TEXT("IA_Reload"), EInputActionValueType::Boolean);
	InteractAction = MakeAction(this, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	FlashlightAction = MakeAction(this, TEXT("IA_Flashlight"), EInputActionValueType::Boolean);

	// Déplacement : X = droite, Y = avant
	const FKey Forward = bAzertyLayout ? EKeys::Z : EKeys::W;
	const FKey Left = bAzertyLayout ? EKeys::Q : EKeys::A;
	const FKey LeanL = bAzertyLayout ? EKeys::A : EKeys::Q;

	AddSwizzle(this, MappingContext->MapKey(MoveAction, Forward));
	{
		FEnhancedActionKeyMapping& Back = MappingContext->MapKey(MoveAction, EKeys::S);
		AddSwizzle(this, Back);
		AddNegate(this, Back, true, true);
	}
	AddNegate(this, MappingContext->MapKey(MoveAction, Left), true, false);
	MappingContext->MapKey(MoveAction, EKeys::D);

	// Manette (bonus)
	MappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	// Souris : l'axe Y est inversé comme dans les templates Unreal
	AddNegate(this, MappingContext->MapKey(LookAction, EKeys::Mouse2D), false, true);

	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(SprintAction, EKeys::LeftShift);
	MappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	MappingContext->MapKey(CrouchAction, EKeys::C);
	MappingContext->MapKey(LeanLeftAction, LeanL);
	MappingContext->MapKey(LeanRightAction, EKeys::E);
	MappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(AimAction, EKeys::RightMouseButton);
	MappingContext->MapKey(ReloadAction, EKeys::R);
	MappingContext->MapKey(InteractAction, EKeys::F);
	MappingContext->MapKey(FlashlightAction, EKeys::T);
}

void ABodycamCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!MappingContext)
	{
		CreateInputActions();
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("LastSignal : Enhanced Input n'est pas actif. Vérifie Config/DefaultInput.ini"));
		return;
	}

	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABodycamCharacter::Move);
	EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ABodycamCharacter::MoveCompleted);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABodycamCharacter::Look);
	EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ABodycamCharacter::StartJump);
	EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ABodycamCharacter::SprintPressed);
	EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ABodycamCharacter::SprintReleased);
	EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABodycamCharacter::ToggleCrouch);
	EIC->BindAction(LeanLeftAction, ETriggerEvent::Started, this, &ABodycamCharacter::LeanLeftPressed);
	EIC->BindAction(LeanLeftAction, ETriggerEvent::Completed, this, &ABodycamCharacter::LeanLeftReleased);
	EIC->BindAction(LeanRightAction, ETriggerEvent::Started, this, &ABodycamCharacter::LeanRightPressed);
	EIC->BindAction(LeanRightAction, ETriggerEvent::Completed, this, &ABodycamCharacter::LeanRightReleased);
	EIC->BindAction(FireAction, ETriggerEvent::Started, this, &ABodycamCharacter::FirePressed);
	EIC->BindAction(AimAction, ETriggerEvent::Started, this, &ABodycamCharacter::AimPressed);
	EIC->BindAction(AimAction, ETriggerEvent::Completed, this, &ABodycamCharacter::AimReleased);
	EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &ABodycamCharacter::ReloadPressed);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ABodycamCharacter::InteractPressed);
	EIC->BindAction(FlashlightAction, ETriggerEvent::Started, this, &ABodycamCharacter::ToggleFlashlight);
}

void ABodycamCharacter::Move(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), MoveInput.Y);
	AddMovementInput(GetActorRightVector(), MoveInput.X);
}

void ABodycamCharacter::MoveCompleted()
{
	MoveInput = FVector2D::ZeroVector;
}

void ABodycamCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Delta = Value.Get<FVector2D>() * LookSensitivity * (bIsAiming ? 0.65f : 1.f);
	AddControllerYawInput(Delta.X);
	AddControllerPitchInput(Delta.Y);

	// Inertie : la caméra "traîne" légèrement derrière les mouvements de souris
	LookSway += Delta * 0.12f;
	LookSway.X = FMath::Clamp(LookSway.X, -3.f, 3.f);
	LookSway.Y = FMath::Clamp(LookSway.Y, -3.f, 3.f);
}

void ABodycamCharacter::StartJump()
{
	if (bIsDead || bIsCrouched || Stamina < JumpStaminaCost || !GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	Stamina -= JumpStaminaCost;
	StaminaRegenDelayLeft = StaminaRegenDelay;
	Jump();
}

void ABodycamCharacter::SprintPressed()
{
	bWantsToSprint = true;
	if (bIsCrouched)
	{
		UnCrouch();
	}
}

void ABodycamCharacter::SprintReleased()
{
	bWantsToSprint = false;
}

void ABodycamCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ABodycamCharacter::LeanLeftPressed() { bLeanLeftHeld = true; }
void ABodycamCharacter::LeanLeftReleased() { bLeanLeftHeld = false; }
void ABodycamCharacter::LeanRightPressed() { bLeanRightHeld = true; }
void ABodycamCharacter::LeanRightReleased() { bLeanRightHeld = false; }

void ABodycamCharacter::FirePressed()
{
	if (Weapon)
	{
		Weapon->TryFire();
	}
}

void ABodycamCharacter::AimPressed() { bIsAiming = true; }
void ABodycamCharacter::AimReleased() { bIsAiming = false; }

void ABodycamCharacter::ReloadPressed()
{
	if (Weapon && !bIsDead)
	{
		Weapon->StartReload();
	}
}

void ABodycamCharacter::InteractPressed()
{
	if (bIsDead)
	{
		return;
	}
	if (AActor* Target = FocusedActor.Get())
	{
		IInteractable::Execute_Interact(Target, this);
		FocusedActor = nullptr;
	}
}

void ABodycamCharacter::ToggleFlashlight()
{
	bFlashlightOn = !bFlashlightOn;
	Flashlight->SetVisibility(bFlashlightOn);
}

// =====================================================================
//  Tick
// =====================================================================

void ABodycamCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsDead)
	{
		UpdateDeathCamera(DeltaSeconds);
		return;
	}

	if (GetCharacterMovement()->IsFalling())
	{
		MaxFallSpeed = FMath::Max(MaxFallSpeed, static_cast<float>(-GetVelocity().Z));
	}

	UpdateStamina(DeltaSeconds);
	UpdateMovementSpeed();
	UpdateCamera(DeltaSeconds);
	UpdateWeaponPose(DeltaSeconds);
	UpdateFlashlight(DeltaSeconds);
	UpdateInteractionFocus();
}

void ABodycamCharacter::UpdateStamina(float DeltaSeconds)
{
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const bool bMovingForward = MoveInput.Y > 0.2f && Speed > 50.f;

	bIsSprinting = bWantsToSprint && bMovingForward && !bIsCrouched && !bIsAiming && !bExhausted
		&& Stamina > 0.f && GetCharacterMovement()->IsMovingOnGround();

	if (bIsSprinting)
	{
		Stamina = FMath::Max(0.f, Stamina - StaminaDrain * DeltaSeconds);
		StaminaRegenDelayLeft = StaminaRegenDelay;
		if (Stamina <= 0.f)
		{
			bExhausted = true;
			ShowMessage(NSLOCTEXT("LastSignal", "Exhausted", "À bout de souffle..."), 2.f);
		}
	}
	else if (StaminaRegenDelayLeft > 0.f)
	{
		StaminaRegenDelayLeft -= DeltaSeconds;
	}
	else
	{
		float Regen = StaminaRegen;
		if (Speed < 10.f) Regen *= 1.5f;   // on récupère mieux à l'arrêt
		if (bIsCrouched) Regen *= 1.2f;
		Stamina = FMath::Min(MaxStamina, Stamina + Regen * DeltaSeconds);
	}

	if (bExhausted && Stamina >= ExhaustionRecoverThreshold)
	{
		bExhausted = false;
	}
}

void ABodycamCharacter::UpdateMovementSpeed()
{
	float Multiplier = Inventory ? Inventory->GetSpeedMultiplier() : 1.f;
	if (bIsAiming) Multiplier *= AimSpeedMultiplier;
	if (bExhausted) Multiplier *= 0.85f;
	if (MoveInput.Y < -0.1f) Multiplier *= BackwardSpeedMultiplier;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = (bIsSprinting ? SprintSpeed : WalkSpeed) * Multiplier;
	Move->MaxWalkSpeedCrouched = CrouchSpeed * Multiplier;
}

void ABodycamCharacter::UpdateCamera(float DeltaSeconds)
{
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	const bool bGrounded = GetCharacterMovement()->IsMovingOnGround();

	// --- Balancement de la marche ---
	const float MoveAlpha = bGrounded ? FMath::Clamp(Speed / WalkSpeed, 0.f, 2.f) : 0.f;
	BobAmp = FMath::FInterpTo(BobAmp, BobAmplitude * MoveAlpha, DeltaSeconds, 6.f);
	const float SprintAlpha = FMath::Clamp((Speed - WalkSpeed) / FMath::Max(1.f, SprintSpeed - WalkSpeed), 0.f, 1.f);
	if (bGrounded && Speed > 5.f)
	{
		BobPhase += DeltaSeconds * FMath::Lerp(7.f, 11.f, SprintAlpha);
	}
	const float BobZ = -FMath::Abs(FMath::Sin(BobPhase)) * BobAmp * 1.3f;
	const float BobY = FMath::Sin(BobPhase) * BobAmp * 0.7f;
	const float BobRoll = FMath::Sin(BobPhase) * BobAmp * 0.55f;
	const float BobPitch = FMath::Sin(BobPhase * 2.f) * BobAmp * 0.25f;

	// --- Respiration (plus marquée quand on est fatigué) ---
	const float Fatigue = 1.f - Stamina / FMath::Max(1.f, MaxStamina);
	BreathPhase += DeltaSeconds * FMath::Lerp(1.3f, 3.4f, Fatigue);
	const float BreathAmp = FMath::Lerp(0.25f, 1.3f, Fatigue);
	const float BreathZ = FMath::Sin(BreathPhase) * BreathAmp;
	const float BreathPitch = FMath::Sin(BreathPhase) * BreathAmp * 0.3f;

	// --- Se pencher ---
	float TargetLean = 0.f;
	if (bLeanLeftHeld) TargetLean -= 1.f;
	if (bLeanRightHeld) TargetLean += 1.f;
	if (bIsSprinting) TargetLean = 0.f;
	CurrentLean = FMath::FInterpTo(CurrentLean, TargetLean, DeltaSeconds, 7.f);

	// --- Hauteur (accroupi) et réception de saut ---
	CurrentCamZ = FMath::FInterpTo(CurrentCamZ, bIsCrouched ? CrouchedCameraHeight : StandingCameraHeight, DeltaSeconds, 9.f);
	LandDip = FMath::FInterpTo(LandDip, 0.f, DeltaSeconds, 6.f);

	float LeanOffset = CurrentLean * LeanDistance;
	if (FMath::Abs(LeanOffset) > 1.f)
	{
		// On ne traverse pas les murs en se penchant
		const FVector Center = GetCapsuleComponent()->GetComponentLocation() + FVector(0.f, 0.f, CurrentCamZ);
		const FVector Target = Center + GetActorRightVector() * LeanOffset;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamLean), false, this);
		if (GetWorld()->SweepSingleByChannel(Hit, Center, Target, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(12.f), Params))
		{
			LeanOffset *= Hit.Time;
		}
	}

	// --- Inertie et secousses ---
	LookSway = FMath::Vector2DInterpTo(LookSway, FVector2D::ZeroVector, DeltaSeconds, 5.f);
	CameraKick = FMath::RInterpTo(CameraKick, FRotator::ZeroRotator, DeltaSeconds, 9.f);

	CameraRoot->SetRelativeLocation(FVector(12.f, LeanOffset, CurrentCamZ + BreathZ + LandDip));
	CameraRoot->SetWorldRotation(GetControlRotation());

	Camera->SetRelativeLocation(FVector(0.f, BobY, BobZ));
	Camera->SetRelativeRotation(FRotator(
		BobPitch + BreathPitch + LookSway.Y * 0.4f + CameraKick.Pitch,
		-LookSway.X * 0.4f + CameraKick.Yaw,
		BobRoll + CurrentLean * LeanRoll + LookSway.X * 0.3f + CameraKick.Roll));

	// --- Champ de vision ---
	const float TargetFOV = bIsAiming ? AimFOV : HipFOV + (bIsSprinting ? 4.f : 0.f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, 8.f));
}

void ABodycamCharacter::UpdateWeaponPose(float DeltaSeconds)
{
	FVector TargetLoc = BodycamPoses::HipLocation;
	FRotator TargetRot = BodycamPoses::HipRotation;
	if (bIsSprinting)
	{
		TargetLoc = BodycamPoses::SprintLocation;
		TargetRot = BodycamPoses::SprintRotation;
	}
	else if (bIsAiming)
	{
		TargetLoc = BodycamPoses::AimLocation;
		TargetRot = BodycamPoses::AimRotation;
	}
	if (Weapon && Weapon->IsReloading())
	{
		TargetLoc += FVector(-6.f, -4.f, -10.f);
		TargetRot += FRotator(-20.f, 0.f, 35.f);
	}

	// Balancement de l'arme avec la marche et la souris
	TargetLoc += FVector(0.f, LookSway.X * -0.6f, LookSway.Y * 0.6f + FMath::Sin(BobPhase * 2.f) * BobAmp * 0.4f);
	WeaponKickBack = FMath::FInterpTo(WeaponKickBack, 0.f, DeltaSeconds, 14.f);
	TargetLoc.X -= WeaponKickBack;

	CurrentWeaponLocation = FMath::VInterpTo(CurrentWeaponLocation, TargetLoc, DeltaSeconds, 12.f);
	CurrentWeaponRotation = FMath::RInterpTo(CurrentWeaponRotation, TargetRot, DeltaSeconds, 10.f);
	WeaponPivot->SetRelativeLocationAndRotation(CurrentWeaponLocation, CurrentWeaponRotation);
}

void ABodycamCharacter::UpdateFlashlight(float DeltaSeconds)
{
	// La lampe suit le regard avec un petit retard (elle est fixée au corps, pas aux yeux)
	FlashlightRotation = FMath::RInterpTo(FlashlightRotation, Camera->GetComponentRotation(), DeltaSeconds, 13.f);
	Flashlight->SetWorldRotation(FlashlightRotation);

	if (!bFlashlightOn)
	{
		return;
	}

	// Grésillement occasionnel : faux contact
	FlickerCooldown -= DeltaSeconds;
	if (FlickerCooldown <= 0.f)
	{
		FlickerTimeLeft = FMath::FRandRange(0.3f, 0.9f);
		FlickerCooldown = FMath::FRandRange(25.f, 60.f);
	}
	if (FlickerTimeLeft > 0.f)
	{
		FlickerTimeLeft -= DeltaSeconds;
		const bool bOn = FlickerTimeLeft <= 0.f || FMath::FRand() > 0.45f;
		Flashlight->SetIntensity(bOn ? FlashlightIntensity : FlashlightIntensity * 0.08f);
	}
}

void ABodycamCharacter::UpdateInteractionFocus()
{
	FocusedActor = nullptr;

	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * InteractDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamInteract), false, this);
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(10.f), Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->Implements<UInteractable>())
		{
			FocusedActor = HitActor;
		}
	}
}

void ABodycamCharacter::UpdateDeathCamera(float DeltaSeconds)
{
	// La bodycam tombe au sol avec le corps
	DeathAlpha = FMath::Min(1.f, DeathAlpha + DeltaSeconds * 1.4f);
	const float Ease = FMath::InterpEaseIn(0.f, 1.f, DeathAlpha, 2.f);
	CameraRoot->SetRelativeLocation(FVector(12.f, 0.f, FMath::Lerp(CurrentCamZ, -75.f, Ease)));
	Camera->SetRelativeRotation(FRotator(FMath::Lerp(0.f, -15.f, Ease), 0.f, FMath::Lerp(0.f, 78.f, Ease)));
}

// =====================================================================
//  Combat / dégâts
// =====================================================================

bool ABodycamCharacter::CanUseWeapon() const
{
	return !bIsDead && !bIsSprinting;
}

void ABodycamCharacter::OnWeaponFired(float Kick)
{
	// Le canon remonte, la caméra tremble
	AddControllerPitchInput(-Kick * FMath::FRandRange(0.7f, 1.f));
	AddControllerYawInput(FMath::FRandRange(-0.25f, 0.25f) * Kick);
	CameraKick += FRotator(Kick * 1.2f, FMath::FRandRange(-0.4f, 0.4f), FMath::FRandRange(-1.5f, 1.5f));
	WeaponKickBack += 5.f;

	MuzzleLight->SetVisibility(true);
	GetWorldTimerManager().SetTimer(MuzzleFlashTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		MuzzleLight->SetVisibility(false);
	}), 0.05f, false);
}

float ABodycamCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.f;
	}

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.f)
	{
		return 0.f;
	}

	Health = FMath::Max(0.f, Health - Applied);
	LastDamageTime = GetWorld()->GetTimeSeconds();
	CameraKick += FRotator(FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-6.f, 6.f));

	if (Health <= 0.f)
	{
		Die();
	}
	return Applied;
}

float ABodycamCharacter::GetTimeSinceLastDamage() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - LastDamageTime : 1000.f;
}

void ABodycamCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	const float ImpactSpeed = MaxFallSpeed;
	MaxFallSpeed = 0.f;

	LandDip = -FMath::Clamp(ImpactSpeed / 60.f, 2.f, 16.f);
	CameraKick += FRotator(-FMath::Clamp(ImpactSpeed / 200.f, 0.5f, 5.f), 0.f, FMath::FRandRange(-1.f, 1.f));

	if (ImpactSpeed > FallDamageSpeedThreshold)
	{
		const float Damage = (ImpactSpeed - FallDamageSpeedThreshold) * 0.12f;
		UGameplayStatics::ApplyDamage(this, Damage, GetController(), this, UDamageType::StaticClass());
	}
}

void ABodycamCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// La capsule descend d'un coup : on compense pour que la caméra descende en douceur
	CurrentCamZ += HalfHeightAdjust;
}

void ABodycamCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CurrentCamZ -= HalfHeightAdjust;
}

void ABodycamCharacter::Die()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;
	bIsAiming = false;
	bIsSprinting = false;
	DeathAlpha = 0.f;
	GetCharacterMovement()->DisableMovement();
	SetGameplayInputEnabled(false);

	if (AZoneGameMode* GM = GetWorld()->GetAuthGameMode<AZoneGameMode>())
	{
		GM->OnPlayerDied(this);
	}
}

// =====================================================================
//  Divers
// =====================================================================

FText ABodycamCharacter::GetFocusedInteractionText() const
{
	if (AActor* Target = FocusedActor.Get())
	{
		return IInteractable::Execute_GetInteractionText(Target);
	}
	return FText::GetEmpty();
}

void ABodycamCharacter::ShowMessage(const FText& Message, float Duration)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ABodycamHUD* HUD = Cast<ABodycamHUD>(PC->GetHUD()))
		{
			HUD->PushMessage(Message, Duration);
		}
	}
}

void ABodycamCharacter::SetGameplayInputEnabled(bool bEnabled)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	if (bEnabled)
	{
		EnableInput(PC);
	}
	else
	{
		DisableInput(PC);
		MoveInput = FVector2D::ZeroVector;
		bWantsToSprint = false;
		bLeanLeftHeld = false;
		bLeanRightHeld = false;
	}
}
