#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BodycamHUD.generated.h"

class ABodycamCharacter;
class AZoneGameMode;
class UFont;

/**
 * Interface dessinée directement par le code (pas besoin de widgets) :
 *  - incrustation de bodycam (REC, date/heure, numéro d'unité)
 *  - santé, endurance, munitions, poids du sac
 *  - objectifs, messages, texte d'interaction
 *  - écran de fin (extraction / mort)
 * Volontairement discrète : pas de réticule, pour le réalisme.
 */
UCLASS()
class LASTSIGNAL_API ABodycamHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	void PushMessage(const FText& Message, float Duration);

	/** Identifiant affiché sur l'incrustation de la bodycam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam")
	FString UnitLabel = TEXT("UNITÉ K-7  //  SN 40-2291-B");

private:
	struct FHUDMessage
	{
		FString Text;
		float ExpireTime = 0.f;
		float Duration = 0.f;
	};

	void DrawBodycamOverlay(float Scale);
	void DrawStatus(ABodycamCharacter* Character, float Scale);
	void DrawInteraction(ABodycamCharacter* Character, float Scale);
	void DrawDamageOverlay(ABodycamCharacter* Character);
	void DrawObjectives(AZoneGameMode* GameMode, float Scale);
	void DrawMessages(float Scale);
	void DrawEndScreen(AZoneGameMode* GameMode, float Scale);

	void DrawShadowText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale);
	float TextWidth(const FString& Text, UFont* Font, float Scale);

	TArray<FHUDMessage> Messages;
};
