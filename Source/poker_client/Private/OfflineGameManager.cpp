#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "MyGameInstance.h" // Если нужен для каких-то глобальных настроек или ID игрока
#include "Kismet/GameplayStatics.h" // Для UE_LOG или других утилит

// Конструктор
UOfflineGameManager::UOfflineGameManager()
{
    GameStateData = nullptr;
    Deck = nullptr;
}

// Инициализация игры
void UOfflineGameManager::InitializeGame(int32 NumRealPlayers, int32 NumBots, int64 InitialStack, int64 InSmallBlindAmount)
{
    UE_LOG(LogTemp, Log, TEXT("UOfflineGameManager::InitializeGame: NumRealPlayers=%d, NumBots=%d, InitialStack=%lld, SB=%lld"),
        NumRealPlayers, NumBots, InitialStack, InSmallBlindAmount);

    // Важно: UObjects (как OfflineGameManager) должны иметь Outer (владельца) при создании.
    // GameInstance является хорошим Outer для менеджеров, которые должны жить всю сессию.
    // Предполагаем, что OfflineGameManager создается с GameInstance в качестве Outer.
    if (!GetOuter())
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeGame: GetOuter() is null! Cannot create UObjects without an outer."));
        return;
    }
    GameStateData = NewObject<UOfflinePokerGameState>(GetOuter());
    Deck = NewObject<UDeck>(GetOuter());

    if (!GameStateData || !Deck)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeGame: Failed to create GameState or Deck objects!"));
        return;
    }

    GameStateData->ResetState(); // Сбрасываем все поля GameStateData

    // Установка размеров блайндов
    if (InSmallBlindAmount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Invalid SmallBlindAmount (%lld) received, defaulting to 5."), InSmallBlindAmount);
        GameStateData->SmallBlindAmount = 5;
    }
    else {
        GameStateData->SmallBlindAmount = InSmallBlindAmount;
    }
    GameStateData->BigBlindAmount = GameStateData->SmallBlindAmount * 2; // BB обычно в два раза больше SB

    Deck->Initialize(); // Инициализируем колоду (создаем 52 карты)

    // Валидация и корректировка количества игроков
    int32 TotalActivePlayers = NumRealPlayers + NumBots;
    const int32 MinPlayers = 2;
    const int32 MaxPlayers = 9; // Типичный максимум для покерного стола

    if (TotalActivePlayers < MinPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Not enough active players (%d). Clamping to %d."), TotalActivePlayers, MinPlayers);
        TotalActivePlayers = MinPlayers;
        // Пересчитываем ботов, если общее число изменилось и реальных игроков меньше
        if (NumRealPlayers < TotalActivePlayers) { NumBots = TotalActivePlayers - NumRealPlayers; }
        else { NumBots = 0; NumRealPlayers = TotalActivePlayers; } // Если реальных игроков достаточно, ботов 0
        if (NumBots < 0) NumBots = 0; // Дополнительная проверка
    }
    if (TotalActivePlayers > MaxPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Too many active players (%d). Clamping to %d."), TotalActivePlayers, MaxPlayers);
        TotalActivePlayers = MaxPlayers;
        NumBots = TotalActivePlayers - NumRealPlayers; // Уменьшаем количество ботов
        if (NumBots < 0) NumBots = 0; // Дополнительная проверка
    }

    // Получаем данные реального игрока из GameInstance
    FString PlayerActualName = TEXT("Player");
    int64 PlayerActualId = -1; // ID для реального игрока
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetOuter()); // Предполагаем, что Outer - это GameInstance
    if (GI)
    {
        if (GI->bIsLoggedIn || GI->bIsInOfflineMode) // Используем данные, если игрок вошел или выбрал оффлайн
        {
            PlayerActualName = GI->LoggedInUsername.IsEmpty() ? TEXT("Player") : GI->LoggedInUsername;
            PlayerActualId = GI->LoggedInUserId;
        }
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Could not get GameInstance from Outer. Using default player name."));
    }

    // Рассаживаем игроков и ботов
    GameStateData->Seats.Empty(); // Очищаем предыдущие места на всякий случай
    GameStateData->Seats.Reserve(TotalActivePlayers);

    for (int32 i = 0; i < TotalActivePlayers; ++i)
    {
        FPlayerSeatData Seat;
        Seat.SeatIndex = i;

        if (i < NumRealPlayers) // Реальные игроки идут первыми
        {
            Seat.PlayerName = PlayerActualName; // Для всех реальных игроков пока одно имя (если NumRealPlayers > 1)
            Seat.PlayerId = PlayerActualId;     // И один ID
            Seat.bIsBot = false;
        }
        else
        {
            Seat.PlayerName = FString::Printf(TEXT("Bot %d"), i - NumRealPlayers + 1);
            Seat.PlayerId = -1; // У ботов нет реального ID пользователя
            Seat.bIsBot = true;
        }

        Seat.Stack = InitialStack;
        Seat.bIsSittingIn = true; // Все начинают в игре
        Seat.Status = EPlayerStatus::Waiting; // Начальный статус

        GameStateData->Seats.Add(Seat);
    }

    // Начальные установки для GameState (перед первой раздачей)
    GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Готовы к StartNewHand
    GameStateData->DealerSeat = -1;          // Дилер еще не назначен
    GameStateData->CurrentTurnSeat = -1;     // Хода еще нет
    GameStateData->CurrentBetToCall = 0;     // Ставок для колла еще нет
    GameStateData->PendingSmallBlindSeat = -1;
    GameStateData->PendingBigBlindSeat = -1;


    UE_LOG(LogTemp, Log, TEXT("Offline game initialized. SB: %lld, BB: %lld. Active Seats in GameState: %d."),
        GameStateData->SmallBlindAmount, GameStateData->BigBlindAmount, GameStateData->Seats.Num());
}

UOfflinePokerGameState* UOfflineGameManager::GetGameState() const
{
    return GameStateData;
}

void UOfflineGameManager::StartNewHand()
{
    if (!GameStateData || !Deck) { UE_LOG(LogTemp, Error, TEXT("StartNewHand - GameStateData or Deck is null!")); return; }

    UE_LOG(LogTemp, Log, TEXT("UOfflineGameManager::StartNewHand - Preparing for new hand."));

    // 1. Сброс состояния стола и игроков от предыдущей руки
    GameStateData->CommunityCards.Empty();
    GameStateData->Pot = 0;
    GameStateData->CurrentBetToCall = 0; // Ставка для колла в начале нового раунда торгов = 0
    int32 NumActivePlayersThisHand = 0;

    for (FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Stack > 0) // Участвует и есть фишки
        {
            Seat.HoleCards.Empty();
            Seat.CurrentBet = 0; // Ставка игрока в этом раунде торгов
            Seat.Status = EPlayerStatus::Waiting; // Игрок ожидает карт/действий
            Seat.bIsTurn = false;
            Seat.bIsSmallBlind = false;
            Seat.bIsBigBlind = false;
            // Seat.bIsDealer флаг будет сброшен и установлен ниже
            NumActivePlayersThisHand++;
        }
        else
        {
            // Если у игрока 0 фишек, но он был bIsSittingIn, он теперь SittingOut
            if (Seat.Stack <= 0 && Seat.bIsSittingIn) {
                Seat.Status = EPlayerStatus::SittingOut;
            }
            // Если игрок был Folded или AllIn, его статус останется таким до полного сброса мест,
            // но для новой руки, если он не SittingOut и имеет фишки, он должен стать Waiting.
            // Логика выше это уже покрывает.
        }
    }

    if (NumActivePlayersThisHand < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartNewHand - Not enough active players (%d) to start a hand."), NumActivePlayersThisHand);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        OnActionRequestedDelegate.Broadcast(-1, {}, 0, 0, 0, GameStateData->Pot); // Уведомляем UI, что действий нет
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("StartNewHand: NumActivePlayers ready for this hand: %d"), NumActivePlayersThisHand);

    // 2. Определение дилера
    // Устанавливаем стадию ПЕРЕД поиском дилера, чтобы GetNextActivePlayerSeat использовал правильные критерии
    // На этом этапе статус игроков 'Waiting' - это нормально для тех, кто будет участвовать.
    GameStateData->CurrentStage = EGameStage::Dealing; // Общая стадия "подготовки руки"

    if (GameStateData->DealerSeat != -1 && GameStateData->Seats.IsValidIndex(GameStateData->DealerSeat))
    {
        GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = false; // Сбрасываем старый флаг
    }

    if (GameStateData->DealerSeat == -1) // Первая рука в игре (или после полного сброса)
    {
        TArray<int32> EligibleDealerIndices;
        for (int32 i = 0; i < GameStateData->Seats.Num(); ++i)
        {
            if (GameStateData->Seats[i].bIsSittingIn && GameStateData->Seats[i].Stack > 0)
                EligibleDealerIndices.Add(i);
        }
        if (EligibleDealerIndices.Num() > 0)
            GameStateData->DealerSeat = EligibleDealerIndices[FMath::RandRange(0, EligibleDealerIndices.Num() - 1)];
        else
        {
            UE_LOG(LogTemp, Error, TEXT("StartNewHand - Critical: No eligible players for initial dealer!")); return;
        }
    }
    else
    {
        // Сдвигаем дилера на следующее активное место
        GameStateData->DealerSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat, false); // false - ищем следующего, не включая текущего
    }

    if (GameStateData->DealerSeat == -1) { UE_LOG(LogTemp, Error, TEXT("StartNewHand - Critical: Could not determine new dealer seat!")); return; }
    GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = true;
    UE_LOG(LogTemp, Log, TEXT("Dealer for this hand is Seat %d (%s)"), GameStateData->DealerSeat, *GameStateData->Seats[GameStateData->DealerSeat].PlayerName);

    // 3. Перемешивание колоды
    Deck->Shuffle(); // Предполагаем, что Deck->Initialize() был вызван в InitializeGame
    UE_LOG(LogTemp, Log, TEXT("Deck shuffled. %d cards remaining."), Deck->NumCardsLeft());

    // 4. Определение позиций для блайндов
    int32 sbSeat = -1;
    int32 bbSeat = -1;

    if (NumActivePlayersThisHand == 2) // Хедз-ап
    {
        sbSeat = GameStateData->DealerSeat;
        bbSeat = GetNextActivePlayerSeat(sbSeat, false); // Следующий активный после дилера
    }
    else // 3+ игроков
    {
        sbSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat, false);
        if (sbSeat != -1) bbSeat = GetNextActivePlayerSeat(sbSeat, false);
    }

    if (sbSeat == -1 || bbSeat == -1)
    {
        UE_LOG(LogTemp, Error, TEXT("StartNewHand - Critical: Could not determine SB (%d) or BB (%d) seat!"), sbSeat, bbSeat);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        OnActionRequestedDelegate.Broadcast(-1, {}, 0, 0, 0, GameStateData->Pot);
        return;
    }

    GameStateData->PendingSmallBlindSeat = sbSeat;
    GameStateData->PendingBigBlindSeat = bbSeat;

    // 5. Переход к стадии ожидания малого блайнда
    GameStateData->CurrentStage = EGameStage::WaitingForSmallBlind;
    if (GameStateData->Seats.IsValidIndex(GameStateData->PendingSmallBlindSeat))
    {
        GameStateData->Seats[GameStateData->PendingSmallBlindSeat].Status = EPlayerStatus::MustPostSmallBlind;
        UE_LOG(LogTemp, Log, TEXT("Hand prepared. Waiting for Small Blind from Seat %d (%s)."), GameStateData->PendingSmallBlindSeat, *GameStateData->Seats[GameStateData->PendingSmallBlindSeat].PlayerName);
        RequestPlayerAction(GameStateData->PendingSmallBlindSeat);
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("StartNewHand - SB Seat index %d is invalid!"), GameStateData->PendingSmallBlindSeat);
        // Обработка ошибки - возможно, пропустить блайнды и перейти к раздаче, если это возможно
    }
}

void UOfflineGameManager::ProcessPlayerAction(EPlayerAction PlayerAction, int64 Amount)
{
    if (!GameStateData || GameStateData->CurrentTurnSeat == -1 || !GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat))
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: Invalid state or CurrentTurnSeat. GameState Valid: %s, TurnSeat: %d"), GameStateData ? TEXT("Yes") : TEXT("No"), GameStateData ? GameStateData->CurrentTurnSeat : -1);
        return;
    }

    int32 ActingPlayerSeat = GameStateData->CurrentTurnSeat;
    FPlayerSeatData& Player = GameStateData->Seats[ActingPlayerSeat];

    UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Seat %d (%s) attempts Action: %s, Amount: %lld. Stage: %s. PlayerStack: %lld, PlayerBet: %lld, BetToCall: %lld"),
        ActingPlayerSeat, *Player.PlayerName, *UEnum::GetValueAsString(PlayerAction), Amount,
        *UEnum::GetValueAsString(GameStateData->CurrentStage), Player.Stack, Player.CurrentBet, GameStateData->CurrentBetToCall);

    // --- Обработка Постановки Блайндов ---
    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind)
    {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeat == GameStateData->PendingSmallBlindSeat)
        {
            int64 ActualSB = FMath::Min(GameStateData->SmallBlindAmount, Player.Stack);
            Player.Stack -= ActualSB;
            Player.CurrentBet = ActualSB; // Эта ставка относится к текущему раунду (префлоп)
            Player.bIsSmallBlind = true;
            Player.Status = EPlayerStatus::Playing; // После постановки блайнда игрок в игре
            GameStateData->Pot += ActualSB;
            UE_LOG(LogTemp, Log, TEXT("SB Posted: %lld by Seat %d. Stack: %lld. Pot: %lld"), ActualSB, ActingPlayerSeat, Player.Stack, GameStateData->Pot);

            // Переход к ожиданию большого блайнда
            GameStateData->CurrentStage = EGameStage::WaitingForBigBlind;
            if (GameStateData->Seats.IsValidIndex(GameStateData->PendingBigBlindSeat))
            {
                GameStateData->Seats[GameStateData->PendingBigBlindSeat].Status = EPlayerStatus::MustPostBigBlind;
                RequestPlayerAction(GameStateData->PendingBigBlindSeat);
            }
            else {
                UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: BB Seat index %d is invalid after SB posted!"), GameStateData->PendingBigBlindSeat);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Invalid action/player for SB. Expected PostBlind from Seat %d. Received %s from %d."),
                GameStateData->PendingSmallBlindSeat, *UEnum::GetValueAsString(PlayerAction), ActingPlayerSeat);
            // Можно повторно запросить действие у SB или обработать как ошибку
            RequestPlayerAction(ActingPlayerSeat); // Повторный запрос у того же игрока
        }
        return; // Завершаем обработку для этой стадии
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind)
    {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeat == GameStateData->PendingBigBlindSeat)
        {
            int64 ActualBB = FMath::Min(GameStateData->BigBlindAmount, Player.Stack);
            Player.Stack -= ActualBB;
            Player.CurrentBet = ActualBB;
            Player.bIsBigBlind = true;
            Player.Status = EPlayerStatus::Playing;
            GameStateData->Pot += ActualBB;
            GameStateData->CurrentBetToCall = GameStateData->BigBlindAmount; // Устанавливаем ставку для колла
            UE_LOG(LogTemp, Log, TEXT("BB Posted: %lld by Seat %d. Stack: %lld. Pot: %lld. BetToCall: %lld"),
                ActualBB, ActingPlayerSeat, Player.Stack, GameStateData->Pot, GameStateData->CurrentBetToCall);

            // После постановки ББ, раздаем карты и начинаем префлоп
            DealHoleCardsAndStartPreflop();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Invalid action/player for BB. Expected PostBlind from Seat %d. Received %s from %d."),
                GameStateData->PendingBigBlindSeat, *UEnum::GetValueAsString(PlayerAction), ActingPlayerSeat);
            RequestPlayerAction(ActingPlayerSeat); // Повторный запрос у того же игрока
        }
        return; // Завершаем обработку для этой стадии
    }
    // --- Конец Обработки Блайндов ---

    // --- Обработка Действий в Раундах Ставок (Preflop, Flop, Turn, River) ---
    // ЭТА ЧАСТЬ БУДЕТ РЕАЛИЗОВАНА НА ДЕНЬ 6
    else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        // TODO: Реализовать логику Fold, Check, Call, Bet, Raise
        // - Обновить стек игрока, Player.CurrentBet, GameStateData.Pot
        // - Обновить GameStateData.CurrentBetToCall, если был Bet или Raise
        // - Обновить Player.Status (Folded, AllIn и т.д.)
        // - Определить, закончился ли раунд ставок (все сделали колл/чек или все кроме одного сфолдили)
        // - Если раунд ставок окончен: вызвать ProceedToNextGameStage()
        // - Если раунд ставок продолжается: найти следующего игрока и вызвать RequestPlayerAction(NextPlayerSeat)

        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Betting round action processing for Action %s NOT YET IMPLEMENTED."), *UEnum::GetValueAsString(PlayerAction));

        // Временная заглушка: просто передаем ход следующему активному игроку
        Player.bIsTurn = false; // Завершаем ход текущего
        int32 NextPlayer = GetNextActivePlayerSeat(ActingPlayerSeat, false);
        if (NextPlayer != -1 && NextPlayer != ActingPlayerSeat) // Проверка, что следующий не тот же и валиден
        {
            // TODO: Нужна проверка на окончание раунда ставок здесь.
            // Если все заколлировали/прочекали, или остался один игрок, то ProceedToNextGameStage()
            // Например, если NextPlayer это тот, кто последним делал агрессивное действие (бет/рейз)
            // или тот, кто начинал раунд (если все чекали).
            RequestPlayerAction(NextPlayer);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("Betting round appears to be over or only one player left (or error in GetNextActivePlayerSeat)."));
            // ProceedToNextGameStage(); // Раскомментировать, когда будет готова
        }
        return;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Action %s received in unhandled game stage %s."),
            *UEnum::GetValueAsString(PlayerAction), *UEnum::GetValueAsString(GameStateData->CurrentStage));
    }
}


void UOfflineGameManager::RequestPlayerAction(int32 SeatIndex)
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(SeatIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Invalid SeatIndex %d or GameStateData is null. Broadcasting empty actions."), SeatIndex);
        OnActionRequestedDelegate.Broadcast(SeatIndex, {}, 0, 0, 0, GameStateData ? GameStateData->Pot : 0);
        return;
    }

    // Сбрасываем флаг bIsTurn у предыдущего игрока (если он был и это не тот же игрок)
    if (GameStateData->CurrentTurnSeat != -1 &&
        GameStateData->CurrentTurnSeat != SeatIndex &&
        GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat))
    {
        GameStateData->Seats[GameStateData->CurrentTurnSeat].bIsTurn = false;
    }

    GameStateData->CurrentTurnSeat = SeatIndex;
    FPlayerSeatData& CurrentPlayer = GameStateData->Seats[SeatIndex];
    CurrentPlayer.bIsTurn = true;

    TArray<EPlayerAction> AllowedActions;
    int64 PlayerStack = CurrentPlayer.Stack;
    int64 PlayerCurrentBetInRound = CurrentPlayer.CurrentBet;
    int64 GameBetToCall = GameStateData->CurrentBetToCall;
    int64 GamePot = GameStateData->Pot;
    int64 CalculatedMinRaiseAmount = GameStateData->BigBlindAmount; // Базовый минимальный бет/рейз

    // Если игрок уже не может действовать (сфолдил, или олл-ин и его ставку уже все уравняли/перекрыли)
    if (CurrentPlayer.Status == EPlayerStatus::Folded ||
        (CurrentPlayer.Stack == 0 && PlayerCurrentBetInRound >= GameBetToCall && GameStateData->CurrentStage >= EGameStage::Preflop))
    {
        UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction: Seat %d (%s) is Folded or All-In and cannot act. Broadcasting empty."), SeatIndex, *CurrentPlayer.PlayerName);
        OnActionRequestedDelegate.Broadcast(SeatIndex, {}, GameBetToCall, 0, PlayerStack, GamePot);
        // ProcessPlayerAction должен будет корректно пропустить ход этого игрока
        return;
    }

    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && SeatIndex == GameStateData->PendingSmallBlindSeat)
    {
        if (PlayerStack > 0) AllowedActions.Add(EPlayerAction::PostBlind);
        GameBetToCall = 0; // Для UI, на момент постановки SB нет "ставки для колла"
        CalculatedMinRaiseAmount = 0;
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && SeatIndex == GameStateData->PendingBigBlindSeat)
    {
        if (PlayerStack > 0) AllowedActions.Add(EPlayerAction::PostBlind);
        GameBetToCall = GameStateData->SmallBlindAmount; // Для UI, показываем, что SB уже есть
        CalculatedMinRaiseAmount = 0;
    }
    else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        if (CurrentPlayer.Status != EPlayerStatus::Playing) { // Должен быть Playing, чтобы делать ставки
            UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Player %d not in 'Playing' status for betting. Status: %s"), SeatIndex, *UEnum::GetValueAsString(CurrentPlayer.Status));
            OnActionRequestedDelegate.Broadcast(SeatIndex, {}, GameBetToCall, 0, PlayerStack, GamePot); return;
        }

        AllowedActions.Add(EPlayerAction::Fold); // Фолд всегда возможен, если есть фишки или не уровненная ставка

        if (PlayerCurrentBetInRound == GameBetToCall) { // Если текущая ставка игрока равна ставке для колла
            AllowedActions.Add(EPlayerAction::Check);
        }

        if (GameBetToCall > PlayerCurrentBetInRound && PlayerStack > 0) { // Если есть что коллировать и есть фишки
            AllowedActions.Add(EPlayerAction::Call);
        }

        // Bet доступен, если можно сделать Check и есть фишки на минимальный бет
        if (AllowedActions.Contains(EPlayerAction::Check) && PlayerStack >= CalculatedMinRaiseAmount) {
            AllowedActions.Add(EPlayerAction::Bet);
        }

        // Raise доступен, если есть предыдущая ставка (GameBetToCall > PlayerCurrentBetInRound)
        // ИЛИ если можно сделать Bet (это будет open-raise),
        // И у игрока достаточно фишек на колл + минимальный рейз.
        int64 AmountToEffectivelyCall = FMath::Max(0LL, GameBetToCall - PlayerCurrentBetInRound);
        if (PlayerStack > AmountToEffectivelyCall) // Если может покрыть колл (или колл 0)
        {
            if (PlayerStack >= AmountToEffectivelyCall + CalculatedMinRaiseAmount) // И хватает на мин. рейз сверху
            {
                if (GameBetToCall > PlayerCurrentBetInRound || AllowedActions.Contains(EPlayerAction::Bet))
                {
                    AllowedActions.Add(EPlayerAction::Raise);
                }
            }
        }
        // TODO: Уточнить логику MinRaiseAmount для разных ситуаций (первый бет, рейз, ре-рейз).
        // Сейчас CalculatedMinRaiseAmount = BB. Для рейза он должен быть равен предыдущему рейзу/бету.
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Unhandled game stage %s for seat %d."), *UEnum::GetValueAsString(GameStateData->CurrentStage), SeatIndex);
    }

    UE_LOG(LogTemp, Log, TEXT("Broadcasting OnActionRequested for Seat %d (%s). Actions Num: %d. Stage: %s. Stack: %lld. Pot: %lld. BetToCall: %lld. MinRaise: %lld"),
        SeatIndex, *CurrentPlayer.PlayerName, AllowedActions.Num(),
        *UEnum::GetValueAsString(GameStateData->CurrentStage), PlayerStack, GamePot, GameBetToCall, CalculatedMinRaiseAmount);

    OnActionRequestedDelegate.Broadcast(SeatIndex, AllowedActions, GameBetToCall, CalculatedMinRaiseAmount, PlayerStack, GamePot);
}


void UOfflineGameManager::DealHoleCardsAndStartPreflop()
{
    if (!GameStateData || !Deck) { UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Null GameStateData or Deck")); return; }
    UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Dealing hole cards..."));

    // Считаем, сколько игроков реально участвуют (статус Playing после блайндов)
    int32 NumPlayersActuallyPlaying = 0;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing) { // Изменили с Seat.Stack > 0 на Status == Playing
            NumPlayersActuallyPlaying++;
        }
    }

    if (NumPlayersActuallyPlaying < 2) {
        UE_LOG(LogTemp, Warning, TEXT("DealHoleCardsAndStartPreflop: Not enough players (%d) with 'Playing' status to deal cards. Ending hand."), NumPlayersActuallyPlaying);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Или другая логика завершения руки
        OnActionRequestedDelegate.Broadcast(-1, {}, 0, 0, 0, GameStateData->Pot);
        return;
    }

    // Определяем, с кого начинать раздачу
    int32 CurrentDealingSeat = GameStateData->PendingSmallBlindSeat; // В большинстве случаев SB получает первую карту
    if (NumPlayersActuallyPlaying == 2) // Хедз-ап, дилер (SB) получает первую карту
    {
        // Проверяем, действительно ли SB = DealerSeat
        if (GameStateData->DealerSeat != GameStateData->PendingSmallBlindSeat) {
            UE_LOG(LogTemp, Warning, TEXT("Heads-up anomality: Dealer is %d, SB is %d. Starting deal with SB."), GameStateData->DealerSeat, GameStateData->PendingSmallBlindSeat);
        }
        CurrentDealingSeat = GameStateData->PendingSmallBlindSeat;
    }
    else { // 3+ игроков, следующий за дилером (SB)
        CurrentDealingSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat, false);
        if (CurrentDealingSeat == -1) CurrentDealingSeat = GameStateData->PendingSmallBlindSeat; // Запасной вариант
    }


    for (int32 CardNum = 0; CardNum < 2; ++CardNum) { // Две карты каждому
        int32 PlayerIndexInDealingOrder = 0;
        int32 SeatToDeal = CurrentDealingSeat;
        while (PlayerIndexInDealingOrder < NumPlayersActuallyPlaying)
        {
            if (SeatToDeal != -1 && GameStateData->Seats.IsValidIndex(SeatToDeal) && GameStateData->Seats[SeatToDeal].Status == EPlayerStatus::Playing)
            {
                TOptional<FCard> DealtCardOptional = Deck->DealCard();
                if (DealtCardOptional.IsSet()) {
                    GameStateData->Seats[SeatToDeal].HoleCards.Add(DealtCardOptional.GetValue());
                }
                else {
                    UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Deck ran out of cards!"));
                    GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
                }
                PlayerIndexInDealingOrder++;
            }
            if (SeatToDeal != -1) SeatToDeal = GetNextActivePlayerSeat(SeatToDeal, false);
            else { UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Could not find next player to deal to.")); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return; }
        }
    }

    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.Status == EPlayerStatus::Playing && Seat.HoleCards.Num() == 2) {
            UE_LOG(LogTemp, Log, TEXT("Seat %d (%s) received: %s, %s"), Seat.SeatIndex, *Seat.PlayerName, *Seat.HoleCards[0].ToString(), *Seat.HoleCards[1].ToString());
        }
    }

    GameStateData->CurrentStage = EGameStage::Preflop;
    int32 FirstToActSeat = DetermineFirstPlayerToActAfterBlinds();
    if (FirstToActSeat == -1) {
        UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Could not determine first to act for Preflop!"));
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
    }

    UE_LOG(LogTemp, Log, TEXT("Preflop round. First to act: Seat %d (%s). CurrentBetToCall: %lld"),
        FirstToActSeat, *GameStateData->Seats[FirstToActSeat].PlayerName, GameStateData->CurrentBetToCall);
    RequestPlayerAction(FirstToActSeat);
}

int32 UOfflineGameManager::DetermineFirstPlayerToActAfterBlinds() const
{
    if (!GameStateData || GameStateData->PendingBigBlindSeat == -1) return -1;

    int32 NumPlayersStillIn = 0;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) { if (Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing) NumPlayersStillIn++; } // Считаем тех, кто Playing

    if (NumPlayersStillIn == 2) {
        // В хедз-апе SB (который также является дилером по нашей логике для 2 игроков) ходит первым на префлопе.
        return GameStateData->PendingSmallBlindSeat;
    }
    else {
        // Обычно это игрок слева от большого блайнда (UTG)
        return GetNextActivePlayerSeat(GameStateData->PendingBigBlindSeat, false);
    }
}

void UOfflineGameManager::ProceedToNextGameStage()
{
    // TODO: Реализовать переход на Flop, Turn, River, Showdown
    // - Раздать общие карты в GameStateData->CommunityCards
    // - Сбросить Player.CurrentBet для всех активных игроков
    // - Установить GameStateData->CurrentBetToCall = 0
    // - Определить первого ходящего (DetermineFirstPlayerToActPostFlop)
    // - Вызвать RequestPlayerAction для него
    UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage: Not yet implemented."));
}

// Вспомогательная функция: Находит следующее активное место игрока
int32 UOfflineGameManager::GetNextActivePlayerSeat(int32 StartSeatIndex, bool bIncludeStartSeat) const
{
    if (!GameStateData || GameStateData->Seats.Num() == 0) { return -1; }
    int32 NumSeats = GameStateData->Seats.Num();
    int32 CurrentIndex = StartSeatIndex;

    if (NumSeats == 0) return -1;

    if (!bIncludeStartSeat)
    {
        CurrentIndex = (StartSeatIndex + 1) % NumSeats;
    }

    for (int32 i = 0; i < NumSeats; ++i) // Проходим не более одного полного круга
    {
        if (GameStateData->Seats.IsValidIndex(CurrentIndex))
        {
            const FPlayerSeatData& Seat = GameStateData->Seats[CurrentIndex];
            bool bIsEligibleToAct = false;

            // Игрок должен быть в игре (bIsSittingIn) и иметь фишки (Stack > 0)
            // И его статус должен позволять ему действовать в текущей стадии
            if (Seat.bIsSittingIn && Seat.Stack > 0)
            {
                if ((GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && Seat.Status == EPlayerStatus::MustPostSmallBlind && CurrentIndex == GameStateData->PendingSmallBlindSeat) ||
                    (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && Seat.Status == EPlayerStatus::MustPostBigBlind && CurrentIndex == GameStateData->PendingBigBlindSeat))
                {
                    bIsEligibleToAct = true; // Для постановки блайнда
                }
                // На стадии Dealing (поиск дилера, SB, BB) или WaitingForPlayers, активным считается тот, у кого статус Waiting
                else if (GameStateData->CurrentStage == EGameStage::Dealing || GameStateData->CurrentStage == EGameStage::WaitingForPlayers)
                {
                    if (Seat.Status == EPlayerStatus::Waiting) bIsEligibleToAct = true;
                }
                // Для раундов ставок (Preflop, Flop, Turn, River)
                else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
                {
                    // Игрок должен быть в статусе Playing (еще не сфолдил, не олл-ин без возможности повлиять)
                    // И (у него есть фишки ИЛИ он олл-ин, но его ставка меньше текущей ставки для колла)
                    if (Seat.Status == EPlayerStatus::Playing && (Seat.Stack > 0 || (Seat.Stack == 0 && Seat.CurrentBet < GameStateData->CurrentBetToCall)))
                    {
                        bIsEligibleToAct = true;
                    }
                }
            }

            if (bIsEligibleToAct)
            {
                return CurrentIndex;
            }
        }
        CurrentIndex = (CurrentIndex + 1) % NumSeats;
    }
    UE_LOG(LogTemp, Warning, TEXT("GetNextActivePlayerSeat: No eligible active player found from Seat %d, Stage: %s"), StartSeatIndex, *UEnum::GetValueAsString(GameStateData->CurrentStage));
    return -1;
}


int32 UOfflineGameManager::DetermineFirstPlayerToActPostFlop() const
{
    if (!GameStateData || GameStateData->DealerSeat == -1) return -1;
    // На постфлопе первым ходит активный игрок слева от дилера
    return GetNextActivePlayerSeat(GameStateData->DealerSeat, false);
}