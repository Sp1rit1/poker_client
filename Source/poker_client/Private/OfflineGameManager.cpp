#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "MyGameInstance.h" // ����� ��� ��������� ����� ������ � ID
#include "Kismet/GameplayStatics.h" // ��� GetGameInstance

// �����������
UOfflineGameManager::UOfflineGameManager()
{
	GameStateData = nullptr;
	Deck = nullptr;
}

// ������������� ����
void UOfflineGameManager::InitializeGame(int32 NumPlayers, int32 NumBots, int64 InitialStack)
{
	UE_LOG(LogTemp, Log, TEXT("UOfflineGameManager::InitializeGame called with NumPlayers=%d, NumBots=%d, InitialStack=%lld"), NumPlayers, NumBots, InitialStack);

	// 1. ������� ������� GameState � Deck
	// ���������� GetOuter() ����� ��������� �� ��������� ���� � GameInstance (������� ������� ���� ����������)
	GameStateData = NewObject<UOfflinePokerGameState>(GetOuter());
	Deck = NewObject<UDeck>(GetOuter());

	if (!GameStateData || !Deck)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create GameState or Deck objects!"));
		return;
	}

	// 2. ���������� ��������� � ��������������/������������ ������
	GameStateData->ResetState();
	Deck->Initialize();
	Deck->Shuffle();

	// 3. ���������� ����� ���������� ���� � ����������
	int32 TotalSeats = NumPlayers + NumBots;
	// ��������, ��� ���� ���������� ��� ���� � �� ������� ����� (��������, ���� 9)
	if (TotalSeats < 2 || TotalSeats > 9)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid number of total seats (%d). Clamping to range [2, 9]."), TotalSeats);
		TotalSeats = FMath::Clamp(TotalSeats, 2, 9);
		// ����� ����������� NumBots, ���� �������� TotalSeats (��������� �����)
		if (TotalSeats < NumPlayers + NumBots)
		{
			NumBots = TotalSeats - NumPlayers;
			if (NumBots < 0) NumBots = 0; // �� ������ ���� NumPlayers > 9
			UE_LOG(LogTemp, Warning, TEXT("Adjusted NumBots to %d"), NumBots);
		}
	}

	// 4. �������� ������ ��������� ������ �� GameInstance
	FString PlayerActualName = TEXT("Player");
	int64 PlayerActualId = -1;
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetOuter()); // �������� GameInstance, ������� ������� ���� ����������
	if (GI)
	{
		// ���������� ���, ������ ���� ����� ��������� ��� ������ ������� ���
		if (GI->bIsLoggedIn || GI->bIsInOfflineMode)
		{
			PlayerActualName = GI->LoggedInUsername.IsEmpty() ? TEXT("Player") : GI->LoggedInUsername;
			PlayerActualId = GI->LoggedInUserId;
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Could not get GameInstance from Outer in OfflineGameManager. Using default player name."));
	}


	// 5. ����������� ������� � �����
	GameStateData->Seats.Reserve(TotalSeats); // ����������� ������
	for (int32 i = 0; i < TotalSeats; ++i)
	{
		bool bIsBotSeat = (i >= NumPlayers); // �������, ��� �������� ������ ���� �������
		FString CurrentName = bIsBotSeat ? FString::Printf(TEXT("Bot %d"), i - NumPlayers + 1) : PlayerActualName;
		int64 CurrentId = bIsBotSeat ? -1 : PlayerActualId;

		FPlayerSeatData Seat(i, CurrentName, CurrentId, InitialStack, bIsBotSeat);
		Seat.Status = EPlayerStatus::Waiting; // ��� �������� � ��������

		GameStateData->Seats.Add(Seat);
	}

	// 6. ������������� ��������� ������ ����
	// ���� �� ������� �����, ������ ������ � ������
	GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // ��� ����� ����� Preflop, ���� ������ ������ �����
	GameStateData->DealerSeat = -1; // ����� ��� �� ��������
	GameStateData->CurrentTurnSeat = -1; // ���� ��� ���
	GameStateData->CurrentBetToCall = 0; // ������ ��� ���

	UE_LOG(LogTemp, Log, TEXT("Offline game initialized. Seats created: %d. Deck shuffled with %d cards left."), GameStateData->Seats.Num(), Deck->NumCardsLeft());
}

// ������ ��� ��������� ����
UOfflinePokerGameState* UOfflineGameManager::GetGameState() const
{
	return GameStateData;
}