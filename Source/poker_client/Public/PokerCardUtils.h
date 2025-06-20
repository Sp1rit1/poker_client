﻿#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PokerDataTypes.h" 
#include "PokerCardUtils.generated.h"

UCLASS()
class POKER_CLIENT_API UPokerCardUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Poker|Card Utils", meta = (DisplayName = "To RussianString (Card)", CompactNodeTitle = "->S"))
    static FString Conv_CardToRussianString(const FCard& Card);

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

    UFUNCTION(BlueprintPure, Category = "Poker|Hand Result", meta = (DisplayName = "Hand Rank To Russian String"))
    static FString Conv_PokerHandRankToRussianString(EPokerHandRank HandRankEnum);
};
