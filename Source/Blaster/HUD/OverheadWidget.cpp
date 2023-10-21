// Fill out your copyright notice in the Description page of Project Settings.

#include "OverheadWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"

void UOverheadWidget::SetDisplayText(FString TextToDisplay) {
	if (DisplayText) {
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn) {
	//ENetRole RemoteRole = InPawn->GetRemoteRole();
	ENetRole LocalRole = InPawn->GetLocalRole();
	FString Role;

	switch (LocalRole) {
	case ENetRole::ROLE_Authority:
		Role = FString("Authority");
		break;
	case ENetRole::ROLE_AutonomousProxy:
		Role = FString("Autonomous Proxy");
		break;
	case ENetRole::ROLE_SimulatedProxy:
		Role = FString("Simulated Proxy");
		break;
	case ENetRole::ROLE_None:
		Role = FString("None");
		break;
	}
	FString LocalRoleString = FString::Printf(TEXT("Local Role: %s"), *Role);
	SetDisplayText(LocalRoleString);
}

void UOverheadWidget::ShowPlayerName(APawn* InPawn) {
	if (APlayerState* PlayerState = InPawn->GetPlayerState()) {
		FString Name = PlayerState->GetPlayerName();
		if (!Name.IsEmpty()) {
			SetDisplayText(Name);
		}
	}
}

void UOverheadWidget::OnLevelRemovedFromWorld(ULevel* Inlevel, UWorld* InWorld) {
	RemoveFromParent();
	Super::NativeDestruct();
}