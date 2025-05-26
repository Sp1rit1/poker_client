#include "PokerHandEvaluator.h"
#include "PokerDataTypes.h"
#include "Algo/Sort.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Math/UnrealMathUtility.h"
#include "Logging/LogMacros.h"

// Определяем лог категорию для более чистого вывода, если хотите фильтровать
DEFINE_LOG_CATEGORY_STATIC(LogPokerEval, Log, All);

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

static void SortCardsByRankDescendingInternal(TArray<FCard>& Cards)
{
    Algo::Sort(Cards, [](const FCard& A, const FCard& B) {
        return A.Rank > B.Rank;
        });
}

FPokerHandResult UPokerHandEvaluator::EvaluatePokerHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards)
{
    TArray<FCard> AllCardsCombined = HoleCards;
    AllCardsCombined.Append(CommunityCards);

    const int32 NumTotalCards = AllCardsCombined.Num();
    FPokerHandResult BestOverallHandResult;

    FString AllCardsStrForLog;
    for (const FCard& Card : AllCardsCombined) { AllCardsStrForLog += Card.ToString() + TEXT(" "); }
    UE_LOG(LogPokerEval, Log, TEXT("--- EvaluatePokerHand START --- Total Cards: %d. Input Cards: [%s]"), NumTotalCards, *AllCardsStrForLog.TrimEnd());

    if (NumTotalCards < 5)
    {
        UE_LOG(LogPokerEval, Log, TEXT("  Not enough cards (%d) for a 5-card hand. Evaluating as HighCard."), NumTotalCards);
        if (NumTotalCards > 0)
        {
            TArray<FCard> TempSort = AllCardsCombined;
            SortCardsByRankDescendingInternal(TempSort);
            BestOverallHandResult.HandRank = EPokerHandRank::HighCard;
            if (TempSort.IsValidIndex(0)) { BestOverallHandResult.Kickers.Add(TempSort[0].Rank); }
        }
        FString KickersPartialStr; for (ECardRank K : BestOverallHandResult.Kickers) KickersPartialStr += UEnum::GetDisplayValueAsText(K).ToString() + TEXT(" ");
        UE_LOG(LogPokerEval, Log, TEXT("--- EvaluatePokerHand END (Partial <5) --- Returning: %s, Kickers: [%s]"),
            *UEnum::GetDisplayValueAsText(BestOverallHandResult.HandRank).ToString(), *KickersPartialStr.TrimEnd());
        return BestOverallHandResult;
    }

    TArray<FCard> CurrentFiveCardCombination;
    CurrentFiveCardCombination.SetNumUninitialized(5);

    TArray<FCard> SourceCardsForCombination = AllCardsCombined;
    if (NumTotalCards > 7)
    {
        UE_LOG(LogPokerEval, Log, TEXT("  More than 7 cards (%d), sorting and taking top 7."), NumTotalCards);
        SortCardsByRankDescendingInternal(SourceCardsForCombination);
        SourceCardsForCombination.SetNum(7, false);
    }

    const int32 ActualNumCardsToChooseFrom = SourceCardsForCombination.Num();
    FString SourceCardsToChooseFromStr; for (const FCard& C : SourceCardsForCombination) SourceCardsToChooseFromStr += C.ToString() + TEXT(" ");
    UE_LOG(LogPokerEval, Log, TEXT("  Actual cards to choose 5 from (%d): [%s]"), ActualNumCardsToChooseFrom, *SourceCardsToChooseFromStr.TrimEnd());


    if (ActualNumCardsToChooseFrom == 5)
    {
        UE_LOG(LogPokerEval, Log, TEXT("  Path: Exactly 5 cards. Evaluating this single combination."));
        BestOverallHandResult = EvaluateSingleFiveCardHand(SourceCardsForCombination);
    }
    else if (ActualNumCardsToChooseFrom == 6)
    {
        UE_LOG(LogPokerEval, Log, TEXT("  Path: 6 cards. Evaluating 6c5 combinations."));
        for (int32 i = 0; i < 6; ++i)
        {
            for (int32 j = 0; j < 5; ++j) {
                CurrentFiveCardCombination[j] = SourceCardsForCombination[Combinations6c5[i][j]];
            }
            FString ComboStr; for (const FCard& C : CurrentFiveCardCombination) ComboStr += C.ToString() + TEXT(" ");
            UE_LOG(LogPokerEval, Verbose, TEXT("    Evaluating 6c5 Combo #%d: [%s]"), i, *ComboStr.TrimEnd());

            FPokerHandResult CurrentCombinationResult = EvaluateSingleFiveCardHand(CurrentFiveCardCombination);
            if (CompareHandResults(CurrentCombinationResult, BestOverallHandResult) > 0) {
                BestOverallHandResult = CurrentCombinationResult;
                UE_LOG(LogPokerEval, Log, TEXT("      >>>> New Best Hand from 6c5: %s"), *UEnum::GetDisplayValueAsText(BestOverallHandResult.HandRank).ToString());
            }
        }
    }
    else if (ActualNumCardsToChooseFrom == 7)
    {
        UE_LOG(LogPokerEval, Log, TEXT("  Path: 7 cards. Evaluating 7c5 combinations."));
        for (int32 i = 0; i < 21; ++i)
        {
            for (int32 j = 0; j < 5; ++j) {
                CurrentFiveCardCombination[j] = SourceCardsForCombination[Combinations7c5[i][j]];
            }
            FString ComboStr; for (const FCard& C : CurrentFiveCardCombination) ComboStr += C.ToString() + TEXT(" ");
            UE_LOG(LogPokerEval, Verbose, TEXT("    Evaluating 7c5 Combo #%d: [%s]"), i, *ComboStr.TrimEnd());

            FPokerHandResult CurrentCombinationResult = EvaluateSingleFiveCardHand(CurrentFiveCardCombination);
            if (CompareHandResults(CurrentCombinationResult, BestOverallHandResult) > 0) {
                BestOverallHandResult = CurrentCombinationResult;
                UE_LOG(LogPokerEval, Log, TEXT("      >>>> New Best Hand from 7c5: %s"), *UEnum::GetDisplayValueAsText(BestOverallHandResult.HandRank).ToString());
            }
        }
    }
    else {
        UE_LOG(LogPokerEval, Error, TEXT("  Path: Unexpected number of cards to choose from: %d. This should have been caught by <5 check."), ActualNumCardsToChooseFrom);
    }

    FString FinalKickersStr; for (ECardRank K : BestOverallHandResult.Kickers) FinalKickersStr += UEnum::GetDisplayValueAsText(K).ToString() + TEXT(" ");
    UE_LOG(LogPokerEval, Log, TEXT("--- EvaluatePokerHand END --- Final Best Hand: %s, Kickers: [%s]"),
        *UEnum::GetDisplayValueAsText(BestOverallHandResult.HandRank).ToString(), *FinalKickersStr.TrimEnd());
    return BestOverallHandResult;
}

FPokerHandResult UPokerHandEvaluator::EvaluateSingleFiveCardHand(const TArray<FCard>& FiveCards)
{
    if (FiveCards.Num() != 5)
    {
        UE_LOG(LogPokerEval, Error, TEXT("EvaluateSingleFiveCardHand: ERROR - Called with %d cards, expected 5. Returning default HighCard."), FiveCards.Num());
        return FPokerHandResult();
    }

    FPokerHandResult Result;
    TArray<FCard> SortedCards = FiveCards;
    SortCardsByRankDescendingInternal(SortedCards);

    FString CardsBeingEvaluatedStr;
    for (const FCard& c : SortedCards) CardsBeingEvaluatedStr += c.ToString() + TEXT(" ");
    UE_LOG(LogPokerEval, Log, TEXT("  --- EvaluateSingleFiveCardHand ENTRY --- Sorted 5 Cards: [%s]"), *CardsBeingEvaluatedStr.TrimEnd());

    TMap<ECardRank, int32> RankCounts;
    TMap<ECardSuit, int32> SuitCounts;
    for (const FCard& Card : SortedCards)
    {
        RankCounts.FindOrAdd(Card.Rank)++;
        SuitCounts.FindOrAdd(Card.Suit)++;
    }

    FString RankCountsStr, SuitCountsStr;
    for (const auto& Entry : RankCounts) RankCountsStr += FString::Printf(TEXT("%s:%d "), *UEnum::GetDisplayValueAsText(Entry.Key).ToString(), Entry.Value);
    for (const auto& Entry : SuitCounts) SuitCountsStr += FString::Printf(TEXT("%s:%d "), *UEnum::GetDisplayValueAsText(Entry.Key).ToString(), Entry.Value);
    UE_LOG(LogPokerEval, Verbose, TEXT("    RankCounts: {%s}, SuitCounts: {%s}"), *RankCountsStr.TrimEnd(), *SuitCountsStr.TrimEnd());

    bool bIsFlush = false;
    ECardSuit FlushSuit = ECardSuit::Clubs;
    for (const auto& SuitEntry : SuitCounts)
    {
        if (SuitEntry.Value >= 5) { bIsFlush = true; FlushSuit = SuitEntry.Key; break; }
    }

    bool bIsStraight = false;
    ECardRank StraightHighRank = ECardRank::Two;
    if (RankCounts.Num() == 5) // Need 5 distinct ranks for a straight
    {
        if (SortedCards[0].Rank == ECardRank::Ace && SortedCards[1].Rank == ECardRank::Five && SortedCards[2].Rank == ECardRank::Four && SortedCards[3].Rank == ECardRank::Three && SortedCards[4].Rank == ECardRank::Two)
        {
            bIsStraight = true; StraightHighRank = ECardRank::Five; // Wheel
        }
        else
        {
            bool bSequential = true;
            for (int32 i = 0; i < 4; ++i) {
                if (static_cast<int32>(SortedCards[i].Rank) != static_cast<int32>(SortedCards[i + 1].Rank) + 1) {
                    bSequential = false; break;
                }
            }
            if (bSequential) { bIsStraight = true; StraightHighRank = SortedCards[0].Rank; }
        }
    }
    UE_LOG(LogPokerEval, Verbose, TEXT("    Initial Checks - IsStraight: %s (High: %s), IsFlush: %s (Suit: %s)"),
        bIsStraight ? TEXT("Yes") : TEXT("No"),
        bIsStraight ? *UEnum::GetDisplayValueAsText(StraightHighRank).ToString() : TEXT("N/A"),
        bIsFlush ? TEXT("Yes") : TEXT("No"),
        bIsFlush ? *UEnum::GetDisplayValueAsText(FlushSuit).ToString() : TEXT("N/A"));

    // 1. Стрит Флеш / Роял Флеш
    if (bIsStraight && bIsFlush)
    {
        bool bIsActualStraightFlush = true; 
        for (const FCard& Card : SortedCards) 
        {
            if (Card.Suit != FlushSuit) { bIsActualStraightFlush = false; break; }
        }
        if (bIsActualStraightFlush)
        {
            if (StraightHighRank == ECardRank::Ace && SortedCards[0].Rank == ECardRank::Ace) { 
                Result.HandRank = EPokerHandRank::RoyalFlush;
            }
            else {
                Result.HandRank = EPokerHandRank::StraightFlush;
            }
            Result.Kickers.Add(StraightHighRank);
            UE_LOG(LogPokerEval, Log, TEXT("    Detected: %s (High: %s)"), *UEnum::GetDisplayValueAsText(Result.HandRank).ToString(), *UEnum::GetDisplayValueAsText(StraightHighRank).ToString());
            return Result;
        }
    }

    TArray<ECardRank> RanksOfFour; TArray<ECardRank> RanksOfThree; TArray<ECardRank> RanksOfPairs;
    for (const auto& Entry : RankCounts) {
        if (Entry.Value == 4) RanksOfFour.Add(Entry.Key);
        else if (Entry.Value == 3) RanksOfThree.Add(Entry.Key);
        else if (Entry.Value == 2) RanksOfPairs.Add(Entry.Key);
    }
    Algo::Sort(RanksOfThree, [](const ECardRank& A, const ECardRank& B) { return A > B; });
    Algo::Sort(RanksOfPairs, [](const ECardRank& A, const ECardRank& B) { return A > B; });
    UE_LOG(LogPokerEval, Verbose, TEXT("    Grouped Ranks - Fours: %d, Threes: %d, Pairs: %d"), RanksOfFour.Num(), RanksOfThree.Num(), RanksOfPairs.Num());

    // 2. Каре (Four of a Kind)
    if (RanksOfFour.Num() == 1)
    {
        Result.HandRank = EPokerHandRank::FourOfAKind;
        Result.Kickers.Add(RanksOfFour[0]);
        for (const FCard& Card : SortedCards) { if (Card.Rank != RanksOfFour[0]) { Result.Kickers.Add(Card.Rank); break; } }
        UE_LOG(LogPokerEval, Log, TEXT("    Detected: FourOfAKind (%s), Kicker: %s"), *UEnum::GetDisplayValueAsText(Result.Kickers[0]).ToString(), Result.Kickers.Num() > 1 ? *UEnum::GetDisplayValueAsText(Result.Kickers[1]).ToString() : TEXT("N/A"));
        return Result;
    }

    // 3. Фулл Хаус (Full House)
    if (RanksOfThree.Num() == 1 && RanksOfPairs.Num() >= 1) 
    {
        Result.HandRank = EPokerHandRank::FullHouse;
        Result.Kickers.Add(RanksOfThree[0]);
        Result.Kickers.Add(RanksOfPairs[0]); 
        UE_LOG(LogPokerEval, Log, TEXT("    Detected: FullHouse (%s over %s)"), *UEnum::GetDisplayValueAsText(Result.Kickers[0]).ToString(), *UEnum::GetDisplayValueAsText(Result.Kickers[1]).ToString());
        return Result;
    }
    if (RanksOfThree.Num() >= 2) 
    {
        Result.HandRank = EPokerHandRank::FullHouse;
        Result.Kickers.Add(RanksOfThree[0]); 
        Result.Kickers.Add(RanksOfThree[1]); 
        UE_LOG(LogPokerEval, Log, TEXT("    Detected: FullHouse (Two Threes: %s over %s)"), *UEnum::GetDisplayValueAsText(Result.Kickers[0]).ToString(), *UEnum::GetDisplayValueAsText(Result.Kickers[1]).ToString());
        return Result;
    }

    // 4. Флеш (Flush)
    if (bIsFlush)
    {
        Result.HandRank = EPokerHandRank::Flush;
        for (const FCard& Card : SortedCards) {
            if (Card.Suit == FlushSuit) Result.Kickers.Add(Card.Rank);
        } 
        UE_LOG(LogPokerEval, Log, TEXT("    Detected: Flush (Suit: %s, High: %s)"), *UEnum::GetDisplayValueAsText(FlushSuit).ToString(), Result.Kickers.Num() > 0 ? *UEnum::GetDisplayValueAsText(Result.Kickers[0]).ToString() : TEXT("N/A"));
        return Result;
    }

    // 5. Стрит (Straight)
    if (bIsStraight)
    {
        Result.HandRank = EPokerHandRank::Straight;
        Result.Kickers.Add(StraightHighRank);
        UE_LOG(LogPokerEval, Log, TEXT("    Detected: Straight (High: %s)"), *UEnum::GetDisplayValueAsText(StraightHighRank).ToString());
        return Result;
    }

    // 6. Сет/Тройка (Three of a Kind)
    if (RanksOfThree.Num() == 1)
    {
        Result.HandRank = EPokerHandRank::ThreeOfAKind;
        Result.Kickers.Add(RanksOfThree[0]);
        int KickersAdded = 0;
        for (const FCard& Card : SortedCards) {
            if (Card.Rank != RanksOfThree[0]) {
                Result.Kickers.Add(Card.Rank); KickersAdded++;
                if (KickersAdded == 2) break;
            }
        }
        UE_LOG(LogPokerEval, Log, TEXT("    Detected: ThreeOfAKind (%s)"), *UEnum::GetDisplayValueAsText(Result.Kickers[0]).ToString());
        return Result;
    }

    // 7. Две Пары (Two Pair)
    if (RanksOfPairs.Num() >= 2)
    {
        Result.HandRank = EPokerHandRank::TwoPair;
        Result.Kickers.Add(RanksOfPairs[0]);
        Result.Kickers.Add(RanksOfPairs[1]);
        for (const FCard& Card : SortedCards) {
            if (Card.Rank != RanksOfPairs[0] && Card.Rank != RanksOfPairs[1]) {
                Result.Kickers.Add(Card.Rank); break;
            }
        }
        UE_LOG(LogPokerEval, Log, TEXT("    Detected: TwoPair (%s and %s), Kicker: %s"),
            *UEnum::GetDisplayValueAsText(Result.Kickers[0]).ToString(),
            *UEnum::GetDisplayValueAsText(Result.Kickers[1]).ToString(),
            Result.Kickers.Num() > 2 ? *UEnum::GetDisplayValueAsText(Result.Kickers[2]).ToString() : TEXT("N/A"));
        return Result;
    }

    // 8. Одна Пара (One Pair)
    if (RanksOfPairs.Num() == 1)
    {
        Result.HandRank = EPokerHandRank::OnePair;
        Result.Kickers.Add(RanksOfPairs[0]);
        int KickersAdded = 0;
        for (const FCard& Card : SortedCards) {
            if (Card.Rank != RanksOfPairs[0]) {
                Result.Kickers.Add(Card.Rank); KickersAdded++;
                if (KickersAdded == 3) break;
            }
        }
        UE_LOG(LogPokerEval, Log, TEXT("    Detected: OnePair (%s)"), *UEnum::GetDisplayValueAsText(Result.Kickers[0]).ToString());
        return Result;
    }

    // 9. Старшая Карта (High Card)
    Result.HandRank = EPokerHandRank::HighCard;
    for (int i = 0; i < FMath::Min(5, SortedCards.Num()); ++i) { Result.Kickers.Add(SortedCards[i].Rank); }
    UE_LOG(LogPokerEval, Log, TEXT("    Detected: HighCard (Top: %s)"), Result.Kickers.Num() > 0 ? *UEnum::GetDisplayValueAsText(Result.Kickers[0]).ToString() : TEXT("N/A"));
    return Result;
}

int32 UPokerHandEvaluator::CompareHandResults(const FPokerHandResult& HandA, const FPokerHandResult& HandB)
{
    if (HandA.HandRank > HandB.HandRank) return 1;
    if (HandA.HandRank < HandB.HandRank) return -1;

    int32 NumKickersToCompare = FMath::Min(HandA.Kickers.Num(), HandB.Kickers.Num());
    for (int32 i = 0; i < NumKickersToCompare; ++i)
    {
        if (HandA.Kickers[i] > HandB.Kickers[i]) return 1;
        if (HandA.Kickers[i] < HandB.Kickers[i]) return -1;
    }
    return 0;
}