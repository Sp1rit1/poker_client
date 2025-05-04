#include "PokerHandEvaluator.h"
#include "PokerDataTypes.h"
#include "Algo/Sort.h"
#include "Containers/Map.h"
#include "Containers/Set.h" // Для TSet в GetKickers
#include "Math/UnrealMathUtility.h" // Для FMath::Min
#include "Logging/LogMacros.h"

// Определяем константу с индексами для комбинаций 7 choose 5 (все 21)
// Это позволяет избежать рекурсии или сложных циклов генерации комбинаций
const int32 UPokerHandEvaluator::Combinations7c5[21][5] = {
	{0, 1, 2, 3, 4}, {0, 1, 2, 3, 5}, {0, 1, 2, 3, 6}, {0, 1, 2, 4, 5}, {0, 1, 2, 4, 6},
	{0, 1, 2, 5, 6}, {0, 1, 3, 4, 5}, {0, 1, 3, 4, 6}, {0, 1, 3, 5, 6}, {0, 1, 4, 5, 6},
	{0, 2, 3, 4, 5}, {0, 2, 3, 4, 6}, {0, 2, 3, 5, 6}, {0, 2, 4, 5, 6}, {0, 3, 4, 5, 6},
	{1, 2, 3, 4, 5}, {1, 2, 3, 4, 6}, {1, 2, 3, 5, 6}, {1, 2, 4, 5, 6}, {1, 3, 4, 5, 6},
	{2, 3, 4, 5, 6}
};


FPokerHandResult UPokerHandEvaluator::EvaluatePokerHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards)
{
	TArray<FCard> AllCards = HoleCards;
	AllCards.Append(CommunityCards);

	const int32 NumTotalCards = AllCards.Num();
	FPokerHandResult BestHandResult; // Инициализируется HighCard по умолчанию

	if (NumTotalCards < 5)
	{
		// Недостаточно карт, возвращаем HighCard с лучшей картой
		if (NumTotalCards > 0)
		{
			TArray<FCard> TempSort = AllCards;
			SortFiveCardsDesc(TempSort); // Используем сортировку для 5, т.к. она обрабатывает массивы < 5
			BestHandResult.HandRank = EPokerHandRank::HighCard;
			BestHandResult.Kickers.Add(TempSort[0].Rank);
		}
		// Если 0 карт, вернется дефолтный HighCard без кикеров
		return BestHandResult;
	}

	// --- Основная логика: Найти лучшую 5-карточную руку ---

	TArray<FCard> CurrentFiveCards;
	CurrentFiveCards.SetNumUninitialized(5); // Выделяем память один раз

	if (NumTotalCards == 5)
	{
		// Если ровно 5 карт, оцениваем только их
		for (int i = 0; i < 5; ++i) CurrentFiveCards[i] = AllCards[i];
		BestHandResult = EvaluateSingleFiveCardHand(CurrentFiveCards);
	}
	else if (NumTotalCards == 6)
	{
		// Перебираем все 6 комбинаций по 5 карт (6 choose 5)
		for (int32 i = 0; i < 6; ++i)
		{
			int32 CurrentIndex = 0;
			for (int32 j = 0; j < 6; ++j)
			{
				if (i == j) continue; // Пропускаем одну карту
				CurrentFiveCards[CurrentIndex++] = AllCards[j];
			}
			FPokerHandResult CurrentResult = EvaluateSingleFiveCardHand(CurrentFiveCards);
			if (CompareHandResults(CurrentResult, BestHandResult) > 0)
			{
				BestHandResult = CurrentResult;
			}
		}
	}
	else // NumTotalCards == 7 (или больше, но мы используем только 7)
	{
		int32 CardsToUse = FMath::Min(NumTotalCards, 7); // На случай если передали > 7 карт
		// Перебираем все 21 комбинацию по 5 карт из 7 (7 choose 5)
		for (int32 i = 0; i < 21; ++i)
		{
			for (int32 j = 0; j < 5; ++j)
			{
				int32 cardIndex = Combinations7c5[i][j];
				if (cardIndex < CardsToUse) // Проверка на случай если карт меньше 7
				{
					CurrentFiveCards[j] = AllCards[cardIndex];
				}
				else // Если индекс выходит за пределы, просто возьмем последнюю карту (маловероятно, но безопасно)
				{
					CurrentFiveCards[j] = AllCards[CardsToUse - 1];
				}

			}
			FPokerHandResult CurrentResult = EvaluateSingleFiveCardHand(CurrentFiveCards);
			if (CompareHandResults(CurrentResult, BestHandResult) > 0)
			{
				BestHandResult = CurrentResult;
			}
		}
	}

	return BestHandResult;
}


// --- Реализация оценки конкретной 5-карточной руки ---
FPokerHandResult UPokerHandEvaluator::EvaluateSingleFiveCardHand(const TArray<FCard>& FiveCards)
{
	if (FiveCards.Num() != 5) return FPokerHandResult(); // Должно быть ровно 5 карт

	FPokerHandResult Result;
	TArray<FCard> SortedCards = FiveCards; // Копируем для сортировки
	SortFiveCardsDesc(SortedCards); // Сортируем от старшей к младшей

	ECardRank StraightHighCard;
	ECardSuit FlushSuit;
	bool bIsStraight = IsStraight(SortedCards, StraightHighCard);
	bool bIsFlush = IsFlush(SortedCards, FlushSuit);

	// Проверка на Стрит Флеш / Роял Флеш
	if (bIsStraight && bIsFlush)
	{
		Result.HandRank = (StraightHighCard == ECardRank::Ace) ? EPokerHandRank::RoyalFlush : EPokerHandRank::StraightFlush;
		Result.Kickers.Add(StraightHighCard); // Кикер - старшая карта стрита
		return Result;
	}

	// Подсчет рангов для Каре, Фулл Хауса, Сета, Пар
	TMap<ECardRank, int32> RankCounts;
	CountRanks(SortedCards, RankCounts);

	TArray<ECardRank> Fours;
	TArray<ECardRank> Threes;
	TArray<ECardRank> Pairs;

	for (const auto& Pair : RankCounts)
	{
		if (Pair.Value == 4) Fours.Add(Pair.Key);
		else if (Pair.Value == 3) Threes.Add(Pair.Key);
		else if (Pair.Value == 2) Pairs.Add(Pair.Key);
	}

	// Проверка на Каре (Four of a Kind)
	if (Fours.Num() == 1)
	{
		Result.HandRank = EPokerHandRank::FourOfAKind;
		Result.Kickers.Add(Fours[0]); // Ранг каре
		GetKickers(SortedCards, 1, Result.Kickers, { Fours[0] }); // 1 кикер
		return Result;
	}

	// Проверка на Фулл Хаус (Full House)
	if (Threes.Num() == 1 && Pairs.Num() == 1)
	{
		Result.HandRank = EPokerHandRank::FullHouse;
		Result.Kickers.Add(Threes[0]); // Ранг тройки
		Result.Kickers.Add(Pairs[0]);  // Ранг пары
		return Result;
	}

	// Проверка на Флеш (Flush)
	if (bIsFlush)
	{
		Result.HandRank = EPokerHandRank::Flush;
		GetKickers(SortedCards, 5, Result.Kickers, {}); // 5 кикеров (все карты)
		return Result;
	}

	// Проверка на Стрит (Straight)
	if (bIsStraight)
	{
		Result.HandRank = EPokerHandRank::Straight;
		Result.Kickers.Add(StraightHighCard); // Кикер - старшая карта стрита
		return Result;
	}

	// Проверка на Сет/Тройку (Three of a Kind)
	if (Threes.Num() == 1)
	{
		Result.HandRank = EPokerHandRank::ThreeOfAKind;
		Result.Kickers.Add(Threes[0]); // Ранг тройки
		GetKickers(SortedCards, 2, Result.Kickers, { Threes[0] }); // 2 кикера
		return Result;
	}

	// Проверка на Две Пары (Two Pair)
	if (Pairs.Num() >= 2) // Может быть 2 пары в 5 картах
	{
		Pairs.Sort([](const ECardRank& A, const ECardRank& B) { return A > B; }); // Сортируем пары
		Result.HandRank = EPokerHandRank::TwoPair;
		Result.Kickers.Add(Pairs[0]); // Старшая пара
		Result.Kickers.Add(Pairs[1]); // Младшая пара
		GetKickers(SortedCards, 1, Result.Kickers, { Pairs[0], Pairs[1] }); // 1 кикер
		return Result;
	}

	// Проверка на Одну Пару (One Pair)
	if (Pairs.Num() == 1)
	{
		Result.HandRank = EPokerHandRank::OnePair;
		Result.Kickers.Add(Pairs[0]); // Ранг пары
		GetKickers(SortedCards, 3, Result.Kickers, { Pairs[0] }); // 3 кикера
		return Result;
	}

	// Если ничего не найдено - Старшая Карта (High Card)
	Result.HandRank = EPokerHandRank::HighCard;
	GetKickers(SortedCards, 5, Result.Kickers, {}); // 5 кикеров (все карты)
	return Result;
}


// --- Реализация вспомогательных функций ---

void UPokerHandEvaluator::SortFiveCardsDesc(TArray<FCard>& FiveCards)
{
	// Сортируем по убыванию ранга
	// Используем Algo::Sort для небольших массивов, может быть эффективнее
	Algo::Sort(FiveCards, [](const FCard& A, const FCard& B) {
		return A.Rank > B.Rank;
		});
}

bool UPokerHandEvaluator::IsStraight(const TArray<FCard>& SortedFiveCards, ECardRank& OutHighCard)
{
	if (SortedFiveCards.Num() != 5) return false;

	bool bSequential = true;
	for (int32 i = 0; i < 4; ++i)
	{
		if (static_cast<uint8>(SortedFiveCards[i].Rank) != static_cast<uint8>(SortedFiveCards[i + 1].Rank) + 1)
		{
			bSequential = false;
			break;
		}
	}

	if (bSequential)
	{
		OutHighCard = SortedFiveCards[0].Rank; // Старшая карта обычного стрита
		return true;
	}

	// Отдельная проверка на стрит A-2-3-4-5 ("Wheel")
	if (SortedFiveCards[0].Rank == ECardRank::Ace &&
		SortedFiveCards[1].Rank == ECardRank::Five &&
		SortedFiveCards[2].Rank == ECardRank::Four &&
		SortedFiveCards[3].Rank == ECardRank::Three &&
		SortedFiveCards[4].Rank == ECardRank::Two)
	{
		OutHighCard = ECardRank::Five; // Старшая карта для A-5 стрита - это 5
		return true;
	}

	return false;
}

bool UPokerHandEvaluator::IsFlush(const TArray<FCard>& FiveCards, ECardSuit& OutSuit)
{
	if (FiveCards.Num() != 5) return false;

	OutSuit = FiveCards[0].Suit;
	for (int32 i = 1; i < 5; ++i)
	{
		if (FiveCards[i].Suit != OutSuit)
		{
			return false;
		}
	}
	return true;
}

void UPokerHandEvaluator::CountRanks(const TArray<FCard>& FiveCards, TMap<ECardRank, int32>& OutRankCounts)
{
	OutRankCounts.Empty();
	for (const FCard& Card : FiveCards)
	{
		OutRankCounts.FindOrAdd(Card.Rank)++;
	}
}

void UPokerHandEvaluator::GetKickers(const TArray<FCard>& SortedFiveCards, int32 NumKickersToGet, TArray<ECardRank>& OutKickers, const TSet<ECardRank>& RanksToExclude)
{
	// OutKickers НЕ очищается здесь, предполагается, что она уже содержит основные карты комбинации
	int32 KickersAdded = 0;
	for (const FCard& Card : SortedFiveCards)
	{
		if (!RanksToExclude.Contains(Card.Rank))
		{
			// Проверяем, нет ли уже такого кикера (на случай, если основные карты уже добавлены)
			if (!OutKickers.Contains(Card.Rank))
			{
				OutKickers.Add(Card.Rank);
				KickersAdded++;
				if (KickersAdded >= NumKickersToGet)
				{
					return;
				}
			}
		}
	}
}

int32 UPokerHandEvaluator::CompareHandResults(const FPokerHandResult& HandA, const FPokerHandResult& HandB)
{
	// 1. Сравниваем ранг комбинации
	if (HandA.HandRank > HandB.HandRank) return 1;
	if (HandA.HandRank < HandB.HandRank) return -1;

	// 2. Ранги равны, сравниваем кикеры
	int32 NumKickers = FMath::Min(HandA.Kickers.Num(), HandB.Kickers.Num());
	for (int32 i = 0; i < NumKickers; ++i)
	{
		if (HandA.Kickers[i] > HandB.Kickers[i]) return 1;
		if (HandA.Kickers[i] < HandB.Kickers[i]) return -1;
	}

	// Если все кикеры равны (или их нет), комбинации считаются равными
	return 0;
}