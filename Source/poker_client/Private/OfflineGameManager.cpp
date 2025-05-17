#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "MyGameInstance.h" 
#include "Kismet/GameplayStatics.h"

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

    GameStateData->ResetState();

    if (InSmallBlindAmount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Invalid SmallBlindAmount (%lld) received, defaulting to 5."), InSmallBlindAmount);
        GameStateData->SmallBlindAmount = 5;
    }
    else {
        GameStateData->SmallBlindAmount = InSmallBlindAmount;
    }
    GameStateData->BigBlindAmount = GameStateData->SmallBlindAmount * 2;

    Deck->Initialize();

    int32 TotalActivePlayers = NumRealPlayers + NumBots;
    const int32 MinPlayers = 2;
    const int32 MaxPlayers = 9;

    if (TotalActivePlayers < MinPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Not enough active players (%d). Clamping to %d."), TotalActivePlayers, MinPlayers);
        TotalActivePlayers = MinPlayers;
        if (NumRealPlayers < TotalActivePlayers) { NumBots = TotalActivePlayers - NumRealPlayers; }
        else { NumBots = 0; NumRealPlayers = TotalActivePlayers; }
        if (NumBots < 0) NumBots = 0;
    }
    if (TotalActivePlayers > MaxPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Too many active players (%d). Clamping to %d."), TotalActivePlayers, MaxPlayers);
        TotalActivePlayers = MaxPlayers;
        NumBots = TotalActivePlayers - NumRealPlayers;
        if (NumBots < 0) NumBots = 0;
    }

    FString PlayerActualName = TEXT("Player");
    int64 PlayerActualId = -1;
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetOuter());
    if (GI)
    {
        if (GI->bIsLoggedIn || GI->bIsInOfflineMode)
        {
            PlayerActualName = GI->LoggedInUsername.IsEmpty() ? TEXT("Player") : GI->LoggedInUsername;
            PlayerActualId = GI->LoggedInUserId;
        }
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Could not get GameInstance from Outer. Using default player name."));
    }

    GameStateData->Seats.Empty();
    GameStateData->Seats.Reserve(TotalActivePlayers);

    for (int32 i = 0; i < TotalActivePlayers; ++i)
    {
        FPlayerSeatData Seat;
        Seat.SeatIndex = i;

        if (i < NumRealPlayers)
        {
            Seat.PlayerName = PlayerActualName;
            Seat.PlayerId = PlayerActualId;
            Seat.bIsBot = false;
        }
        else
        {
            Seat.PlayerName = FString::Printf(TEXT("Bot %d"), i - NumRealPlayers + 1);
            Seat.PlayerId = -1;
            Seat.bIsBot = true;
        }

        Seat.Stack = InitialStack;
        Seat.bIsSittingIn = true;
        Seat.Status = EPlayerStatus::Waiting;

        GameStateData->Seats.Add(Seat);
    }

    GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
    GameStateData->DealerSeat = -1;
    GameStateData->CurrentTurnSeat = -1;
    GameStateData->CurrentBetToCall = 0;
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

    UE_LOG(LogTemp, Log, TEXT("StartNewHand: Preparing for new hand."));

    GameStateData->CommunityCards.Empty();
    GameStateData->Pot = 0;
    GameStateData->CurrentBetToCall = 0;
    int32 NumActivePlayers = 0;

    for (FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Stack > 0)
        {
            Seat.HoleCards.Empty();
            Seat.CurrentBet = 0;
            Seat.Status = EPlayerStatus::Waiting; // <-- ВАЖНО: Устанавливаем Waiting
            Seat.bIsTurn = false;
            Seat.bIsSmallBlind = false;
            Seat.bIsBigBlind = false;
            NumActivePlayers++;
        }
        else { if (Seat.Stack <= 0 && Seat.bIsSittingIn) Seat.Status = EPlayerStatus::SittingOut; }
    }

    if (NumActivePlayers < 2) { UE_LOG(LogTemp, Warning, TEXT("StartNewHand - Not enough active players (%d)."), NumActivePlayers); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; OnActionRequestedDelegate.Broadcast(-1, {}, 0, 0, 0); return; }
    UE_LOG(LogTemp, Log, TEXT("StartNewHand: NumActivePlayers ready: %d"), NumActivePlayers);

    // Устанавливаем стадию ПЕРЕД поиском дилера, чтобы GetNextActivePlayerSeat использовал правильные критерии
    GameStateData->CurrentStage = EGameStage::Dealing; // Стадия "подготовки руки", когда ищем дилера/блайнды

    if (GameStateData->DealerSeat != -1 && GameStateData->Seats.IsValidIndex(GameStateData->DealerSeat)) { GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = false; }

    if (GameStateData->DealerSeat == -1)
    {
        TArray<int32> ActivePlayerIndices;
        for (int32 i = 0; i < GameStateData->Seats.Num(); ++i) { if (GameStateData->Seats[i].bIsSittingIn && GameStateData->Seats[i].Stack > 0) ActivePlayerIndices.Add(i); }
        if (ActivePlayerIndices.Num() > 0) GameStateData->DealerSeat = ActivePlayerIndices[FMath::RandRange(0, ActivePlayerIndices.Num() - 1)];
        else { UE_LOG(LogTemp, Error, TEXT("StartNewHand - No active players for initial dealer!")); return; }
    }
    else { GameStateData->DealerSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat); }

    if (GameStateData->DealerSeat == -1) { UE_LOG(LogTemp, Error, TEXT("StartNewHand - Could not determine new dealer seat!")); return; }
    GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = true;
    UE_LOG(LogTemp, Log, TEXT("Dealer is Seat %d (%s)"), GameStateData->DealerSeat, *GameStateData->Seats[GameStateData->DealerSeat].PlayerName);

    Deck->Shuffle();

    int32 sbSeat = -1;
    int32 bbSeat = -1;

    if (NumActivePlayers == 2) {
        sbSeat = GameStateData->DealerSeat;
        bbSeat = GetNextActivePlayerSeat(sbSeat);
    }
    else {
        sbSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat);
        bbSeat = GetNextActivePlayerSeat(sbSeat);
    }

    if (sbSeat == -1 || bbSeat == -1) { UE_LOG(LogTemp, Error, TEXT("StartNewHand - Could not determine SB or BB seat! SB: %d, BB: %d"), sbSeat, bbSeat); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return; }

    GameStateData->PendingSmallBlindSeat = sbSeat;
    GameStateData->PendingBigBlindSeat = bbSeat;

    GameStateData->CurrentStage = EGameStage::WaitingForSmallBlind;
    GameStateData->Seats[GameStateData->PendingSmallBlindSeat].Status = EPlayerStatus::MustPostSmallBlind;
    RequestPlayerAction(GameStateData->PendingSmallBlindSeat);
    UE_LOG(LogTemp, Log, TEXT("Hand prepared. Waiting for Small Blind from Seat %d (%s)."), GameStateData->PendingSmallBlindSeat, *GameStateData->Seats[GameStateData->PendingSmallBlindSeat].PlayerName);
}

void UOfflineGameManager::ProcessPlayerAction(EPlayerAction PlayerAction, int64 Amount)
{
    if (!GameStateData || GameStateData->CurrentTurnSeat == -1 || !GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat))
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: Invalid state.")); return;
    }

    int32 ActingPlayerSeat = GameStateData->CurrentTurnSeat;
    FPlayerSeatData& Player = GameStateData->Seats[ActingPlayerSeat];

    UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Seat %d (%s) Action: %s Amount: %lld Stage: %s"),
        ActingPlayerSeat, *Player.PlayerName, *UEnum::GetValueAsString(PlayerAction), Amount, *UEnum::GetValueAsString(GameStateData->CurrentStage));

    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind)
    {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeat == GameStateData->PendingSmallBlindSeat)
        {
            int64 ActualSB = FMath::Min(GameStateData->SmallBlindAmount, Player.Stack); // Player.CurrentBet здесь всегда 0
            Player.Stack -= ActualSB;
            Player.CurrentBet = ActualSB;
            Player.bIsSmallBlind = true;
            Player.Status = EPlayerStatus::Playing;
            GameStateData->Pot += ActualSB;
            UE_LOG(LogTemp, Log, TEXT("SB Posted: %lld by Seat %d. Stack: %lld. Pot: %lld"), ActualSB, ActingPlayerSeat, Player.Stack, GameStateData->Pot);

            GameStateData->CurrentStage = EGameStage::WaitingForBigBlind;
            GameStateData->Seats[GameStateData->PendingBigBlindSeat].Status = EPlayerStatus::MustPostBigBlind;
            RequestPlayerAction(GameStateData->PendingBigBlindSeat);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("Invalid action for SB. Expected PostBlind from Seat %d."), GameStateData->PendingSmallBlindSeat); }
        return;
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind)
    {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeat == GameStateData->PendingBigBlindSeat)
        {
            int64 ActualBB = FMath::Min(GameStateData->BigBlindAmount, Player.Stack); // Player.CurrentBet здесь всегда 0
            Player.Stack -= ActualBB;
            Player.CurrentBet = ActualBB;
            Player.bIsBigBlind = true;
            Player.Status = EPlayerStatus::Playing;
            GameStateData->Pot += ActualBB;
            GameStateData->CurrentBetToCall = GameStateData->BigBlindAmount;
            UE_LOG(LogTemp, Log, TEXT("BB Posted: %lld by Seat %d. Stack: %lld. Pot: %lld. BetToCall: %lld"),
                ActualBB, ActingPlayerSeat, Player.Stack, GameStateData->Pot, GameStateData->CurrentBetToCall);
            DealHoleCardsAndStartPreflop();
        }
        else { UE_LOG(LogTemp, Warning, TEXT("Invalid action for BB. Expected PostBlind from Seat %d."), GameStateData->PendingBigBlindSeat); }
        return;
    }
    else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Betting round action processing NOT YET IMPLEMENTED."));
        Player.bIsTurn = false;
        int32 NextPlayer = GetNextActivePlayerSeat(ActingPlayerSeat);
        if (NextPlayer != -1 && NextPlayer != ActingPlayerSeat) RequestPlayerAction(NextPlayer);
        else { UE_LOG(LogTemp, Log, TEXT("Betting round over or one player left.")); /* ProceedToNextGameStage(); */ }
        return;
    }
}

// OfflineGameManager.cpp

void UOfflineGameManager::RequestPlayerAction(int32 SeatIndex)
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(SeatIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Invalid SeatIndex %d or GameStateData is null. Broadcasting empty actions."), SeatIndex);
        // Отправляем пустые действия, чтобы UI мог это обработать (например, ничего не показывать)
        OnActionRequestedDelegate.Broadcast(SeatIndex, {}, 0, 0, 0);
        return;
    }

    // Сбрасываем флаг bIsTurn у предыдущего игрока, если он был и это не тот же игрок
    if (GameStateData->CurrentTurnSeat != -1 &&
        GameStateData->CurrentTurnSeat != SeatIndex &&
        GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat))
    {
        GameStateData->Seats[GameStateData->CurrentTurnSeat].bIsTurn = false;
    }

    // Устанавливаем текущего игрока
    GameStateData->CurrentTurnSeat = SeatIndex;
    FPlayerSeatData& CurrentPlayer = GameStateData->Seats[SeatIndex];
    CurrentPlayer.bIsTurn = true;

    TArray<EPlayerAction> AllowedActions;
    int64 PlayerStack = CurrentPlayer.Stack;
    int64 CurrentBetOnTableForPlayer = CurrentPlayer.CurrentBet; // Сколько игрок уже поставил в ЭТОМ раунде торгов
    int64 GameBetToCall = GameStateData->CurrentBetToCall;       // Общая сумма, до которой нужно доставить, чтобы остаться в игре

    // Минимальный бет или рейз обычно равен размеру большого блайнда (для начала)
    // TODO: Этот CalculatedMinRaiseAmount нужно будет улучшить для учета предыдущих рейзов в раунде.
    // Пока что для MVP и первого хода на префлопе, BB - это разумный минимум для бета или рейза.
    int64 CalculatedMinRaiseAmount = GameStateData->BigBlindAmount;

    // Если игрок уже сфолдил или он олл-ин и не может больше влиять на банк (его ставка равна или больше BetToCall)
    // (Для стадии блайндов эта проверка не нужна, т.к. там свой статус)
    if (CurrentPlayer.Status == EPlayerStatus::Folded ||
        (CurrentPlayer.Stack == 0 && CurrentPlayer.CurrentBet >= GameBetToCall && GameStateData->CurrentStage >= EGameStage::Preflop))
    {
        UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction: Seat %d (%s) is Folded or All-In and cannot act further. Broadcasting empty actions."), SeatIndex, *CurrentPlayer.PlayerName);
        OnActionRequestedDelegate.Broadcast(SeatIndex, {}, GameBetToCall, 0, PlayerStack);
        // ProcessPlayerAction должен будет обработать этот "пропуск" и автоматически передать ход дальше.
        return;
    }

    // --- Определение доступных действий в зависимости от стадии игры ---

    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && SeatIndex == GameStateData->PendingSmallBlindSeat)
    {
        if (PlayerStack > 0) // Может ли игрок вообще поставить блайнд
        {
            AllowedActions.Add(EPlayerAction::PostBlind);
        }
        // Если SB не может поставить (0 стек), игра не должна была дойти сюда или должна быть спец. логика в StartNewHand.
        // Сейчас, если он не может, делегат будет вызван с пустым AllowedActions. ProcessPlayerAction должен будет это обработать.
        GameBetToCall = 0; // На момент постановки SB, нет "ставки для колла" с точки зрения опций
        CalculatedMinRaiseAmount = 0; // И рейза тоже нет
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && SeatIndex == GameStateData->PendingBigBlindSeat)
    {
        if (PlayerStack > 0)
        {
            AllowedActions.Add(EPlayerAction::PostBlind);
        }
        // GameBetToCall здесь будет равен сумме малого блайнда, если мы хотим, чтобы BB "коллировал" SB перед тем как поставить свой BB.
        // Но по нашей логике, он просто ставит свой BB.
        // Для UI, BetToCall можно установить в SmallBlindAmount, чтобы показать, что SB уже есть.
        // Но т.к. действие только одно (PostBlind), это не так критично.
        GameBetToCall = GameStateData->SmallBlindAmount; // Для информации, что SB уже в банке
        CalculatedMinRaiseAmount = 0;
    }
    else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        // Игрок не может быть в статусе MustPost...Blind на этих стадиях, он должен быть Playing
        if (CurrentPlayer.Status != EPlayerStatus::Playing) {
            UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Player %d is not in 'Playing' status for betting round. Status: %s"), SeatIndex, *UEnum::GetValueAsString(CurrentPlayer.Status));
            // Отправляем пустые действия, ProcessPlayerAction должен пропустить ход
            OnActionRequestedDelegate.Broadcast(SeatIndex, {}, GameBetToCall, 0, PlayerStack);
            return;
        }

        // 1. Fold (Всегда доступен, если игрок не олл-ин так, что не может повлиять на банк)
        //    Проверка на Stack == 0 && CurrentBet >= GameBetToCall уже была выше.
        //    Если Stack > 0, Fold всегда возможен. Если Stack == 0, но CurrentBet < GameBetToCall, Fold тоже возможен (хотя это будет его олл-ин).
        AllowedActions.Add(EPlayerAction::Fold);

        // 2. Check (Если текущая ставка игрока равна ставке для колла)
        if (CurrentBetOnTableForPlayer == GameBetToCall)
        {
            AllowedActions.Add(EPlayerAction::Check);
        }

        // 3. Call (Если ставка для колла больше текущей ставки игрока И у игрока есть фишки, чтобы доставить)
        //    Сумма для колла = GameBetToCall - CurrentBetOnTableForPlayer
        //    Игрок должен иметь хотя бы эту разницу в стеке, ИЛИ он может пойти олл-ин на меньшую сумму.
        if (GameBetToCall > CurrentBetOnTableForPlayer && PlayerStack > 0)
        {
            AllowedActions.Add(EPlayerAction::Call);
        }

        // 4. Bet (Если можно сделать Check, т.е. GameBetToCall == CurrentBetOnTableForPlayer, И у игрока достаточно фишек на минимальный бет)
        //    Минимальный бет обычно равен размеру большого блайнда.
        if (AllowedActions.Contains(EPlayerAction::Check) && PlayerStack >= CalculatedMinRaiseAmount)
        {
            AllowedActions.Add(EPlayerAction::Bet);
        }

        // 5. Raise (Если у игрока достаточно фишек, чтобы:
        //    а) Уравнять текущую ставку для колла (GameBetToCall - CurrentBetOnTableForPlayer)
        //    б) И затем добавить сверху хотя бы CalculatedMinRaiseAmount)
        //    И (есть предыдущая ставка для повышения ИЛИ можно сделать Bet (т.е. это будет первый Bet/OpenRaise))
        int64 AmountToEffectivelyCall = GameBetToCall - CurrentBetOnTableForPlayer;
        if (AmountToEffectivelyCall < 0) AmountToEffectivelyCall = 0; // Не может быть отрицательным

        if (PlayerStack > AmountToEffectivelyCall) // Если есть фишки сверх необходимого для колла
        {
            // Если игрок может покрыть колл и еще хотя бы минимальный рейз
            if (PlayerStack >= AmountToEffectivelyCall + CalculatedMinRaiseAmount)
            {
                // Raise доступен, если есть предыдущая ставка для повышения (GameBetToCall > 0 и он ее еще не уравнял полностью)
                // ИЛИ если он может сделать Bet (т.е. это будет open-raise).
                if (GameBetToCall > CurrentBetOnTableForPlayer || AllowedActions.Contains(EPlayerAction::Bet))
                {
                    AllowedActions.Add(EPlayerAction::Raise);
                }
            }
        }
        // TODO: Здесь можно добавить логику для AllIn как явного действия, если стек игрока
        // меньше, чем полный колл + мин.рейз, но больше, чем просто колл.
        // Или если игрок хочет поставить все фишки при Bet/Raise.
        // Пока что AllIn будет неявным (Call на все фишки, Bet на все фишки, Raise на все фишки).
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Unhandled game stage %s for seat %d. Broadcasting empty actions."),
            *UEnum::GetValueAsString(GameStateData->CurrentStage), SeatIndex);
        // Отправляем пустые действия для неизвестных стадий
        OnActionRequestedDelegate.Broadcast(SeatIndex, {}, GameBetToCall, 0, PlayerStack);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Broadcasting OnActionRequested for Seat %d (%s). Actions Num: %d. Stage: %s. Stack: %lld. BetToCall: %lld. MinRaise: %lld"),
        SeatIndex, *CurrentPlayer.PlayerName, AllowedActions.Num(),
        *UEnum::GetValueAsString(GameStateData->CurrentStage), PlayerStack, GameBetToCall, CalculatedMinRaiseAmount);

    OnActionRequestedDelegate.Broadcast(SeatIndex, AllowedActions, GameBetToCall, CalculatedMinRaiseAmount, PlayerStack);
}

void UOfflineGameManager::DealHoleCardsAndStartPreflop()
{
    if (!GameStateData || !Deck) { UE_LOG(LogTemp, Error, TEXT("DealHoleCards: Null state/deck")); return; }
    UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Dealing hole cards..."));
    int32 NumPlayersStillIn = 0; // Считаем тех, кто успешно поставил блайнды и остался в игре
    for (const FPlayerSeatData& Seat : GameStateData->Seats) { if (Seat.Status == EPlayerStatus::Playing) NumPlayersStillIn++; }

    if (NumPlayersStillIn < 2) { UE_LOG(LogTemp, Warning, TEXT("DealHoleCards: Not enough players (%d) after blinds to deal cards."), NumPlayersStillIn); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; OnActionRequestedDelegate.Broadcast(-1, {}, 0, 0, 0); return; }

    int32 CurrentDealingSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat);
    if (NumPlayersStillIn == 2) CurrentDealingSeat = GameStateData->PendingSmallBlindSeat; // В хедз-апе SB/Дилер получает первую карту

    for (int32 CardNum = 0; CardNum < 2; ++CardNum) {
        for (int32 i = 0; i < NumPlayersStillIn; ++i) {
            if (CurrentDealingSeat != -1 && GameStateData->Seats.IsValidIndex(CurrentDealingSeat) && GameStateData->Seats[CurrentDealingSeat].Status == EPlayerStatus::Playing) {
                TOptional<FCard> DealtCardOptional = Deck->DealCard();
                if (DealtCardOptional.IsSet()) { GameStateData->Seats[CurrentDealingSeat].HoleCards.Add(DealtCardOptional.GetValue()); }
                else { UE_LOG(LogTemp, Error, TEXT("Deck ran out of cards!")); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return; }
            }
            if (CurrentDealingSeat != -1) CurrentDealingSeat = GetNextActivePlayerSeat(CurrentDealingSeat); // Ищем следующего со статусом Playing
            else if (i < NumPlayersStillIn - 1) { UE_LOG(LogTemp, Error, TEXT("Invalid dealing seat logic.")); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return; }
        }
    }
    for (const FPlayerSeatData& Seat : GameStateData->Seats) { if (Seat.Status == EPlayerStatus::Playing && Seat.HoleCards.Num() == 2) { UE_LOG(LogTemp, Log, TEXT("Seat %d (%s) received: %s, %s"), Seat.SeatIndex, *Seat.PlayerName, *Seat.HoleCards[0].ToString(), *Seat.HoleCards[1].ToString()); } }

    GameStateData->CurrentStage = EGameStage::Preflop;
    int32 FirstToActSeat = DetermineFirstPlayerToActAfterBlinds();
    if (FirstToActSeat == -1) { UE_LOG(LogTemp, Error, TEXT("Could not determine first to act for Preflop!")); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return; }

    UE_LOG(LogTemp, Log, TEXT("Preflop round. First to act: Seat %d (%s). BetToCall: %lld"), FirstToActSeat, *GameStateData->Seats[FirstToActSeat].PlayerName, GameStateData->CurrentBetToCall);
    RequestPlayerAction(FirstToActSeat);
}

int32 UOfflineGameManager::DetermineFirstPlayerToActAfterBlinds() const
{
    if (!GameStateData || GameStateData->PendingBigBlindSeat == -1) return -1;
    int32 NumPlayersStillIn = 0;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) { if (Seat.Status == EPlayerStatus::Playing) NumPlayersStillIn++; }

    if (NumPlayersStillIn == 2) {
        return GameStateData->PendingSmallBlindSeat; // В хедз-апе SB (который дилер) ходит первым
    }
    else {
        return GetNextActivePlayerSeat(GameStateData->PendingBigBlindSeat); // Слева от ББ
    }
}

void UOfflineGameManager::ProceedToNextGameStage()
{
    UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage: Not yet implemented."));
}

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

    for (int32 i = 0; i < NumSeats; ++i)
    {
        if (GameStateData->Seats.IsValidIndex(CurrentIndex))
        {
            const FPlayerSeatData& Seat = GameStateData->Seats[CurrentIndex];
            bool bIsEligible = false;

            // Общая проверка на участие и наличие фишек
            if (Seat.bIsSittingIn && Seat.Stack > 0)
            {
                // Если стадия ожидания блайндов, и это очередь этого игрока ставить блайнд
                if ((GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && Seat.Status == EPlayerStatus::MustPostSmallBlind) ||
                    (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && Seat.Status == EPlayerStatus::MustPostBigBlind))
                {
                    bIsEligible = true;
                }
                // Если это стадия определения дилера, или ранняя стадия раздачи (до установки статусов MustPost...)
                else if (GameStateData->CurrentStage == EGameStage::Dealing || GameStateData->CurrentStage == EGameStage::WaitingForPlayers)
                {
                    // На этих этапах статус Waiting - это нормально для активного игрока
                    if (Seat.Status == EPlayerStatus::Waiting) bIsEligible = true;
                }
                // Для раундов ставок
                else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
                {
                    if (Seat.Status != EPlayerStatus::Folded) // Не сфолдил
                    {
                        // Может действовать, если есть фишки ИЛИ если он олл-ин, но текущая ставка для колла больше его ставки (он может "чекнуть" свой олл-ин)
                        if (Seat.Stack > 0 || (Seat.Stack == 0 && Seat.CurrentBet < GameStateData->CurrentBetToCall))
                        {
                            bIsEligible = true;
                        }
                    }
                }
            }

            if (bIsEligible)
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
    return GetNextActivePlayerSeat(GameStateData->DealerSeat);
}