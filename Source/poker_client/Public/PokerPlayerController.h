#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PokerDataTypes.h" // Для EPlayerAction и FCard
#include "PokerPlayerController.generated.h"

// Прямые объявления
class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UGameHUDInterface;
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class POKER_CLIENT_API APokerPlayerController : public APlayerController // Замените POKER_CLIENT_API
{
    GENERATED_BODY()

public:
    APokerPlayerController();

    // --- Enhanced Input Свойства ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Mappings")
    UInputMappingContext* PlayerInputMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions")
    UInputAction* LookUpAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions")
    UInputAction* TurnAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions")
    UInputAction* ToggleToUIAction;

    // --- UI Свойства ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> GameHUDClass;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* GameHUDWidgetInstance;

    // --- Функции Управления Режимом Ввода ---
    UFUNCTION(BlueprintCallable, Category = "Input Management")
    void SwitchToGameInputMode();

    UFUNCTION(BlueprintCallable, Category = "Input Management")
    void SwitchToUIInputMode(UUserWidget* WidgetToFocus = nullptr);


protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // Функции-обработчики для Input Actions
    void HandleLookUp(const struct FInputActionValue& Value);
    void HandleTurn(const struct FInputActionValue& Value);
    void HandleToggleToUI(const struct FInputActionValue& Value);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Management")
    bool bIsInUIMode;

    // --- Функции-обработчики для НОВЫХ делегатов от OfflineGameManager ---
    UFUNCTION()
    void HandlePlayerTurnStarted(int32 MovingPlayerSeatIndex);

    UFUNCTION()
    void HandlePlayerActionsAvailable(const TArray<EPlayerAction>& AllowedActions);

    UFUNCTION()
    void HandleTableStateInfo(const FString& MovingPlayerName, int64 CurrentPot);

    UFUNCTION()
    void HandleActionUIDetails(int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStackOfMovingPlayer);

    UFUNCTION()
    void HandleGameHistoryEvent(const FString& HistoryMessage); // Этот уже был для истории

private:
    // --- Переменные для агрегации данных от делегатов перед обновлением HUD ---
    // Эти переменные будут хранить последние полученные данные от каждого делегата.
    // Мы используем TOptional, чтобы знать, было ли значение уже установлено для текущего запроса действия.
    TOptional<int32> OptMovingPlayerSeatIndex;
    TOptional<FString> OptMovingPlayerName;
    TOptional<TArray<EPlayerAction>> OptAllowedActions;
    TOptional<int64> OptBetToCall;
    TOptional<int64> OptMinRaiseAmount;
    TOptional<int64> OptMovingPlayerStack;
    TOptional<int64> OptCurrentPot;

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
    void HandlePostBlindAction();

    // Событие для Blueprint для отображения карт локального игрока
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Player UI|Cards")
    void OnLocalPlayerCardsDealt_BP(const TArray<FCard>& DealtHoleCards);
};