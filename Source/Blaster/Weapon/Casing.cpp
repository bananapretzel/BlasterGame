// Fill out your copyright notice in the Description page of Project Settings.

#include "Casing.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
ACasing::ACasing() {
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	CasingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CasingMesh"));
	SetRootComponent(CasingMesh);
	CasingMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera,
		ECollisionResponse::ECR_Ignore);
	CasingMesh->SetSimulatePhysics(true);
	CasingMesh->SetEnableGravity(true);
	CasingMesh->SetNotifyRigidBodyCollision(true);
	ShellEjectionImpulse = 10.f;
}

// Called when the game starts or when spawned
void ACasing::BeginPlay() {
	Super::BeginPlay();
	CasingMesh->OnComponentHit.AddDynamic(this, &ACasing::OnHit);
	FVector RandomCasingImpulse = GetActorForwardVector();
	RandomCasingImpulse.Z += FMath::RandRange(-0.3f, 0.3f);
	//UKismetMathLibrary::RandomUnitVectorInConeInDegrees(GetActorForwardVector(), 20.f);
	CasingMesh->AddImpulse(RandomCasingImpulse * ShellEjectionImpulse);
}

void ACasing::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse,
	const FHitResult& Hit) {
	if (ShellSound) {
		UGameplayStatics::PlaySoundAtLocation(this,
			ShellSound,
			GetActorLocation());
		CasingMesh->SetNotifyRigidBodyCollision(false);
	}
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ACasing::DestroyCasing,
		3.f, false);
}

void ACasing::DestroyCasing() {
	Destroy();
}