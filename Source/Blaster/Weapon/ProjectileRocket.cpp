// Fill out your copyright notice in the Description page of Project Settings.

#include "ProjectileRocket.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "NiagaraSystemInstanceController.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "RocketMovementComponent.h"

AProjectileRocket::AProjectileRocket() {
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rocket Mesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RocketMovementComponent = CreateDefaultSubobject<URocketMovementComponent>(TEXT("RocketMovementComponent"));
	RocketMovementComponent->SetIsReplicated(true);
}

void AProjectileRocket::BeginPlay() {
	Super::BeginPlay();

	if (!HasAuthority()) {
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectileRocket::OnHit);
	}

	SpawnTrailSystem();

	if (ProjectileLoop && LoopingSoundAttenuation) {
		ProjectileLoopComponent = UGameplayStatics::SpawnSoundAttached(
			ProjectileLoop,						// Sound
			GetRootComponent(),					// Sound origin
			FName(),							// Optional Point name
			GetActorLocation(),					// Location
			EAttachLocation::KeepWorldPosition, // Relative or absolute position
			false,								// Stop playing when attach owner is destroyed
			1.f,								// Volume multiplier
			1.f,								// Pitch multiplier
			0.f,								// Start time
			LoopingSoundAttenuation,			// Attenuation override
			(USoundConcurrency*)nullptr,		// Concurrency override
			false);								// Auto destroy
	}
}

void AProjectileRocket::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit) {
	if (OtherActor == GetOwner()) {
		UE_LOG(LogTemp, Warning, TEXT("Hit self lmao"));
		return;
	}

	ExplodeDamage();

	//Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
	StartDestroyTimer();

	if (ImpactParticles) {
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles,
			GetActorTransform());
	}
	if (ImpactSound) {
		UGameplayStatics::PlaySoundAtLocation(this,
			ImpactSound,
			GetActorLocation());
	}
	if (ProjectileMesh) {
		ProjectileMesh->SetVisibility(false);
	}
	if (CollisionBox) {
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (TrailSystemComponent && TrailSystemComponent->GetSystemInstanceController()) {
		TrailSystemComponent->GetSystemInstanceController()->Deactivate();
	}
	if (ProjectileLoopComponent && ProjectileLoopComponent->IsPlaying()) {
		ProjectileLoopComponent->Stop();
	}
}

void AProjectileRocket::Destroyed() {
}