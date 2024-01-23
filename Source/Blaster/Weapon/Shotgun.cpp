// Fill out your copyright notice in the Description page of Project Settings.

#include "Shotgun.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blaster/BlasterComponents/LagCompensationComponent.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/BlasterComponents/LagCompensationComponent.h"

void AShotgun::FireShotgun(const TArray<FVector_NetQuantize>& HitTargets) {
	AWeapon::Fire(FVector());
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket) {
		const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		const FVector Start = SocketTransform.GetLocation();

		// Maps hit character to number of times hit
		TMap<ABlasterCharacter*, uint32> HitMap;
		TMap<ABlasterCharacter*, uint32> HeadShotHitMap;

		for (const FVector_NetQuantize HitTarget : HitTargets) {
			FHitResult FireHit;
			WeaponTraceHit(Start, HitTarget, FireHit);

			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
			if (BlasterCharacter) {
				const bool bHeadShot = FireHit.BoneName.ToString() == FString("head");

				if (bHeadShot) {
					if (HeadShotHitMap.Contains(BlasterCharacter)) {
						HeadShotHitMap[BlasterCharacter]++;
					} else {
						HeadShotHitMap.Emplace(BlasterCharacter, 1);
					}
				} else {
					if (HitMap.Contains(BlasterCharacter)) {
						HitMap[BlasterCharacter]++;
					} else {
						HitMap.Emplace(BlasterCharacter, 1);
					}
				}
				
				if (ImpactParticles) {
					UGameplayStatics::SpawnEmitterAtLocation(
						GetWorld(),
						ImpactParticles,
						FireHit.ImpactPoint,
						FireHit.ImpactNormal.Rotation());
				}
				if (ImpactSound) {
					UGameplayStatics::PlaySoundAtLocation(
						this,
						ImpactSound,
						FireHit.ImpactPoint,
						.5f,
						FMath::FRandRange(-.5f, .5f));
				}
			}
		}

		TArray<ABlasterCharacter*> HitCharacters;
		// Maps character hit to total damage
		TMap<ABlasterCharacter*, float> DamageMap;

		// Calculating body shot damage by multiplying times hit x Damage
		for (auto HitPair : HitMap) {
			if (HitPair.Key) {
				DamageMap.Emplace(HitPair.Key, HitPair.Value * Damage);
				HitCharacters.AddUnique(HitPair.Key);
			}
		}

		// Calculating head shot damage by multiplying times hit x HeadShotDamage - store in DamageMap
		for (auto HeadShotHitPair : HeadShotHitMap) {
			if (HeadShotHitPair.Key) {
				if (DamageMap.Contains(HeadShotHitPair.Key)) {
					DamageMap[HeadShotHitPair.Key] += HeadShotHitPair.Value * HeadShotDamage;
				} else {
					DamageMap.Emplace(HeadShotHitPair.Key, HeadShotHitPair.Value * HeadShotDamage);
				}
				HitCharacters.AddUnique(HeadShotHitPair.Key);
			}
		}

		// Loop through DamageMap to get total damage for each character
		for (auto DamagePair : DamageMap) {
			if (DamagePair.Key && InstigatorController) {
				bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
				if (HasAuthority() && bCauseAuthDamage) {
					UGameplayStatics::ApplyDamage(
						DamagePair.Key, //  Character that was hit
						DamagePair.Value, // Damage calculuted above
						InstigatorController,
						this,
						UDamageType::StaticClass());
				}
			}
		}


		if (!HasAuthority() && bUseServerSideRewind) { // Not the server therefore need to use lag compensation for fair gameplay
			BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ?
				Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;

			BlasterOwnerController = BlasterOwnerController == nullptr ?
				Cast<ABlasterPlayerController>(OwnerPawn) : BlasterOwnerController;

			if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensation() && BlasterOwnerCharacter->IsLocallyControlled()) {
				BlasterOwnerCharacter->GetLagCompensation()->ShotgunServerScoreRequest(
					HitCharacters, Start, HitTargets,
					BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime);
			}
		}
	}
}

	void AShotgun::ShotgunTraceEndWithScatter(const FVector & HitTarget, TArray<FVector_NetQuantize>&HitTargets) {
		const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
		if (MuzzleFlashSocket == nullptr) return;
		const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		const FVector TraceStart = SocketTransform.GetLocation();
		const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
		const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;

		for (uint32 i = 0; i < NumberofPellets; i++) {
			const FVector RandomVector = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
			const FVector EndLocation = SphereCenter + RandomVector;
			FVector ToEndLocation = EndLocation - TraceStart;
			ToEndLocation = TraceStart + ToEndLocation * TRACE_LENGTH / ToEndLocation.Size();
			HitTargets.Add(ToEndLocation);
		}
	}