// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BlasterGameMode.generated.h"

namespace MatchState {
	extern BLASTER_API const FName Cooldown; // Match duration has been reached. Display winner and begin cooldown timer.
}

/**
 *
 */
UCLASS()
class BLASTER_API ABlasterGameMode : public AGameMode {
	GENERATED_BODY()

public:
	ABlasterGameMode();
	virtual void Tick(float DeltaTime) override;
	virtual void PlayerEliminated(class ABlasterCharacter* EliminatedCharacter,
		class ABlasterPlayerController* VictimController,
		ABlasterPlayerController* AttackerController);

	virtual void RequestRespawn(ACharacter* EliminatedCharacter, AController* EliminatedController);

	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 20.f;

	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;

	UPROPERTY(EditDefaultsOnly)
	float CooldownTime = 20.f;

	float LevelStartingTime = 0.f;

	FORCEINLINE float GetCountdownTime() const { return CountdownTime; }

protected:
	virtual void BeginPlay() override;
	virtual void OnMatchStateSet() override;

private:

	float CountdownTime = 0.f;
};
