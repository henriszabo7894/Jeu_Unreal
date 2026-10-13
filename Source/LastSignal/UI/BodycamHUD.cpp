#include "UI/BodycamHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

#include "Game/ZoneGameMode.h"
#include "Items/InventoryComponent.h"
#include "Player/BodycamCharacter.h"
#include "Weapons/BodycamWeaponComponent.h"

namespace HUDColors
{
	const FLinearColor Text(0.86f, 0.88f, 0.84f, 0.92f);
	const FLinearColor Dim(0.6f, 0.62f, 0.6f, 0.7f);
	const FLinearColor Red(0.9f, 0.08f, 0.06f, 1.f);
	const FLinearColor Warn(0.95f, 0.65f, 0.2f, 1.f);
	const FLinearColor Good(0.55f, 0.85f, 0.55f, 1.f);
}

void ABodycamHUD::PushMessage(const FText& Message, float Duration)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	FHUDMessage Msg;
	Msg.Text = Message.ToString();
	Msg.Duration = Duration;
	Msg.ExpireTime = Now + Duration;
	Messages.Add(Msg);
	if (Messages.Num() > 4)
	{
		Messages.RemoveAt(0);
	}
}

void ABodycamHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}

	const float Scale = Canvas->ClipY / 1080.f;
	ABodycamCharacter* Character = Cast<ABodycamCharacter>(GetOwningPawn());
	AZoneGameMode* GameMode = GetWorld()->GetAuthGameMode<AZoneGameMode>();

	if (Character)
	{
		DrawDamageOverlay(Character);
	}
	DrawBodycamOverlay(Scale);

	const bool bPlaying = !GameMode || GameMode->GetZoneState() == EZoneState::Playing;
	if (bPlaying)
	{
		if (Character)
		{
			DrawStatus(Character, Scale);
			DrawInteraction(Character, Scale);
		}
		if (GameMode)
		{
			DrawObjectives(GameMode, Scale);
		}
		DrawMessages(Scale);
	}
	else
	{
		DrawEndScreen(GameMode, Scale);
	}
}

void ABodycamHUD::DrawShadowText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
{
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + 2.f * Scale, Y + 2.f * Scale, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
}

float ABodycamHUD::TextWidth(const FString& Text, UFont* Font, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	return W;
}

void ABodycamHUD::DrawBodycamOverlay(float Scale)
{
	UFont* Font = GEngine->GetSmallFont();
	const float TextScale = 1.25f * Scale;
	const float Margin = 40.f * Scale;
	const float Right = Canvas->ClipX - Margin;
	const float Now = GetWorld()->GetTimeSeconds();

	// ● REC qui clignote
	const FString Rec = TEXT("REC");
	const float RecW = TextWidth(Rec, Font, TextScale);
	if (FMath::Fmod(Now, 1.4f) < 0.9f)
	{
		const float Dot = 12.f * Scale;
		DrawRect(HUDColors::Red, Right - RecW - Dot - 10.f * Scale, Margin + 5.f * Scale, Dot, Dot);
	}
	DrawShadowText(Rec, HUDColors::Text, Right - RecW, Margin, Font, TextScale);

	// Date et heure : la nuit de l'incident
	const FDateTime Base(2031, 11, 4, 3, 17, 42);
	const FDateTime Stamp = Base + FTimespan::FromSeconds(Now);
	const FString Time = Stamp.ToString(TEXT("%Y-%m-%d  %H:%M:%S"));
	DrawShadowText(Time, HUDColors::Text, Right - TextWidth(Time, Font, TextScale), Margin + 26.f * Scale, Font, TextScale);

	DrawShadowText(UnitLabel, HUDColors::Dim, Right - TextWidth(UnitLabel, Font, TextScale), Margin + 52.f * Scale, Font, TextScale);
}

void ABodycamHUD::DrawStatus(ABodycamCharacter* Character, float Scale)
{
	UFont* Font = GEngine->GetSmallFont();
	const float TextScale = 1.3f * Scale;
	const float Margin = 40.f * Scale;
	const float Bottom = Canvas->ClipY - Margin;

	// --- Santé / endurance (en bas à gauche) ---
	const float HealthPct = Character->GetHealth() / FMath::Max(1.f, Character->GetMaxHealth());
	const FLinearColor HealthColor = HealthPct > 0.5f ? HUDColors::Text : (HealthPct > 0.25f ? HUDColors::Warn : HUDColors::Red);
	DrawShadowText(FString::Printf(TEXT("SANTÉ  %d"), FMath::RoundToInt(Character->GetHealth())), HealthColor, Margin, Bottom - 70.f * Scale, Font, TextScale);

	const float StaminaPct = Character->GetStamina() / FMath::Max(1.f, Character->GetMaxStamina());
	if (StaminaPct < 0.999f)
	{
		const float BarW = 220.f * Scale;
		const float BarH = 5.f * Scale;
		const float BarY = Bottom - 40.f * Scale;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), Margin, BarY, BarW, BarH);
		DrawRect(StaminaPct < 0.25f ? HUDColors::Warn : FLinearColor(0.8f, 0.82f, 0.78f, 0.8f), Margin, BarY, BarW * StaminaPct, BarH);
	}

	if (UInventoryComponent* Inventory = Character->GetInventory())
	{
		DrawShadowText(FString::Printf(TEXT("SAC  %.1f kg"), Inventory->GetTotalWeight()), HUDColors::Dim, Margin, Bottom - 25.f * Scale, Font, TextScale);
	}

	// --- Arme (en bas à droite) ---
	if (UBodycamWeaponComponent* Weapon = Character->GetWeapon())
	{
		const float Right = Canvas->ClipX - Margin;
		const FString Ammo = FString::Printf(TEXT("%d  |  %d"), Weapon->GetAmmoInMagazine(), Weapon->GetReserveAmmo());
		const FLinearColor AmmoColor = Weapon->GetAmmoInMagazine() == 0 ? HUDColors::Red : HUDColors::Text;
		DrawShadowText(Ammo, AmmoColor, Right - TextWidth(Ammo, Font, TextScale * 1.4f), Bottom - 60.f * Scale, Font, TextScale * 1.4f);

		FString Sub = Weapon->WeaponName.ToString();
		if (Weapon->IsReloading())
		{
			Sub = FString::Printf(TEXT("RECHARGEMENT %d%%"), FMath::RoundToInt(Weapon->GetReloadProgress() * 100.f));
		}
		DrawShadowText(Sub, HUDColors::Dim, Right - TextWidth(Sub, Font, TextScale), Bottom - 25.f * Scale, Font, TextScale);
	}
}

void ABodycamHUD::DrawInteraction(ABodycamCharacter* Character, float Scale)
{
	if (!Character->HasInteractionFocus())
	{
		return;
	}
	UFont* Font = GEngine->GetSmallFont();
	const float TextScale = 1.35f * Scale;
	const FString Prompt = FString::Printf(TEXT("[F]  %s"), *Character->GetFocusedInteractionText().ToString());
	const float W = TextWidth(Prompt, Font, TextScale);
	DrawShadowText(Prompt, HUDColors::Text, (Canvas->ClipX - W) * 0.5f, Canvas->ClipY * 0.58f, Font, TextScale);
}

void ABodycamHUD::DrawDamageOverlay(ABodycamCharacter* Character)
{
	const float HealthPct = Character->GetHealth() / FMath::Max(1.f, Character->GetMaxHealth());
	const float Flash = FMath::Clamp(1.f - Character->GetTimeSinceLastDamage() / 0.6f, 0.f, 1.f);
	const float Alpha = FMath::Clamp((1.f - HealthPct) * 0.3f + Flash * 0.35f, 0.f, 0.6f);
	if (Alpha > 0.01f)
	{
		DrawRect(FLinearColor(0.35f, 0.f, 0.f, Alpha), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
}

void ABodycamHUD::DrawObjectives(AZoneGameMode* GameMode, float Scale)
{
	const TArray<FZoneObjective>& Objectives = GameMode->GetObjectives();
	if (Objectives.Num() == 0)
	{
		return;
	}

	UFont* Font = GEngine->GetSmallFont();
	const float TextScale = 1.2f * Scale;
	const float Margin = 40.f * Scale;
	float Y = Margin;

	DrawShadowText(TEXT("OBJECTIFS"), HUDColors::Dim, Margin, Y, Font, TextScale);
	Y += 26.f * Scale;

	for (const FZoneObjective& Objective : Objectives)
	{
		const FString Line = FString::Printf(TEXT("%s  %s"), Objective.bCollected ? TEXT("[x]") : TEXT("[ ]"), *Objective.DisplayName.ToString());
		DrawShadowText(Line, Objective.bCollected ? HUDColors::Good : HUDColors::Text, Margin, Y, Font, TextScale);
		Y += 22.f * Scale;
	}

	Y += 6.f * Scale;
	const bool bAllDone = GameMode->AreAllObjectivesCollected();
	DrawShadowText(bAllDone ? TEXT("> Rejoindre l'extraction (fumigène rouge, bout de la rue)") : TEXT("> Fouiller les bâtiments"),
		bAllDone ? HUDColors::Warn : HUDColors::Dim, Margin, Y, Font, TextScale);
}

void ABodycamHUD::DrawMessages(float Scale)
{
	const float Now = GetWorld()->GetTimeSeconds();
	Messages.RemoveAll([Now](const FHUDMessage& Msg) { return Msg.ExpireTime < Now; });

	UFont* Font = GEngine->GetSmallFont();
	const float TextScale = 1.3f * Scale;
	float Y = Canvas->ClipY * 0.72f;
	for (const FHUDMessage& Msg : Messages)
	{
		const float Remaining = Msg.ExpireTime - Now;
		const float Alpha = FMath::Clamp(Remaining / 0.6f, 0.f, 1.f);
		FLinearColor Color = HUDColors::Text;
		Color.A *= Alpha;
		const float W = TextWidth(Msg.Text, Font, TextScale);
		DrawShadowText(Msg.Text, Color, (Canvas->ClipX - W) * 0.5f, Y, Font, TextScale);
		Y += 28.f * Scale;
	}
}

void ABodycamHUD::DrawEndScreen(AZoneGameMode* GameMode, float Scale)
{
	if (!GameMode)
	{
		return;
	}
	const float T = GameMode->GetTimeSinceEnd();
	const bool bWon = GameMode->GetZoneState() == EZoneState::Extracted;

	const float Fade = FMath::Clamp(T / 2.5f, 0.f, 1.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, Fade * 0.85f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();
	const FString Title = bWon ? TEXT("EXTRACTION RÉUSSIE") : TEXT("SIGNAL PERDU");
	const FString Sub = bWon
		? TEXT("Unité K-7 récupérée. Enregistrement archivé.")
		: TEXT("Télémétrie : arrêt des constantes vitales. Fin de l'enregistrement.");

	FLinearColor TitleColor = bWon ? HUDColors::Good : HUDColors::Red;
	TitleColor.A = Fade;
	FLinearColor SubColor = HUDColors::Text;
	SubColor.A = Fade;

	const float TitleScale = 2.f * Scale;
	const float SubScale = 1.3f * Scale;
	DrawShadowText(Title, TitleColor, (Canvas->ClipX - TextWidth(Title, Big, TitleScale)) * 0.5f, Canvas->ClipY * 0.42f, Big, TitleScale);
	DrawShadowText(Sub, SubColor, (Canvas->ClipX - TextWidth(Sub, Small, SubScale)) * 0.5f, Canvas->ClipY * 0.42f + 70.f * Scale, Small, SubScale);
}
