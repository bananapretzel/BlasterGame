// Fill out your copyright notice in the Description page of Project Settings.

#include "BlasterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterAnimInstance.h"
#include "Blaster/Blaster.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Blaster/BlasterComponents/BuffComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "Blaster/BlasterComponents/LagCompensationComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Blaster/GameState/BlasterGameState.h"

// Sets default values
ABlasterCharacter::ABlasterCharacter() {
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f; // The camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Combat->SetIsReplicated(true);

	BuffComponent = CreateDefaultSubobject<UBuffComponent>(TEXT("BuffComponent"));
	BuffComponent->SetIsReplicated(true);

	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));


	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 64.f;
	MinNetUpdateFrequency = 32.f;

	AttachedGrenade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttachedGrenade"));
	AttachedGrenade->SetupAttachment(GetMesh(), FName("GrenadeSocket"));
	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/**
	* Hit boxes for server-side rewind
	*/

	head = CreateDefaultSubobject<UBoxComponent>(TEXT("head"));
	head->SetupAttachment(GetMesh(), FName("head"));
	HitCollisionBoxes.Add(FName("head"), head);

	pelvis = CreateDefaultSubobject<UBoxComponent>(TEXT("pelvis"));
	pelvis->SetupAttachment(GetMesh(), FName("pelvis"));
	HitCollisionBoxes.Add(FName("pelvis"), pelvis);

	spine_02 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_02"));
	spine_02->SetupAttachment(GetMesh(), FName("spine_02"));
	HitCollisionBoxes.Add(FName("spine_02"), spine_02);

	spine_03 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_03"));
	spine_03->SetupAttachment(GetMesh(), FName("spine_03"));
	HitCollisionBoxes.Add(FName("spine_03"), spine_03);

	upperarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_l"));
	upperarm_l->SetupAttachment(GetMesh(), FName("upperarm_l"));
	HitCollisionBoxes.Add(FName("upperarm_l"), upperarm_l);

	upperarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_r"));
	upperarm_r->SetupAttachment(GetMesh(), FName("upperarm_r"));
	HitCollisionBoxes.Add(FName("upperarm_r"), upperarm_r);

	lowerarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_l"));
	lowerarm_l->SetupAttachment(GetMesh(), FName("lowerarm_l"));
	HitCollisionBoxes.Add(FName("lowerarm_l"), lowerarm_l);

	lowerarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_r"));
	lowerarm_r->SetupAttachment(GetMesh(), FName("lowerarm_r"));
	HitCollisionBoxes.Add(FName("lowerarm_r"), lowerarm_r);

	hand_l = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_l"));
	hand_l->SetupAttachment(GetMesh(), FName("hand_l"));
	HitCollisionBoxes.Add(FName("hand_l"), hand_l);

	hand_r = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_r"));
	hand_r->SetupAttachment(GetMesh(), FName("hand_r"));
	HitCollisionBoxes.Add(FName("hand_r"), hand_r);

	blanket = CreateDefaultSubobject<UBoxComponent>(TEXT("blanket"));
	blanket->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("blanket"), blanket);

	backpack = CreateDefaultSubobject<UBoxComponent>(TEXT("backpack"));
	backpack->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("backpack"), backpack);

	thigh_l = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_l"));
	thigh_l->SetupAttachment(GetMesh(), FName("thigh_l"));
	HitCollisionBoxes.Add(FName("thigh_l"), thigh_l);

	thigh_r = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_r"));
	thigh_r->SetupAttachment(GetMesh(), FName("thigh_r"));
	HitCollisionBoxes.Add(FName("thigh_r"), thigh_r);

	calf_l = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_l"));
	calf_l->SetupAttachment(GetMesh(), FName("calf_l"));
	HitCollisionBoxes.Add(FName("calf_l"), calf_l);

	calf_r = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_r"));
	calf_r->SetupAttachment(GetMesh(), FName("calf_r"));
	HitCollisionBoxes.Add(FName("calf_r"), calf_r);

	foot_l = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_l"));
	foot_l->SetupAttachment(GetMesh(), FName("foot_l"));
	HitCollisionBoxes.Add(FName("foot_l"), foot_l);

	foot_r = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_r"));
	foot_r->SetupAttachment(GetMesh(), FName("foot_r"));
	HitCollisionBoxes.Add(FName("foot_r"), foot_r);

	for (auto Box : HitCollisionBoxes) {
		if (Box.Value) {
			Box.Value->SetCollisionObjectType(ECC_HitBox);
			Box.Value->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
			Box.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			Box.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(ABlasterCharacter, Health);
	DOREPLIFETIME(ABlasterCharacter, Shield);
	DOREPLIFETIME(ABlasterCharacter, bDisableGameplay);
}

void ABlasterCharacter::MulticastGainedTheLead_Implementation() {
	if (CrownSystem == nullptr) return;
	if (CrownComponent == nullptr) {
		CrownComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			CrownSystem,
			GetCapsuleComponent(),
			FName(),
			GetActorLocation() + FVector(0.f, 0.f, 110.f),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false);
	}
	if (CrownComponent) {
		CrownComponent->Activate();
	}
}

void ABlasterCharacter::MulticastLostTheLead_Implementation() {
	if (CrownComponent) {
		CrownComponent->DestroyComponent();
	}
}

// Called when the game starts or when spawned
void ABlasterCharacter::BeginPlay() {
	// Call the base class
	Super::BeginPlay();
	SpawnDefaultWeapon();
	UpdateHUDAmmo();
	//Add Input Mapping Context
	if (const ULocalPlayer* Player = (GEngine && GetWorld()) ? GEngine->GetFirstGamePlayer(GetWorld()) : nullptr) {
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player);
		if (DefaultMappingContext) {
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	UpdateHUDHealth();
	UpdateHUDShield();

	if (HasAuthority()) {
		OnTakeAnyDamage.AddDynamic(this, &ABlasterCharacter::RecieveDamage);
	}
	if (AttachedGrenade) {
		AttachedGrenade->SetVisibility(false);
	}

	

	
}

// Called every frame
void ABlasterCharacter::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	RotateInPlace(DeltaTime);
	HideCameraIfCharacterClose();
	PollInit();
}

void ABlasterCharacter::PollInit() {
	if (BlasterPlayerState == nullptr) {
		BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
		if (BlasterPlayerState) {
			BlasterPlayerState->AddToScore(0.f);
			BlasterPlayerState->AddToDefeats(0);

			ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
			if (BlasterGameState && BlasterGameState->TopScoringPlayers.Contains(BlasterPlayerState)) {
				MulticastGainedTheLead();
			}
		}
	}
}

void ABlasterCharacter::RotateInPlace(float DeltaTime) {
	if (bDisableGameplay) {
		bUseControllerRotationYaw = false;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;

		return;
	}
	// If not a simulated proxy
	if (GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled()) {
		AimOffset(DeltaTime);
	} else {
		TimeSinceLastMovemementReplication += DeltaTime;
		if (TimeSinceLastMovemementReplication > 0.25f) {
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
}

// Called to bind functionality to input
void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ABlasterCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::Move);
		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::Look);
		// Equipping
		EnhancedInputComponent->BindAction(EquipAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::EquipButtonPressed);
		// Crouching
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABlasterCharacter::CrouchButtonPressed);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ABlasterCharacter::CrouchButtonReleased);
		// Aiming
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ABlasterCharacter::AimButtonPressed);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ABlasterCharacter::AimButtonReleased);
		// Firing
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ABlasterCharacter::FireButtonPressed);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &ABlasterCharacter::FireButtonReleased);
		// Reloading
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::ReloadButtonPressed);
		// Throwing grenade
		EnhancedInputComponent->BindAction(ThrowGrenadeAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::GrenadeButtonPressed);
		// Swap weapons
		EnhancedInputComponent->BindAction(SwapWeaponsAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::SwapWeaponsButtonPressed);
	}
}

void ABlasterCharacter::PostInitializeComponents() {
	Super::PostInitializeComponents();
	if (Combat) {
		Combat->Character = this;
	}
	if (BuffComponent) {
		BuffComponent->Character = this;
		BuffComponent->SetInitialSpeeds(
			GetCharacterMovement()->MaxWalkSpeed,
			GetCharacterMovement()->MaxWalkSpeedCrouched);
		BuffComponent->SetInitialJumpVelocity(GetCharacterMovement()->JumpZVelocity);
	}
	if (LagCompensation) {
		LagCompensation->Character = this;
		if (Controller) {
			LagCompensation->Controller = Cast<ABlasterPlayerController>(Controller);
		}
	}
}

void ABlasterCharacter::OnRep_ReplicatedMovement() {
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	TimeSinceLastMovemementReplication = 0.f;
}

void ABlasterCharacter::Move(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	//UE_LOG(LogTemp, Warning, TEXT("moving"));
	// Input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr) {
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		// Get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		// Get right vector
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// Add movement
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ABlasterCharacter::Look(const FInputActionValue& Value) {
	// Input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr) {
		// Add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon) {
	if (OverlappingWeapon) {
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon) {
		LastWeapon->ShowPickupWidget(false);
	}
}

void ABlasterCharacter::EquipButtonPressed(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Equipbuttonpressed function activated"));

	if (Combat) {
		if (Combat->CombatState == ECombatState::ECS_Unoccupied) {
			ServerEquipButtonPressed();
		}
		
	}
}

void ABlasterCharacter::ReloadButtonPressed(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	if (Combat) {
		Combat->Reload();
	}
}

void ABlasterCharacter::GrenadeButtonPressed(const FInputActionValue& Value) {
	UE_LOG(LogTemp, Warning, TEXT("GrenadeButtonPressed function activated"));
	if (Combat) {
		Combat->ThrowGrenade();
	}
}

void ABlasterCharacter::SwapWeaponsButtonPressed(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Equipbuttonpressed function activated"));

	if (Combat && Combat->ShouldSwapWeapons()) {
		ServerSwapWeaponsButtonPressed();
	}
	bool bSwap = Combat->ShouldSwapWeapons() &&
		!HasAuthority() &&
		Combat->CombatState == ECombatState::ECS_Unoccupied &&
		OverlappingWeapon == nullptr;
	if (bSwap) {
		PlaySwapMontage();
		Combat->CombatState = ECombatState::ECS_SwappingWeapons;
		bFinishedSwapping = false;
	}
}

void ABlasterCharacter::CrouchButtonPressed(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("CrouchButtonPressed function activated"));
	Crouch();
}

void ABlasterCharacter::CrouchButtonReleased(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("CrouchButtonReleased function activated"));
	UnCrouch();
}

void ABlasterCharacter::AimButtonPressed(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("AimButtonPressed function activated"));
	if (Combat) {
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("AimButtonReleased function activated"));
	if (Combat) {
		Combat->SetAiming(false);
	}
}
void ABlasterCharacter::FireButtonPressed(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("FireButtonPressed function activated"));
	if (Combat) {
		Combat->FireButtonPressed(true);
	}
}
void ABlasterCharacter::FireButtonReleased(const FInputActionValue& Value) {
	if (bDisableGameplay) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("FireButtonReleased function activated"));
	if (Combat) {
		Combat->FireButtonPressed(false);
	}
}

float ABlasterCharacter::CalculateSpeed() {
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::OnRep_Health(float LastHealth) {
	UpdateHUDHealth();
	if (Health < LastHealth) {
		PlayHitReactMontage();
	}
}

void ABlasterCharacter::OnRep_Shield(float LastShield) {
	UpdateHUDShield();
	if (Shield < LastShield) {
		PlayHitReactMontage();
	}
}

void ABlasterCharacter::UpdateHUDHealth() {
	BlasterPlayerController = BlasterPlayerController == nullptr ?
		Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;

	if (BlasterPlayerController) {
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void ABlasterCharacter::UpdateHUDShield() {
	BlasterPlayerController = BlasterPlayerController == nullptr ?
		Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;

	if (BlasterPlayerController) {
		BlasterPlayerController->SetHUDShield(Shield, MaxShield);
	}
}

void ABlasterCharacter::UpdateHUDAmmo() {
	BlasterPlayerController = BlasterPlayerController == nullptr ?
		Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;

	if (BlasterPlayerController && Combat && Combat->EquippedWeapon) {
		BlasterPlayerController->SetHUDCarriedAmmo(Combat->CarriedAmmo);
		BlasterPlayerController->SetHUDWeaponAmmo(Combat->EquippedWeapon->GetAmmo());
	}
}

void ABlasterCharacter::SpawnDefaultWeapon() {
	// Check if we're in a blaster game mode so we only spawn the weapon in this mode
	ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	UWorld* World = GetWorld();
	if (BlasterGameMode && World && !bEliminated && DefaultWeaponClass) {
		AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
		StartingWeapon->bDestroyWeapon = true;
		if (Combat) {
			Combat->EquipWeapon(StartingWeapon);
		}
	}
}



/**
* Calculating the offset in degrees between the zero pose and the current aim
*
**/
void ABlasterCharacter::AimOffset(float DeltaTime) {
	if (Combat && Combat->EquippedWeapon == nullptr) {
		return;
	}
	float Speed = CalculateSpeed();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir) { // Standing still, not jumping
		bRotateRootBone = true;
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); // Get only the current yaw
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning) {
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
	}
	if (Speed > 0.f || bIsInAir) { // Running or jumping
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
	CalculateAO_Pitch();

	//if (!HasAuthority() && IsLocallyControlled()) { // if is not server and is itself (client) and not another client
	//	UE_LOG(LogTemp, Warning, TEXT("AO_Pitch: %f"), AO_Pitch);
	//}
}

void ABlasterCharacter::CalculateAO_Pitch() {
	AO_Pitch = GetBaseAimRotation().Pitch;

	// A fix for pitch in multiplayer clients since unreal compresses the pitch value and the correct pitch angle is lost due to this
	if (AO_Pitch > 90.f && !IsLocallyControlled()) {
		// map pitch from [270, 360] to [-90, 0]
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
		// Another solution
		// AO_Pitch = GetBaseAimRotation().GetNormalized().Pitch;
	}
}

void ABlasterCharacter::SimProxiesTurn() {
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) {
		return;
	}
	bRotateRootBone = false;
	float Speed = CalculateSpeed();
	if (Speed > 0.f) {
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation,
		ProxyRotationLastFrame).Yaw;
	//UE_LOG(LogTemp, Warning, TEXT("ProxyYaw: %f"), ProxyYaw);
	if (FMath::Abs(ProxyYaw) > TurnThreshold) {
		if (ProxyYaw > TurnThreshold) {
			TurningInPlace = ETurningInPlace::ETIP_Right;
		} else if (ProxyYaw < -TurnThreshold) {
			TurningInPlace = ETurningInPlace::ETIP_Left;
		} else {
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void ABlasterCharacter::Jump() {
	if (bDisableGameplay) {
		return;
	}
	if (bIsCrouched) {
		UnCrouch();
	} else {
		Super::Jump();
	}
}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation() {
	if (Combat) {
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

void ABlasterCharacter::ServerSwapWeaponsButtonPressed_Implementation() {
	if (Combat) {
		Combat->SwapWeapons();
	}
}

/**
* Functon used so the character will turn his body to face the direction the crosshair is pointing towards after a specific threshold is reached.
* For example, after turning the camera more than 90 degrees to the left or right.
*/
void ABlasterCharacter::TurnInPlace(float DeltaTime) {
	//UE_LOG(LogTemp, Warning, TEXT("AO_Yaw: %f"), AO_Yaw);
	if (AO_Yaw > 67.5f) {
		TurningInPlace = ETurningInPlace::ETIP_Right;
		//UE_LOG(LogTemp, Warning, TEXT("TurningInPlace: Right"));
	} else if (AO_Yaw < -67.5f) {
		TurningInPlace = ETurningInPlace::ETIP_Left;
		//UE_LOG(LogTemp, Warning, TEXT("TurningInPlace: Left"));
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning) {
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f) {
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void ABlasterCharacter::HideCameraIfCharacterClose() {
	if (!IsLocallyControlled()) {
		return;
	}
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold) {
		GetMesh()->SetVisibility(false);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh()) {
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	} else {
		GetMesh()->SetVisibility(true);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh()) {
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon) {
	if (IsLocallyControlled()) {
		if (OverlappingWeapon) {
			OverlappingWeapon->ShowPickupWidget(false);
		}
	}
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled()) {
		if (OverlappingWeapon) {
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

bool ABlasterCharacter::IsWeaponEquipped() {
	return (Combat && Combat->EquippedWeapon);
}

bool ABlasterCharacter::IsAiming() {
	return (Combat && Combat->bAiming);
}

void ABlasterCharacter::Eliminate(bool bPlayerLeftGame) {
	//UE_LOG(LogTemp, Warning, TEXT("Eliminate()"));
	DropOrDestroyWeapons();
	MulticastEliminate(bPlayerLeftGame);
	
}

void ABlasterCharacter::EliminateTimerFinished() {
	ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
	if (BlasterGameMode && !bLeftGame) {
		BlasterGameMode->RequestRespawn(this, Controller);
	}
	if (bLeftGame && IsLocallyControlled()) {
		OnLeftGame.Broadcast();
	}
}

void ABlasterCharacter::ServerLeaveGame_Implementation() {
	ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
	BlasterPlayerState = BlasterPlayerState == nullptr ? GetPlayerState<ABlasterPlayerState>() : BlasterPlayerState;
	if (BlasterGameMode && BlasterPlayerState) {
		BlasterGameMode->PlayerLeftGame(BlasterPlayerState);
	}
}

void ABlasterCharacter::MulticastEliminate_Implementation(bool bPlayerLeftGame) {
	bLeftGame = bPlayerLeftGame;
	if (BlasterPlayerController) {
		BlasterPlayerController->SetHUDWeaponAmmo(0);
	}
	bEliminated = true;
	PlayEliminateMontage();

	// Starting dissolve effect
	if (DissolveMaterialInstance) {
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	StartDissolve();

	if (Combat) {
		Combat->FireButtonPressed(false);
	}

	// Disable character movement
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();

	bDisableGameplay = true;

	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Spawn elim bot
	if (ElimBotEffect) {
		FVector ElimBotSpawnPoint(GetActorLocation().X,
			GetActorLocation().Y,
			GetActorLocation().Z + 200.f);

		ElimBotComponent =
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),
				ElimBotEffect,
				ElimBotSpawnPoint,
				GetActorRotation());
	}
	if (ElimBotSound) {
		UGameplayStatics::SpawnSoundAtLocation(this,
			ElimBotSound,
			GetActorLocation());
	}

	bool bHideSniperScope = IsLocallyControlled() && Combat && Combat->bAiming &&
		Combat->EquippedWeapon &&
		Combat->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle;

	if (bHideSniperScope) {
		ShowSniperScopeWidget(false);
	}
	if (CrownComponent) {
		CrownComponent->DestroyComponent();
	}
	GetWorldTimerManager().SetTimer(EliminateTimer,
		this,
		&ABlasterCharacter::EliminateTimerFinished,
		EliminateDelay);
}

void ABlasterCharacter::DropOrDestroyWeapons() {
	if (Combat) {
		if (Combat->EquippedWeapon) {
			DropOrDestroyWeapon(Combat->EquippedWeapon);
		}
		if (Combat->SecondaryWeapon) {
			DropOrDestroyWeapon(Combat->SecondaryWeapon);
		}
	}
}

void ABlasterCharacter::DropOrDestroyWeapon(AWeapon* Weapon) {
	if (Weapon == nullptr) return;

	if (Weapon->bDestroyWeapon) {
		Weapon->Destroy();
	} else {
		Weapon->Dropped();
	}
}

void ABlasterCharacter::Destroyed() {
	Super::Destroyed();
	if (ElimBotComponent) {
		ElimBotComponent->DestroyComponent();
	}
	ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	bool bMatchNotInProgress = BlasterGameMode && BlasterGameMode->GetMatchState() != MatchState::InProgress;

	if (Combat && Combat->EquippedWeapon && bMatchNotInProgress) {
		Combat->EquippedWeapon->Destroy();
	}
}

void ABlasterCharacter::UpdateDissolveMaterial(float DissolveValue) {
	if (DynamicDissolveMaterialInstance) {
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void ABlasterCharacter::StartDissolve() {
	// Dynamic delegate. Can bind because of this
	DissolveTrack.BindDynamic(this, &ABlasterCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline) {
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}
}

AWeapon* ABlasterCharacter::GetEquippedWeapon() {
	if (Combat == nullptr) {
		return nullptr;
	}
	return Combat->EquippedWeapon;
}



void ABlasterCharacter::PlayFireMontage(bool bAiming) {
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("PlayFireMontage function returned false!"));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage) {
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
		//UE_LOG(LogTemp, Warning, TEXT("fireMontage"));
	}
}

void ABlasterCharacter::PlayReloadMontage() {
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("PlayReloadMontage function returned false!"));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ReloadMontage) {
		AnimInstance->Montage_Play(ReloadMontage);
		FName SectionName;
		switch (Combat->EquippedWeapon->GetWeaponType()) {
		case EWeaponType::EWT_AssaultRifle:
			SectionName = FName("Rifle");
			break;
		case EWeaponType::EWT_RocketLauncher:
			SectionName = FName("RocketLauncher");
			break;
		case EWeaponType::EWT_Pistol:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_SubmachineGun:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_Shotgun:
			SectionName = FName("Shotgun");
			break;
		case EWeaponType::EWT_SniperRifle:
			SectionName = FName("SniperRifle");
			break;
		case EWeaponType::EWT_GrenadeLauncher:
			SectionName = FName("GrenadeLauncher");
			break;
		}
		AnimInstance->Montage_JumpToSection(SectionName);

		/* Fix for when a character is hit in the middle of the reload montage so it actually finishes the montage and doesn't get stuck in reload*/
		/*
		AnimInstance->Montage_JumpToSection(SectionName, ReloadMontage);
		FOnMontageEnded MontageCompleted;
		MontageCompleted.BindWeakLambda(this, [this](UAnimMontage* AnimMontage, bool bInterrupted) {
			if (bInterrupted) {
				UE_LOG(LogTemp, Warning, TEXT("We were interrupted"));
				if (HasAuthority()) {
					//This is replicated.
					Combat->CombatState = ECombatState::ECS_Unoccupied;
				}
			}
			else {
				//completed
				UE_LOG(LogTemp, Warning, TEXT("We completed"));
				Combat->FinishReloading();
			}
			});
		AnimInstance->Montage_SetEndDelegate(MontageCompleted, ReloadMontage);
		//UE_LOG(LogTemp, Warning, TEXT("ReloadMontage"));
		*/
	}
}

void ABlasterCharacter::PlayEliminateMontage() {
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && EliminateMontage) {
		AnimInstance->Montage_Play(EliminateMontage);
		//UE_LOG(LogTemp, Warning, TEXT("EliminateMontage"));
	}
}

void ABlasterCharacter::PlayThrowGrenadeMontage() {
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ThrowGrenadeMontage) {
		AnimInstance->Montage_Play(ThrowGrenadeMontage);
	}
}

void ABlasterCharacter::PlaySwapMontage() {
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && SwapMontage) {
		AnimInstance->Montage_Play(SwapMontage);
	}

}

void ABlasterCharacter::PlayHitReactMontage() {
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("PlayHitReactMontage function returned false!"));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage) {
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
		UE_LOG(LogTemp, Warning, TEXT("hitMontage"));
	}
}

void ABlasterCharacter::RecieveDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatorController,
	AActor* DamageCauser) {
	if (bEliminated) return;

	float DamageToHealth = Damage;
	if (Shield > 0.f) {
		if (Shield >= Damage) {
			Shield = FMath::Clamp(Shield - Damage, 0.f, MaxShield);
			DamageToHealth = 0.f;
		} else {
			DamageToHealth = FMath::Clamp(DamageToHealth - Shield, 0.f, Damage);
			Shield = 0.f;
		}
	}
	Health = FMath::Clamp(Health - DamageToHealth, 0.f, MaxHealth);
	UpdateHUDHealth();
	UpdateHUDShield();
	PlayHitReactMontage();

	if (Health == 0.f) {
		UE_LOG(LogTemp, Warning, TEXT("Someone's health is 0!"));
		ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
		if (BlasterGameMode) {
			BlasterPlayerController = BlasterPlayerController == nullptr ?
				Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;

			ABlasterPlayerController* AttackerController =
				Cast<ABlasterPlayerController>(InstigatorController);

			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerController);
		}
	}
}

FVector ABlasterCharacter::GetHitTarget() const {
	if (Combat == nullptr) {
		return FVector();
	}
	return Combat->HitTarget;
}

ECombatState ABlasterCharacter::GetCombatState() const {
	if (Combat == nullptr) {
		return ECombatState::ECS_MAX;
	}
	return Combat->CombatState;
}

void ABlasterCharacter::UpdateHUDWeaponType() {
	BlasterPlayerController = BlasterPlayerController == nullptr ?
		Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController == nullptr) {
		return;
	}
	BlasterPlayerController->SetHUDWeaponName(GetEquippedWeapon()->GetWeaponType());
}

bool ABlasterCharacter::IsLocallyReloading() {
	if (Combat == nullptr) return false;
	return Combat->bLocallyReloading;
}