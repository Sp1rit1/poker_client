#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PokerDataTypes.h" // Для EPlayerAction, FCard, TArray
#include "Misc/Optional.h" // Для TOptional
#include "PokerPlayerController.generated.h"

// Прямые объявления
class UInputMappingContext;
class UInputAction;
class UUserWidget;
class IGameHUDInterface; // Используем C++ интерфейс
class ICommunityCardDisplayInterface; // Интерфейс для общих карт
class IPlayerSeatVisualizerInterface; // Интерфейс для карманных карт
class UEnhancedInputLocalPlayerSubsystem;
class UOfflinePokerGameState; // Для передачи в функции обновления UI

UCLASS()
class POKER_CLIENT_API APokerPlayerController : public APlayerController // Замените POKER_CLIENT_API
{
    GENERATED_BODY()

public:
    APokerPlayerController();

    // --- Enhanced Input Свойства ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Mappings", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputMappingContext> PlayerInputMappingContext; // Используем TObjectPtr

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputAction> LookUpAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputAction> TurnAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputAction> ToggleToUIAction; // Наша клавиша для переключения в UI (бывший ToggleCursorMode)

    // --- UI Свойства ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> GameHUDClass;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    TObjectPtr<UUserWidget> GameHUDWidgetInstance; // Используем TObjectPtr

    // Ссылка на актора, отображающего общие карты (будет найдена в BeginPlay или передана)
    UPROPERTY(BlueprintReadOnly, Category = "UI")
    TObjectPtr<AActor> CommunityCardDisplayActor; // Тип AActor, так как мы будем использовать интерфейс

    // --- Функции Управления Режимом Ввода ---
    UFUNCTION(BlueprintCallable, Category = "Input Management")
    void SwitchToGameInputMode();

    UFUNCTION(BlueprintCallable, Category = "Input Management")
    void SwitchToUIInputMode(UUserWidget* WidgetToFocus = nullptr);

    // Возвращает текущий GameState для удобства доступа из Blueprint (например, для HUD)
    UFUNCTION(BlueprintPure, Category = "Game State")
    UOfflinePokerGameState* GetCurrentGameState() const;

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void RequestStartNewHandFromUI();

    UFUNCTION(BlueprintCallable, Category = "Navigation")
    void RequestReturnToMainMenu();


protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override; // Должна быть public или protected, если вы ее переопределяете

    // Функции-обработчики для Input Actions
    void HandleLookUp(const struct FInputActionValue& Value);
    void HandleTurn(const struct FInputActionValue& Value);
    void HandleToggleToUI(const struct FInputActionValue& Value); // Обработчик для IA_ToggleToUIAction

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Management")
    bool bIsInUIMode;

    // --- Функции-обработчики для делегатов от OfflineGameManager ---
    UFUNCTION()
    void HandlePlayerTurnStarted(int32 MovingPlayerSeatIndex);

    UFUNCTION()
    void HandlePlayerActionsAvailable(const TArray<EPlayerAction>& AllowedActions);

    UFUNCTION()
    void HandleTableStateInfo(const FString& MovingPlayerName, int64 CurrentPot);

    UFUNCTION()
    void HandleActionUIDetails(int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStackOfMovingPlayer, int64 CurrentBetOfMovingPlayer);

    UFUNCTION()
    void HandleGameHistoryEvent(const FString& HistoryMessage);

    // НОВЫЕ ОБРАБОТЧИКИ ДЕЛЕГАТОВ
    UFUNCTION()
    void HandleCommunityCardsUpdated(const TArray<FCard>& CommunityCards);

    UFUNCTION()
    void HandleActualHoleCardsDealt();

    UFUNCTION()
    void HandleShowdownResults(const TArray<FShowdownPlayerInfo>& ShowdownResults, const FString& WinnerAnnouncement);

    UFUNCTION() 
    void HandleNewHandAboutToStart();
    // TODO: Возможно, сюда нужно будет передавать и результаты рук (TArray<FPokerHandResult>)

private:
    // --- Переменные для агрегации данных от делегатов перед обновлением HUD ---
    TOptional<int32> OptMovingPlayerSeatIndex;
    TOptional<FString> OptMovingPlayerName;
    TOptional<TArray<EPlayerAction>> OptAllowedActions;
    TOptional<int64> OptBetToCall;
    TOptional<int64> OptMinRaiseAmount;
    TOptional<int64> OptMovingPlayerStack;
    TOptional<int64> OptCurrentPot;
    TOptional<int64> OptMovingPlayerCurrentBet;

    // Вспомогательная функция для проверки, все ли данные собраны, и вызова обновления HUD
    void TryAggregateAndTriggerHUDUpdate();


public:
    // Функции-обработчики нажатий кнопок HUD
    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleFoldAction();

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleCheckCallAction();

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleBetRaiseAction(int64 Amount);

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandlePostBlindAction(); // Вызывается из WBP_GameHUD, когда кнопка "PostBlind" нажата

    UFUNCTION(BlueprintCallable, Category = "Player Visualizers")
    void UpdateAllSeatVisualizersFromGameState();
};