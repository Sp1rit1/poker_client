#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PokerDataTypes.h" // Убедитесь, что FCard и FPokerHandResult здесь
#include "Containers/Map.h" 
#include "Containers/Set.h"
#include "PokerHandEvaluator.generated.h"

UCLASS()
class POKER_CLIENT_API UPokerHandEvaluator : public UBlueprintFunctionLibrary // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Poker|Evaluation")
    static FPokerHandResult EvaluatePokerHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards);

    UFUNCTION(BlueprintPure, Category = "Poker|Evaluation")
    static int32 CompareHandResults(const FPokerHandResult& HandA, const FPokerHandResult& HandB);

private:
    static FPokerHandResult EvaluateSingleFiveCardHand(const TArray<FCard>& FiveCards);
    static void SortFiveCardsDesc(TArray<FCard>& FiveCardsToSort);
    static bool IsStraight(const TArray<FCard>& SortedFiveCards, ECardRank& OutHighCardRank);
    static bool IsFlush(const TArray<FCard>& FiveCards, ECardSuit& OutFlushSuit);
    static void CountRanks(const TArray<FCard>& FiveCardsToCount, TMap<ECardRank, int32>& OutRankCounts);
    static void GetKickers(const TArray<FCard>& SortedFiveCards, int32 NumKickersToGet, TArray<ECardRank>& OutKickersArray, const TSet<ECardRank>& RanksToExclude);

    // Индексы для комбинаций 7 карт по 5
    static const int32 Combinations7c5[21][5];
    // Индексы для комбинаций 6 карт по 5
    static const int32 Combinations6c5[6][5];
};