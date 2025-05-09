#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
// Включаем наши типы и классы состояния/колоды
#include "PokerDataTypes.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "OfflineGameManager.generated.h" // Должен быть последним

/**
 * Класс UObject, отвечающий за управление логикой оффлайн игры в покер.
 * Создается и хранится в GameInstance для доступа на протяжении сессии.
 */
UCLASS(BlueprintType) // BlueprintType на случай, если захотим вызывать что-то из BP
class POKER_CLIENT_API UOfflineGameManager : public UObject
{
	GENERATED_BODY()

public:
	// Указатель на объект, хранящий текущее состояние игры
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game")
	UOfflinePokerGameState* GameStateData;

	// Указатель на объект колоды карт
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game")
	UDeck* Deck;

	// Конструктор по умолчанию
	UOfflineGameManager();

	/**
	 * Инициализирует новую оффлайн игру.
	 * Создает GameState, Deck, рассаживает игроков/ботов.
	 * @param NumPlayers Количество реальных игроков (обычно 1).
	 * @param NumBots Количество ботов.
	 * @param InitialStack Начальный стек фишек для всех.
	 */
	UFUNCTION(BlueprintCallable, Category = "Offline Game")
	void InitializeGame(int32 NumPlayers, int32 NumBots, int64 InitialStack);

	/**
	 * Возвращает текущее состояние игры (для чтения).
	 * @return Указатель на UOfflinePokerGameState или nullptr, если игра не инициализирована.
	 */
	UFUNCTION(BlueprintPure, Category = "Offline Game")
	UOfflinePokerGameState* GetGameState() const;

	UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow")
	void StartNewHand();


private:
	int32 GetNextActivePlayerSeat(int32 StartSeatIndex, bool bIncludeStartSeat = false) const;
	void PostBlinds(int32 SmallBlindSeat, int32 BigBlindSeat, int64 SmallBlindAmount, int64 BigBlindAmount);
	void RequestPlayerAction(int32 SeatIndex);
};