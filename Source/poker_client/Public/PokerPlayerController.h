#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PokerDataTypes.h" // Для EPlayerAction
#include "PokerPlayerController.generated.h" // Убедитесь, что это имя вашего файла

// Прямые объявления для UPROPERTY
class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UGameHUDInterface; // Ваш C++ интерфейс для HUD
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class POKER_CLIENT_API APokerPlayerController : public APlayerController // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    APokerPlayerController();

    // --- Enhanced Input Свойства ---
    // Назначаются в Blueprint наследнике этого контроллера
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Mappings")
    UInputMappingContext* PlayerInputMappingContext; // Контекст для игрового режима (переименован для ясности)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions")
    UInputAction* LookUpAction; // Для осмотра вверх/вниз

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions")
    UInputAction* TurnAction; // Для осмотра влево/вправо

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions")
    UInputAction* ToggleToUIAction; // Для переключения В UI-режим (Tab или другая клавиша)

    // --- UI Свойства ---
    // Назначается в Blueprint наследнике этого контроллера
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> GameHUDClass;

    // Экземпляр созданного HUD
    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* GameHUDWidgetInstance;

    // --- Функции Управления Режимом Ввода (вызываются из BP или C++) ---
    /** Переключает контроллер в режим "только игра": ввод для Pawn'а, курсор скрыт.
     *  Эта функция будет вызываться из WBP_GameHUD.
     */
    UFUNCTION(BlueprintCallable, Category = "Input Management")
    void SwitchToGameInputMode();

    /** Переключает контроллер в режим "только UI": ввод для UI, курсор виден.
     *  Эта функция будет вызываться при нажатии ToggleToUIAction.
     */
    UFUNCTION(BlueprintCallable, Category = "Input Management") // Сделаем BlueprintCallable на всякий случай
        void SwitchToUIInputMode(UUserWidget* WidgetToFocus = nullptr);


protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // Функции-обработчики для Input Actions
    void HandleLookUp(const struct FInputActionValue& Value);
    void HandleTurn(const struct FInputActionValue& Value);
    void HandleToggleToUI(const struct FInputActionValue& Value); // Обработчик для ToggleToUIAction

    // Переменная для отслеживания текущего UI режима (все еще полезна)
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Management")
    bool bIsInUIMode;

    // Функция-обработчик делегата от OfflineGameManager
    UFUNCTION()
    void HandleActionRequested(int32 SeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack, int64 CurrentPot);

public:
    // Функции-заглушки для обработки нажатий кнопок HUD
    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleFoldAction();

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleCheckCallAction();

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleBetRaiseAction(int64 Amount);

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandlePostBlindAction();

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Player UI|Cards") // Категория для удобства в BP
    void OnLocalPlayerCardsDealt_BP(const TArray<FCard>& DealtHoleCards);
};