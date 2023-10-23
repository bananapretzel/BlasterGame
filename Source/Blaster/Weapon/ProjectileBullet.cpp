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
	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;
}



void AProjectileBullet::BeginPlay() {
	Super::BeginPlay();



	FPredictProjectilePathParams PathParams;
	PathParams.bTraceWithChannel = true;
	PathParams.bTraceWithCollision = true;
	PathParams.DrawDebugTime = 5.f;
	PathParams.DrawDebugType = EDrawDebugTrace::ForDuration;
	PathParams.LaunchVelocity = GetActorForwardVector() * InitialSpeed;
	PathParams.MaxSimTime = 4.f;
	PathParams.ProjectileRadius = 5.f;
	PathParams.SimFrequency = 30.f;
	PathParams.StartLocation = GetActorLocation();
	PathParams.TraceChannel = ECollisionChannel::ECC_Visibility;
	PathParams.ActorsToIgnore.Add(this);
	FPredictProjectilePathResult PathResult;
	// Predict proectile parabolic path
	UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
}


/**
* Used to auto update variables changed in blueprints
*/
#if WITH_EDITOR
void AProjectileBullet::PostEditChangeProperty(FPropertyChangedEvent& Event) {
	Super::PostEditChangeProperty(Event);

	FName PropertyName = Event.Property != nullptr ? Event.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AProjectileBullet, InitialSpeed)) {
		if (ProjectileMovementComponent) {
			ProjectileMovementComponent->InitialSpeed = InitialSpeed;
			ProjectileMovementComponent->MaxSpeed = InitialSpeed;
		}
	}

}
#endif

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