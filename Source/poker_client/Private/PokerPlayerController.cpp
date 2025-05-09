// Fill out your copyright notice in the Description page of Project Settings.


#include "PokerPlayerController.h"
#include "Blueprint/UserWidget.h" // Для CreateWidget и UUserWidget

APokerPlayerController::APokerPlayerController()
{
    // Здесь можно установить значения по умолчанию, если нужно
}

void APokerPlayerController::BeginPlay()
{
    Super::BeginPlay(); // ВАЖНО: Вызвать родительский BeginPlay

    // Ваша логика, которая была в Blueprint, может быть здесь или в BP наследнике
    // Например, установка начального режима ввода для UI, если это всегда нужно при старте контроллера
    // SetInputMode(FInputModeUIOnly()); // Или GameAndUI
    // bShowMouseCursor = true;
}

void APokerPlayerController::ShowGameHUD()
{
    if (GameHUDClass)
    {
        if (GameHUDInstance && GameHUDInstance->IsInViewport()) return;
        GameHUDInstance = CreateWidget<UUserWidget>(this, GameHUDClass);
        if (GameHUDInstance)
        {
            GameHUDInstance->AddToViewport();
            // Можно здесь установить режим ввода для игры с HUD
            // FInputModeGameAndUI InputMode;
            // InputMode.SetWidgetToFocus(GameHUDInstance->TakeWidget()); // Опционально для фокуса
            // InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            // SetInputMode(InputMode);
            // bShowMouseCursor = true;
        }
    }
}

void APokerPlayerController::HideGameHUD()
{
    if (GameHUDInstance && GameHUDInstance->IsInViewport())
    {
        GameHUDInstance->RemoveFromViewport();
        GameHUDInstance = nullptr;
        // Можно здесь восстановить режим ввода только для игры
        // SetInputMode(FInputModeGameOnly());
        // bShowMouseCursor = false;
    }
}