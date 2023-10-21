// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensation.h"


ALagCompensation::ALagCompensation()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}


void ALagCompensation::BeginPlay()
{
	Super::BeginPlay();
	
}


void ALagCompensation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

