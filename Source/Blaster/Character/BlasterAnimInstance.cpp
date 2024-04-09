// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/BlasterTypes/CombatState.h"

void UBlasterAnimInstance::NativeInitializeAnimation() {
	// Call the base class's initialization method
	Super::NativeInitializeAnimation();

	// Attempt to cast the owning pawn to an ABlasterCharacter
	BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaTime) {
	// Call the base class's update animation method
	Super::NativeUpdateAnimation(DeltaTime);

	// If BlasterCharacter is not set, try to set it again by casting the owning pawn
	if (BlasterCharacter == nullptr) {
		BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
	}

	// If BlasterCharacter is still null, return from this function
	if (BlasterCharacter == nullptr) {
		return;
	}

	FVector Velocity = BlasterCharacter->GetVelocity();
	Velocity.Z = 0.f;
	Speed = Velocity.Size();
	bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling();
	bIsAccelerating =
		BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false;
	bWeaponEquipped = BlasterCharacter->IsWeaponEquipped();
	EquippedWeapon = BlasterCharacter->GetEquippedWeapon();
	bIsCrouched = BlasterCharacter->bIsCrouched;
	bAiming = BlasterCharacter->IsAiming();
	TurningInPlace = BlasterCharacter->GetTurningInPlace();
	bRotateRootBone = BlasterCharacter->ShouldRotateRootBone();
	bEliminated = BlasterCharacter->IsEliminated();
	bHoldingTheFlag = BlasterCharacter->IsHoldingTheFlag();

	// Calculate Yaw offset for strafing
	FRotator AimRotation = BlasterCharacter->GetBaseAimRotation();

	FRotator MovementRotation =
		UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity());

	FRotator DeltaRot =
		UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);

	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 15.f);
	YawOffset = DeltaRotation.Yaw;

	// Calculate character lean based on rotation changes
	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = BlasterCharacter->GetActorRotation();
	const FRotator Delta =
		UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation,
			CharacterRotationLastFrame);

	const float Target = Delta.Yaw / DeltaTime;
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f);
	Lean = FMath::Clamp(Interp, -90.f, 90.f);

	AO_Yaw = BlasterCharacter->GetAO_Yaw();
	AO_Pitch = BlasterCharacter->GetAO_Pitch();

	if (bWeaponEquipped && EquippedWeapon &&
		EquippedWeapon->GetWeaponMesh() && BlasterCharacter->GetMesh()) {
		LeftHandTransform =
			EquippedWeapon->GetWeaponMesh()->
			GetSocketTransform(FName("LeftHandSocket"),
				ERelativeTransformSpace::RTS_World);

		FVector OutPosition;
		FRotator OutRotation;
		BlasterCharacter->GetMesh()->
			TransformToBoneSpace(FName("hand_r"),
				LeftHandTransform.GetLocation(),
				FRotator::ZeroRotator,
				OutPosition,
				OutRotation);

		LeftHandTransform.SetLocation(OutPosition);
		LeftHandTransform.SetRotation(FQuat(OutRotation));

		if (BlasterCharacter->IsLocallyControlled()) {
			bLocallyControlled = true;
			FTransform RightHandTransform = BlasterCharacter->GetMesh()->
				GetSocketTransform(FName("Hand_R"), ERelativeTransformSpace::RTS_World);

			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
				RightHandTransform.GetLocation(),
				RightHandTransform.GetLocation() +
				(RightHandTransform.GetLocation() - BlasterCharacter->GetHitTarget()));

			RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaTime, 30.f);
		}
		bUseFABRIK = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied;
		bool bFABRIKOverride = BlasterCharacter->IsLocallyControlled() && 
			BlasterCharacter->GetCombatState() != ECombatState::ECS_ThrowingGrenade &&
			BlasterCharacter->bFinishedSwapping;
			
		if (bFABRIKOverride) {
			bUseFABRIK = !BlasterCharacter->IsLocallyReloading();
		}
		bUseAimOffsets = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied && !BlasterCharacter->GetDisableGameplay();
		bTransformRightHand = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied && !BlasterCharacter->GetDisableGameplay();
		/*
		 // Uncomment if you want to see the line trace pointing outwards from the
		 // muzzle of the gun to whereever it's pointing and the linetrace from
		 // the muzzle of the gun to where the crosshair is pointing
		 // in worldspace.

		// Get the muzzle flash's socket transform in world space
		FTransform MuzzleTipTransform =
			EquippedWeapon->GetWeaponMesh()->
			GetSocketTransform(FName("MuzzleFlash"),
							   ERelativeTransformSpace::RTS_World);
		// Get the muzzle flash's X rotation
		FVector MuzzleX (FRotationMatrix(MuzzleTipTransform.GetRotation().Rotator()).GetUnitAxis(EAxis::X));

		// Draw a line from the muzzletip straight outwards based on its own orientation
		DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(),
					  MuzzleTipTransform.GetLocation() + MuzzleX * 1000.f,
					  FColor::Magenta);

		// Draw a line straight from the muzzletip to the hit target
		DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(),
					  BlasterCharacter->GetHitTarget(),
					  FColor::Orange);
		*/
	}
}