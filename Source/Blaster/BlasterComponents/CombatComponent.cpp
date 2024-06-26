// Fill out your copyright notice in the Description page of Project Settings.

#include "CombatComponent.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Sound/SoundCue.h"
#include "Blaster/Character/BlasterAnimInstance.h"
#include "Blaster/Weapon/Projectile.h"
#include "Blaster/Weapon/Shotgun.h"

UCombatComponent::UCombatComponent() {
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed = 600.f;
	AimWalkSpeed = 450.f;
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, SecondaryWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly); // Only replicate to the owning client
	DOREPLIFETIME(UCombatComponent, CombatState);
	DOREPLIFETIME(UCombatComponent, Grenades);
	DOREPLIFETIME(UCombatComponent, bHoldingTheFlag);
}

void UCombatComponent::BeginPlay() {
	Super::BeginPlay();

	if (Character) {
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
		if (Character->GetFollowCamera()) {
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}
		if (Character->HasAuthority()) {
			InitializeCarriedAmmo();
		}
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character && Character->IsLocallyControlled()) { // if autonomous proxy or locally controlled server character
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;

		SetHUDCrosshairs(DeltaTime);
		InterpFOV(DeltaTime);
	}
}

void UCombatComponent::SetHUDCrosshairs(float DeltaTime) {
	if (Character == nullptr || Character->Controller == nullptr) {
		return;
	}
	Controller = Controller == nullptr ?
		Cast<ABlasterPlayerController>(Character->Controller) : Controller;

	if (Controller) {
		HUD = HUD == nullptr ? Cast<ABlasterHUD>(Controller->GetHUD()) : HUD;
		if (HUD) {
			if (EquippedWeapon) {
				HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
				HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
				HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
				HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
				HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;
			} else {
				HUDPackage.CrosshairsCenter = nullptr;
				HUDPackage.CrosshairsLeft = nullptr;
				HUDPackage.CrosshairsRight = nullptr;
				HUDPackage.CrosshairsTop = nullptr;
				HUDPackage.CrosshairsBottom = nullptr;
			}

			// Caluclate crosshair spread when moving

			// Map the range [0, 600(MaxWalkSpeed)] to the range of [0, 1]
			FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			// Used to clamp to 0 to 1
			FVector2D VelocityMultiplierRange(0.f, 1.f);
			// Need our velocity to map to the 0, 1 range
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;
			// Where the clamping magic happens
			CrosshairVelocityFactor =
				FMath::GetMappedRangeValueClamped(WalkSpeedRange,
					VelocityMultiplierRange,
					Velocity.Size());

			// When falling the crosshair should spread more slowly and larger
			if (Character->GetCharacterMovement()->IsFalling()) {
				CrosshairInAirFactor =
					FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
			} else {
				CrosshairInAirFactor =
					FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
			}

			if (bAiming) {
				CrosshairAimFactor =
					FMath::FInterpTo(CrosshairAimFactor, 0.48f, DeltaTime, 30.f);
			} else {
				CrosshairAimFactor =
					FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
			}

			if (bEnemyInCrosshair) {
				CrosshairEnemyFactor =
					FMath::FInterpTo(CrosshairEnemyFactor, 0.1f, DeltaTime, 30.f);
			} else {
				CrosshairEnemyFactor =
					FMath::FInterpTo(CrosshairEnemyFactor, 0.f, DeltaTime, 30.f);
			}

			CrosshairShootingFactor =
				FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 40.f);

			HUDPackage.CrosshairSpread = CrosshairVelocityFactor +
				CrosshairInAirFactor -
				CrosshairAimFactor +
				CrosshairShootingFactor -
				CrosshairEnemyFactor +
				0.5f;
			//UE_LOG(LogTemp, Warning, TEXT("Crosshair Spread: %f"), HUDPackage.CrosshairSpread);
			HUD->SetHUDPackage(HUDPackage);
		}
	}
}

void UCombatComponent::InterpFOV(float DeltaTime) {
	if (EquippedWeapon == nullptr) return;

	if (bAiming) {
		CurrentFOV = FMath::FInterpTo(CurrentFOV,
			EquippedWeapon->GetZoomedFOV(),
			DeltaTime,
			EquippedWeapon->GetZoomInterpSpeed());
	} else {
		CurrentFOV = FMath::FInterpTo(CurrentFOV,
			DefaultFOV,
			DeltaTime,
			EquippedWeapon->GetZoomInterpSpeed());
	}

	if (Character && Character->GetFollowCamera()) {
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}

void UCombatComponent::SetAiming(bool bIsAiming) {
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	bAiming = bIsAiming;
	ServerSetAiming(bIsAiming);
	if (Character) {
		Character->GetCharacterMovement()->MaxWalkSpeed =
			bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
	if (Character->IsLocallyControlled() && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle) {
		Character->ShowSniperScopeWidget(bIsAiming);
	}
	if (Character->IsLocallyControlled()) {
		bAimButtonPressed = bAiming;
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming) {
	bAiming = bIsAiming;
	if (Character) {
		Character->GetCharacterMovement()->MaxWalkSpeed =
			bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::OnRep_EquippedWeapon() {
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;

	if (EquippedWeapon && Character) {
		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
		AttachActorToRightHand(EquippedWeapon);
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
		PlayEquipWeaponSound(EquippedWeapon);
		EquippedWeapon->EnableCustomDepth(false);
		EquippedWeapon->SetHUDAmmo();

		if (Controller) {
			Controller->SetHUDWeaponName(EquippedWeapon->GetWeaponType());
		}
	}
}

void UCombatComponent::OnRep_SecondaryWeapon() {
	if (SecondaryWeapon && Character) {
		SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
		AttachActorToBackpack(SecondaryWeapon);
		PlayEquipWeaponSound(EquippedWeapon);
	}
}

// Will always be called locally from a client or server
void UCombatComponent::FireButtonPressed(bool bPressed) {
	bFireButtonPressed = bPressed;

	if (bFireButtonPressed && EquippedWeapon) {
		UE_LOG(LogTemp, Warning, TEXT("fire button pressed"));
		Fire();
	}
}

void UCombatComponent::ShotgunShellReload() {
	if (Character && Character->HasAuthority()) {
		UpdateShotgunAmmoValues();
	}
}

bool UCombatComponent::CanFire() {
	if (EquippedWeapon == nullptr) {
		return false;
	}
	
	if (!EquippedWeapon->IsEmpty() &&
		bCanFire &&
		CombatState == ECombatState::ECS_Reloading &&
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun) {
		return true;
	}
	if (bLocallyReloading) return false;
	return !EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Unoccupied;
}

void UCombatComponent::Fire() {
	if (CanFire()) {
		bCanFire = false;

		if (EquippedWeapon) {
			CrosshairShootingFactor = 1.f;
			switch (EquippedWeapon->FireType) {
			case EFireType::EFT_Projectile:
				FireProjectileWeapon();
				break;
			case EFireType::EFT_HitScan:
				FireHitScanWeapon();
				break;
			case EFireType::EFT_Shotgun:
				FireShotgun();
				break;
			}
		}
		StartFireTimer();
	}
}

void UCombatComponent::FireProjectileWeapon() {
	if (EquippedWeapon && Character) {
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		if (!Character->HasAuthority()) {
			LocalFire(HitTarget);
		}
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireHitScanWeapon() {
	if (EquippedWeapon && Character) {
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		if (!Character->HasAuthority()) {
			LocalFire(HitTarget);
		}
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireShotgun() {
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	if (Shotgun && Character) {
		TArray<FVector_NetQuantize> HitTargets;
		Shotgun->ShotgunTraceEndWithScatter(HitTarget, HitTargets);
		if (!Character->HasAuthority()) {
			LocalShotgunFire(HitTargets);
		}
		ServerShotgunFire(HitTargets, EquippedWeapon->FireDelay);
	}
}

/**
* One advantage of a fire delay is so a character can't spam the fire button faster
* than the automatic fire rate.
*/
void UCombatComponent::StartFireTimer() {
	if (EquippedWeapon == nullptr || Character == nullptr) return;

	UE_LOG(LogTemp, Warning, TEXT("fire timer"));
	Character->GetWorldTimerManager().SetTimer(FireTimer,
		this,
		&UCombatComponent::FireTimerFinished,
		EquippedWeapon->FireDelay);
}

void UCombatComponent::FireTimerFinished() {
	if (EquippedWeapon == nullptr) return;

	bCanFire = true;
	UE_LOG(LogTemp, Warning, TEXT("fire timer finished"));
	if (bFireButtonPressed && EquippedWeapon->bAutomatic) {
		Fire();
	}
	ReloadEmptyWeapon();
}

// If invoked by client, this function's block of code will now run on the server.
// If server, code will also run on server.
void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget, float FireDelay) {
	MulticastFire(TraceHitTarget);
}

bool UCombatComponent::ServerFire_Validate(const FVector_NetQuantize& TraceHitTarget, float FireDelay) {
	if (EquippedWeapon) {
		bool bNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
		return bNearlyEqual;
	}
	return true;
}

// This function will always run on the server due to ServerFire being a Server keyword
void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget) {
	// If we get past this line we are either on the server,
	// or on a client who is not controlling this particular character
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	LocalFire(TraceHitTarget);
}

void UCombatComponent::ServerShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay) {
	MulticastShotgunFire(TraceHitTargets);
}

bool UCombatComponent::ServerShotgunFire_Validate(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay) {
	if (EquippedWeapon) {
		bool bNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
		return bNearlyEqual;
	}
	return true;
}

void UCombatComponent::MulticastShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets) {
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	LocalShotgunFire(TraceHitTargets);
}

void UCombatComponent::LocalFire(const FVector_NetQuantize& TraceHitTarget) {
	if (EquippedWeapon == nullptr) return;

	if (Character && CombatState == ECombatState::ECS_Unoccupied) {
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::LocalShotgunFire(const TArray<FVector_NetQuantize>& TraceHitTargets) {
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	if (Shotgun == nullptr || Character == nullptr) return;
	if (CombatState == ECombatState::ECS_Reloading || CombatState == ECombatState::ECS_Unoccupied) {
		bLocallyReloading = false;
		Character->PlayFireMontage(bAiming);
		Shotgun->FireShotgun(TraceHitTargets);
		CombatState = ECombatState::ECS_Unoccupied;
	}
}

void UCombatComponent::OnRep_CarriedAmmo() {
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	bool bJumpToShotgunEnd =
		CombatState == ECombatState::ECS_Reloading &&
		EquippedWeapon != nullptr &&
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun &&
		CarriedAmmo == 0;

	if (bJumpToShotgunEnd) {
		JumpToShotgunEnd();
	}
}

void UCombatComponent::InitializeCarriedAmmo() {
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingSniperAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
}

/**
* A Method used to trace a line exactly from the middle of the screen to a point
* in world space where that line would be projecting outwards.
*
* In other words, this method will make it so that where your crosshair is
* aiming is where to
* projectile will hit.
* @param TraceHitResult TODO
*/
void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult) {
	// Get centre of screen (crosshair)
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport) {
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	// Getting the crosshair's location which is the centre of the viewport.
	// These are coordinates in screen-space.
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition; //outparam
	FVector CrosshairWorldDirection; // outparam
	// Transforms the given 2D screen space coordinate into a 3D world-space point and direction.
	bool bScreenToWorld =
		UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0),
			CrosshairLocation,
			CrosshairWorldPosition,
			CrosshairWorldDirection);

	if (bScreenToWorld) {
		// Performing linetrace
		FVector Start = CrosshairWorldPosition;

		if (Character) {
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			Start += CrosshairWorldDirection * (DistanceToCharacter + 0.f);
			//DrawDebugSphere(GetWorld(), Start, 16.f, 12, FColor::Red, false);
		}

		// Position of start + CrosshairWorldDirection moved out by one unit * trace_length
		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;
		// Results will be put into the outparam TraceHitResult
		GetWorld()->LineTraceSingleByChannel(TraceHitResult, Start, End,
			ECollisionChannel::ECC_Visibility);
		// if didn't hit anything..

		if (!TraceHitResult.bBlockingHit) {
			// Need to set impact point to a value
			TraceHitResult.ImpactPoint = End;
			HitTarget = End;
		} else {
			HitTarget = TraceHitResult.ImpactPoint;
			//DrawDebugSphere(GetWorld(), TraceHitResult.ImpactPoint, 12.f, 12.f, FColor::Magenta);
		}
		if (TraceHitResult.GetActor() &&
			TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>()) {
			HUDPackage.CrosshairsColor = FLinearColor::Red;
			bEnemyInCrosshair = true;
			//UE_LOG(LogTemp, Warning, TEXT("Enemy in sights"));
		} else {
			HUDPackage.CrosshairsColor = FLinearColor::White;
			bEnemyInCrosshair = false;
			//UE_LOG(LogTemp, Warning, TEXT("Enemy not in sights"));
		}
	}
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip) {
	if (Character == nullptr || WeaponToEquip == nullptr) return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (WeaponToEquip->GetWeaponType() == EWeaponType::EWT_Flag) {
		Character->Crouch();
		bHoldingTheFlag = true;
		WeaponToEquip->SetWeaponState(EWeaponState::EWS_Equipped);
		AttachFlagToLeftHand(WeaponToEquip);
		WeaponToEquip->SetOwner(Character);
		TheFlag = WeaponToEquip;
	} else {
		if (EquippedWeapon != nullptr && SecondaryWeapon == nullptr) {
			EquipSecondaryWeapon(WeaponToEquip);
		} else {
			EquipPrimaryWeapon(WeaponToEquip);
		}
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
	}
}

void UCombatComponent::SwapWeapons() {
	if (CombatState != ECombatState::ECS_Unoccupied || Character == nullptr || !Character->HasAuthority()) return;

	Character->PlaySwapMontage();
	Character->bFinishedSwapping = false;
	CombatState = ECombatState::ECS_SwappingWeapons;
	
	if (SecondaryWeapon) SecondaryWeapon->EnableCustomDepth(false);
}

void UCombatComponent::EquipPrimaryWeapon(AWeapon* WeaponToEquip) {
	if (WeaponToEquip == nullptr) return;

	DropEquippedWeapon();
	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	AttachActorToRightHand(EquippedWeapon);
	EquippedWeapon->SetOwner(Character);
	EquippedWeapon->SetHUDAmmo();
	UpdateCarriedAmmo();
	PlayEquipWeaponSound(WeaponToEquip);
	ReloadEmptyWeapon();
}

void UCombatComponent::EquipSecondaryWeapon(AWeapon* WeaponToEquip) {
	if (WeaponToEquip == nullptr) return;

	SecondaryWeapon = WeaponToEquip;
	SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
	AttachActorToBackpack(WeaponToEquip);
	PlayEquipWeaponSound(WeaponToEquip);
	SecondaryWeapon->SetOwner(Character);
}

void UCombatComponent::OnRep_Aiming() {
	if (Character && Character->IsLocallyControlled()) {
		bAiming = bAimButtonPressed;
	}
}

void UCombatComponent::DropEquippedWeapon() {
	if (EquippedWeapon) {
		EquippedWeapon->Dropped();
	}
}

void UCombatComponent::AttachActorToRightHand(AActor* ActorToAttach) {
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr) {
		return;
	}
	const USkeletalMeshSocket* HandSocket =
		Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));

	if (HandSocket) {
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::AttachActorToLeftHand(AActor* ActorToAttach) {
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr || EquippedWeapon == nullptr) {
		return;
	}
	bool bUsePistolSocket = EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol ||
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SubmachineGun;
	FName SocketName = bUsePistolSocket ? FName("PistolSocket") : FName("LeftHandSocket");
	const USkeletalMeshSocket* HandSocket =
		Character->GetMesh()->GetSocketByName(SocketName);
	if (HandSocket) {
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::AttachFlagToLeftHand(AWeapon* Flag) {
	if (Character == nullptr || Character->GetMesh() == nullptr || Flag == nullptr) {
		return;
	}
	const USkeletalMeshSocket* HandSocket =
		Character->GetMesh()->GetSocketByName(FName("FlagSocket"));

	if (HandSocket) {
		HandSocket->AttachActor(Flag, Character->GetMesh());
	}
}

void UCombatComponent::AttachActorToBackpack(AActor* ActorToAttach) {
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr || EquippedWeapon == nullptr) {
		return;
	}
	const USkeletalMeshSocket* BackpackSocket =
		Character->GetMesh()->GetSocketByName(FName("BackpackSocket"));
	if (BackpackSocket) {
		BackpackSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::UpdateCarriedAmmo() {
	if (EquippedWeapon == nullptr) return;

	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDWeaponName(EquippedWeapon->GetWeaponType());
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
}

void UCombatComponent::PlayEquipWeaponSound(AWeapon* WeaponToEquip) {
	if (Character && WeaponToEquip && WeaponToEquip->EquipSound) {
		UGameplayStatics::PlaySoundAtLocation(
			this,
			WeaponToEquip->EquipSound,
			Character->GetActorLocation());
	}
}

void UCombatComponent::ReloadEmptyWeapon() {
	if (EquippedWeapon && EquippedWeapon->IsEmpty()) {
		Reload();
	}
}

void UCombatComponent::Reload() {
	if (CarriedAmmo > 0 && CombatState == ECombatState::ECS_Unoccupied && EquippedWeapon && !EquippedWeapon->IsFull() && !bLocallyReloading) {
		bool auth = Character->HasAuthority();
		ServerReload();
		HandleReload();
		bLocallyReloading = true;
	}
}

void UCombatComponent::ServerReload_Implementation() {
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	CombatState = ECombatState::ECS_Reloading;
	if (!Character->IsLocallyControlled()) {
		HandleReload();
	}
}

void UCombatComponent::HandleReload() {
	if (Character) {
		Character->PlayReloadMontage();
	}
}

int32 UCombatComponent::AmountToReload() {
	if (EquippedWeapon == nullptr) {
		return 0;
	}
	int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		int32 AmountCarried = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
		int32 Least = FMath::Min(RoomInMag, AmountCarried);
		return FMath::Clamp(RoomInMag, 0, Least);
	}
	return 0;
}

/**
* Called from pressing the grenade throw button in BlasterCharacter.
* This function will play the grenade throwing animation locally first for
* quick response then to the character's counterpart on the server so everyone
* else can see.
*/
void UCombatComponent::ThrowGrenade() {
	if (Grenades == 0) return;
	if (CombatState != ECombatState::ECS_Unoccupied || EquippedWeapon == nullptr) return;

	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (Character) {
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		ShowAttachedGrenade(true);
	}
	if (Character && !Character->HasAuthority()) {
		ServerThrowGrenade();
	}
	if (Character && Character->HasAuthority()) {
		Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
		UpdateHUDGrenades();
	}
}

void UCombatComponent::ThrowGrenadeFinished() {
	CombatState = ECombatState::ECS_Unoccupied;
	AttachActorToRightHand(EquippedWeapon);
}

void UCombatComponent::LaunchGrenade() {
	ShowAttachedGrenade(false);
	if (Character && Character->IsLocallyControlled()) {
		// Reminder: HitTarget is calculated every frame
		ServerLaunchGrenade(HitTarget);
	}
}

void UCombatComponent::OnRep_Grenades() {
	UpdateHUDGrenades();
}

void UCombatComponent::UpdateHUDGrenades() {
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDGrenades(Grenades);
	}
}

void UCombatComponent::OnRep_HoldingTheFlag() {

	if (bHoldingTheFlag && Character && Character->IsLocallyControlled()) {
		Character->Crouch();
	}
}

bool UCombatComponent::ShouldSwapWeapons() {
	return (EquippedWeapon != nullptr && SecondaryWeapon != nullptr);
}

void UCombatComponent::ServerLaunchGrenade_Implementation(const FVector_NetQuantize& Target) {
	if (Character && GrenadeClass && Character->GetAttachedGrenande()) {
		const FVector StartingLocation = Character->GetAttachedGrenande()->GetComponentLocation();
		FVector ToTarget = Target - StartingLocation;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Character;
		SpawnParams.Instigator = Character;
		UWorld* World = GetWorld();
		if (World) {
			World->SpawnActor<AProjectile>(
				GrenadeClass,
				StartingLocation,
				ToTarget.Rotation(),
				SpawnParams);
		}
	}
}

void UCombatComponent::ServerThrowGrenade_Implementation() {
	if (Grenades == 0) return;
	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (Character) {
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		ShowAttachedGrenade(true);
	}
	Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
	UpdateHUDGrenades();
}

void UCombatComponent::ShowAttachedGrenade(bool bShowGrenade) {
	if (Character && Character->GetAttachedGrenande()) {
		Character->GetAttachedGrenande()->SetVisibility(bShowGrenade);
	}
}

void UCombatComponent::PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount) {
	if (CarriedAmmoMap.Contains(WeaponType)) {
		CarriedAmmoMap[WeaponType] = FMath::Clamp(CarriedAmmoMap[WeaponType] + AmmoAmount, 0, MaxCarriedAmmo);
		UpdateCarriedAmmo();
	}
	if (EquippedWeapon && EquippedWeapon->IsEmpty() && EquippedWeapon->GetWeaponType() == WeaponType) {
		Reload();
	}
}

void UCombatComponent::FinishReloading() {
	if (Character == nullptr) return;
	bLocallyReloading = false;

	if (Character->HasAuthority()) {
		CombatState = ECombatState::ECS_Unoccupied;
		UpdateAmmoValues();
	}
	if (bFireButtonPressed) {
		Fire();
	}
}

void UCombatComponent::FinishSwap() {
	if (Character && Character->HasAuthority()) {
		CombatState = ECombatState::ECS_Unoccupied;
	}
	if (Character) {
		Character->bFinishedSwapping = true;
	}
	if (SecondaryWeapon) {
		SecondaryWeapon->EnableCustomDepth(true);
	}
}

void UCombatComponent::FinishSwapAttachWeapons() {
	AWeapon* TempWeapon = EquippedWeapon;
	EquippedWeapon = SecondaryWeapon;
	SecondaryWeapon = TempWeapon;

	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	AttachActorToRightHand(EquippedWeapon);
	EquippedWeapon->SetHUDAmmo();
	UpdateCarriedAmmo();
	PlayEquipWeaponSound(EquippedWeapon);

	SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
	AttachActorToBackpack(SecondaryWeapon);
}

void UCombatComponent::UpdateAmmoValues() {
	if (EquippedWeapon == nullptr) return;

	int32 ReloadAmount = AmountToReload();
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(ReloadAmount);
}

void UCombatComponent::UpdateShotgunAmmoValues() {
	if (Character == nullptr && EquippedWeapon == nullptr) return;

	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= 1;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(1);
	bCanFire = true;
	if (EquippedWeapon->IsFull() || CarriedAmmo == 0) {
		JumpToShotgunEnd();
	}
}

void UCombatComponent::JumpToShotgunEnd() {
	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (AnimInstance && Character->GetReloadMontage()) {
		AnimInstance->Montage_JumpToSection(FName("ShotgunEnd"));
	}
}

void UCombatComponent::OnRep_CombatState() {
	switch (CombatState) {
	case ECombatState::ECS_Reloading:
		if (Character && !Character->IsLocallyControlled()) {
			HandleReload();
		}
		break;
	case ECombatState::ECS_Unoccupied:
		if (bFireButtonPressed) {
			Fire();
		}
		break;
	case ECombatState::ECS_ThrowingGrenade:
		if (Character && !Character->IsLocallyControlled()) {
			Character->PlayThrowGrenadeMontage();
			AttachActorToLeftHand(EquippedWeapon);
			ShowAttachedGrenade(true);
		}
		break;
	case ECombatState::ECS_SwappingWeapons:
		if (Character && !Character->IsLocallyControlled()) {
			Character->PlaySwapMontage(); // Played on other clients perspectives
		}
		break;
	}
}