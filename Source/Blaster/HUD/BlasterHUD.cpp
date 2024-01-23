// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterHUD.h"
#include "GameFramework/PlayerController.h"
#include "CharacterOverlay.h"
#include "AnnouncementOverlay.h"
#include "EliminatedAnnouncement.h"
#include "Components/HorizontalBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

void ABlasterHUD::BeginPlay() {
	Super::BeginPlay();
	AddEliminatedAnnouncement("Player1", "Player2");
	
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

void ABlasterHUD::AddEliminatedAnnouncement(FString Attacker, FString Victim) {
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
	if (OwningPlayer && EliminatedAnnouncementClass) {
		UEliminatedAnnouncement* EliminatedAnnouncementWidget = CreateWidget<UEliminatedAnnouncement>(OwningPlayer, EliminatedAnnouncementClass);
		if (EliminatedAnnouncementWidget) {
			EliminatedAnnouncementWidget->SetEliminatedAnnouncementText(Attacker, Victim);
			EliminatedAnnouncementWidget->AddToViewport();

			for (UEliminatedAnnouncement* Msg : EliminatedMessages) {
				if (Msg && Msg->AnnouncementBox) {
					UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Cast<UWidget>(Msg->AnnouncementBox));
					if (CanvasSlot) {
						FVector2D Position = CanvasSlot->GetPosition();
						FVector2D NewPosition(CanvasSlot->GetPosition().X,
							Position.Y - CanvasSlot->GetSize().Y);
						CanvasSlot->SetPosition(NewPosition);
					}
				}
			}
			EliminatedMessages.Add(EliminatedAnnouncementWidget);

			FTimerHandle EliminatedMsgTimer;
			FTimerDelegate EliminatedMsgDelegate;
			EliminatedMsgDelegate.BindUFunction(this, FName("EliminatedAnnouncementTimerFinished"), EliminatedAnnouncementWidget);
			GetWorldTimerManager().SetTimer(EliminatedMsgTimer,
				EliminatedMsgDelegate, EliminatedAnnouncementTime, false);
		}
	}
}

void ABlasterHUD::EliminatedAnnouncementTimerFinished(UEliminatedAnnouncement* MsgToRemove) {
	if (MsgToRemove) {
		MsgToRemove->RemoveFromParent();
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


