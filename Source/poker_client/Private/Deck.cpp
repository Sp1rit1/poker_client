#include "Deck.h" // Замените на правильный путь, если нужно
#include "Algo/RandomShuffle.h"
#include "Containers/EnumAsByte.h" // Для UEnum и итерации
#include "UObject/Package.h"       // Для StaticEnum (если еще не включен в PokerDataTypes.cpp для FCard::ToString)

UDeck::UDeck()
{
	// Конструктор можно оставить пустым или добавить начальную инициализацию,
	// но обычно Initialize() вызывается явно.
}

void UDeck::Initialize()
{
	Cards.Empty();
	Cards.Reserve(52);

	const UEnum* SuitEnum = StaticEnum<ECardSuit>();
	const UEnum* RankEnum = StaticEnum<ECardRank>();

	if (!SuitEnum || !RankEnum)
	{
		UE_LOG(LogTemp, Error, TEXT("UDeck::Initialize - Failed to get Suit or Rank Enum!"));
		return;
	}

	// Проходим по всем мастям, пропуская последний элемент, если это '_MAX'
	for (int32 s = 0; s < SuitEnum->NumEnums() - 1; ++s)
	{
		ECardSuit CurrentSuit = static_cast<ECardSuit>(SuitEnum->GetValueByIndex(s));

		// Проходим по всем рангам, пропуская последний элемент, если это '_MAX'
		for (int32 r = 0; r < RankEnum->NumEnums() - 1; ++r)
		{
			ECardRank CurrentRank = static_cast<ECardRank>(RankEnum->GetValueByIndex(r));
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
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UDeck::Shuffle - Cannot shuffle an empty deck."));
	}
}

TOptional<FCard> UDeck::DealCard()
{
	if (!IsEmpty())
	{
		// Pop удаляет последний элемент и возвращает его.
		// TOptional будет неявно сконструирован из FCard.
		return Cards.Pop();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UDeck::DealCard - Deck is empty!"));
		// Возвращаем "пустой" TOptional, указывая на отсутствие значения.
		return TOptional<FCard>();
		// Альтернативно: return {}; // C++17 стиль для создания пустого optional
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