#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"       // Для EPlayerAction, FCard и других покерных типов
#include "OfflinePokerGameState.h" // Для UOfflinePokerGameState
#include "Deck.h"                  // Для UDeck
#include "OfflineGameManager.generated.h" // Должен быть последним инклюдом

// --- ДЕЛЕГАТЫ ДЛЯ СВЯЗИ С APokerPlayerController ---

// Делегат 1: Уведомление о том, чей сейчас ход начался. Передает индекс места.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerTurnStartedSignature, int32, MovingPlayerSeatIndex);

// Делегат 2: Передача списка доступных действий для текущего игрока.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerActionsAvailableSignature, const TArray<EPlayerAction>&, AllowedActions);

// Делегат 3: Передача основной информации о состоянии стола (имя ходящего, текущий банк).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTableStateInfoSignature, const FString&, MovingPlayerName, int64, CurrentPot);

// Делегат 4: Передача деталей, необходимых для UI кнопок действий (стоимость колла, мин. рейз, стек ходящего).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnActionUIDetailsSignature, int64, BetToCall, int64, MinRaiseAmount, int64, PlayerStackOfMovingPlayer);

// Делегат 5: Сообщение для истории игры.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameHistoryEventSignature, const FString&, HistoryMessage);

// Делегат 6: Уведомление об обновлении общих карт на столе.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommunityCardsUpdatedSignature, const TArray<FCard>&, CommunityCards);

// Делегат 7: Уведомление о результатах шоудауна (опционально, можно передавать данные победителя и комбинации).
// Пока просто сигнализирует о начале шоудауна и кто участвует.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShowdownSignature, const TArray<int32>&, ShowdownPlayerSeatIndices);


UCLASS(BlueprintType)
class POKER_CLIENT_API UOfflineGameManager : public UObject // Замените POKER_CLIENT_API на ваш YOURPROJECT_API
{
	GENERATED_BODY()

public:
	// Указатель на объект, хранящий текущее состояние игры
	// Сделаем его UPROPERTY, чтобы GC его не собрал, и BlueprintReadOnly для доступа из BP (если нужно для отладки)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game State")
	TObjectPtr<UOfflinePokerGameState> GameStateData;

	// Указатель на объект колоды карт
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game Deck")
	TObjectPtr<UDeck> Deck;

	// --- Делегаты для уведомления внешних систем (APokerPlayerController) ---
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
	FOnShowdownSignature OnShowdownDelegate;


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
	UOfflinePokerGameState* GetGameState() const; // Оставим GetGameState, так как вы его уже использовали

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
	 * @param ActingPlayerSeatIndex Индекс места игрока, совершившего действие.
	 * @param PlayerAction Совершенное действие (Fold, Call, Bet, Raise, PostBlind).
	 * @param Amount Сумма ставки/рейза (0 для Fold, Check, Call, PostBlind).
	 */
	UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow")
	void ProcessPlayerAction(int32 ActingPlayerSeatIndex, EPlayerAction PlayerAction, int64 Amount);


private:
	// --- Внутренние Функции Управления Игрой ---

	/**
	 * Запрашивает действие у игрока на указанном месте.
	 * Определяет доступные действия и вызывает соответствующие делегаты для UI.
	 * @param SeatIndex Индекс места игрока, чей ход.
	 */
	void RequestPlayerAction(int32 SeatIndex);

	/**
	 * Вызывается после того, как игрок на SB поставил блайнд.
	 * Запрашивает постановку большого блайнда.
	 */
	void RequestBigBlind();

	/**
	 * Раздает карманные карты активным игрокам и начинает раунд Preflop.
	 * Вызывается после успешной постановки всех блайндов.
	 */
	void DealHoleCardsAndStartPreflop();

	/**
	 * Переходит к следующей стадии игры (Flop, Turn, River, Showdown).
	 * Раздает общие карты, сбрасывает ставки раунда, определяет первого ходящего.
	 */
	void ProceedToNextGameStage();

	/**
	 * Запускает процесс вскрытия карт.
	 * Определяет участников, оценивает руки, находит победителя(ей).
	 */
	void ProceedToShowdown();

	/**
	 * Начисляет банк победителю(ям) раздачи.
	 * @param WinningSeatIndices Массив индексов мест победителей (может быть несколько при ничьей).
	 */
	void AwardPotToWinner(const TArray<int32>& WinningSeatIndices);
	// Если победитель всегда один для MVP, можно упростить до AwardPotToWinner(int32 WinnerSeatIndex)

	// --- Вспомогательные Функции ---

	/**
	 * Находит следующее активное место игрока по часовой стрелке,
	 * учитывая статус игрока (не Folded, есть фишки или AllIn).
	 * @param StartSeatIndex Индекс места, с которого начинать поиск.
	 * @param bExcludeStartSeat Если true, то StartSeatIndex не проверяется как первый кандидат.
	 * @return Индекс следующего активного игрока или -1, если не найден.
	 */
	int32 GetNextPlayerToAct(int32 StartSeatIndex, bool bExcludeStartSeat = true) const;
	// Переименовал из GetNextActivePlayerSeat для большей ясности, что ищем именно для хода

	/**
	 * Определяет первого игрока, который должен действовать после постановки блайндов (на Preflop).
	 * @return Индекс места первого ходящего или -1.
	 */
	int32 DetermineFirstPlayerToActAtPreflop() const; // Переименовал для ясности

	/**
	 * Определяет первого игрока, который должен действовать на постфлоп раундах (Flop, Turn, River).
	 * @return Индекс места первого ходящего или -1.
	 */
	int32 DetermineFirstPlayerToActPostflop() const;

	/**
	 * Проверяет, завершен ли текущий круг торгов.
	 * @return true, если круг торгов завершен, иначе false.
	 */
	bool IsBettingRoundOver() const;

	/**
	 * Вспомогательная функция для постановки блайндов. Вызывается из StartNewHand.
	 */
	void PostBlinds(); // Убрал параметры, так как SB/BB Seat и Amount будут в GameStateData
};