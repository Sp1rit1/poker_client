#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "PokerBotAI.h" 
#include "OfflineGameManager.generated.h"

// --- ДЕЛЕГАТЫ ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerTurnStartedSignature, int32, MovingPlayerSeatIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerActionsAvailableSignature, const TArray<EPlayerAction>&, AllowedActions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTableStateInfoSignature, const FString&, MovingPlayerName, int64, CurrentPot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnActionUIDetailsSignature, int64, ActualAmountToCallForUI, int64, MinPureRaiseValueForUI, int64, PlayerStackOfMovingPlayer, int64, CurrentBetOfMovingPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameHistoryEventSignature, const FString&, HistoryMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommunityCardsUpdatedSignature, const TArray<FCard>&, CommunityCards);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShowdownResultsSignature, const TArray<FShowdownPlayerInfo>&, ShowdownResults, const FString&, WinnerAnnouncementText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActualHoleCardsDealtSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNewHandAboutToStartSignature);

UCLASS(BlueprintType) // BlueprintType уже был, он позволяет создавать экземпляры этого класса в BP
class POKER_CLIENT_API UOfflineGameManager : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game State")
    TObjectPtr<UOfflinePokerGameState> GameStateData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game Deck")
    TObjectPtr<UDeck> Deck;

    // --- Делегаты ---
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnPlayerTurnStartedSignature OnPlayerTurnStartedDelegate;
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnPlayerActionsAvailableSignature OnPlayerActionsAvailableDelegate;
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnTableStateInfoSignature OnTableStateInfoDelegate;
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnActionUIDetailsSignature OnActionUIDetailsDelegate;
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnGameHistoryEventSignature OnGameHistoryEventDelegate;
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnCommunityCardsUpdatedSignature OnCommunityCardsUpdatedDelegate;
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnShowdownResultsSignature OnShowdownResultsDelegate;
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnActualHoleCardsDealtSignature OnActualHoleCardsDealtDelegate;
    UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
    FOnNewHandAboutToStartSignature OnNewHandAboutToStartDelegate;

    // --- Настройки ИИ Бота ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bot Settings", meta = (ClampMin = "0.1", UIMin = "0.1"))
    float BotActionDelayMin = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bot Settings", meta = (ClampMin = "0.2", UIMin = "0.2"))
    float BotActionDelayMax = 2.2f;

    UOfflineGameManager();

    /**
     * Инициализирует новую оффлайн игру.
     * Создает GameState, Deck, рассаживает игроков/ботов, устанавливает размеры блайндов.
     */
    UFUNCTION(BlueprintCallable, Category = "Offline Game|Setup") // ИЗМЕНЕНО: Добавлен BlueprintCallable
    void InitializeGame(int32 NumRealPlayers, int32 NumBots, int64 InitialStack, int64 InSmallBlindAmount);

    /**
     * Возвращает текущее состояние игры (для чтения).
     */
    UFUNCTION(BlueprintPure, Category = "Offline Game State|Getters") // ИЗМЕНЕНО: Добавлен BlueprintPure
    UOfflinePokerGameState* GetGameState() const;

    /**
     * Начинает новую покерную раздачу.
     */
    UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow") // ИЗМЕНЕНО: Добавлен BlueprintCallable
    void StartNewHand();

    /**
     * Проверяет, можно ли начать новую руку.
     * @param OutReasonIfNotPossible Выходной параметр, содержащий причину, если начать нельзя.
     * @return true, если можно начать новую руку, иначе false.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Offline Game|Game Flow") // ИЗМЕНЕНО: Добавлен BlueprintCallable и BlueprintPure
    bool CanStartNewHand(FString& OutReasonIfNotPossible); // FString& для Out параметра в BP должен быть настроен в деталях функции в BP

    /**
     * Вызывается UI (через PlayerController), когда игрок совершает действие.
     */
    UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow") // Уже был BlueprintCallable
    void ProcessPlayerAction(int32 ActingPlayerSeatIndex, EPlayerAction PlayerAction, int64 Amount);

    UFUNCTION(BlueprintPure, Category = "Offline Game|Getters") // BlueprintPure, если хотите вызывать из BP
    UPokerBotAI* GetBotAIInstance() const;

    UFUNCTION(BlueprintPure, Category = "Offline Game|Getters")
    UDeck* GetDeck() const;

private:
    UPROPERTY()
    TObjectPtr<UPokerBotAI> BotAIInstance;

    FTimerHandle BotActionTimerHandle;

    TMap<int32, int64> StacksAtHandStart_Internal;

    // --- Внутренние Функции ---
    void BuildTurnOrderMap();
    void RequestPlayerAction(int32 SeatIndex);
    void RequestBigBlind();
    void DealHoleCardsAndStartPreflop();
    void ProceedToNextGameStage();
    void ProceedToShowdown();
    TMap<int32, int32> CurrentTurnOrderMap_Internal;
    TMap<int32, int64> AwardPotToWinner(const TArray<int32>& WinningSeatIndices);

    // --- Вспомогательные Функции ---
    int32 GetNextPlayerToAct(int32 StartSeatIndex, bool bExcludeStartSeat = true, EPlayerStatus RequiredStatus = EPlayerStatus::MAX_None) const;
    int32 DetermineFirstPlayerToActAtPreflop() const;
    int32 DetermineFirstPlayerToActPostflop() const;
    bool IsBettingRoundOver() const;
    void PostBlinds();

    UFUNCTION()
    void TriggerBotDecision(int32 BotSeatIndex);

    struct FActionDecisionContext
    {
        TArray<EPlayerAction> AvailableActions;
        int64 AmountToCallUI = 0;
        int64 MinPureRaiseUI = 0;
        int64 PlayerCurrentStack = 0;
        int64 PlayerCurrentBetInRound = 0;
        int64 CurrentBetToCallOnTable = 0;
    };
    FActionDecisionContext GetActionContextForSeat(int32 SeatIndex) const;
};