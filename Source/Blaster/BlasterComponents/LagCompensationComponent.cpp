// Fill out your copyright notice in the Description page of Project Settings.

#include "LagCompensationComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/Weapon/Weapon.h"

ULagCompensationComponent::ULagCompensationComponent() {
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

void ULagCompensationComponent::BeginPlay() {
	Super::BeginPlay();
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SaveFramePackage();
}

void ULagCompensationComponent::SaveFramePackage() {
	if (Character == nullptr || !Character->HasAuthority()) return;

	// If Frame history is empty, Save a frame and make it the head
	if (FrameHistory.Num() <= 1) {
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);
	} else {
		// Latest frame time minus the oldest frame time
		float HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		// While the history is greater than the maximum record time, remove the tail(s)
		while (HistoryLength > MaxRecordTime) {
			FrameHistory.RemoveNode(FrameHistory.GetTail());
			HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		}
		// Add the current frame to the history
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);

		// Show full history of hitboxes in game
		// ShowFramePackage(ThisFrame, FColor::Red);
	}
}

/**
* This function will be called every frame. It will take an empty FFramePackage
* as its input and fill the package with the this owning character's hitbox
* location, rotation, and extent in world space.
*
* @param Package An empty package for storing hotbox data
*/
void ULagCompensationComponent::SaveFramePackage(FFramePackage& Package) {
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : Character;
	if (Character) {
		Package.Time = GetWorld()->GetTimeSeconds();
		Package.Character = Character;
		for (auto& BoxPair : Character->HitCollisionBoxes) {
			FBoxInformation BoxInformation;
			BoxInformation.Location = BoxPair.Value->GetComponentLocation();
			BoxInformation.Rotation = BoxPair.Value->GetComponentRotation();
			BoxInformation.BoxExtent = BoxPair.Value->GetScaledBoxExtent();
			Package.HitBoxInfo.Add(BoxPair.Key, BoxInformation);
		}
	}
}

/**
* When a HitTime is found to be within the range of the oldest frame history
* and youngest frame history, which was judged in the ServerSideRewind function,
* An Interpolation is done between younger and older to determine where the
* hitboxes were at HitTime
*
* @param  OlderFrame The frame that was found to be adjacent to HiTime on
*		  the older side of the frame history
* @param  YoungerFrame The frame that was found to be adjacent to HitTime on
*		  the younger side of the frame history
* @param  HitTime The local time of when a player shot their bullet
* @return A FramePackage with hitboxes interpolated between younger and older
*		  frame at the point of HitTime
*/
FFramePackage ULagCompensationComponent::InterpBetweenFrames(
	FFramePackage& OlderFrame, FFramePackage& YoungerFrame, float HitTime) {
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	const float InterpFraction = FMath::Clamp((HitTime - OlderFrame.Time) / Distance, 0.f, 1.f);

	FFramePackage InterpFramePackage;
	InterpFramePackage.Time = HitTime;

	// Loop through hit boxes
	for (auto& YoungerPair : YoungerFrame.HitBoxInfo) {
		const FName& BoxInfoName = YoungerPair.Key;

		const FBoxInformation& OlderBox = OlderFrame.HitBoxInfo[BoxInfoName];
		const FBoxInformation& YoungerBox = YoungerFrame.HitBoxInfo[BoxInfoName];

		FBoxInformation InterpBoxInfo;
		InterpBoxInfo.Location = FMath::VInterpTo(OlderBox.Location, YoungerBox.Location, 1.f, InterpFraction);
		InterpBoxInfo.Rotation = FMath::RInterpTo(OlderBox.Rotation, YoungerBox.Rotation, 1.f, InterpFraction);
		InterpBoxInfo.BoxExtent = YoungerBox.BoxExtent;

		InterpFramePackage.HitBoxInfo.Add(BoxInfoName, InterpBoxInfo);
	}

	return InterpFramePackage;
}
/**
* The definitive function to check if the character was hit or not with lag
* compensation in mind. This function take a note (cache) of where the current
* HitCharacter is on the server and then moves all of its hitboxes to the
* position of where it was at HitTime (which likely has been interpolated).
* This is done so a line trace can be used to determine a hit or not.
*
* @param  Package A frame, likely interpolated, which denotes where the
		  HitCharacter's position was in the past when the instigator fired
		  their bullet.
* @param  HitCharacter The Character which is being checked to see whether they
*		  got hit or not.
* @param  TraceStart The starting vector to be used for the line trace
* @param  HitLocation The location in world space where the instigator hit the
*		  character on their client.
* @return The result of the line trace and whether there was a hit or not and
		  whether it was a headshot or not
*/
FServerSideRewindResult ULagCompensationComponent::ConfirmHit(
	const FFramePackage& Package,
	ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation) {
	if (HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, Package);
	// Disabling collision for HitCharacter's mesh so the line trace wouldn't interfere with it
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	// Enable collision for the head first
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	// Get the head hitbox
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// Turn on its collision for line trace
	HeadBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	FHitResult ConfirmHitResult;

	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	UWorld* World = GetWorld();
	if (World) {
		World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart,
			TraceEnd, ECollisionChannel::ECC_Visibility);
		if (ConfirmHitResult.bBlockingHit) { // Headshot confirmed
			ResetHitBoxes(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
			return FServerSideRewindResult{ true, true };
		} else { // No headshot so checking the other hitboxes
			for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
				if (HitBoxPair.Value != nullptr) { // Turning on collision for hit boxes
					HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					HitBoxPair.Value->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
				}
			}
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart,
				TraceEnd, ECollisionChannel::ECC_Visibility);
			if (ConfirmHitResult.bBlockingHit) {
				ResetHitBoxes(HitCharacter, CurrentFrame);
				EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
				return FServerSideRewindResult{ true, false };
			}
		}
	}
	// Past this comment, no confirmed hit detected
	ResetHitBoxes(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{ false, false };
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunConfirmHit(
	const TArray<FFramePackage>& FramePackages,
	const FVector_NetQuantize& TraceStart,
	const TArray<FVector_NetQuantize>& HitLocations) {
	for (auto& Frame : FramePackages) {
		if (Frame.Character == nullptr) return FShotgunServerSideRewindResult();
	}

	FShotgunServerSideRewindResult ShotgunResult;
	TArray<FFramePackage> CurrentFrames;
	UWorld* World = GetWorld();

	for (const FFramePackage& Frame : FramePackages) {
		FFramePackage CurrentFrame;
		CurrentFrame.Character = Frame.Character;
		CacheBoxPositions(Frame.Character, CurrentFrame);
		MoveBoxes(Frame.Character, Frame);
		// Disabling collision for Frame.Character's mesh so the line trace wouldn't interfere with it
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::NoCollision);
		CurrentFrames.Add(CurrentFrame);
	}

	for (const FFramePackage& Frame : FramePackages) {
		// Enable collision for the head first
		UBoxComponent* HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		// Get the head hitbox
		HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		// Turn on its collision for line trace
		HeadBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	}

	// Check for headshots
	for (const FVector_NetQuantize& HitLocation : HitLocations) {
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;

		if (World) {
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart,
				TraceEnd, ECollisionChannel::ECC_Visibility);

			ABlasterCharacter* BlasterCharacter =
				Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());

			if (BlasterCharacter) {
				if (ShotgunResult.HeadShots.Contains(BlasterCharacter)) {
					ShotgunResult.HeadShots[BlasterCharacter]++;
				} else {
					ShotgunResult.HeadShots.Emplace(BlasterCharacter, 1);
				}
			}
		}
		// Enable collision for all boxes, then disable for headbox
		for (const FFramePackage& Frame : FramePackages) {
			for (auto& HitBoxPair : Frame.Character->HitCollisionBoxes) {
				if (HitBoxPair.Value != nullptr) { // Turning on collision for hit boxes
					HitBoxPair.Value->SetCollisionEnabled(
						ECollisionEnabled::QueryAndPhysics);
					HitBoxPair.Value->SetCollisionResponseToChannel(
						ECollisionChannel::ECC_Visibility,
						ECollisionResponse::ECR_Block);
				}
			}
		}
	}

	// Check for bodyshots
	for (const FVector_NetQuantize& HitLocation : HitLocations) {
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;

		if (World) {
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart,
				TraceEnd, ECollisionChannel::ECC_Visibility);

			ABlasterCharacter* BlasterCharacter =
				Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());

			if (BlasterCharacter) {
				if (ShotgunResult.BodyShots.Contains(BlasterCharacter)) {
					ShotgunResult.BodyShots[BlasterCharacter]++;
				} else {
					ShotgunResult.BodyShots.Emplace(BlasterCharacter, 1);
				}
			}
		}
	}

	for (const FFramePackage& Frame : CurrentFrames) {
		ResetHitBoxes(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character,
			ECollisionEnabled::QueryAndPhysics);
	}
	return ShotgunResult;
}

/**
* Since the server will be checking the HitCharacters location in the past, it
* will move its hit boxes to that position but it will also need to to cache its
* current position so it can switch back after checking if a hit was confirmed
* or not.
*
* @param HitCharacter The character which will have its hitboxes cached
* @param OutFramePackage The out-parameter for which hitbox data will be stored
*/
void ULagCompensationComponent::CacheBoxPositions(
	ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage) {
	if (HitCharacter == nullptr) return;

	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
		if (HitBoxPair.Value != nullptr) {
			FBoxInformation BoxInfo;
			BoxInfo.Location = HitBoxPair.Value->GetComponentLocation();
			BoxInfo.Rotation = HitBoxPair.Value->GetComponentRotation();
			BoxInfo.BoxExtent = HitBoxPair.Value->GetScaledBoxExtent();
			OutFramePackage.HitBoxInfo.Add(HitBoxPair.Key, BoxInfo);
		}
	}
}

/**
* A function closely relevant to CacheBoxPositions. This function will take a
* character and shift its hitboxes to the position from the given frame package.
*
* @param HitCharacter The character to have their hitboxes shifted
* @param Package A package with hitbox location data to shift the HitCharacter's
*		 hitboxes to
*/
void ULagCompensationComponent::MoveBoxes(ABlasterCharacter* HitCharacter,
	const FFramePackage& Package) {
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
		if (HitBoxPair.Value != nullptr) {
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
		}
	}
}
/**
* This function should be used to shift the character's hitboxes back to their
* original current position after shifting them prior to check for a lag
* compensated hit. This is different than MoveBoxes because collision shall be
* re-enabled in this process.
*
* @param HitCharacter The character to have their hitboxes shifted
* @param Package A package with hitbox location data to shift the HitCharacter's
*		 hitboxes to
*/
void ULagCompensationComponent::ResetHitBoxes(ABlasterCharacter* HitCharacter,
	const FFramePackage& Package) {
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
		if (HitBoxPair.Value != nullptr) {
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
			HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}
/**
* A function to enable to disable a character's mesh collision. This could be
* used so the mesh would not interfere with a hitbox linetrace.
*
* @param HitCharacter A character to enable/disable collision. Typically this
*		 should be the character that will be getting checked for a line trace.
*
* @param CollisionEnabled Enable or disable collision for HitCharacter.
*/
void ULagCompensationComponent::EnableCharacterMeshCollision(
	ABlasterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled) {
	if (HitCharacter && HitCharacter->GetMesh()) {
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
	}
}

/**
* Debug function which will show all the frame history of the character's
* hitboxes in game.
*
* @param Package A frame package for which its stored frame data will be
*		 displayed.
* @param Color Hitbox colour
*/
void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package,
	const FColor& Color) {
	for (auto& BoxInfo : Package.HitBoxInfo) {
		DrawDebugBox(
			GetWorld(),
			BoxInfo.Value.Location,
			BoxInfo.Value.BoxExtent,
			FQuat(BoxInfo.Value.Rotation),
			Color,
			false,
			MaxRecordTime);
	}
}
/**
* TODO Update comments
*
* The primary function of this class. This function is called when a server-side
* rewind will take place. It will Check where the HitTime takes place in the
* frame history and if it the HitTime is beyond the oldest frame, it will return
* an empty result. Otherwise if the HitTime is determined to be within the range
* of the frame history, the history will be iterated until it finds the two
* adject frames between HitTime and an interpolation will take place to
* determine if the victim character was hit with lag compensation enabled.
*
* @param  HitCharacter The character for which to check if they were hit or not
* @param  TraceStart The start of the line trace from where the instigator shot
* @param  HitLocation Location in world space where there was a hit on the client
* @param  HitTime The time at which a client hit a target on their end
* @return A Struct which confirms if a hit was made with lag compensation in
*		  mind using this function and whether it was a headshot or not
*/
FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(
	ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation, float HitTime) {
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunServerSideRewind(
	const TArray<ABlasterCharacter*>& HitCharacters,
	const FVector_NetQuantize& TraceStart,
	const TArray<FVector_NetQuantize>& HitLocations, float HitTime) {
	TArray<FFramePackage> FramesToCheck;
	for (ABlasterCharacter* HitCharacter : HitCharacters) {
		FramesToCheck.Add(GetFrameToCheck(HitCharacter, HitTime));
	}
	return ShotgunConfirmHit(FramesToCheck, TraceStart, HitLocations);
}

FFramePackage ULagCompensationComponent::GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime) {
	bool bReturn =
		HitCharacter == nullptr ||
		HitCharacter->GetLagCompensation() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetHead() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetTail() == nullptr;

	if (bReturn) return FFramePackage();

	// Frame package that we check to verify a hit
	FFramePackage FrameToCheck;
	bool bShouldInterpolate = true;
	// Frame history of the HitCharacter (character that you shot at).
	const TDoubleLinkedList<FFramePackage>& History = HitCharacter->GetLagCompensation()->FrameHistory;

	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	const float LatestHistoryTime = History.GetHead()->GetValue().Time;

	/**
	* Checking cases where the frame to check (HitTime) is either the first
	* or last in the frame history or if it is out of bounds
	*/
	if (OldestHistoryTime > HitTime) {
		// HitTime is beyond the maximum history range, could be too laggy
		return FFramePackage();
	}
	if (LatestHistoryTime <= HitTime) {
		// HitTime is newer than the latest frame or equal to
		FrameToCheck = History.GetHead()->GetValue();
		bShouldInterpolate = false;
	}
	if (OldestHistoryTime == HitTime) {
		// Checking edge case
		FrameToCheck = History.GetTail()->GetValue();
		bShouldInterpolate = false;
	}

	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = History.GetHead();
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = History.GetHead();
	while (Older->GetValue().Time > HitTime && FrameToCheck.Time == 0.f) { // Is Older still younger than HitTime?
		// March back until Oldertime < HitTime < YoungerTime
		if (Older->GetNextNode() == nullptr) {
			break;
		}
		Older = Older->GetNextNode();
		if (Older->GetValue().Time > HitTime) {
			Younger = Older;
		}
	}
	if (Older->GetValue().Time == HitTime) { // Unlikely but needs to be checked
		FrameToCheck = Older->GetValue();
		bShouldInterpolate = false;
	}
	if (bShouldInterpolate) {
		// Interpolate between younger and older
		FrameToCheck = InterpBetweenFrames(Older->GetValue(), Younger->GetValue(), HitTime);
	}
	return FrameToCheck;
}

/**
*
*/
void ULagCompensationComponent::ServerScoreRequest_Implementation(
	ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation,
	float HitTime, AWeapon* DamageCauser) {
	FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart,
		HitLocation, HitTime);

	if (Character && HitCharacter && DamageCauser && Confirm.bHitConfirmed) {
		UGameplayStatics::ApplyDamage(HitCharacter, DamageCauser->GetDamage(),
			Character->Controller, DamageCauser, UDamageType::StaticClass());
	}
}

void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(
	const TArray<ABlasterCharacter*>& HitCharacters,
	const FVector_NetQuantize& TraceStart,
	const TArray<FVector_NetQuantize>& HitLocations, float HitTime) {

	FShotgunServerSideRewindResult Confirm = ShotgunServerSideRewind(
		HitCharacters, TraceStart, HitLocations, HitTime);

	for (ABlasterCharacter* HitCharacter : HitCharacters) {
		if (HitCharacter == nullptr ||
			HitCharacter->GetEquippedWeapon() == nullptr ||
			Character == nullptr) {
			continue;
		}
		float TotalDamage = 0.f;
		if (Confirm.HeadShots.Contains(HitCharacter)) {
			float HeadShotDamage =
				Confirm.HeadShots[HitCharacter] * HitCharacter->GetEquippedWeapon()->GetDamage();
			TotalDamage += HeadShotDamage;
		}
		if (Confirm.BodyShots.Contains(HitCharacter)) {
			float BodyShotDamage =
				Confirm.BodyShots[HitCharacter] * HitCharacter->GetEquippedWeapon()->GetDamage();
			TotalDamage += BodyShotDamage;
		}
		UGameplayStatics::ApplyDamage(HitCharacter, TotalDamage,
			Character->Controller, HitCharacter->GetEquippedWeapon(),
			UDamageType::StaticClass());
	}
}