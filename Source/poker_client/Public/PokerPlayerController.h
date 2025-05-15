#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PokerDataTypes.h" // Для EPlayerAction
#include "PokerPlayerController.generated.h"

class UUserWidget;
class UGameHUDInterface; // Прямое объявление C++ интерфейса

UCLASS()
class POKER_CLIENT_API APokerPlayerController : public APlayerController // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    APokerPlayerController();

    // Класс виджета HUD, назначается в Blueprint наследнике этого контроллера
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> GameHUDClass;

    // Экземпляр созданного HUD
    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* GameHUDWidgetInstance;

    // Вызывается для установки игрового режима ввода (осмотр)
    UFUNCTION(BlueprintCallable, Category = "Input")
    void SetInputModeGameOnlyAdvanced();

    // Вызывается для установки UI режима ввода (курсор)
    UFUNCTION(BlueprintCallable, Category = "Input")
    void SetInputModeUIOnlyAdvanced(UUserWidget* InWidgetToFocus = nullptr, bool bLockMouseToViewport = false);

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // Функции обработки ввода осей
    void LookUp(float Value);
    void Turn(float Value);

    // Функция переключения режимов
    void ToggleInputMode();

    // Переменная для отслеживания текущего UI режима
    bool bIsUIModeActive;

    // Функция-обработчик делегата от OfflineGameManager
    UFUNCTION()
    void HandleActionRequested(int32 SeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack);

public:
    // Функции-заглушки для обработки нажатий кнопок HUD (вызываются из WBP_GameHUD)
    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleFoldAction();

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleCheckCallAction();

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleBetRaiseAction(int64 Amount);
};