#include "Player/BodycamPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"

void ABodycamPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -80.f;
		PlayerCameraManager->ViewPitchMax = 80.f;
	}

	ApplyLensDistortion(LensDistortion);
}

void ABodycamPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
	{
		ApplyLensDistortion(0.f);
	}
	Super::EndPlay(EndPlayReason);
}

void ABodycamPlayerController::ApplyLensDistortion(float Strength)
{
	if (IConsoleVariable* PaniniD = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Upscale.Panini.D")))
	{
		PaniniD->Set(Strength, ECVF_SetByCode);
	}
}
