#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"
#include "OfflinePokerGameState.generated.h"

UCLASS(BlueprintType)
class POKER_CLIENT_API UOfflinePokerGameState : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    TArray<FPlayerSeatData> Seats;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    TArray<FCard> CommunityCards;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int64 Pot = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int32 DealerSeat = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int32 CurrentTurnSeat = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int32 PlayerWhoOpenedBettingThisRound = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    EGameStage CurrentStage = EGameStage::WaitingForPlayers;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int64 CurrentBetToCall = 0;

    // --- Новые Поля ---
    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int64 SmallBlindAmount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int64 BigBlindAmount = 0;

    // Опционально: отслеживать, кто должен поставить следующий блайнд
    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int32 PendingSmallBlindSeat = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int32 PendingBigBlindSeat = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int64 LastBetOrRaiseAmountInCurrentRound = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
    int32 LastAggressorSeatIndex = -1;



    UOfflinePokerGameState()
    {
        Seats.Reserve(9);
        CommunityCards.Reserve(5);
    }

    void ResetState()
    {
        Seats.Empty();
        CommunityCards.Empty();
        Pot = 0;
        DealerSeat = -1;
        CurrentTurnSeat = -1;
        PlayerWhoOpenedBettingThisRound = -1;
        CurrentStage = EGameStage::WaitingForPlayers;
        CurrentBetToCall = 0;
        SmallBlindAmount = 0;
        BigBlindAmount = 0;
        PendingSmallBlindSeat = -1;
        PendingBigBlindSeat = -1;
    }

    // --- Новые Геттеры (BlueprintCallable) ---
    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    int32 GetNumSeats() const { return Seats.Num(); }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    const TArray<FPlayerSeatData>& GetSeatsArray() const { return Seats; }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    FPlayerSeatData GetSeatData(int32 SeatIndex) const
    {
        if (Seats.IsValidIndex(SeatIndex)) return Seats[SeatIndex];
        return FPlayerSeatData(); // Возвращаем пустую структуру в случае ошибки
    }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    const TArray<FCard>& GetCommunityCardsArray() const { return CommunityCards; }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    int64 GetPotAmount() const { return Pot; }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    int32 GetDealerSeatIndex() const { return DealerSeat; }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    int32 GetCurrentTurnSeatIndex() const { return CurrentTurnSeat; }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    EGameStage GetCurrentGameStage() const { return CurrentStage; }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    int64 GetCurrentBetToCall() const { return CurrentBetToCall; }

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    int64 GetSmallBlindDefineAmount() const { return SmallBlindAmount; } // Определенный SB

    UFUNCTION(BlueprintPure, Category = "Poker Game State|Getters")
    int64 GetBigBlindDefineAmount() const { return BigBlindAmount; }   // Определенный BB
};