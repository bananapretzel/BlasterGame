// Fill out your copyright notice in the Description page of Project Settings.

#include "MenuSettingsUI.h"

void UMenuSettingsUI::SettingsMenuSetup() {
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	/* Sets the UI element so it can respond to keyboard/mouse input */
	SetIsFocusable(true);
	UE_LOG(LogTemp, Warning, TEXT("Hello2"));
	/* Get reference to current world */
	UWorld* World = GetWorld();
	if (World) {
		/* Get reference to the first player controller, typically the main character */
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController) {
			/* Sets up an input mode which allows only the UI to respons to the user input */
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			/* Specifies that the mouse should not be locked to viewport */
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			/* Applies the input mode to the player controller, allowing the menu to capture input */
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}
}