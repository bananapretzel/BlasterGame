// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BlasterHUD.generated.h"

USTRUCT(BlueprintType)
struct FHUDPackage {
	GENERATED_BODY()

public:
	UPROPERTY()
	class UTexture2D* CrosshairsCenter;
	UTexture2D* CrosshairsLeft;
	UTexture2D* CrosshairsRight;
	UTexture2D* CrosshairsTop;
	UTexture2D* CrosshairsBottom;
	float CrosshairSpread;
	FLinearColor CrosshairsColor;
};

/**
 *
 */
UCLASS()
class BLASTER_API ABlasterHUD : public AHUD {
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	FORCEINLINE void SetHUDPackage(const FHUDPackage& Package) {
		HUDPackage = Package;
	}

	UPROPERTY(EditAnywhere, Category = "Player Stats")
	TSubclassOf<class UUserWidget> CharacterOverlayClass;

	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay;

	UPROPERTY(EditAnywhere, Category = "Announcements")
	TSubclassOf<class UUserWidget> AnnouncementOverlayClass;

	UPROPERTY()
	class UAnnouncementOverlay* AnnouncementOverlay;

	void AddCharacterOverlay();
	void AddAnnouncementOverlay();
	void AddEliminatedAnnouncement(FString Attacker, FString Victim);

protected:
	virtual void BeginPlay() override;

private:

	UPROPERTY()
	class APlayerController* OwningPlayer;

	FHUDPackage HUDPackage;

	void DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter,
		FVector2D Spread, FLinearColor CrosshairColor);

	UPROPERTY(EditAnywhere)
	float CrosshairSpreadMax = 16.f;

	UPROPERTY(EditAnywhere)
	TSubclassOf<class UEliminatedAnnouncement> EliminatedAnnouncementClass;

	UPROPERTY(EditAnywhere)
	float EliminatedAnnouncementTime = 5.f;

	UFUNCTION()
	void EliminatedAnnouncementTimerFinished(UEliminatedAnnouncement* MsgToRemove);

	UPROPERTY()
	TArray<UEliminatedAnnouncement*> EliminatedMessages;
};
