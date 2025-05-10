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

    UE_LOG(LogTemp, Log, TEXT("UOfflineGameManager::StartNewHand - Starting new hand process..."));

    // 1. Сброс состояния предыдущей руки для стола и игроков
    GameStateData->CommunityCards.Empty();
    GameStateData->Pot = 0;
    GameStateData->CurrentBetToCall = 0;
    // TODO: Сбросить SidePots

    int32 NumActivePlayers = 0;
    for (FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Stack > 0) // Участвует и есть фишки
        {
            Seat.HoleCards.Empty();
            Seat.CurrentBet = 0;
            Seat.Status = EPlayerStatus::Playing; // Игрок активен в раздаче
            Seat.bIsTurn = false;
            Seat.bIsSmallBlind = false;
            Seat.bIsBigBlind = false;
            // Seat.bIsDealer = false; // Флаг дилера сбросим и установим ниже
            NumActivePlayers++;
        }
        else
        {
            // Игроки, которые не участвуют (bIsSittingIn = false) или у кого нет фишек,
            // сохраняют свой статус (например, SittingOut или если были выбиты).
            // Если Stack == 0, можно принудительно поставить SittingOut, если это не часть логики выбывания.
            if (Seat.Stack <= 0) Seat.Status = EPlayerStatus::SittingOut;
        }
    }

    if (NumActivePlayers < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("UOfflineGameManager::StartNewHand - Not enough active players (%d) to start a hand."), NumActivePlayers);
        // TODO: Логика завершения игры, ожидания игроков или уведомления
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Остаемся в ожидании
        // Здесь можно вызвать делегат/событие для UI, чтобы показать сообщение
        return;
    }

    // 2. Определение дилера
    // Сбрасываем старый флаг дилера, если он был
    if (GameStateData->DealerSeat != -1 && GameStateData->Seats.IsValidIndex(GameStateData->DealerSeat))
    {
        GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = false;
    }

    if (GameStateData->DealerSeat == -1) // Первая рука в игре
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
        else // Это не должно произойти, если NumActivePlayers >= 2
        {
            UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - Critical: No active players to choose initial dealer from despite NumActivePlayers >= 2!"));
            return;
        }
    }
    else
    {
        // Сдвигаем дилера на следующее активное место по часовой стрелке
        GameStateData->DealerSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat);
    }

    if (GameStateData->DealerSeat == -1) // Если GetNextActivePlayerSeat не нашел (опять же, не должно быть при NumActivePlayers >= 2)
    {
        UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - Critical: Could not determine new dealer seat!"));
        return;
    }
    GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = true;
    UE_LOG(LogTemp, Log, TEXT("New hand. Dealer is Seat %d (%s)"), GameStateData->DealerSeat, *GameStateData->Seats[GameStateData->DealerSeat].PlayerName);

    // 3. Перемешивание колоды
    Deck->Initialize();
    Deck->Shuffle();
    UE_LOG(LogTemp, Log, TEXT("Deck shuffled. %d cards remaining."), Deck->NumCardsLeft());

    // 4. Определение и сбор блайндов
    // TODO: Сделать размеры блайндов настраиваемыми
    int64 SmallBlindAmount = 5;
    int64 BigBlindAmount = 10;
    GameStateData->CurrentBetToCall = BigBlindAmount; // Начальная ставка для колла - это ББ

    int32 SBSeat = -1;
    int32 BBSeat = -1;

    if (NumActivePlayers == 2) // Хедз-ап: дилер ставит малый блайнд
    {
        SBSeat = GameStateData->DealerSeat;
        BBSeat = GetNextActivePlayerSeat(SBSeat);
    }
    else // 3+ игроков
    {
        SBSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat);
        BBSeat = GetNextActivePlayerSeat(SBSeat);
    }

    if (SBSeat != -1 && BBSeat != -1) {
        PostBlinds(SBSeat, BBSeat, SmallBlindAmount, BigBlindAmount); // Вызываем новую функцию
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - Critical: Could not determine SB or BB seat!"));
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        return;
    }

    // 5. Раздача карманных карт (по 2 каждому активному игроку)
    UE_LOG(LogTemp, Log, TEXT("Dealing hole cards..."));
    int32 CurrentDealingSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat); // Начинаем раздачу слева от дилера (SB)
    if (NumActivePlayers == 2) CurrentDealingSeat = GameStateData->DealerSeat; // В хедз-апе дилер/SB раздает себе первым

    for (int32 CardNum = 0; CardNum < 2; ++CardNum) // Две карты каждому
    {
        for (int32 i = 0; i < NumActivePlayers; ++i)
        {
            if (CurrentDealingSeat != -1 &&
                GameStateData->Seats.IsValidIndex(CurrentDealingSeat) &&
                GameStateData->Seats[CurrentDealingSeat].Status == EPlayerStatus::Playing) // Раздаем только тем, кто в игре
            {
                TOptional<FCard> DealtCardOptional = Deck->DealCard();
                if (DealtCardOptional.IsSet())
                {
                    FCard ActualCard = DealtCardOptional.GetValue();
                    GameStateData->Seats[CurrentDealingSeat].HoleCards.Add(ActualCard);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - Deck ran out of cards during hole card dealing for seat %d! Hand cannot continue."), CurrentDealingSeat);
                    GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
                    return;
                }
            }

            if (CurrentDealingSeat != -1)
            {
                CurrentDealingSeat = GetNextActivePlayerSeat(CurrentDealingSeat);
                if (CurrentDealingSeat == -1 && i < NumActivePlayers - 1 && CardNum * NumActivePlayers + i < NumActivePlayers * 2 - 1)
                {
                    UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - Could not find next active player to deal to while cards still need to be dealt. Aborting hand."));
                    GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
                    return;
                }
            }
            else if (i < NumActivePlayers - 1 && CardNum * NumActivePlayers + i < NumActivePlayers * 2 - 1) {
                UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - CurrentDealingSeat became invalid during dealing. Aborting hand."));
                GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
                return;
            }
        }
    }

    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.Status == EPlayerStatus::Playing && Seat.HoleCards.Num() == 2) {
            UE_LOG(LogTemp, Log, TEXT("Seat %d (%s) received cards: %s, %s"),
                Seat.SeatIndex, *Seat.PlayerName, *Seat.HoleCards[0].ToString(), *Seat.HoleCards[1].ToString());
        }
    }

    // 6. Установка текущей стадии игры
    GameStateData->CurrentStage = EGameStage::Preflop;

    // 7. Определение первого ходящего игрока на префлопе
    int32 FirstToActSeat;
    if (NumActivePlayers == 2)
    {
        FirstToActSeat = GameStateData->DealerSeat; // В хедз-апе дилер/SB ходит первым на префлопе
    }
    else
    {
        FirstToActSeat = GetNextActivePlayerSeat(BBSeat); // Обычно слева от ББ (UTG)
    }
    if (FirstToActSeat == -1) {
        UE_LOG(LogTemp, Error, TEXT("UOfflineGameManager::StartNewHand - Critical: Could not determine first to act seat!"));
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        return;
    }

    GameStateData->CurrentTurnSeat = FirstToActSeat;
    GameStateData->Seats[FirstToActSeat].bIsTurn = true;

    UE_LOG(LogTemp, Log, TEXT("Preflop round. First to act is Seat %d (%s). CurrentBetToCall: %lld"),
        FirstToActSeat, *GameStateData->Seats[FirstToActSeat].PlayerName, GameStateData->CurrentBetToCall);

    // 8. Запрос действия у первого игрока (Функция RequestPlayerAction будет реализована на День 5)
    RequestPlayerAction(FirstToActSeat);
}


// Вспомогательная функция: Находит следующее активное место игрока
// (Эта функция остается такой же, как вы предоставили)
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
        // Добавлена проверка IsValidIndex для CurrentIndex
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


// --- НОВАЯ Вспомогательная функция для Постановки Блайндов ---
void UOfflineGameManager::PostBlinds(int32 SmallBlindSeat, int32 BigBlindSeat, int64 SmallBlindAmount, int64 BigBlindAmount)
{
    if (!GameStateData)
    {
        UE_LOG(LogTemp, Error, TEXT("PostBlinds: GameStateData is null!"));
        return;
    }

    // Малый блайнд
    if (GameStateData->Seats.IsValidIndex(SmallBlindSeat))
    {
        FPlayerSeatData& SBPlayer = GameStateData->Seats[SmallBlindSeat];
        if (SBPlayer.Status == EPlayerStatus::Playing) // Ставим блайнд, только если игрок активен
        {
            int64 ActualSB = FMath::Min(SmallBlindAmount, SBPlayer.Stack); // Не может поставить больше, чем есть
            SBPlayer.Stack -= ActualSB;
            SBPlayer.CurrentBet = ActualSB; // Сумма, поставленная в текущем раунде
            SBPlayer.bIsSmallBlind = true;
            GameStateData->Pot += ActualSB;
            UE_LOG(LogTemp, Log, TEXT("Seat %d (%s) posts Small Blind: %lld. Stack left: %lld"), SBPlayer.SeatIndex, *SBPlayer.PlayerName, ActualSB, SBPlayer.Stack);
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Seat %d (%s) is SB but not in Playing status, skipping blind."), SBPlayer.SeatIndex, *SBPlayer.PlayerName);
        }
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("PostBlinds: Invalid SmallBlindSeat index: %d"), SmallBlindSeat);
    }

    // Большой блайнд
    if (GameStateData->Seats.IsValidIndex(BigBlindSeat))
    {
        FPlayerSeatData& BBPlayer = GameStateData->Seats[BigBlindSeat];
        if (BBPlayer.Status == EPlayerStatus::Playing)
        {
            int64 ActualBB = FMath::Min(BigBlindAmount, BBPlayer.Stack);
            BBPlayer.Stack -= ActualBB;
            BBPlayer.CurrentBet = ActualBB;
            BBPlayer.bIsBigBlind = true;
            GameStateData->Pot += ActualBB;
            UE_LOG(LogTemp, Log, TEXT("Seat %d (%s) posts Big Blind: %lld. Stack left: %lld"), BBPlayer.SeatIndex, *BBPlayer.PlayerName, ActualBB, BBPlayer.Stack);
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Seat %d (%s) is BB but not in Playing status, skipping blind."), BBPlayer.SeatIndex, *BBPlayer.PlayerName);
        }
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("PostBlinds: Invalid BigBlindSeat index: %d"), BigBlindSeat);
    }

    // Устанавливаем CurrentBetToCall. Если кто-то из блайндов пошел олл-ин на меньшую сумму,
    // это будет скорректировано позже при обработке их "неявного" действия.
    // Для начала этого раунда CurrentBetToCall - это сумма ББ.
    GameStateData->CurrentBetToCall = BigBlindAmount;
}

void UOfflineGameManager::RequestPlayerAction(int32 SeatIndex)
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(SeatIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Invalid SeatIndex %d or GameStateData is null"), SeatIndex);
        // TODO: Возможно, нужно обработать ситуацию, когда не можем запросить действие (например, конец игры)
        return;
    }

    FPlayerSeatData& CurrentPlayer = GameStateData->Seats[SeatIndex];

    // Сначала сбрасываем флаг bIsTurn у предыдущего игрока, если он был
    if (GameStateData->CurrentTurnSeat != -1 && GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat))
    {
        GameStateData->Seats[GameStateData->CurrentTurnSeat].bIsTurn = false;
    }

    // Устанавливаем текущего игрока
    GameStateData->CurrentTurnSeat = SeatIndex;
    CurrentPlayer.bIsTurn = true;

    // Проверяем, может ли игрок вообще действовать (не в фолде, не олл-ин без возможности повлиять на банк)
    if (CurrentPlayer.Status == EPlayerStatus::Folded || CurrentPlayer.Stack == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction: Seat %d (%s) is Folded or All-In with 0 stack. Skipping turn."), SeatIndex, *CurrentPlayer.PlayerName);
        // TODO: Логика автоматического перехода хода (вызов ProcessPlayerAction с "авто-действием" или поиск следующего)
        // Пока просто ничего не делаем, это будет обработано в ProcessPlayerAction
        // Для простоты сейчас мы вызовем делегат с пустым набором действий или только Fold.
        // Но лучше это обрабатывать в цикле определения следующего игрока в ProcessPlayerAction.
        // Сейчас, для MVP, если он не может ходить, UI ничего не покажет.
        // ProcessPlayerAction должен будет это корректно обработать.
        // Пока что вызовем делегат с пустыми действиями, чтобы UI мог отреагировать.
        OnActionRequestedDelegate.Broadcast(SeatIndex, {}, GameStateData->CurrentBetToCall, 0, CurrentPlayer.Stack);
        return;
    }


    TArray<EPlayerAction> AllowedActions;
    int64 PlayerStack = CurrentPlayer.Stack;
    int64 CurrentBetOnTable = CurrentPlayer.CurrentBet; // Ставка игрока в этом раунде
    int64 BetToCall = GameStateData->CurrentBetToCall;
    int64 PotSize = GameStateData->Pot; // Для потенциальных ставок по размеру пота

    // 1. Fold - всегда доступен, если игрок не олл-ин
    AllowedActions.Add(EPlayerAction::Fold);

    // 2. Check - доступен, если текущая ставка для колла равна ставке игрока в этом раунде
    // (т.е. никто до него не повышал в этом круге торгов или он уже уравнял предыдущий рейз)
    if (CurrentBetOnTable == BetToCall)
    {
        AllowedActions.Add(EPlayerAction::Check);
    }

    // 3. Call - доступен, если ставка для колла больше текущей ставки игрока и у игрока есть фишки
    if (BetToCall > CurrentBetOnTable && PlayerStack > 0)
    {
        AllowedActions.Add(EPlayerAction::Call);
        // Сумма для колла = BetToCall - CurrentBetOnTable, но не больше стека игрока
        // int64 AmountToCall = FMath::Min(PlayerStack, BetToCall - CurrentBetOnTable);
    }

    // 4. Bet - доступен, если можно сделать Check (т.е. BetToCall == CurrentBetOnTable) и есть фишки
    // Bet также означает, что до этого не было ставок в текущем круге торгов (или все уравняли и прочекали)
    int64 MinBetAmount = 10; // TODO: Заменить на реальный BigBlindAmount или другой минимум
    if (AllowedActions.Contains(EPlayerAction::Check) && PlayerStack >= MinBetAmount)
    {
        AllowedActions.Add(EPlayerAction::Bet);
    }

    // 5. Raise - доступен, если можно сделать Call (BetToCall > CurrentBetOnTable) ИЛИ если уже была ставка (Bet)
    // и у игрока достаточно фишек для минимального рейза.
    // Минимальный рейз обычно равен размеру последнего бета/рейза.
    // Если перед нами был только колл или чек, то MinRaise = MinBetAmount (BigBlind)
    // Если перед нами был бет/рейз, то MinRaise = этот бет/рейз.
    // Сумма для рейза = (BetToCall - CurrentBetOnTable) + MinRaiseAmount (но не больше стека)

    // TODO: Более точный расчет MinRaiseAmount, учитывающий предыдущие рейзы в этом раунде.
    // Пока упрощенно: минимальный рейз = удвоение ставки для колла или просто BigBlind, если нет ставок.
    int64 MinRaiseAmount = BetToCall > 0 ? BetToCall : MinBetAmount; // Очень упрощенно!
    if (BetToCall > CurrentBetOnTable) // Если есть что коллировать, значит был бет/рейз
    {
        // Минимальный рейз должен быть как минимум на сумму предыдущего бета/рейза.
        // Если предыдущий бет был Х, то CurrentBetToCall = Х.
        // Игрок уже поставил CurrentBetOnTable. Ему нужно доставить (X - CurrentBetOnTable).
        // И затем еще сделать рейз как минимум на (X - предыдущая_ставка_до_бета_X).
        // Это сложная часть, для MVP можно упростить до "рейз хотя бы на ББ сверх суммы колла".
        // MinRaiseAmount = GameStateData->LastRaiseAmount > 0 ? GameStateData->LastRaiseAmount : MinBetAmount;
        // Пока просто поставим фиксированное значение или на основе BigBlind
        MinRaiseAmount = MinBetAmount; // Для простоты MVP, минимальный рейз - это ББ
    }


    // Игрок может сделать рейз, если у него достаточно фишек, чтобы:
    // 1. Заколлировать текущую ставку (BetToCall - CurrentBetOnTable)
    // 2. И затем добавить сверху хотя бы MinRaiseAmount
    int64 TotalCostForMinRaise = (BetToCall - CurrentBetOnTable) + MinRaiseAmount;
    if (PlayerStack >= TotalCostForMinRaise && MinRaiseAmount > 0) // MinRaiseAmount должен быть > 0
    {
        AllowedActions.Add(EPlayerAction::Raise);
    }
    else if (PlayerStack > 0 && BetToCall > CurrentBetOnTable && PlayerStack < TotalCostForMinRaise)
    {
        // Если игрок не может сделать минимальный рейз, но может пойти олл-ин,
        // и этот олл-ин больше текущего колла, то это тоже может считаться рейзом (All-In Raise).
        // Для MVP это можно опустить или добавить EPlayerAction::AllIn как отдельное.
        // Пока просто не добавляем Raise, если нет на MinRaise.
    }


    // Дополнительно: AllIn - может быть доступен, если у игрока есть фишки
    // Можно обрабатывать AllIn как специальный случай Bet/Call/Raise.
    // Или добавить как отдельное действие. Пока не добавляем явно, т.к. Bet(stack), Call(stack), Raise(stack) покроют это.

    UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction for Seat %d (%s). Stack: %lld. BetToCall: %lld. MinRaise: %lld"),
        SeatIndex, *CurrentPlayer.PlayerName, PlayerStack, BetToCall, MinRaiseAmount);

    // Вызываем делегат, передавая всю необходимую информацию
    OnActionRequestedDelegate.Broadcast(SeatIndex, AllowedActions, BetToCall, MinRaiseAmount, PlayerStack);
}