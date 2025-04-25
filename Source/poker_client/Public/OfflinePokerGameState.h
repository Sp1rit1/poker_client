#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h" // Включаем наш файл с типами
#include "OfflinePokerGameState.generated.h"

/**
 * Класс UObject, хранящий полное состояние одного стола для оффлайн игры.
 */
UCLASS(BlueprintType)
class POKER_CLIENT_API UOfflinePokerGameState : public UObject // Замени POKER_CLIENT_API на YOURPROJECT_API
{
	GENERATED_BODY()

public:
	// Массив данных для каждого места за столом (включая пустые места)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	TArray<FPlayerSeatData> Seats;

	// Общие карты на столе (борд)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	TArray<FCard> CommunityCards;

	// Текущий общий размер банка (основной пот)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	int64 Pot = 0;

	// Индекс места, на котором находится кнопка дилера
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	int32 DealerSeat = -1;

	// Индекс места игрока, чей сейчас ход
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	int32 CurrentTurnSeat = -1;

	// Текущая стадия игры (префлоп, флоп и т.д.)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	EGameStage CurrentStage = EGameStage::WaitingForPlayers;

	// Сумма, которую нужно доставить, чтобы остаться в игре (сделать колл)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	int64 CurrentBetToCall = 0;

	// TODO: Позже добавить массив для побочных банков (Side Pots)
	// UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	// TArray<FSidePot> SidePots;

	UOfflinePokerGameState() // Конструктор по умолчанию
	{
		// Резервируем место под обычное количество игроков + карты борда
		Seats.Reserve(9);
		CommunityCards.Reserve(5);
	}

	// (Опционально) Метод для сброса состояния к начальному
	void ResetState()
	{
		Seats.Empty();
		CommunityCards.Empty();
		Pot = 0;
		DealerSeat = -1;
		CurrentTurnSeat = -1;
		CurrentStage = EGameStage::WaitingForPlayers;
		CurrentBetToCall = 0;
		// Reset SidePots
	}

};