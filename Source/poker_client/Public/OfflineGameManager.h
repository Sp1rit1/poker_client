#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"       // Для EPlayerAction, FCard и других покерных типов
#include "OfflinePokerGameState.h" // Для UOfflinePokerGameState
#include "Deck.h"                  // Для UDeck
#include "OfflineGameManager.generated.h" // Должен быть последним инклюдом

// Объявление делегата для уведомления UI о необходимости запроса действия у игрока
// Добавлен параметр CurrentPot
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnActionRequestedSignature, int32, PlayerSeatIndex, const TArray<EPlayerAction>&, AllowedActions, int64, BetToCall, int64, MinRaiseAmount, int64, PlayerStack, int64, CurrentPot);

UCLASS(BlueprintType)
class POKER_CLIENT_API UOfflineGameManager : public UObject // Замените YOURPROJECT_API
{
	GENERATED_BODY()

public:
	// Указатель на объект, хранящий текущее состояние игры
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game State")
	UOfflinePokerGameState* GameStateData;

	// Указатель на объект колоды карт
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game Deck")
	UDeck* Deck;

	// Делегат, вызываемый, когда нужно запросить действие у игрока
	UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
	FOnActionRequestedSignature OnActionRequestedDelegate;

	// Конструктор по умолчанию
	UOfflineGameManager();

	/**
	 * Инициализирует новую оффлайн игру.
	 * Создает GameState, Deck, рассаживает игроков/ботов, устанавливает размеры блайндов.
	 * @param NumRealPlayers Количество реальных игроков (обычно 1).
	 * @param NumBots Количество ботов.
	 * @param InitialStack Начальный стек фишек для всех.
	 * @param InSmallBlindAmount Размер малого блайнда, заданный игроком.
	 */
	UFUNCTION(BlueprintCallable, Category = "Offline Game|Setup")
	void InitializeGame(int32 NumRealPlayers, int32 NumBots, int64 InitialStack, int64 InSmallBlindAmount);

	/**
	 * Возвращает текущее состояние игры (для чтения).
	 * @return Указатель на UOfflinePokerGameState или nullptr, если игра не инициализирована.
	 */
	UFUNCTION(BlueprintPure, Category = "Offline Game State|Getters")
	UOfflinePokerGameState* GetGameState() const; // Переименован для единообразия с GetGameStateData, если такой был

	/**
	 * Начинает новую покерную раздачу.
	 * Определяет дилера, инициирует постановку блайндов, перемешивает колоду,
	 * затем вызывает раздачу карт и начало префлопа.
	 */
	UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow")
	void StartNewHand();

	/**
	 * Вызывается UI (через PlayerController), когда игрок совершает действие.
	 * Обрабатывает действие игрока и определяет дальнейший ход игры.
	 * @param PlayerAction Совершенное действие (Fold, Call, Bet, Raise, PostBlind).
	 * @param Amount Сумма ставки/рейза (0 для Fold, Check, Call, PostBlind).
	 */
	UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow")
	void ProcessPlayerAction(EPlayerAction PlayerAction, int64 Amount);

	/**
	 * Запрашивает действие у игрока на указанном месте.
	 * Определяет доступные действия и вызывает OnActionRequestedDelegate.
	 * @param SeatIndex Индекс места игрока, чей ход.
	 */
	 // Эта функция остается, так как она вызывается из StartNewHand, DealHoleCardsAndStartPreflop, и ProcessPlayerAction
	UFUNCTION(BlueprintCallable, Category = "Offline Game|Internal")
	void RequestPlayerAction(int32 SeatIndex);

private:
	// --- Вспомогательные Функции ---

	/**
	 * Находит следующее активное место игрока по часовой стрелке,
	 * учитывая статус игрока и текущую стадию игры.
	 * @param StartSeatIndex Индекс места, с которого начинать поиск.
	 * @param bIncludeStartSeat Если true, то StartSeatIndex также проверяется.
	 * @return Индекс следующего активного игрока или -1, если не найден.
	 */
	int32 GetNextActivePlayerSeat(int32 StartSeatIndex, bool bIncludeStartSeat = false) const;

	/**
	 * Переходит к следующей стадии игры (Flop, Turn, River, Showdown).
	 * Раздает общие карты, сбрасывает ставки раунда, определяет первого ходящего.
	 */
	void ProceedToNextGameStage(); // Будет реализована позже

	/**
	 * Раздает карманные карты активным игрокам и начинает раунд Preflop.
	 * Вызывается после успешной постановки всех блайндов.
	 */
	void DealHoleCardsAndStartPreflop();

	/**
	 * Определяет первого игрока, который должен действовать после постановки блайндов (на Preflop).
	 * @return Индекс места первого ходящего или -1.
	 */
	int32 DetermineFirstPlayerToActAfterBlinds() const;

	/**
	 * Определяет первого игрока, который должен действовать на постфлоп раундах (Flop, Turn, River).
	 * @return Индекс места первого ходящего или -1.
	 */
	int32 DetermineFirstPlayerToActPostFlop() const; // Будет использоваться в ProceedToNextGameStage
};