#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"     // Убедитесь, что FCard здесь определена
#include "Misc/Optional.h" // <-- ВАЖНО: Включить для TOptional
#include "Deck.generated.h"

UCLASS()
class POKER_CLIENT_API UDeck : public UObject // Замените POKER_CLIENT_API на ваш API макрос
{
	GENERATED_BODY()

private:
	UPROPERTY() // Добавим UPROPERTY для корректной работы с GC, хотя для TArray<FCard> это может быть избыточно, но не повредит
		TArray<FCard> Cards;

public:
	UDeck(); // Добавим конструктор для возможной инициализации

	// Инициализирует колоду стандартными 52 картами
	UFUNCTION(BlueprintCallable, Category = "Poker Deck")
	void Initialize();

	// Перемешивает карты в колоде
	UFUNCTION(BlueprintCallable, Category = "Poker Deck")
	void Shuffle();

	// Раздает (возвращает и удаляет) одну карту с верха колоды.
	// Возвращает TOptional<FCard>, который будет пуст, если колода пуста.
	TOptional<FCard> DealCard(); // <-- Тип возвращаемого значения изменен

	// Проверяет, пуста ли колода
	UFUNCTION(BlueprintPure, Category = "Poker Deck")
	bool IsEmpty() const;

	// Возвращает количество оставшихся карт
	UFUNCTION(BlueprintPure, Category = "Poker Deck")
	int32 NumCardsLeft() const;
};