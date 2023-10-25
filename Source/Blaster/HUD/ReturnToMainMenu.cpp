// Fill out your copyright notice in the Description page of Project Settings.


#include "ReturnToMainMenu.h"
#include "GameFramework/PlayerController.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blaster/Character/BlasterCharacter.h"

void UReturnToMainMenu::MenuSetup() {
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	UWorld* World = GetWorld();
	if (World) {
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController) {
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
			PlayerController->GetPawnOrSpectator()->DisableInput(PlayerController);
			ACharacter* Character = Cast<ACharacter>(PlayerController->GetPawn());
			if (Character) {
				Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking); // Adjust this to the appropriate movement mode based on your game's settings.
				Character->GetCharacterMovement()->DisableMovement();
			}
		}
	}
	if (ReturnButton && !ReturnButton->OnClicked.IsBound()) {
		ReturnButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance) {
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		if (MultiplayerSessionsSubsystem) {
			MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &UReturnToMainMenu::OnDestroySession);
		}
	}
}

bool UReturnToMainMenu::Initialize() {
	if (!Super::Initialize()) return false;
	
	return false;
}

void UReturnToMainMenu::OnDestroySession(bool bWasSuccessful) {
	if (!bWasSuccessful) {
		ReturnButton->SetIsEnabled(true);
		return;
	}
	UWorld* World = GetWorld();
	if (World) {
		AGameModeBase* GameMode = World->GetAuthGameMode<AGameModeBase>();
		if (GameMode) { // If true then we are the server
			GameMode->ReturnToMainMenuHost();
		} else { // Is a client
			PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
			if (PlayerController) {
				PlayerController->ClientReturnToMainMenuWithTextReason(FText());
			}
		}

	}
}

void UReturnToMainMenu::MenuTearDown() {
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World) {
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController) {
			PlayerController->GetPawnOrSpectator()->DisableInput(PlayerController);
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
			PlayerController->GetPawnOrSpectator()->EnableInput(PlayerController);
			ACharacter* Character = PlayerController->GetCharacter();
			if (Character) {
				Character->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
			}
		}
	}
	if (ReturnButton && ReturnButton->OnClicked.IsBound()) {
		ReturnButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}
	if (MultiplayerSessionsSubsystem && MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.IsBound()) {
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.RemoveDynamic(this, &UReturnToMainMenu::OnDestroySession);
	}
}



void UReturnToMainMenu::ReturnButtonClicked() {
	ReturnButton->SetIsEnabled(false);
	UWorld* World = GetWorld();
	if (World) {
		APlayerController* FirstPlayerController = World->GetFirstPlayerController();
	
		if (FirstPlayerController) {
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FirstPlayerController->GetPawn());
			if (BlasterCharacter) {
				BlasterCharacter->ServerLeaveGame();
				BlasterCharacter->OnLeftGame.AddDynamic(this, &UReturnToMainMenu::OnPlayerLeftGame);
			} else { // If BlasterCharacter doesn't have a pawn, assume it is reloading after being killed and enable the button so it can be clicked again
				ReturnButton->SetIsEnabled(true);
			}
		}

	}
	
}

void UReturnToMainMenu::OnPlayerLeftGame() {
	UE_LOG(LogTemp, Warning, TEXT("OnPlayerLeftGame()"))
if (MultiplayerSessionsSubsystem) {
	UE_LOG(LogTemp, Warning, TEXT("MultiplayerSessionsSubsystem valid"))
		MultiplayerSessionsSubsystem->DestroySession();
	}
}
