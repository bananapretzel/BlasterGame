// Fill out your copyright notice in the Description page of Project Settings.

#include "LobbyGameMode.h"
#include "GameFramework/GameStateBase.h"

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer) {
	Super::PostLogin(NewPlayer);

	int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();

	if (NumberOfPlayers == 2) {
		UWorld* World = GetWorld();
		if (World) {
			bUseSeamlessTravel = true;
			if (GEngine) {
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString(TEXT("Found 2 players, travelling to world.")));
			}
			World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));
			if (GEngine) {
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString(TEXT("ServerTravel called")));
			}
		}
	}
}