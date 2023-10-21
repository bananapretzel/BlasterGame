// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LagCompensation.generated.h"

UCLASS()
class BLASTER_API ALagCompensation : public AActor
{
	GENERATED_BODY()
	
public:	

	ALagCompensation();
	virtual void Tick(float DeltaTime) override;

protected:

	virtual void BeginPlay() override;

private:

	

};
