// Fill out your copyright notice in the Description page of Project Settings.


#include "EliminatedAnnouncement.h"
#include "Components/TextBlock.h"


void UEliminatedAnnouncement::SetEliminatedAnnouncementText(FString AttackerName, 
	FString VictimName) {

	FString EliminatedAnnouncementText = FString::Printf(TEXT("%s killed %s"), *AttackerName, *VictimName);
	if (AnnouncementText) {
		AnnouncementText->SetText(FText::FromString(EliminatedAnnouncementText));
	}
}
