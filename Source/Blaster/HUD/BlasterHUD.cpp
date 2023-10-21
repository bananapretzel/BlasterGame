// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterHUD.h"
#include "GameFramework/PlayerController.h"
#include "CharacterOverlay.h"
#include "AnnouncementOverlay.h"

void ABlasterHUD::BeginPlay() {
	Super::BeginPlay();
}

void ABlasterHUD::DrawHUD() {
	Super::DrawHUD();

	FVector2D ViewportSize;
	if (GEngine) {
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X / 2.f,
			ViewportSize.Y / 2.f);

		float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;

		FVector2D Spread(0.f, 0.f);
		if (HUDPackage.CrosshairsCenter) {
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		Spread.X = SpreadScaled;
		Spread.Y = 0.f;
		if (HUDPackage.CrosshairsLeft) {
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		Spread.X = -SpreadScaled;
		Spread.Y = 0.f;
		if (HUDPackage.CrosshairsRight) {
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		Spread.X = 0.f;
		Spread.Y = SpreadScaled;
		if (HUDPackage.CrosshairsTop) {
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		Spread.X = 0.f;
		Spread.Y = -SpreadScaled;
		if (HUDPackage.CrosshairsBottom) {
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
	}
}

void ABlasterHUD::AddCharacterOverlay() {
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController && CharacterOverlayClass) {
		// Create a character overlay widget
		CharacterOverlay = CreateWidget<UCharacterOverlay>(PlayerController, CharacterOverlayClass);
		// Add to viewport
		CharacterOverlay->AddToViewport();
	}
}

void ABlasterHUD::AddAnnouncementOverlay() {
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController && AnnouncementOverlayClass) {
		// Create a Announcement overlay widget
		AnnouncementOverlay = CreateWidget<UAnnouncementOverlay>(PlayerController, AnnouncementOverlayClass);
		// Add to viewport
		AnnouncementOverlay->AddToViewport();
	}
}

void ABlasterHUD::DrawCrosshair(UTexture2D* Texture,
	FVector2D ViewportCenter,
	FVector2D Spread,
	FLinearColor CrosshairColor) {
	const float TextureWidth = Texture->GetSizeX();
	const float TextureHeight = Texture->GetSizeY();
	const FVector2D TextureDrawPoint(ViewportCenter.X -
		(TextureWidth / 2.f + Spread.X),
		ViewportCenter.Y -
		(TextureHeight / 2.f + Spread.Y));

	DrawTexture(Texture, TextureDrawPoint.X, TextureDrawPoint.Y,
		TextureWidth, TextureHeight, 0.f, 0.f, 1.f, 1.f,
		CrosshairColor);
}