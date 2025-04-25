#include "GameClasses/Deck.h" // Включаем .h файл
#include "Algo/RandomShuffle.h" // Для перемешивания
#include "Containers/EnumAsByte.h" // Для итерации по Enum

void UDeck::Initialize()
{
	Cards.Empty(); // Очищаем на всякий случай
	Cards.Reserve(52); // Резервируем память

	// Получаем доступ к перечислениям
	const UEnum* SuitEnum = StaticEnum<ECardSuit>();
	const UEnum* RankEnum = StaticEnum<ECardRank>();

	if (!SuitEnum || !RankEnum)
	{
		UE_LOG(LogTemp, Error, TEXT("UDeck::Initialize - Failed to get Suit or Rank Enum!"));
		return;
	}

	// Проходим по всем мастям
	for (int32 s = 0; s < SuitEnum->NumEnums() - 1; ++s) // -1 чтобы пропустить UMETA(Hidden) или MAX
	{
		ECardSuit CurrentSuit = static_cast<ECardSuit>(SuitEnum->GetValueByIndex(s));

		// Проходим по всем рангам
		for (int32 r = 0; r < RankEnum->NumEnums() - 1; ++r)
		{
			ECardRank CurrentRank = static_cast<ECardRank>(RankEnum->GetValueByIndex(r));

			// Создаем и добавляем карту
			Cards.Add(FCard(CurrentSuit, CurrentRank));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UDeck::Initialize - Deck created with %d cards."), Cards.Num());
}

void UDeck::Shuffle()
{
	if (Cards.Num() > 0)
	{
		Algo::RandomShuffle(Cards);
		UE_LOG(LogTemp, Verbose, TEXT("UDeck::Shuffle - Deck shuffled."));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("UDeck::Shuffle - Cannot shuffle an empty deck."));
	}
}

FCard UDeck::DealCard()
{
	if (!IsEmpty())
	{
		// Раздаем последнюю карту из массива (эффективнее, чем первую)
		return Cards.Pop(); // Pop удаляет и возвращает элемент
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UDeck::DealCard - Deck is empty!"));
		// Возвращаем "невалидную" карту или можно было бы сделать FCard* и вернуть nullptr
		return FCard(); // Возвращаем карту по умолчанию (2 Clubs)
	}
}

bool UDeck::IsEmpty() const
{
	return Cards.IsEmpty();
}

int32 UDeck::NumCardsLeft() const
{
	return Cards.Num();
}