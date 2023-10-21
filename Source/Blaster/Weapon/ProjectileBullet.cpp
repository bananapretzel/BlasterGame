// Fill out your copyright notice in the Description page of Project Settings.

#include "ProjectileBullet.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Sound/SoundCue.h"
//#include "Net/UnrealNetwork.h"

AProjectileBullet::AProjectileBullet() {
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->SetIsReplicated(true);
}

void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit) {
	if (OtherActor == GetOwner()) {
		UE_LOG(LogTemp, Warning, TEXT("Hit self lmao"));
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	ACharacter* OtherCharacter = Cast<ACharacter>(OtherActor);
	if (OwnerCharacter) {
		AController* OwnerController = OwnerCharacter->Controller;
		if (OwnerController) {
			UGameplayStatics::ApplyDamage(OtherActor, Damage, OwnerController, this, UDamageType::StaticClass());
			if (OtherCharacter != nullptr) {
				ServerImpactEffects(true);
			} else {
				ServerImpactEffects(false);
			}
		}
	}
	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
	//Destroy();
}

void AProjectileBullet::ServerImpactEffects_Implementation(bool bHitPlayer) {
	MulticastImpactEffects(bHitPlayer);
}

void AProjectileBullet::MulticastImpactEffects_Implementation(bool bHitPlayer) {
	if (bHitPlayer) {
		UE_LOG(LogTemp, Warning, TEXT("bhitplayer"));
		if (BloodParticles) {
			UE_LOG(LogTemp, Warning, TEXT("blood"));
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodParticles,
				GetActorTransform());
		}
		if (ImpactSoundBody) {
			UE_LOG(LogTemp, Warning, TEXT("sound"));
			UGameplayStatics::PlaySoundAtLocation(this,
				ImpactSoundBody,
				GetActorLocation());
		}
	} else {
		UE_LOG(LogTemp, Warning, TEXT("bhitplayer = false"));
		if (ImpactParticles) {
			UE_LOG(LogTemp, Warning, TEXT("impact particles"));
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles,
				GetActorTransform());
		}
		if (ImpactSound) {
			UE_LOG(LogTemp, Warning, TEXT("impact noise"));
			UGameplayStatics::PlaySoundAtLocation(this,
				ImpactSound,
				GetActorLocation());
		}
	}
}