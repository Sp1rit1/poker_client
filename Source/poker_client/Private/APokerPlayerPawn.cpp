// Fill out your copyright notice in the Description page of Project Settings.


#include "APokerPlayerPawn.h"

// Sets default values
AAPokerPlayerPawn::AAPokerPlayerPawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AAPokerPlayerPawn::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AAPokerPlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AAPokerPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

