// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PokerPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class POKER_CLIENT_API APokerPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
    APokerPlayerController(); // Конструктор

    // Функции для управления HUD (как мы обсуждали для Дня 4)
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowGameHUD();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideGameHUD();

protected:
    virtual void BeginPlay() override; // Переопределяем BeginPlay

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUserWidget> GameHUDClass; // Класс для игрового HUD

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "UI")
    class UUserWidget* GameHUDInstance;
	
};
