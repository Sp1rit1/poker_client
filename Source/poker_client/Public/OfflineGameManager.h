#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "OfflineGameManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnActionRequestedSignature, int32, PlayerSeatIndex, const TArray<EPlayerAction>&, AllowedActions, int64, BetToCall, int64, MinRaiseAmount, int64, PlayerStack);

UCLASS(BlueprintType)
class POKER_CLIENT_API UOfflineGameManager : public UObject // Замените YOURPROJECT_API
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game")
	UOfflinePokerGameState* GameStateData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game")
	UDeck* Deck;

	UPROPERTY(BlueprintAssignable, Category = "Offline Game|Events")
	FOnActionRequestedSignature OnActionRequestedDelegate;

	UOfflineGameManager();

	/**
	 * Инициализирует новую оффлайн игру.
	 * @param NumPlayers Количество реальных игроков (обычно 1).
	 * @param NumBots Количество ботов.
	 * @param InitialStack Начальный стек фишек для всех.
	 * @param InSmallBlindAmount Размер малого блайнда, заданный игроком.
	 */
	UFUNCTION(BlueprintCallable, Category = "Offline Game")
	void InitializeGame(int32 NumPlayers, int32 NumBots, int64 InitialStack, int64 InSmallBlindAmount);

	UFUNCTION(BlueprintPure, Category = "Offline Game")
	UOfflinePokerGameState* GetGameState() const;

	UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow")
	void StartNewHand();

	// Вызывается UI, когда игрок совершает действие
	UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow")
	void ProcessPlayerAction(EPlayerAction PlayerAction, int64 Amount);

	// (RequestPlayerAction остается, но его логика сильно изменится)
	UFUNCTION(BlueprintCallable, Category = "Offline Game|Game Flow")
	void RequestPlayerAction(int32 SeatIndex);

private:
	int32 GetNextActivePlayerSeat(int32 StartSeatIndex, bool bIncludeStartSeat = false) const;
	// PostBlinds больше не будет вызываться напрямую из StartNewHand таким же образом
	// void PostBlinds(int32 SmallBlindSeat, int32 BigBlindSeat, int64 SmallBlindAmount, int64 BigBlindAmount);

	void ProceedToNextGameStage(); // Для перехода между стадиями
	void DealHoleCardsAndStartPreflop(); // Новая функция для раздачи карт после блайндов
	int32 DetermineFirstPlayerToActAfterBlinds() const; // Для определения первого ходящего на префлопе
	int32 DetermineFirstPlayerToActPostFlop() const;  // Для определения первого ходящего на флопе, терне, ривере
};