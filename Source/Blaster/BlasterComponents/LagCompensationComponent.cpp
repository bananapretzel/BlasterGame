// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"


ULagCompensationComponent::ULagCompensationComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;


}



void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();

	
}


void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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
		ShowFramePackage(ThisFrame, FColor::Red);
	}
}

void ULagCompensationComponent::SaveFramePackage(FFramePackage& Package) {
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : Character;
	if (Character) {
		Package.Time = GetWorld()->GetTimeSeconds();
		for (auto& BoxPair : Character->HitCollisionBoxes) {
			FBoxInformation BoxInformation;
			BoxInformation.Location = BoxPair.Value->GetComponentLocation();
			BoxInformation.Rotation = BoxPair.Value->GetComponentRotation();
			BoxInformation.BoxExtent = BoxPair.Value->GetScaledBoxExtent();
			Package.HitBoxInfo.Add(BoxPair.Key, BoxInformation);
		}
	}
}

FFramePackage ULagCompensationComponent::InterpBetweenFrames(FFramePackage& OlderFrame, FFramePackage& YoungerFrame, float HitTime) {

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

void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package, const FColor& Color) {
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

void ULagCompensationComponent::ServerSideRewind(ABlasterCharacter* HitCharacter, 
	const FVector_NetQuantize& TraceStart, 
	const FVector_NetQuantize& HitLocation, float HitTime) {

	bool bReturn = 
		HitCharacter == nullptr || 
		HitCharacter->GetLagCompensation() == nullptr || 
		HitCharacter->GetLagCompensation()->FrameHistory.GetHead() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetTail() == nullptr;

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
		return;
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
	while(Older->GetValue().Time > HitTime && FrameToCheck.Time == 0.f) { // Is Older still younger than HitTime?
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
	}
	if (bReturn) return;

}




