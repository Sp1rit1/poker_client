#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PokerDataTypes.h" // Убедитесь, что FCard здесь
#include "PokerCardUtils.generated.h"

UCLASS()
class POKER_CLIENT_API UPokerCardUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Poker|Card Utils", meta = (DisplayName = "To String (Card)", CompactNodeTitle = "->S"))
    static FString Conv_CardToString(const FCard& Card);

    UFUNCTION(BlueprintPure, Category = "Poker|Showdown Info", meta = (DisplayName = "Get Seat Index (Showdown)"))
    static int32 GetShowdownPlayerSeatIndex(const FShowdownPlayerInfo& ShowdownInfo);

    UFUNCTION(BlueprintPure, Category = "Poker|Showdown Info", meta = (DisplayName = "Get Player Name (Showdown)"))
    static FString GetShowdownPlayerName(const FShowdownPlayerInfo& ShowdownInfo);

    UFUNCTION(BlueprintPure, Category = "Poker|Showdown Info", meta = (DisplayName = "Get Hole Cards (Showdown)"))
    static TArray<FCard> GetShowdownPlayerHoleCards(const FShowdownPlayerInfo& ShowdownInfo);

    UFUNCTION(BlueprintPure, Category = "Poker|Showdown Info", meta = (DisplayName = "Get Hand Result (Showdown)"))
    static FPokerHandResult GetShowdownPlayerHandResult(const FShowdownPlayerInfo& ShowdownInfo);

    UFUNCTION(BlueprintPure, Category = "Poker|Showdown Info", meta = (DisplayName = "Is Winner (Showdown)"))
    static bool IsShowdownPlayerWinner(const FShowdownPlayerInfo& ShowdownInfo);

    UFUNCTION(BlueprintPure, Category = "Poker|Showdown Info", meta = (DisplayName = "Get Amount Won (Showdown)"))
    static int64 GetShowdownPlayerAmountWon(const FShowdownPlayerInfo& ShowdownInfo);

    UFUNCTION(BlueprintPure, Category = "Poker|Showdown Info", meta = (DisplayName = "Get Player Status (Showdown)"))
    static EPlayerStatus GetShowdownPlayerStatus(const FShowdownPlayerInfo& ShowdownInfo);
    
    // --- Функции для FPokerHandResult ---

    UFUNCTION(BlueprintPure, Category = "Poker|Hand Result", meta = (DisplayName = "Get Hand Rank (Result)"))
    static EPokerHandRank GetHandRankFromResult(const FPokerHandResult& HandResult);

    UFUNCTION(BlueprintPure, Category = "Poker|Hand Result", meta = (DisplayName = "Get Kickers (Result)"))
    static TArray<ECardRank> GetKickersFromResult(const FPokerHandResult& HandResult);

    UFUNCTION(BlueprintPure, Category = "Poker|Showdown Info", meta = (DisplayName = "Get Net Result (Showdown)"))
    static int64 GetShowdownPlayerNetResult(const FShowdownPlayerInfo& ShowdownInfo);

    // Функция для преобразования EPokerHandRank в читаемую строку (может быть полезной)
    UFUNCTION(BlueprintPure, Category = "Poker|Hand Result", meta = (DisplayName = "Hand Rank To String"))
    static FString Conv_PokerHandRankToString(EPokerHandRank HandRankEnum);
};