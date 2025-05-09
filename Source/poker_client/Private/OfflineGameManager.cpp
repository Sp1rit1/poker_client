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


void UOfflineGameManager::StartNewHand()
{
    if (!GameStateData || !Deck)
    {
        UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - GameStateData or Deck is null!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("UOfflineGameManager::StartNewHand - Preparing for new hand."));

    // 1. Сброс состояния предыдущей руки для стола (карты, банк)
    GameStateData->CommunityCards.Empty();
    GameStateData->Pot = 0;
    GameStateData->CurrentBetToCall = 0;
    // TODO: Сбросить SidePots, если они будут

    int32 NumActivePlayers = 0;
    for (FPlayerSeatData& Seat : GameStateData->Seats)
    {
        // Сбрасываем только то, что точно относится к новой руке,
        // ставки и карты будут позже. Статус пока можно не трогать или поставить Waiting.
        Seat.HoleCards.Empty(); // Карты точно чистим
        Seat.CurrentBet = 0;    // Ставки этой руки чистим
        Seat.bIsTurn = false;
        Seat.bIsSmallBlind = false;
        Seat.bIsBigBlind = false;
        // Seat.Status = EPlayerStatus::Waiting; // Можно установить в ожидание, если нужно

        if (Seat.bIsSittingIn && Seat.Stack > 0)
        {
            NumActivePlayers++;
        }
    }

    if (NumActivePlayers < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("UOfflineGameManager::StartNewHand - Not enough active players (%d) to start a hand."), NumActivePlayers);
        // TODO: Логика завершения игры или ожидания игроков
        return;
    }

    // 2. Определение дилера
    if (GameStateData->DealerSeat != -1 && GameStateData->Seats.IsValidIndex(GameStateData->DealerSeat))
    {
        GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = false; // Убираем старый флаг
    }

    if (GameStateData->DealerSeat == -1) // Первая рука в игре или после сброса
    {
        // Для первой руки дилер выбирается случайно среди активных игроков
        TArray<int32> ActivePlayerIndices;
        for (int32 i = 0; i < GameStateData->Seats.Num(); ++i)
        {
            if (GameStateData->Seats[i].bIsSittingIn && GameStateData->Seats[i].Stack > 0)
            {
                ActivePlayerIndices.Add(i);
            }
        }
        if (ActivePlayerIndices.Num() > 0)
        {
            GameStateData->DealerSeat = ActivePlayerIndices[FMath::RandRange(0, ActivePlayerIndices.Num() - 1)];
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - No active players to choose dealer from!"));
            return; // Не можем выбрать дилера
        }
    }
    else
    {
        // Сдвигаем дилера на следующее активное место по часовой стрелке
        GameStateData->DealerSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat);
    }

    if (GameStateData->DealerSeat == -1) {
        UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - Could not determine dealer seat!"));
        return;
    }
    GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = true;
    UE_LOG(LogTemp, Log, TEXT("New hand. Dealer is Seat %d (%s)"), GameStateData->DealerSeat, *GameStateData->Seats[GameStateData->DealerSeat].PlayerName);

    // 3. Перемешивание колоды
    Deck->Initialize(); // Убедимся, что колода полная перед перемешиванием
    Deck->Shuffle();
    UE_LOG(LogTemp, Log, TEXT("Deck shuffled. %d cards remaining."), Deck->NumCardsLeft());

    // 4. Установка начальной стадии игры
    // Блайнды и раздача карт будут на День 5, поэтому пока можно поставить стадию "ожидания блайндов"
    // или оставить более общую, если процесс будет атомарным.
    // Для Дня 4 мы еще не готовы к Preflop, т.к. нет блайндов и карт.
    GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Или создайте EGameStage::PreparingHand
    GameStateData->CurrentTurnSeat = -1; // Хода еще нет

    // Вывод в лог о готовности к следующему этапу (постановка блайндов и раздача - День 5)
    UE_LOG(LogTemp, Log, TEXT("Hand prepared. Ready for blinds and dealing."));

    // На День 4 RequestPlayerAction() НЕ вызывается, так как еще нет ставок и карт.
}

// Вспомогательная функция: Находит следующее активное место игрока
// (Эта функция остается такой же, как в полном варианте StartNewHand)
int32 UOfflineGameManager::GetNextActivePlayerSeat(int32 StartSeatIndex, bool bIncludeStartSeat) const
{
    if (!GameStateData || GameStateData->Seats.Num() == 0) return -1;

    int32 CurrentIndex = StartSeatIndex;
    if (!bIncludeStartSeat)
    {
        CurrentIndex = (StartSeatIndex + 1) % GameStateData->Seats.Num();
    }

    for (int32 i = 0; i < GameStateData->Seats.Num(); ++i)
    {
        if (GameStateData->Seats.IsValidIndex(CurrentIndex) &&
            GameStateData->Seats[CurrentIndex].bIsSittingIn &&
            GameStateData->Seats[CurrentIndex].Stack > 0)
        {
            return CurrentIndex;
        }
        CurrentIndex = (CurrentIndex + 1) % GameStateData->Seats.Num();
    }
    return -1;
}