#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "MyGameInstance.h" // Нужен для получения имени игрока и ID
#include "Kismet/GameplayStatics.h" // Для GetGameInstance

// Конструктор
UOfflineGameManager::UOfflineGameManager()
{
	GameStateData = nullptr;
	Deck = nullptr;
}

// Инициализация игры
void UOfflineGameManager::InitializeGame(int32 NumPlayers, int32 NumBots, int64 InitialStack)
{
	UE_LOG(LogTemp, Log, TEXT("UOfflineGameManager::InitializeGame called with NumPlayers=%d, NumBots=%d, InitialStack=%lld"), NumPlayers, NumBots, InitialStack);

	// 1. Создаем объекты GameState и Deck
	// Используем GetOuter() чтобы привязать их жизненный цикл к GameInstance (который владеет этим менеджером)
	GameStateData = NewObject<UOfflinePokerGameState>(GetOuter());
	Deck = NewObject<UDeck>(GetOuter());

	if (!GameStateData || !Deck)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create GameState or Deck objects!"));
		return;
	}

	// 2. Сбрасываем состояние и инициализируем/перемешиваем колоду
	GameStateData->ResetState();
	Deck->Initialize();
	Deck->Shuffle();

	// 3. Определяем общее количество мест и валидируем
	int32 TotalSeats = NumPlayers + NumBots;
	// Убедимся, что мест достаточно для игры и не слишком много (например, макс 9)
	if (TotalSeats < 2 || TotalSeats > 9)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid number of total seats (%d). Clamping to range [2, 9]."), TotalSeats);
		TotalSeats = FMath::Clamp(TotalSeats, 2, 9);
		// Нужно пересчитать NumBots, если изменили TotalSeats (уменьшаем ботов)
		if (TotalSeats < NumPlayers + NumBots)
		{
			NumBots = TotalSeats - NumPlayers;
			if (NumBots < 0) NumBots = 0; // На случай если NumPlayers > 9
			UE_LOG(LogTemp, Warning, TEXT("Adjusted NumBots to %d"), NumBots);
		}
	}

	// 4. Получаем данные реального игрока из GameInstance
	FString PlayerActualName = TEXT("Player");
	int64 PlayerActualId = -1;
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetOuter()); // Получаем GameInstance, который владеет этим менеджером
	if (GI)
	{
		// Используем имя, только если игрок залогинен или выбрал оффлайн сам
		if (GI->bIsLoggedIn || GI->bIsInOfflineMode)
		{
			PlayerActualName = GI->LoggedInUsername.IsEmpty() ? TEXT("Player") : GI->LoggedInUsername;
			PlayerActualId = GI->LoggedInUserId;
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Could not get GameInstance from Outer in OfflineGameManager. Using default player name."));
	}


	// 5. Рассаживаем игроков и ботов
	GameStateData->Seats.Reserve(TotalSeats); // Резервируем память
	for (int32 i = 0; i < TotalSeats; ++i)
	{
		bool bIsBotSeat = (i >= NumPlayers); // Считаем, что реальные игроки идут первыми
		FString CurrentName = bIsBotSeat ? FString::Printf(TEXT("Bot %d"), i - NumPlayers + 1) : PlayerActualName;
		int64 CurrentId = bIsBotSeat ? -1 : PlayerActualId;

		FPlayerSeatData Seat(i, CurrentName, CurrentId, InitialStack, bIsBotSeat);
		Seat.Status = EPlayerStatus::Waiting; // Все начинают в ожидании

		GameStateData->Seats.Add(Seat);
	}

	// 6. Устанавливаем начальную стадию игры
	// Пока не раздаем карты, просто готовы к началу
	GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Или можно сразу Preflop, если готовы начать раунд
	GameStateData->DealerSeat = -1; // Дилер еще не назначен
	GameStateData->CurrentTurnSeat = -1; // Хода еще нет
	GameStateData->CurrentBetToCall = 0; // Ставок еще нет

	UE_LOG(LogTemp, Log, TEXT("Offline game initialized. Seats created: %d. Deck shuffled with %d cards left."), GameStateData->Seats.Num(), Deck->NumCardsLeft());
}

// Геттер для состояния игры
UOfflinePokerGameState* UOfflineGameManager::GetGameState() const
{
	return GameStateData;
}