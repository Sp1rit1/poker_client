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

    GameStateData->SmallBlindAmount = (InSmallBlindAmount <= 0) ? 5 : InSmallBlindAmount;
    if (InSmallBlindAmount <= 0) UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Invalid SmallBlindAmount (%lld), defaulted to 5."), InSmallBlindAmount);
    GameStateData->BigBlindAmount = GameStateData->SmallBlindAmount * 2;

    Deck->Initialize();

    int32 TotalActivePlayers = NumRealPlayers + NumBots;
    const int32 MinPlayers = 2;
    const int32 MaxPlayers = 9;

    if (TotalActivePlayers < MinPlayers || TotalActivePlayers > MaxPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Player count %d out of range [%d, %d]. Clamping."), TotalActivePlayers, MinPlayers, MaxPlayers);
        TotalActivePlayers = FMath::Clamp(TotalActivePlayers, MinPlayers, MaxPlayers);
        if (NumRealPlayers > TotalActivePlayers) NumRealPlayers = TotalActivePlayers;
        NumBots = TotalActivePlayers - NumRealPlayers;
        if (NumBots < 0) NumBots = 0;
    }

    FString PlayerActualName = TEXT("Player");
    int64 PlayerActualId = -1;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetOuter())) {
        if (GI->bIsLoggedIn || GI->bIsInOfflineMode) {
            PlayerActualName = GI->LoggedInUsername.IsEmpty() ? TEXT("Player") : GI->LoggedInUsername;
            PlayerActualId = GI->LoggedInUserId;
        }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Could not get GameInstance. Using default player name.")); }

    GameStateData->Seats.Empty();
    GameStateData->Seats.Reserve(TotalActivePlayers);

    for (int32 i = 0; i < TotalActivePlayers; ++i) {
        FPlayerSeatData Seat;
        Seat.SeatIndex = i;
        Seat.bIsBot = (i >= NumRealPlayers);
        Seat.PlayerName = Seat.bIsBot ? FString::Printf(TEXT("Bot %d"), i - NumRealPlayers + 1) : PlayerActualName;
        Seat.PlayerId = Seat.bIsBot ? -1 : PlayerActualId;
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

    UE_LOG(LogTemp, Log, TEXT("Offline game initialized. SB: %lld, BB: %lld. Active Seats: %d."),
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

    GameStateData->CommunityCards.Empty();
    GameStateData->Pot = 0;
    GameStateData->CurrentBetToCall = 0;
    int32 NumActivePlayersThisHand = 0;

    for (FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Stack > 0) {
            Seat.HoleCards.Empty(); Seat.CurrentBet = 0; Seat.Status = EPlayerStatus::Waiting;
            Seat.bIsTurn = false; Seat.bIsSmallBlind = false; Seat.bIsBigBlind = false;
            NumActivePlayersThisHand++;
        }
        else if (Seat.Stack <= 0 && Seat.bIsSittingIn) { Seat.Status = EPlayerStatus::SittingOut; }
    }

    if (NumActivePlayersThisHand < 2) {
        UE_LOG(LogTemp, Warning, TEXT("StartNewHand - Not enough active players (%d)."), NumActivePlayersThisHand);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        OnActionRequestedDelegate.Broadcast(-1, FString(TEXT("N/A")), {}, 0, 0, 0, GameStateData->Pot);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("StartNewHand: NumActivePlayers ready for this hand: %d"), NumActivePlayersThisHand);

    GameStateData->CurrentStage = EGameStage::Dealing;

    if (GameStateData->DealerSeat != -1 && GameStateData->Seats.IsValidIndex(GameStateData->DealerSeat)) {
        GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = false;
    }

    if (GameStateData->DealerSeat == -1) {
        TArray<int32> EligibleDealerIndices;
        for (int32 i = 0; i < GameStateData->Seats.Num(); ++i) { if (GameStateData->Seats[i].bIsSittingIn && GameStateData->Seats[i].Stack > 0) EligibleDealerIndices.Add(i); }
        if (EligibleDealerIndices.Num() > 0) GameStateData->DealerSeat = EligibleDealerIndices[FMath::RandRange(0, EligibleDealerIndices.Num() - 1)];
        else { UE_LOG(LogTemp, Error, TEXT("StartNewHand - No eligible players for initial dealer!")); return; }
    }
    else { GameStateData->DealerSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat, false); }

    if (GameStateData->DealerSeat == -1) { UE_LOG(LogTemp, Error, TEXT("StartNewHand - Could not determine new dealer seat!")); return; }
    GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = true;
    UE_LOG(LogTemp, Log, TEXT("Dealer is Seat %d (%s)"), GameStateData->DealerSeat, *GameStateData->Seats[GameStateData->DealerSeat].PlayerName);

    Deck->Shuffle();
    UE_LOG(LogTemp, Log, TEXT("Deck shuffled. %d cards remaining."), Deck->NumCardsLeft());

    int32 sbSeat = -1;
    int32 bbSeat = -1;

    if (NumActivePlayersThisHand == 2) {
        sbSeat = GameStateData->DealerSeat;
        bbSeat = GetNextActivePlayerSeat(sbSeat, false);
    }
    else {
        sbSeat = GetNextActivePlayerSeat(GameStateData->DealerSeat, false);
        if (sbSeat != -1) bbSeat = GetNextActivePlayerSeat(sbSeat, false);
    }

    if (sbSeat == -1 || bbSeat == -1) {
        UE_LOG(LogTemp, Error, TEXT("StartNewHand - Could not determine SB (%d) or BB (%d) seat!"), sbSeat, bbSeat);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        OnActionRequestedDelegate.Broadcast(-1, FString(TEXT("N/A")), {}, 0, 0, 0, GameStateData->Pot);
        return;
    }

    GameStateData->PendingSmallBlindSeat = sbSeat;
    GameStateData->PendingBigBlindSeat = bbSeat;

    GameStateData->CurrentStage = EGameStage::WaitingForSmallBlind;
    if (GameStateData->Seats.IsValidIndex(GameStateData->PendingSmallBlindSeat)) {
        GameStateData->Seats[GameStateData->PendingSmallBlindSeat].Status = EPlayerStatus::MustPostSmallBlind;
        UE_LOG(LogTemp, Log, TEXT("Hand prepared. Waiting for Small Blind from Seat %d (%s)."), GameStateData->PendingSmallBlindSeat, *GameStateData->Seats[GameStateData->PendingSmallBlindSeat].PlayerName);
        RequestPlayerAction(GameStateData->PendingSmallBlindSeat);
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("StartNewHand - SB Seat index %d is invalid!"), GameStateData->PendingSmallBlindSeat);
        // TODO: Более умная обработка ошибки, если SB невалиден
        OnActionRequestedDelegate.Broadcast(-1, FString(TEXT("Error")), {}, 0, 0, 0, GameStateData->Pot);
    }
}

void UOfflineGameManager::ProcessPlayerAction(EPlayerAction PlayerAction, int64 Amount)
{
    if (!GameStateData || GameStateData->CurrentTurnSeat == -1 || !GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat))
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: Invalid state. CurrentTurnSeat: %d"), GameStateData ? GameStateData->CurrentTurnSeat : -1);
        return;
    }

    int32 ActingPlayerSeat = GameStateData->CurrentTurnSeat;
    FPlayerSeatData& Player = GameStateData->Seats[ActingPlayerSeat];

    UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Seat %d (%s) Action: %s Amount: %lld Stage: %s. Stack: %lld, Bet: %lld, ToCall: %lld"),
        ActingPlayerSeat, *Player.PlayerName, *UEnum::GetValueAsString(PlayerAction), Amount,
        *UEnum::GetValueAsString(GameStateData->CurrentStage), Player.Stack, Player.CurrentBet, GameStateData->CurrentBetToCall);

    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeat == GameStateData->PendingSmallBlindSeat) {
            int64 ActualSB = FMath::Min(GameStateData->SmallBlindAmount, Player.Stack);
            Player.Stack -= ActualSB; Player.CurrentBet = ActualSB; Player.bIsSmallBlind = true; Player.Status = EPlayerStatus::Playing;
            GameStateData->Pot += ActualSB;
            UE_LOG(LogTemp, Log, TEXT("SB Posted: %lld by Seat %d. Stack: %lld. Pot: %lld"), ActualSB, ActingPlayerSeat, Player.Stack, GameStateData->Pot);
            GameStateData->CurrentStage = EGameStage::WaitingForBigBlind;
            if (GameStateData->Seats.IsValidIndex(GameStateData->PendingBigBlindSeat)) {
                GameStateData->Seats[GameStateData->PendingBigBlindSeat].Status = EPlayerStatus::MustPostBigBlind;
                RequestPlayerAction(GameStateData->PendingBigBlindSeat);
            }
            else { UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: BB Seat index %d invalid after SB!"), GameStateData->PendingBigBlindSeat); }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("Invalid action/player for SB. Expected PostBlind from %d. Got %s from %d."), GameStateData->PendingSmallBlindSeat, *UEnum::GetValueAsString(PlayerAction), ActingPlayerSeat); RequestPlayerAction(ActingPlayerSeat); }
        return;
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeat == GameStateData->PendingBigBlindSeat) {
            int64 ActualBB = FMath::Min(GameStateData->BigBlindAmount, Player.Stack);
            Player.Stack -= ActualBB; Player.CurrentBet = ActualBB; Player.bIsBigBlind = true; Player.Status = EPlayerStatus::Playing;
            GameStateData->Pot += ActualBB;
            GameStateData->CurrentBetToCall = GameStateData->BigBlindAmount; // <-- УСТАНАВЛИВАЕТСЯ ЗДЕСЬ
            UE_LOG(LogTemp, Log, TEXT("BB Posted: %lld by Seat %d. Stack: %lld. Pot: %lld. BetToCall: %lld"), ActualBB, ActingPlayerSeat, Player.Stack, GameStateData->Pot, GameStateData->CurrentBetToCall);
            DealHoleCardsAndStartPreflop();
        }
        else { UE_LOG(LogTemp, Warning, TEXT("Invalid action/player for BB. Expected PostBlind from %d. Got %s from %d."), GameStateData->PendingBigBlindSeat, *UEnum::GetValueAsString(PlayerAction), ActingPlayerSeat); RequestPlayerAction(ActingPlayerSeat); }
        return;
    }
    else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River) {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Betting round action %s NOT YET IMPLEMENTED."), *UEnum::GetValueAsString(PlayerAction));
        Player.bIsTurn = false;
        int32 NextPlayer = GetNextActivePlayerSeat(ActingPlayerSeat, false);
        if (NextPlayer != -1 && NextPlayer != ActingPlayerSeat) { RequestPlayerAction(NextPlayer); }
        else { UE_LOG(LogTemp, Log, TEXT("Betting round over or one player left.")); /* ProceedToNextGameStage(); */ }
        return;
    }
    else { UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Unhandled stage %s."), *UEnum::GetValueAsString(GameStateData->CurrentStage)); }
}

void UOfflineGameManager::RequestPlayerAction(int32 SeatIndex)
{
    FString PlayerNameForBroadcast = TEXT("N/A");
    int64 PlayerStackForBroadcast = 0;
    int64 BetToCallForBroadcast = 0;
    int64 MinRaiseForBroadcast = GameStateData ? GameStateData->BigBlindAmount : 0; // Безопасное значение по умолчанию
    int64 PotForBroadcast = GameStateData ? GameStateData->Pot : 0;
    TArray<EPlayerAction> AllowedActions;

    if (!GameStateData || !GameStateData->Seats.IsValidIndex(SeatIndex)) {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Invalid SeatIndex %d or GameStateData null. Broadcasting default/error state."), SeatIndex);
        // Используем значения по умолчанию для Broadcast, SeatIndex может быть невалидным
        OnActionRequestedDelegate.Broadcast(SeatIndex, PlayerNameForBroadcast, AllowedActions, BetToCallForBroadcast, MinRaiseForBroadcast, PlayerStackForBroadcast, PotForBroadcast);
        return;
    }

    if (GameStateData->CurrentTurnSeat != -1 && GameStateData->CurrentTurnSeat != SeatIndex && GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat)) {
        GameStateData->Seats[GameStateData->CurrentTurnSeat].bIsTurn = false;
    }
    GameStateData->CurrentTurnSeat = SeatIndex;
    FPlayerSeatData& CurrentPlayer = GameStateData->Seats[SeatIndex];
    CurrentPlayer.bIsTurn = true;

    PlayerNameForBroadcast = CurrentPlayer.PlayerName;
    PlayerStackForBroadcast = CurrentPlayer.Stack;
    // BetToCallForBroadcast и MinRaiseForBroadcast будут установлены ниже в зависимости от стадии
    // PotForBroadcast уже GameStateData->Pot

    if (CurrentPlayer.Status == EPlayerStatus::Folded || (CurrentPlayer.Stack == 0 && CurrentPlayer.CurrentBet >= GameStateData->CurrentBetToCall && GameStateData->CurrentStage >= EGameStage::Preflop)) {
        UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction: Seat %d (%s) Folded/All-In. Broadcasting empty actions."), SeatIndex, *CurrentPlayer.PlayerName);
        // BetToCall берем из GameState, т.к. игрок не может действовать, но UI должен знать текущую ставку
        OnActionRequestedDelegate.Broadcast(SeatIndex, PlayerNameForBroadcast, {}, GameStateData->CurrentBetToCall, 0, PlayerStackForBroadcast, PotForBroadcast);
        return;
    }

    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && SeatIndex == GameStateData->PendingSmallBlindSeat) {
        if (PlayerStackForBroadcast > 0) AllowedActions.Add(EPlayerAction::PostBlind);
        BetToCallForBroadcast = 0;
        MinRaiseForBroadcast = 0;
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && SeatIndex == GameStateData->PendingBigBlindSeat) {
        if (PlayerStackForBroadcast > 0) AllowedActions.Add(EPlayerAction::PostBlind);
        BetToCallForBroadcast = 0; // ИЗМЕНЕНО: BetToCall = 0 для BB
        MinRaiseForBroadcast = 0;
    }
    else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River) {
        if (CurrentPlayer.Status != EPlayerStatus::Playing) {
            UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Player %d not Playing. Status: %s"), SeatIndex, *UEnum::GetValueAsString(CurrentPlayer.Status));
            OnActionRequestedDelegate.Broadcast(SeatIndex, PlayerNameForBroadcast, {}, GameStateData->CurrentBetToCall, MinRaiseForBroadcast, PlayerStackForBroadcast, PotForBroadcast); return;
        }
        AllowedActions.Add(EPlayerAction::Fold);
        if (CurrentPlayer.CurrentBet == GameStateData->CurrentBetToCall) AllowedActions.Add(EPlayerAction::Check);
        if (GameStateData->CurrentBetToCall > CurrentPlayer.CurrentBet && PlayerStackForBroadcast > 0) AllowedActions.Add(EPlayerAction::Call);
        if (AllowedActions.Contains(EPlayerAction::Check) && PlayerStackForBroadcast >= MinRaiseForBroadcast) AllowedActions.Add(EPlayerAction::Bet);

        int64 AmountToEffectivelyCall = FMath::Max(0LL, GameStateData->CurrentBetToCall - CurrentPlayer.CurrentBet);
        if (PlayerStackForBroadcast > AmountToEffectivelyCall && PlayerStackForBroadcast >= AmountToEffectivelyCall + MinRaiseForBroadcast && MinRaiseForBroadcast > 0) {
            if (GameStateData->CurrentBetToCall > CurrentPlayer.CurrentBet || AllowedActions.Contains(EPlayerAction::Bet)) {
                AllowedActions.Add(EPlayerAction::Raise);
            }
        }
        BetToCallForBroadcast = GameStateData->CurrentBetToCall; // Для префлопа и далее используем текущий BetToCall из GameState
        // MinRaiseForBroadcast уже установлен в BB (TODO: уточнить логику для ре-рейзов)
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Unhandled stage %s for seat %d."), *UEnum::GetValueAsString(GameStateData->CurrentStage), SeatIndex);
    }

    UE_LOG(LogTemp, Log, TEXT("Broadcasting OnActionRequested: Seat=%d, Name='%s', Actions#=%d, Stage=%s, Stack=%lld, Pot=%lld, ToCall=%lld, MinRaise=%lld"),
        SeatIndex, *PlayerNameForBroadcast, AllowedActions.Num(),
        *UEnum::GetValueAsString(GameStateData->CurrentStage), PlayerStackForBroadcast, PotForBroadcast, BetToCallForBroadcast, MinRaiseForBroadcast);

    OnActionRequestedDelegate.Broadcast(SeatIndex, PlayerNameForBroadcast, AllowedActions, BetToCallForBroadcast, MinRaiseForBroadcast, PlayerStackForBroadcast, PotForBroadcast);
}

void UOfflineGameManager::DealHoleCardsAndStartPreflop()
{
    if (!GameStateData || !Deck) { UE_LOG(LogTemp, Error, TEXT("DealHoleCards: Null GameState/Deck")); return; }
    UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Dealing hole cards..."));
    int32 NumPlayersActuallyPlaying = 0;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) { if (Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing) NumPlayersActuallyPlaying++; }

    if (NumPlayersActuallyPlaying < 2) {
        UE_LOG(LogTemp, Warning, TEXT("DealHoleCards: Not enough players (%d) after blinds."), NumPlayersActuallyPlaying);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        OnActionRequestedDelegate.Broadcast(-1, FString(TEXT("N/A")), {}, 0, 0, 0, GameStateData->Pot);
        return;
    }

    int32 StartDealingFrom = GetNextActivePlayerSeat(GameStateData->DealerSeat, false);
    if (NumPlayersActuallyPlaying == 2) StartDealingFrom = GameStateData->PendingSmallBlindSeat; // В хедз-апе SB/Дилер получает первую карту

    int32 SeatToDeal = StartDealingFrom;
    for (int32 CardNum = 0; CardNum < 2; ++CardNum) {
        for (int32 i = 0; i < NumPlayersActuallyPlaying; ++i) {
            if (SeatToDeal != -1 && GameStateData->Seats.IsValidIndex(SeatToDeal) && GameStateData->Seats[SeatToDeal].Status == EPlayerStatus::Playing) {
                TOptional<FCard> DealtCardOptional = Deck->DealCard();
                if (DealtCardOptional.IsSet()) { GameStateData->Seats[SeatToDeal].HoleCards.Add(DealtCardOptional.GetValue()); }
                else { UE_LOG(LogTemp, Error, TEXT("Deck ran out of cards!")); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return; }
            }
            else { UE_LOG(LogTemp, Error, TEXT("Invalid SeatToDeal %d during dealing."), SeatToDeal); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return; }

            SeatToDeal = GetNextActivePlayerSeat(SeatToDeal, false);
            if (SeatToDeal == -1 && i < NumPlayersActuallyPlaying - 1) { // Если не нашли следующего, а карты еще нужно раздать
                UE_LOG(LogTemp, Error, TEXT("Could not find next player to deal card to.")); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
            }
        }
    }
    for (const FPlayerSeatData& Seat : GameStateData->Seats) { if (Seat.Status == EPlayerStatus::Playing && Seat.HoleCards.Num() == 2) { UE_LOG(LogTemp, Log, TEXT("Seat %d (%s) received: %s, %s"), Seat.SeatIndex, *Seat.PlayerName, *Seat.HoleCards[0].ToString(), *Seat.HoleCards[1].ToString()); } }

    GameStateData->CurrentStage = EGameStage::Preflop;
    int32 FirstToActSeat = DetermineFirstPlayerToActAfterBlinds();
    if (FirstToActSeat == -1) { UE_LOG(LogTemp, Error, TEXT("Could not determine first to act for Preflop!")); GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return; }

    UE_LOG(LogTemp, Log, TEXT("Preflop round. First to act: Seat %d (%s). CurrentBetToCall: %lld"), FirstToActSeat, *GameStateData->Seats[FirstToActSeat].PlayerName, GameStateData->CurrentBetToCall);
    RequestPlayerAction(FirstToActSeat);
}

int32 UOfflineGameManager::DetermineFirstPlayerToActAfterBlinds() const
{
    if (!GameStateData || GameStateData->PendingBigBlindSeat == -1) return -1;
    int32 NumPlayersStillIn = 0;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) { if (Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing) NumPlayersStillIn++; }
    return (NumPlayersStillIn == 2) ? GameStateData->PendingSmallBlindSeat : GetNextActivePlayerSeat(GameStateData->PendingBigBlindSeat, false);
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

    if (!bIncludeStartSeat) { CurrentIndex = (StartSeatIndex + 1) % NumSeats; }

    for (int32 i = 0; i < NumSeats; ++i) {
        if (GameStateData->Seats.IsValidIndex(CurrentIndex)) {
            const FPlayerSeatData& Seat = GameStateData->Seats[CurrentIndex];
            bool bIsEligibleToAct = false;
            if (Seat.bIsSittingIn && Seat.Stack > 0) {
                if (((GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && Seat.Status == EPlayerStatus::MustPostSmallBlind && CurrentIndex == GameStateData->PendingSmallBlindSeat)) ||
                    ((GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && Seat.Status == EPlayerStatus::MustPostBigBlind && CurrentIndex == GameStateData->PendingBigBlindSeat))) {
                    bIsEligibleToAct = true;
                }
                else if (GameStateData->CurrentStage == EGameStage::Dealing || GameStateData->CurrentStage == EGameStage::WaitingForPlayers) {
                    if (Seat.Status == EPlayerStatus::Waiting) bIsEligibleToAct = true;
                }
                else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River) {
                    if (Seat.Status == EPlayerStatus::Playing && (Seat.Stack > 0 || (Seat.Stack == 0 && Seat.CurrentBet < GameStateData->CurrentBetToCall))) {
                        bIsEligibleToAct = true;
                    }
                }
            }
            if (bIsEligibleToAct) return CurrentIndex;
        }
        CurrentIndex = (CurrentIndex + 1) % NumSeats;
    }
    UE_LOG(LogTemp, Warning, TEXT("GetNextActivePlayerSeat: No eligible player found from Seat %d, Stage: %s"), StartSeatIndex, *UEnum::GetValueAsString(GameStateData->CurrentStage));
    return -1;
}

int32 UOfflineGameManager::DetermineFirstPlayerToActPostFlop() const
{
    if (!GameStateData || GameStateData->DealerSeat == -1) return -1;
    return GetNextActivePlayerSeat(GameStateData->DealerSeat, false);
}