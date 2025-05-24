#include "PokerHandEvaluator.h"
#include "PokerDataTypes.h"
#include "Algo/Sort.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Math/UnrealMathUtility.h"
#include "Logging/LogMacros.h"

// Можете определить свою лог категорию, если хотите
// DECLARE_LOG_CATEGORY_EXTERN(LogPokerEval, Log, All); 
// DEFINE_LOG_CATEGORY(LogPokerEval);

const int32 UPokerHandEvaluator::Combinations7c5[21][5] = {
    {0, 1, 2, 3, 4}, {0, 1, 2, 3, 5}, {0, 1, 2, 3, 6}, {0, 1, 2, 4, 5}, {0, 1, 2, 4, 6},
    {0, 1, 2, 5, 6}, {0, 1, 3, 4, 5}, {0, 1, 3, 4, 6}, {0, 1, 3, 5, 6}, {0, 1, 4, 5, 6},
    {0, 2, 3, 4, 5}, {0, 2, 3, 4, 6}, {0, 2, 3, 5, 6}, {0, 2, 4, 5, 6}, {0, 3, 4, 5, 6},
    {1, 2, 3, 4, 5}, {1, 2, 3, 4, 6}, {1, 2, 3, 5, 6}, {1, 2, 4, 5, 6}, {1, 3, 4, 5, 6},
    {2, 3, 4, 5, 6}
};

const int32 UPokerHandEvaluator::Combinations6c5[6][5] = {
    {0, 1, 2, 3, 4}, {0, 1, 2, 3, 5}, {0, 1, 2, 4, 5},
    {0, 1, 3, 4, 5}, {0, 2, 3, 4, 5}, {1, 2, 3, 4, 5}
};


FPokerHandResult UPokerHandEvaluator::EvaluatePokerHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards)
{
    TArray<FCard> AllCardsCombined = HoleCards;
    AllCardsCombined.Append(CommunityCards);

    const int32 NumTotalCards = AllCardsCombined.Num();
    FPokerHandResult BestOverallHandResult; // Инициализируется HighCard по умолчанию

    FString AllCardsStrForLog;
    for (const FCard& Card : AllCardsCombined) { AllCardsStrForLog += Card.ToString() + TEXT(" "); }
    UE_LOG(LogTemp, Log, TEXT("--- UPokerHandEvaluator::EvaluatePokerHand START --- Total Cards: %d. Cards: [%s]"), NumTotalCards, *AllCardsStrForLog.TrimEnd());

    if (NumTotalCards < 5)
    {
        UE_LOG(LogTemp, Log, TEXT("  Not enough cards (%d) for a 5-card hand. Evaluating as HighCard."), NumTotalCards);
        if (NumTotalCards > 0)
        {
            TArray<FCard> TempSort = AllCardsCombined;
            SortFiveCardsDesc(TempSort);
            BestOverallHandResult.HandRank = EPokerHandRank::HighCard;
            if (TempSort.IsValidIndex(0)) { BestOverallHandResult.Kickers.Add(TempSort[0].Rank); }
        }
        UE_LOG(LogTemp, Log, TEXT("--- UPokerHandEvaluator::EvaluatePokerHand END --- Returning (Partial): %s"), *UEnum::GetDisplayValueAsText(BestOverallHandResult.HandRank).ToString());
        return BestOverallHandResult;
    }

    TArray<FCard> CurrentFiveCardCombination;
    CurrentFiveCardCombination.SetNumUninitialized(5);

    if (NumTotalCards == 5)
    {
        UE_LOG(LogTemp, Verbose, TEXT("  Path: Exactly 5 cards. Evaluating this single combination."));
        for (int i = 0; i < 5; ++i) CurrentFiveCardCombination[i] = AllCardsCombined[i];
        BestOverallHandResult = EvaluateSingleFiveCardHand(CurrentFiveCardCombination);
    }
    else if (NumTotalCards == 6)
    {
        UE_LOG(LogTemp, Verbose, TEXT("  Path: 6 cards. Evaluating 6 choose 5 combinations."));
        for (int32 i = 0; i < 6; ++i) // Index of the card to EXCLUDE from AllCardsCombined
        {
            int32 TempIdx = 0;
            FString ComboStrForLog;
            for (int32 j = 0; j < 6; ++j) {
                if (i == j) continue; // Skip card i
                CurrentFiveCardCombination[TempIdx] = AllCardsCombined[j];
                ComboStrForLog += AllCardsCombined[j].ToString() + TEXT(" ");
                TempIdx++;
            }
            UE_LOG(LogTemp, Verbose, TEXT("    Evaluating 6c5 Combo (Skipping card at AllCardsCombined index %d): [%s]"), i, *ComboStrForLog.TrimEnd());
            FPokerHandResult CurrentCombinationResult = EvaluateSingleFiveCardHand(CurrentFiveCardCombination);
            if (CompareHandResults(CurrentCombinationResult, BestOverallHandResult) > 0) {
                BestOverallHandResult = CurrentCombinationResult;
                UE_LOG(LogTemp, Verbose, TEXT("      >>>> New Best Hand from 6c5: %s"), *UEnum::GetDisplayValueAsText(BestOverallHandResult.HandRank).ToString());
            }
        }
    }
    else // NumTotalCards >= 7 (используем первые 7, если больше)
    {
        TArray<FCard> CardsToEvaluateFrom; // Будет содержать первые 7 карт
        for (int32 i = 0; i < FMath::Min(NumTotalCards, 7); ++i) { CardsToEvaluateFrom.Add(AllCardsCombined[i]); }

        UE_LOG(LogTemp, Verbose, TEXT("  Path: %d cards (using first %d). Evaluating 7 choose 5 combinations."), NumTotalCards, CardsToEvaluateFrom.Num());

        for (int32 i = 0; i < 21; ++i) // 21 комбинация для 7c5
        {
            FString ComboStrForLog;
            for (int32 j = 0; j < 5; ++j) {
                int32 cardIndexInSubArray = Combinations7c5[i][j];
                // Индексы в Combinations7c5 для массива из 7 элементов.
                // CardsToEvaluateFrom всегда будет иметь размер 7 (или меньше, если изначально было <7 карт, но это обработано выше)
                CurrentFiveCardCombination[j] = CardsToEvaluateFrom[cardIndexInSubArray];
                ComboStrForLog += CardsToEvaluateFrom[cardIndexInSubArray].ToString() + TEXT(" ");
            }
            UE_LOG(LogTemp, Verbose, TEXT("    Evaluating 7c5 Combo #%d: [%s]"), i, *ComboStrForLog.TrimEnd());
            FPokerHandResult CurrentCombinationResult = EvaluateSingleFiveCardHand(CurrentFiveCardCombination);
            if (CompareHandResults(CurrentCombinationResult, BestOverallHandResult) > 0) {
                BestOverallHandResult = CurrentCombinationResult;
                UE_LOG(LogTemp, Verbose, TEXT("      >>>> New Best Hand from 7c5: %s"), *UEnum::GetDisplayValueAsText(BestOverallHandResult.HandRank).ToString());
            }
        }
    }

    FString KickersStrForLog;
    for (ECardRank Kicker : BestOverallHandResult.Kickers) KickersStrForLog += UEnum::GetDisplayValueAsText(Kicker).ToString() + TEXT(" ");
    UE_LOG(LogTemp, Log, TEXT("--- UPokerHandEvaluator::EvaluatePokerHand END --- Final Best Hand: %s, Kickers: [%s]"),
        *UEnum::GetDisplayValueAsText(BestOverallHandResult.HandRank).ToString(), *KickersStrForLog.TrimEnd());
    return BestOverallHandResult;
}

FPokerHandResult UPokerHandEvaluator::EvaluateSingleFiveCardHand(const TArray<FCard>& FiveCards)
{
    if (FiveCards.Num() != 5) {
        UE_LOG(LogTemp, Error, TEXT("EvaluateSingleFiveCardHand: ERROR - Called with %d cards, expected 5. Returning default HighCard."), FiveCards.Num());
        return FPokerHandResult();
    }

    FString CardsStrForLog;
    for (const FCard& c : FiveCards) CardsStrForLog += c.ToString() + TEXT(" ");
    UE_LOG(LogTemp, Verbose, TEXT("  EvaluateSingleFiveCardHand: Input for single eval [%s]"), *CardsStrForLog.TrimEnd());

    FPokerHandResult Result;
    TArray<FCard> SortedCards = FiveCards;
    SortFiveCardsDesc(SortedCards); // Сортируем от старшей к младшей

    FString SortedCardsStrForLog;
    for (const FCard& c : SortedCards) SortedCardsStrForLog += c.ToString() + TEXT(" ");
    UE_LOG(LogTemp, Verbose, TEXT("    Sorted Input: [%s]"), *SortedCardsStrForLog.TrimEnd());

    ECardRank StraightHighCardValue;
    ECardSuit FlushSuitValue;
    bool bIsStraight = IsStraight(SortedCards, StraightHighCardValue);
    bool bIsFlush = IsFlush(SortedCards, FlushSuitValue);

    UE_LOG(LogTemp, Verbose, TEXT("    Checks - IsStraight: %s (High: %s), IsFlush: %s (Suit: %s)"),
        bIsStraight ? TEXT("Yes") : TEXT("No"),
        bIsStraight ? *UEnum::GetDisplayValueAsText(StraightHighCardValue).ToString() : TEXT("N/A"),
        bIsFlush ? TEXT("Yes") : TEXT("No"),
        bIsFlush ? *UEnum::GetDisplayValueAsText(FlushSuitValue).ToString() : TEXT("N/A"));

    // 1. Стрит Флеш / Роял Флеш
    if (bIsStraight && bIsFlush) {
        // Проверка на Роял Флеш: стрит от туза (A K Q J T) той же масти.
        // StraightHighCardValue для A-5 стрита будет Five. Для T-A стрита будет Ace.
        Result.HandRank = (StraightHighCardValue == ECardRank::Ace && SortedCards[1].Rank == ECardRank::King)
            ? EPokerHandRank::RoyalFlush : EPokerHandRank::StraightFlush;
        Result.Kickers.Add(StraightHighCardValue);
        UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: %s (High: %s)"),
            *UEnum::GetDisplayValueAsText(Result.HandRank).ToString(), *UEnum::GetDisplayValueAsText(StraightHighCardValue).ToString());
        return Result;
    }

    // 2. Подсчет рангов для Каре, Фулл Хауса, Сета, Пар
    TMap<ECardRank, int32> RankCountsMap;
    CountRanks(SortedCards, RankCountsMap);
    FString RankCountsStrForLog;
    for (const auto& Elem : RankCountsMap) RankCountsStrForLog += FString::Printf(TEXT("%s:%d "), *UEnum::GetDisplayValueAsText(Elem.Key).ToString(), Elem.Value);
    UE_LOG(LogTemp, Verbose, TEXT("    RankCounts: {%s}"), *RankCountsStrForLog.TrimEnd());

    TArray<ECardRank> RanksOfFours; TArray<ECardRank> RanksOfThrees; TArray<ECardRank> RanksOfPairs;
    for (const auto& CountEntry : RankCountsMap) {
        if (CountEntry.Value == 4) RanksOfFours.Add(CountEntry.Key);
        else if (CountEntry.Value == 3) RanksOfThrees.Add(CountEntry.Key);
        else if (CountEntry.Value == 2) RanksOfPairs.Add(CountEntry.Key);
    }
    // Сортируем массивы троек и пар по убыванию, на случай если их несколько (хотя для 5 карт это ограничено)
    RanksOfThrees.Sort([](const ECardRank& A, const ECardRank& B) { return A > B; });
    RanksOfPairs.Sort([](const ECardRank& A, const ECardRank& B) { return A > B; });
    UE_LOG(LogTemp, Verbose, TEXT("    Group Counts - Fours: %d, Threes: %d, Pairs: %d"), RanksOfFours.Num(), RanksOfThrees.Num(), RanksOfPairs.Num());

    // 3. Каре (Four of a Kind)
    if (RanksOfFours.Num() == 1) {
        Result.HandRank = EPokerHandRank::FourOfAKind;
        Result.Kickers.Add(RanksOfFours[0]); // Ранг каре
        GetKickers(SortedCards, 1, Result.Kickers, { RanksOfFours[0] }); // 1 кикер
        UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: FourOfAKind (%s)"), *UEnum::GetDisplayValueAsText(RanksOfFours[0]).ToString());
        return Result;
    }

    // 4. Фулл Хаус (Full House)
    if (RanksOfThrees.Num() == 1 && RanksOfPairs.Num() >= 1) { // Тройка и хотя бы одна пара
        Result.HandRank = EPokerHandRank::FullHouse;
        Result.Kickers.Add(RanksOfThrees[0]); // Ранг тройки
        Result.Kickers.Add(RanksOfPairs[0]);  // Ранг старшей из пар (если их несколько, что невозможно с тройкой на 5 картах)
        UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: FullHouse (%ss over %ss)"),
            *UEnum::GetDisplayValueAsText(RanksOfThrees[0]).ToString(), *UEnum::GetDisplayValueAsText(RanksOfPairs[0]).ToString());
        return Result;
    }
    // Еще один случай Фулл Хауса: две тройки (невозможно на 5 картах, но для полноты)
    if (RanksOfThrees.Num() >= 2) { // Например, 3-3-3-2-2 (одна тройка и одна пара) или 3-3-3- K-K (если бы было >5 карт)
        Result.HandRank = EPokerHandRank::FullHouse;
        Result.Kickers.Add(RanksOfThrees[0]); // Старшая тройка
        // В качестве "пары" для фулл-хауса берем старшую карту из второй тройки.
        // Но это не совсем корректно для кикеров фулл-хауса, если тройки две.
        // Правильнее было бы: старшая тройка, и самая старшая из оставшихся двух карт как пара (если они пара)
        // Для 5 карт две тройки невозможны. Если есть одна тройка и одна пара, это уже обработано.
        // Этот блок избыточен для 5 карт.
    }


    // 5. Флеш (Flush)
    if (bIsFlush) {
        Result.HandRank = EPokerHandRank::Flush;
        GetKickers(SortedCards, 5, Result.Kickers, {}); // 5 кикеров (все карты флеша по старшинству)
        UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: Flush (Suit: %s, HighCard: %s)"),
            *UEnum::GetDisplayValueAsText(FlushSuitValue).ToString(),
            SortedCards.IsValidIndex(0) ? *UEnum::GetDisplayValueAsText(SortedCards[0].Rank).ToString() : TEXT("N/A"));
        return Result;
    }

    // 6. Стрит (Straight)
    if (bIsStraight) {
        Result.HandRank = EPokerHandRank::Straight;
        Result.Kickers.Add(StraightHighCardValue); // Кикер - старшая карта стрита
        UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: Straight (High: %s)"), *UEnum::GetDisplayValueAsText(StraightHighCardValue).ToString());
        return Result;
    }

    // 7. Сет/Тройка (Three of a Kind) - Фулл-хаус уже проверен
    if (RanksOfThrees.Num() == 1) {
        Result.HandRank = EPokerHandRank::ThreeOfAKind;
        Result.Kickers.Add(RanksOfThrees[0]);
        GetKickers(SortedCards, 2, Result.Kickers, { RanksOfThrees[0] }); // 2 кикера
        UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: ThreeOfAKind (%s)"), *UEnum::GetDisplayValueAsText(RanksOfThrees[0]).ToString());
        return Result;
    }

    // 8. Две Пары (Two Pair)
    if (RanksOfPairs.Num() >= 2) { // Если есть 2 или 3 пары (3 пары на 5 картах = 2 пары + кикер)
        // RanksOfPairs уже отсортирован по убыванию
        Result.HandRank = EPokerHandRank::TwoPair;
        Result.Kickers.Add(RanksOfPairs[0]); // Старшая пара
        Result.Kickers.Add(RanksOfPairs[1]); // Младшая пара
        GetKickers(SortedCards, 1, Result.Kickers, { RanksOfPairs[0], RanksOfPairs[1] }); // 1 кикер
        UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: TwoPair (%s and %s)"),
            *UEnum::GetDisplayValueAsText(RanksOfPairs[0]).ToString(), *UEnum::GetDisplayValueAsText(RanksOfPairs[1]).ToString());
        return Result;
    }

    // 9. Одна Пара (One Pair)
    if (RanksOfPairs.Num() == 1) {
        Result.HandRank = EPokerHandRank::OnePair;
        Result.Kickers.Add(RanksOfPairs[0]);
        GetKickers(SortedCards, 3, Result.Kickers, { RanksOfPairs[0] }); // 3 кикера
        UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: OnePair (%s)"), *UEnum::GetDisplayValueAsText(RanksOfPairs[0]).ToString());
        return Result;
    }

    // 10. Старшая Карта (High Card)
    Result.HandRank = EPokerHandRank::HighCard;
    GetKickers(SortedCards, 5, Result.Kickers, {}); // 5 кикеров (все карты по старшинству)
    UE_LOG(LogTemp, Verbose, TEXT("    Detected Hand: HighCard (Highest: %s)"),
        SortedCards.IsValidIndex(0) ? *UEnum::GetDisplayValueAsText(SortedCards[0].Rank).ToString() : TEXT("N/A"));
    return Result;
}

void UPokerHandEvaluator::SortFiveCardsDesc(TArray<FCard>& CardsToSort)
{
    Algo::Sort(CardsToSort, [](const FCard& A, const FCard& B) {
        return A.Rank > B.Rank;
        });
}

bool UPokerHandEvaluator::IsStraight(const TArray<FCard>& SortedFiveCards, ECardRank& OutHighCardRank)
{
    if (SortedFiveCards.Num() != 5) return false;
    // Карты УЖЕ должны быть отсортированы по убыванию ранга
    bool bIsWheel = SortedFiveCards[0].Rank == ECardRank::Ace &&
        SortedFiveCards[1].Rank == ECardRank::Five &&
        SortedFiveCards[2].Rank == ECardRank::Four &&
        SortedFiveCards[3].Rank == ECardRank::Three &&
        SortedFiveCards[4].Rank == ECardRank::Two;
    if (bIsWheel) {
        OutHighCardRank = ECardRank::Five; // Старшая карта "колеса" - это 5
        return true;
    }

    // Проверка на обычный стрит
    for (int32 i = 0; i < 4; ++i) {
        if (static_cast<int32>(SortedFiveCards[i].Rank) != static_cast<int32>(SortedFiveCards[i + 1].Rank) + 1) {
            return false; // Последовательность нарушена
        }
    }
    OutHighCardRank = SortedFiveCards[0].Rank; // Старшая карта обычного стрита
    return true;
}

bool UPokerHandEvaluator::IsFlush(const TArray<FCard>& FiveCardsToTest, ECardSuit& OutFlushSuit)
{
    if (FiveCardsToTest.Num() != 5) return false;
    OutFlushSuit = FiveCardsToTest[0].Suit;
    for (int32 i = 1; i < 5; ++i) {
        if (FiveCardsToTest[i].Suit != OutFlushSuit) { return false; }
    }
    return true;
}

void UPokerHandEvaluator::CountRanks(const TArray<FCard>& FiveCardsToCount, TMap<ECardRank, int32>& OutRankCountsMap)
{
    OutRankCountsMap.Empty();
    for (const FCard& Card : FiveCardsToCount) {
        OutRankCountsMap.FindOrAdd(Card.Rank)++;
    }
}

void UPokerHandEvaluator::GetKickers(const TArray<FCard>& SortedFiveCardsInput, int32 NumKickersToGet, TArray<ECardRank>& OutKickersArray, const TSet<ECardRank>& RanksToExcludeSet)
{
    // OutKickersArray ПРЕДПОЛАГАЕТСЯ УЖЕ СОДЕРЖИТ ОСНОВНЫЕ КАРТЫ КОМБИНАЦИИ, если они есть (пары, тройки и т.д.)
    // Эта функция ДОБАВЛЯЕТ оставшиеся кикеры.
    int32 CurrentKickersInArray = OutKickersArray.Num(); // Сколько основных карт уже добавлено
    int32 ActualNumKickersToAdd = NumKickersToGet;
    // Для пар/троек/каре, NumKickersToGet - это количество именно КИКЕРОВ.
    // Для флеша/стрита/хайкарда, NumKickersToGet = 5, и мы просто добавляем все 5 карт, если их еще нет.

    for (const FCard& Card : SortedFiveCardsInput) {
        if (!RanksToExcludeSet.Contains(Card.Rank)) { // Карта не входит в основную часть комбинации
            // Проверяем, нет ли уже такого кикера (если основные карты уже были добавлены И это тот же ранг)
            // ИЛИ если мы просто собираем 5 лучших карт для флеша/стрита/хайкарда
            if (!OutKickersArray.Contains(Card.Rank) || RanksToExcludeSet.IsEmpty())
            {
                // Если RanksToExcludeSet пуст, мы просто добавляем NumKickersToGet старших карт
                if (RanksToExcludeSet.IsEmpty() && OutKickersArray.Num() >= ActualNumKickersToAdd) break;
                // Если RanksToExcludeSet НЕ пуст, мы добавляем NumKickersToGet *дополнительных* карт
                if (!RanksToExcludeSet.IsEmpty() && (OutKickersArray.Num() - CurrentKickersInArray) >= ActualNumKickersToAdd) break;


                OutKickersArray.Add(Card.Rank);
            }
        }
    }
    // После добавления всех возможных кикеров, если их больше чем нужно (для флеша/стрита/хайкарда),
    // оставляем только нужное количество старших. OutKickersArray уже должен быть отсортирован по добавлению от старших.
    // Основные карты комбинации (если были) должны быть в начале OutKickersArray.
    // Если это флеш/стрит/хайкард, RanksToExclude пуст, CurrentKickersInArray = 0.
    // Мы просто берем первые NumKickersToGet карт.
    if (OutKickersArray.Num() > (CurrentKickersInArray + ActualNumKickersToAdd) && !RanksToExcludeSet.IsEmpty()) {
        OutKickersArray.RemoveAt(CurrentKickersInArray + ActualNumKickersToAdd, OutKickersArray.Num() - (CurrentKickersInArray + ActualNumKickersToAdd), true);
    }
    else if (OutKickersArray.Num() > ActualNumKickersToAdd && RanksToExcludeSet.IsEmpty()) {
        OutKickersArray.RemoveAt(ActualNumKickersToAdd, OutKickersArray.Num() - ActualNumKickersToAdd, true);
    }
}

int32 UPokerHandEvaluator::CompareHandResults(const FPokerHandResult& HandA, const FPokerHandResult& HandB)
{
    if (HandA.HandRank > HandB.HandRank) return 1;
    if (HandA.HandRank < HandB.HandRank) return -1;

    // Ранги равны, сравниваем кикеры
    // Kickers должны быть отсортированы от старшего к младшему
    // HandA.Kickers[0] - ранг основной комбинации (пара, тройка, старшая карта флеша/стрита)
    // HandA.Kickers[1] - ранг второй пары ИЛИ первый кикер
    // и т.д.
    int32 NumKickersToCompare = FMath::Min(HandA.Kickers.Num(), HandB.Kickers.Num());
    for (int32 i = 0; i < NumKickersToCompare; ++i) {
        if (HandA.Kickers[i] > HandB.Kickers[i]) return 1;
        if (HandA.Kickers[i] < HandB.Kickers[i]) return -1;
    }
    return 0; // Руки полностью равны
}