#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h" // Включаем наш файл с типами
#include "Deck.generated.h"

/**
 * Класс UObject, представляющий игральную колоду карт.
 */
UCLASS() // Не Blueprintable, т.к. используется только внутри C++ логики
class POKER_CLIENT_API UDeck : public UObject // Замени POKER_CLIENT_API
{
	GENERATED_BODY()

private:
	// Массив карт в колоде
	TArray<FCard> Cards;

public:
	// Инициализирует колоду стандартными 52 картами
	void Initialize();

	// Перемешивает карты в колоде
	void Shuffle();

	// Раздает (возвращает и удаляет) одну карту с верха колоды
	// Возвращает невалидную карту (можно проверить через доп. метод), если колода пуста
	FCard DealCard();

	// Проверяет, пуста ли колода
	bool IsEmpty() const;

	// Возвращает количество оставшихся карт
	int32 NumCardsLeft() const;
};