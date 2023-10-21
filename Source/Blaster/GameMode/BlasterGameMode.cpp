// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterGameMode.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/GameState/BlasterGameState.h"

namespace MatchState {
	const FName Cooldown = FName("Cooldown");
}

ABlasterGameMode::ABlasterGameMode() {
	bDelayedStart = true;
}

void ABlasterGameMode::BeginPlay() {
	Super::BeginPlay();

	LevelStartingTime = GetWorld()->GetTimeSeconds();
}

void ABlasterGameMode::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (MatchState == MatchState::WaitingToStart) {
		CountdownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f) {
			StartMatch();
		}
	} else if (MatchState == MatchState::InProgress) {
		CountdownTime = WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f) {
			SetMatchState(MatchState::Cooldown);
		}
	} else if (MatchState == MatchState::Cooldown) {
		CountdownTime = CooldownTime + WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f) {
			RestartGame();
		}
	}
}

void ABlasterGameMode::OnMatchStateSet() {
	Super::OnMatchStateSet();

	// Loop through all the player controllers
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
		ABlasterPlayerController* BlasterPlayer = Cast<ABlasterPlayerController>(*It); // Dereferencing It
		if (BlasterPlayer) {
			BlasterPlayer->OnMatchStateSet(MatchState);
		}
	}
}

void ABlasterGameMode::PlayerEliminated(ABlasterCharacter* EliminatedCharacter,
	ABlasterPlayerController* VictimController,
	ABlasterPlayerController* AttackerController) {
	UE_LOG(LogTemp, Warning, TEXT("PlayerEliminated()"));
	ABlasterPlayerState* AttackerPlayerState = AttackerController ? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
	ABlasterPlayerState* VictimPlayerState = VictimController ? Cast<ABlasterPlayerState>(VictimController->PlayerState) : nullptr;

	ABlasterGameState* BlasterGameState = GetGameState<ABlasterGameState>();

	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && BlasterGameState) {
		AttackerPlayerState->AddToScore(1.f);
		BlasterGameState->UpdateTopScore(AttackerPlayerState);
	}
	if (VictimPlayerState) {
		VictimPlayerState->AddToDefeats(1);
	}
	if (VictimPlayerState && AttackerPlayerState) {
		//VictimPlayerState->KilledBy(AttackerPlayerState->GetPlayerName());
		VictimPlayerState->ClientUpdateKillFeed(AttackerPlayerState->GetPlayerName());
	}
	if (EliminatedCharacter) {
		EliminatedCharacter->Eliminate();
	}
}

void ABlasterGameMode::RequestRespawn(ACharacter* EliminatedCharacter,
	AController* EliminatedController) {
	UE_LOG(LogTemp, Warning, TEXT("RequestRespawn activated"));
	if (EliminatedCharacter) {
		EliminatedCharacter->Reset();
		EliminatedCharacter->Destroy();
	}
	if (EliminatedController) {
		TArray<AActor*> SpawnPoints;
		TArray<AActor*> Players;
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), SpawnPoints);
		UGameplayStatics::GetAllActorsOfClass(this, ABlasterCharacter::StaticClass(), Players); // Gets the current alive players
		float FurthestDistance = 0.f;
		int32 FurthestSpawnPointIndex = 0;

		/*
		* For each spawn location, check the distance from it to every player.
		* Find the distance from the spawn point and nearest actor.
		* If the distance is larger than the previous most longest distance, store it.
		* Repeat for all spawn locations.
		* Spawn player at the spawn location which has the largest shortest distance.
		*/
		if (!SpawnPoints.IsEmpty() && !Players.IsEmpty()) {
			for (int i = 0; i <= SpawnPoints.Num() - 1; i++) { // For each spawn location
				FVector SpawnPointLocation = SpawnPoints[i]->GetActorLocation(); // Get the Location of the spawn in world space
				float CurrentDistance;
				UGameplayStatics::FindNearestActor(SpawnPointLocation, Players, CurrentDistance); // Find the closest actor to the spawn
				if (i == 0) { // If this is the first iteration through spawn points, set it for future reference
					FurthestDistance = CurrentDistance;
					FurthestSpawnPointIndex = i;
				} else { // Check the distance between the current spawn point and nearest actor. If the distance is larger, take note of the index.
					if (CurrentDistance > FurthestDistance) {
						FurthestDistance = CurrentDistance;
						FurthestSpawnPointIndex = i;
					}
				}
			}
		}
		RestartPlayerAtPlayerStart(EliminatedController,
			SpawnPoints[FurthestSpawnPointIndex]);
	}
}