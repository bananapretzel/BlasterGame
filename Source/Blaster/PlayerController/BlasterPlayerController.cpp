// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterPlayerController.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "Blaster/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/HUD/AnnouncementOverlay.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Components/Image.h"
#include "Blaster/HUD/ReturnToMainMenu.h"
#include "EnhancedInputComponent.h"

void ABlasterPlayerController::BeginPlay() {
	Super::BeginPlay();

	BlasterHUD = Cast<ABlasterHUD>(GetHUD());
	ServerCheckMatchState();
}

void ABlasterPlayerController::OnPossess(APawn* InPawn) {
	Super::OnPossess(InPawn);
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(InPawn);
	if (BlasterCharacter) {
		SetHUDHealth(BlasterCharacter->GetHealth(), BlasterCharacter->GetMaxHealth());
		SetHUDShield(BlasterCharacter->GetShield(), BlasterCharacter->GetMaxShield());
	}
}

void ABlasterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABlasterPlayerController, MatchState);
}

void ABlasterPlayerController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	SetHUDTime();
	CheckTimeSync(DeltaTime);
	PollInit();
	CheckPing(DeltaTime);
	
}

void ABlasterPlayerController::CheckPing(float DeltaTime) {
	if (HasAuthority()) return;
	HighPingRunningTime += DeltaTime;
	if (HighPingRunningTime > CheckPingFrequency) {
 		PlayerState = GetPlayerState<APlayerState>();
		if (PlayerState) {
			
			UE_LOG(LogTemp, Warning, TEXT("PlayerState->GetPing(): %d"), PlayerState->GetCompressedPing());
			if (PlayerState->GetCompressedPing() > HighPingThreshold) {
				HighPingWarning();
				PingAnimationRunningTime = 0.f;
				ServerReportPingStatus(true);
			} else {
				ServerReportPingStatus(false);
			}
		}
		HighPingRunningTime = 0.f;
	}
	bool bHighPingAnimationPlaying =
		BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingAnimation &&
		BlasterHUD->CharacterOverlay->IsAnimationPlaying(BlasterHUD->CharacterOverlay->HighPingAnimation);

	if (bHighPingAnimationPlaying) {
		PingAnimationRunningTime += DeltaTime;
		if (PingAnimationRunningTime > HighPingDuration) {
			StopHighPingWarning();
		}
	}
}

// Is the ping too high?
void ABlasterPlayerController::ServerReportPingStatus_Implementation(bool bHighPing) {
	HighPingDelegate.Broadcast(bHighPing);
}

void ABlasterPlayerController::PollInit() {
	if (CharacterOverlay == nullptr) {
		if (BlasterHUD && BlasterHUD->CharacterOverlay) {
			CharacterOverlay = BlasterHUD->CharacterOverlay;
			if (CharacterOverlay) {
				if (bInitializeHealth) SetHUDHealth(HUDHealth, HUDMaxHealth);
				if (bInitializeShield) SetHUDShield(HUDShield, HUDMaxShield);
				if (bInitializeScore) SetHUDScore(HUDScore);
				if (bInitializeDefeats) SetHUDDefeats(HUDDefeats);
				if (bInitializeCarriedAmmo) SetHUDCarriedAmmo(HUDCarriedAmmo);
				if (bInitializeWeaponAmmo) SetHUDWeaponAmmo(HUDWeaponAmmo);

				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
				if (BlasterCharacter && BlasterCharacter->GetCombat()) {
					SetHUDGrenades(BlasterCharacter->GetCombat()->GetGrenades());
				}
			}
		}
	}
}

void ABlasterPlayerController::HighPingWarning() {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;

	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingImage &&
		BlasterHUD->CharacterOverlay->HighPingAnimation;
	if (bHUDValid) {
		BlasterHUD->CharacterOverlay->HighPingImage->SetOpacity(1.f);
		BlasterHUD->CharacterOverlay->PlayAnimation(
			BlasterHUD->CharacterOverlay->HighPingAnimation, 0.f, 5);
	}
}

void ABlasterPlayerController::StopHighPingWarning() {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;

	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingImage &&
		BlasterHUD->CharacterOverlay->HighPingAnimation;
	if (bHUDValid) {
		BlasterHUD->CharacterOverlay->HighPingImage->SetOpacity(0.f);

		if (BlasterHUD->CharacterOverlay->IsAnimationPlaying(
			BlasterHUD->CharacterOverlay->HighPingAnimation)) {
			BlasterHUD->CharacterOverlay->StopAnimation(
				BlasterHUD->CharacterOverlay->HighPingAnimation);
		}
	}
}



void ABlasterPlayerController::ServerCheckMatchState_Implementation() {
	ABlasterGameMode* GameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode) {
		WarmupTime = GameMode->WarmupTime;
		MatchTime = GameMode->MatchTime;
		CooldownTime = GameMode->CooldownTime;
		LevelStartingTime = GameMode->LevelStartingTime;
		MatchState = GameMode->GetMatchState();
		ClientJoinMidGame(MatchState, WarmupTime, MatchTime, CooldownTime, LevelStartingTime);
	}
}

void ABlasterPlayerController::ClientJoinMidGame_Implementation(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime) {
	WarmupTime = Warmup;
	MatchTime = Match;
	CooldownTime = Cooldown;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch;
	OnMatchStateSet(MatchState);
	if (BlasterHUD && MatchState == MatchState::WaitingToStart) {
		BlasterHUD->AddAnnouncementOverlay();
	}
}

// Every five seconds do a sync
void ABlasterPlayerController::CheckTimeSync(float DeltaTime) {
	TimeSyncRunningTime += DeltaTime;
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency) {
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		TimeSyncRunningTime = 0.f;
	}
}

void ABlasterPlayerController::SetHUDHealth(float Health, float MaxHealth) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;

	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HealthBar &&
		BlasterHUD->CharacterOverlay->HealthText;
	if (bHUDValid) {
		const float HealthPercent = Health / MaxHealth;
		BlasterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		BlasterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	} else {
		bInitializeHealth = true;
		HUDHealth = Health;
		HUDMaxHealth = MaxHealth;
	}
}

void ABlasterPlayerController::SetHUDShield(float Shield, float MaxShield) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;

	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HealthBar &&
		BlasterHUD->CharacterOverlay->ShieldText;
	if (bHUDValid) {
		const float ShieldPercent = Shield / MaxShield;
		BlasterHUD->CharacterOverlay->ShieldBar->SetPercent(ShieldPercent);
		FString ShieldText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Shield), FMath::CeilToInt(MaxShield));
		BlasterHUD->CharacterOverlay->ShieldText->SetText(FText::FromString(ShieldText));
	} else {
		bInitializeShield = true;
		HUDShield = Shield;
		HUDMaxShield = MaxShield;
	}
}

void ABlasterPlayerController::SetHUDScore(float Score) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->ScoreAmount;
	if (bHUDValid) {
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		BlasterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	} else {
		bInitializeScore = true;
		HUDScore = Score;
	}
}

void ABlasterPlayerController::SetHUDDefeats(int32 Defeats) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->DefeatsAmount;
	if (bHUDValid) {
		FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
		BlasterHUD->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
	} else {
		bInitializeDefeats = true;
		HUDDefeats = Defeats;
	}
}

void ABlasterPlayerController::SetHUDKilledBy(FString Killer) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	UE_LOG(LogTemp, Warning, TEXT("SetHudKilledBy"));

	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->KilledBy;
	if (bHUDValid) {
		UE_LOG(LogTemp, Warning, TEXT("bhudvalid"));
		BlasterHUD->CharacterOverlay->KillerName->SetText(FText::FromString(Killer));
		BlasterHUD->CharacterOverlay->KilledBy->SetVisibility(ESlateVisibility::Visible);
		BlasterHUD->CharacterOverlay->KillerName->SetVisibility(ESlateVisibility::Visible);
		FTimerHandle KillFeedTimer;
		GetWorldTimerManager().SetTimer(KillFeedTimer, this, &ABlasterPlayerController::KillFeedTimerFinished, 4.f);
	}
}

void ABlasterPlayerController::KillFeedTimerFinished() {
	BlasterHUD->CharacterOverlay->KillerName->SetText(FText::FromString(""));
	BlasterHUD->CharacterOverlay->KilledBy->SetVisibility(ESlateVisibility::Hidden);
	BlasterHUD->CharacterOverlay->KillerName->SetVisibility(ESlateVisibility::Hidden);
}

void ABlasterPlayerController::SetHUDWeaponAmmo(int32 Ammo) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount;
	if (bHUDValid) {
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	} else {
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
	}
}

void ABlasterPlayerController::SetHUDCarriedAmmo(int32 Ammo) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount;
	if (bHUDValid) {
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	} else {
		bInitializeCarriedAmmo = true;
		HUDCarriedAmmo = Ammo;
	}
}

void ABlasterPlayerController::SetHUDWeaponName(EWeaponType WeaponName) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->WeaponName;
	if (bHUDValid) {
		FText WeaponNameText;
		UEnum::GetDisplayValueAsText(WeaponName, WeaponNameText);
		BlasterHUD->CharacterOverlay->WeaponName->SetText(WeaponNameText);
	}
}

void ABlasterPlayerController::SetHUDMatchCountdown(float CountdownTime) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->MatchCountdownText;
	if (bHUDValid) {
		if (CountdownTime < 0.f) {
			BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60);
		int32 Seconds = CountdownTime - Minutes * 60;
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDAnnouncementCountdown(float CountdownTime) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->AnnouncementOverlay &&
		BlasterHUD->AnnouncementOverlay->WarmupTime;
	if (bHUDValid) {
		if (CountdownTime < 0.f) {
			BlasterHUD->AnnouncementOverlay->WarmupTime->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60);
		int32 Seconds = CountdownTime - Minutes * 60;
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->AnnouncementOverlay->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDGrenades(int32 Grenades) {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->GrenadesText;
	if (bHUDValid) {
		FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		BlasterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	} else {
		bInitializeGrenades = true;
		HUDGrenades = Grenades;
	}
}

void ABlasterPlayerController::SetHUDTime() {
	if (HasAuthority()) {
		BlasterGameMode = BlasterGameMode == nullptr ? Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this)) : BlasterGameMode;
		if (BlasterGameMode) {
			LevelStartingTime = BlasterGameMode->LevelStartingTime;
		}
	}

	float TimeLeft = 0.f;
	if (MatchState == MatchState::WaitingToStart) {
		TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	} else if (MatchState == MatchState::InProgress) {
		TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	} else if (MatchState == MatchState::Cooldown) {
		TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	}

	uint32 SecondsLeft = FMath::CeilToInt(MatchTime - GetServerTime());
	if (HasAuthority()) { // If we are the server, get the game mode
		BlasterGameMode = BlasterGameMode == nullptr ? Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this)) : BlasterGameMode;
		if (BlasterGameMode) {
			SecondsLeft = FMath::CeilToInt(BlasterGameMode->GetCountdownTime() + LevelStartingTime);
		}
	}
	if (CountdownInt != SecondsLeft) {
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown) {
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		if (MatchState == MatchState::InProgress) {
			SetHUDMatchCountdown(TimeLeft);
		}
	}
	CountdownInt = SecondsLeft;
}

void ABlasterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest) {
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void ABlasterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest) {
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	SingleTripTime = 0.5f * RoundTripTime;
	float CurrentServerTime = TimeServerReceivedClientRequest + SingleTripTime;
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

float ABlasterPlayerController::GetServerTime() {
	return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void ABlasterPlayerController::ReceivedPlayer() {
	Super::ReceivedPlayer();
	if (IsLocalController()) {
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

void ABlasterPlayerController::OnMatchStateSet(FName State) {
	MatchState = State;
	if (MatchState == MatchState::InProgress) {
		HandleMatchHasStarted();
	} else if (MatchState == MatchState::Cooldown) {
		HandleCooldown();
	}
}

void ABlasterPlayerController::OnRep_MatchState() {
	if (MatchState == MatchState::InProgress) {
		HandleMatchHasStarted();
	} else if (MatchState == MatchState::Cooldown) {
		HandleCooldown();
	}
}



void ABlasterPlayerController::HandleMatchHasStarted() {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	if (BlasterHUD) {
		if (BlasterHUD->CharacterOverlay == nullptr) {
			BlasterHUD->AddCharacterOverlay();
		}
		if (BlasterHUD->AnnouncementOverlay) {
			BlasterHUD->AnnouncementOverlay->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void ABlasterPlayerController::HandleCooldown() {
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	if (BlasterHUD) {
		BlasterHUD->CharacterOverlay->RemoveFromParent();
		bool bHUDValid = BlasterHUD->AnnouncementOverlay &&
			BlasterHUD->AnnouncementOverlay->AnnouncementText &&
			BlasterHUD->AnnouncementOverlay->InfoText;
		if (bHUDValid) {
			BlasterHUD->AnnouncementOverlay->SetVisibility(ESlateVisibility::Visible);
			FString AnnouncementText("NEW MATCH STARTS IN:");
			BlasterHUD->AnnouncementOverlay->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
			ABlasterPlayerState* BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
			if (BlasterGameState) {
				TArray<ABlasterPlayerState*> TopPlayers = BlasterGameState->TopScoringPlayers;
				FString InfoTextString;
				if (TopPlayers.Num() == 0) {
					InfoTextString = FString("There is no winner");
				} else if (TopPlayers.Num() == 1 && TopPlayers[0] == BlasterPlayerState) {
					InfoTextString = FString("You are the winner!");
				} else if (TopPlayers.Num() == 1) {
					InfoTextString = FString::Printf(TEXT("Winner: \n%s"), *TopPlayers[0]->GetPlayerName());
				} else if (TopPlayers.Num() > 1) {
					InfoTextString = FString("Players tied for the win:\n");
					for (ABlasterPlayerState* TiedPlayer : TopPlayers) {
						InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
					}
				}
				BlasterHUD->AnnouncementOverlay->InfoText->SetText(FText::FromString(InfoTextString));
			}
		}
	}
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterCharacter && BlasterCharacter->GetCombat()) {
		BlasterCharacter->bDisableGameplay = true;
		BlasterCharacter->GetCombat()->FireButtonPressed(false);
	}
}

void ABlasterPlayerController::SetupInputComponent() {
	Super::SetupInputComponent();
	if (InputComponent == nullptr) return;

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent)) {
		EnhancedInputComponent->BindAction(EscapeMenuAction, ETriggerEvent::Triggered, this, &ABlasterPlayerController::ShowReturnToMainMenu);
	}
}

void ABlasterPlayerController::ShowReturnToMainMenu() {
	// TODO show the ReturnToMainMenu widget
	if (ReturnToMainMenuWidget == nullptr) return;
	if (ReturnToMainMenu == nullptr) {
		ReturnToMainMenu = CreateWidget<UReturnToMainMenu>(this, ReturnToMainMenuWidget);
	}
	if (ReturnToMainMenu) {
		bReturnToMainMenuOpen = !bReturnToMainMenuOpen; // set to it's current opposite value
		if (bReturnToMainMenuOpen) {
			ReturnToMainMenu->MenuSetup();
		} else {
			ReturnToMainMenu->MenuTearDown();
		}
	}
}

