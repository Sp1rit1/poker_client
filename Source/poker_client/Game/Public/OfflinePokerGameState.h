#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h" // �������� ��� ���� � ������
#include "OfflinePokerGameState.generated.h"

/**
 * ����� UObject, �������� ������ ��������� ������ ����� ��� ������� ����.
 */
UCLASS(BlueprintType)
class POKER_CLIENT_API UOfflinePokerGameState : public UObject // ������ POKER_CLIENT_API �� YOURPROJECT_API
{
	GENERATED_BODY()

public:
	// ������ ������ ��� ������� ����� �� ������ (������� ������ �����)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	TArray<FPlayerSeatData> Seats;

	// ����� ����� �� ����� (����)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	TArray<FCard> CommunityCards;

	// ������� ����� ������ ����� (�������� ���)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	int64 Pot = 0;

	// ������ �����, �� ������� ��������� ������ ������
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	int32 DealerSeat = -1;

	// ������ ����� ������, ��� ������ ���
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	int32 CurrentTurnSeat = -1;

	// ������� ������ ���� (�������, ���� � �.�.)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	EGameStage CurrentStage = EGameStage::WaitingForPlayers;

	// �����, ������� ����� ���������, ����� �������� � ���� (������� ����)
	UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	int64 CurrentBetToCall = 0;

	// TODO: ����� �������� ������ ��� �������� ������ (Side Pots)
	// UPROPERTY(BlueprintReadOnly, Category = "Poker Game State")
	// TArray<FSidePot> SidePots;

	UOfflinePokerGameState() // ����������� �� ���������
	{
		// ����������� ����� ��� ������� ���������� ������� + ����� �����
		Seats.Reserve(9);
		CommunityCards.Reserve(5);
	}

	// (�����������) ����� ��� ������ ��������� � ����������
	void ResetState()
	{
		Seats.Empty();
		CommunityCards.Empty();
		Pot = 0;
		DealerSeat = -1;
		CurrentTurnSeat = -1;
		CurrentStage = EGameStage::WaitingForPlayers;
		CurrentBetToCall = 0;
		// Reset SidePots
	}

};