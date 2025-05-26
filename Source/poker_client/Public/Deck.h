#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"     
#include "Misc/Optional.h" 
#include "Deck.generated.h"

UCLASS()
class POKER_CLIENT_API UDeck : public UObject 
{
	GENERATED_BODY()

private:
	UPROPERTY() 
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