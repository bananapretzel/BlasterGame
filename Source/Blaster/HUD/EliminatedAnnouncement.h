// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EliminatedAnnouncement.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API UEliminatedAnnouncement : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetEliminatedAnnouncementText(FString AttackerName, FString VictimName);

	UPROPERTY(meta = (BindWidget))
	class UHorizontalBox* AnnouncementBox;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* AnnouncementText;
};
