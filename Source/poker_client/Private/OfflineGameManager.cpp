#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "MyGameInstance.h" // Если нужен для получения имени игрока
#include "Kismet/GameplayStatics.h" // Для UE_LOG
#include "PokerHandEvaluator.h"   // Для оценки рук на шоудауне

// Конструктор
UOfflineGameManager::UOfflineGameManager()
{
    // Используем TObjectPtr, поэтому инициализация в nullptr происходит автоматически
    // GameStateData = nullptr; // Больше не нужно явно
    // Deck = nullptr;        // Больше не нужно явно
}

void UOfflineGameManager::InitializeGame(int32 NumRealPlayers, int32 NumBots, int64 InitialStack, int64 InSmallBlindAmount)
{
    UE_LOG(LogTemp, Log, TEXT("UOfflineGameManager::InitializeGame: NumRealPlayers=%d, NumBots=%d, InitialStack=%lld, SB=%lld"),
        NumRealPlayers, NumBots, InitialStack, InSmallBlindAmount);

    if (!GetOuter()) // Важно, чтобы UObject имел "владельца" для корректного управления памятью
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeGame: GetOuter() is null! Cannot create UObjects without an outer."));
        return;
    }

    // Создаем GameState и Deck, GetOuter() обычно GameInstance
    GameStateData = NewObject<UOfflinePokerGameState>(GetOuter());
    Deck = NewObject<UDeck>(GetOuter());

    if (!GameStateData || !Deck) // Проверка TObjectPtr
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeGame: Failed to create GameState or Deck objects!"));
        return;
    }

    GameStateData->ResetState(); // Сброс всех полей GameState к начальным значениям

    // Установка размеров блайндов
    GameStateData->SmallBlindAmount = (InSmallBlindAmount <= 0) ? 5 : InSmallBlindAmount; // Базовый SB, если невалидный ввод
    if (InSmallBlindAmount <= 0) UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Invalid SmallBlindAmount (%lld), defaulted to 5."), InSmallBlindAmount);
    GameStateData->BigBlindAmount = GameStateData->SmallBlindAmount * 2; // BB обычно в два раза больше SB

    Deck->Initialize(); // Убедимся, что колода инициализирована при старте игры

    // Валидация и корректировка количества игроков
    int32 TotalActivePlayers = NumRealPlayers + NumBots;
    const int32 MinPlayers = 2;
    const int32 MaxPlayers = 9; // Максимальное количество мест за столом

    if (TotalActivePlayers < MinPlayers || TotalActivePlayers > MaxPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Player count %d out of range [%d, %d]. Clamping."), TotalActivePlayers, MinPlayers, MaxPlayers);
        TotalActivePlayers = FMath::Clamp(TotalActivePlayers, MinPlayers, MaxPlayers);
        if (NumRealPlayers > TotalActivePlayers) NumRealPlayers = TotalActivePlayers;
        NumBots = TotalActivePlayers - NumRealPlayers;
        if (NumBots < 0) NumBots = 0;
    }

    // Получение данных реального игрока (если используется)
    FString PlayerActualName = TEXT("Player");
    int64 PlayerActualId = -1;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetOuter())) {
        if (GI->bIsLoggedIn || GI->bIsInOfflineMode) {
            PlayerActualName = GI->LoggedInUsername.IsEmpty() ? TEXT("Player") : GI->LoggedInUsername;
            PlayerActualId = GI->LoggedInUserId;
        }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Could not get GameInstance. Using default player name.")); }

    // Заполнение информации о местах игроков
    GameStateData->Seats.Empty();
    GameStateData->Seats.Reserve(TotalActivePlayers);
    for (int32 i = 0; i < TotalActivePlayers; ++i) {
        FPlayerSeatData Seat;
        Seat.SeatIndex = i;
        Seat.bIsBot = (i >= NumRealPlayers); // Первые NumRealPlayers - это реальные игроки
        Seat.PlayerName = Seat.bIsBot ? FString::Printf(TEXT("Bot %d"), i - (NumRealPlayers - 1)) : PlayerActualName; // Нумерация ботов с 1
        Seat.PlayerId = Seat.bIsBot ? -1 : PlayerActualId;
        Seat.Stack = InitialStack;
        Seat.bIsSittingIn = true; // Все игроки начинают "в игре"
        Seat.Status = EPlayerStatus::Waiting; // Начальный статус до первой раздачи
        GameStateData->Seats.Add(Seat);
    }

    // Начальные значения для состояния игры (многие из них уже в ResetState)
    GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Готовы к StartNewHand
    GameStateData->DealerSeat = -1; // Дилер будет определен в первой StartNewHand
    // Остальные поля, такие как CurrentTurnSeat, CurrentBetToCall и т.д., будут установлены в StartNewHand.

    UE_LOG(LogTemp, Log, TEXT("Offline game initialized. SB: %lld, BB: %lld. Active Seats: %d."),
        GameStateData->SmallBlindAmount, GameStateData->BigBlindAmount, GameStateData->Seats.Num());
}

UOfflinePokerGameState* UOfflineGameManager::GetGameState() const
{
    return GameStateData.Get();
}

void UOfflineGameManager::StartNewHand()
{
    if (!GameStateData || !Deck) { 
        UE_LOG(LogTemp, Error, TEXT("StartNewHand Error: GameStateData or Deck is null!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("--- STARTING NEW HAND ---"));
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- New Hand Starting ---"));

    // 1. Сброс состояния стола для новой руки
    GameStateData->CommunityCards.Empty();
    if (OnCommunityCardsUpdatedDelegate.IsBound()) OnCommunityCardsUpdatedDelegate.Broadcast(GameStateData->CommunityCards);
    GameStateData->Pot = 0;
    GameStateData->CurrentBetToCall = 0;
    GameStateData->LastBetOrRaiseAmountInCurrentRound = 0;
    GameStateData->LastAggressorSeatIndex = -1;
    GameStateData->PlayerWhoOpenedBettingThisRound = -1;

    int32 NumActivePlayersThisHand = 0;
    for (FPlayerSeatData& Seat : GameStateData->Seats) {
        Seat.HoleCards.Empty();
        Seat.CurrentBet = 0;
        Seat.bIsTurn = false;
        Seat.bIsSmallBlind = false;
        Seat.bIsBigBlind = false;
        // Seat.bIsDealer будет сброшен и установлен ниже

        if (Seat.bIsSittingIn && Seat.Stack > 0) {
            // ВРЕМЕННО УСТАНАВЛИВАЕМ СТАТУС EPlayerStatus::Playing
            // чтобы GetNextPlayerToAct мог корректно найти игроков для определения дилера и блайндов.
            // Этот статус будет уточнен позже на MustPostSmallBlind/MustPostBigBlind.
            Seat.Status = EPlayerStatus::Playing;
            NumActivePlayersThisHand++;
        }
        else if (Seat.Stack <= 0 && Seat.bIsSittingIn) {
            Seat.Status = EPlayerStatus::SittingOut;
        }
        // Если !Seat.bIsSittingIn, их статус не меняем, они и так не участвуют.
    }

    if (NumActivePlayersThisHand < 2) {
        UE_LOG(LogTemp, Warning, TEXT("StartNewHand: Not enough active players (%d) to start a hand. Waiting for players."), NumActivePlayersThisHand);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Waiting for players..."), GameStateData->Pot);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("StartNewHand: Number of active players for this hand: %d"), NumActivePlayersThisHand);

    // 2. Определение дилера
    if (GameStateData->DealerSeat != -1 && GameStateData->Seats.IsValidIndex(GameStateData->DealerSeat)) {
        GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = false;
    }

    if (GameStateData->DealerSeat == -1) { // Первая рука в игре
        TArray<int32> EligibleDealerIndices;
        for (int32 i = 0; i < GameStateData->Seats.Num(); ++i) {
            // Используем тот же критерий, что и для NumActivePlayersThisHand
            if (GameStateData->Seats[i].bIsSittingIn && GameStateData->Seats[i].Stack > 0) {
                EligibleDealerIndices.Add(i);
            }
        }
        if (EligibleDealerIndices.Num() > 0) {
            GameStateData->DealerSeat = EligibleDealerIndices[FMath::RandRange(0, EligibleDealerIndices.Num() - 1)];
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("StartNewHand CRITICAL: No eligible players for initial dealer (this should not happen if NumActivePlayersThisHand >= 2)!"));
            return;
        }
    }
    else {
        // GetNextPlayerToAct теперь должен находить игроков со статусом Playing
        GameStateData->DealerSeat = GetNextPlayerToAct(GameStateData->DealerSeat, false);
    }

    if (GameStateData->DealerSeat == -1) {
        UE_LOG(LogTemp, Error, TEXT("StartNewHand CRITICAL: Could not determine new dealer seat!"));
        return;
    }
    GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = true;
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Dealer is %s (Seat %d)"), *GameStateData->Seats[GameStateData->DealerSeat].PlayerName, GameStateData->DealerSeat));

    // 3. Перемешивание колоды
    Deck->Initialize();
    Deck->Shuffle();
    UE_LOG(LogTemp, Log, TEXT("Deck shuffled. Cards remaining: %d"), Deck->NumCardsLeft());

    // 4. Определение позиций блайндов
    GameStateData->PendingSmallBlindSeat = -1;
    GameStateData->PendingBigBlindSeat = -1;

    if (NumActivePlayersThisHand == 2) { // Хедз-ап: дилер = SB
        GameStateData->PendingSmallBlindSeat = GameStateData->DealerSeat;
        // Ищем BB, начиная СРАЗУ СО СЛЕДУЮЩЕГО после SB, исключая самого SB из первой проверки
        GameStateData->PendingBigBlindSeat = GetNextPlayerToAct(GameStateData->PendingSmallBlindSeat, true); // <--- ИЗМЕНЕНИЕ: bExcludeStartSeat = true
    }
    else { // 3+ игроков
        GameStateData->PendingSmallBlindSeat = GetNextPlayerToAct(GameStateData->DealerSeat, true); // <--- ИЗМЕНЕНИЕ: bExcludeStartSeat = true (следующий после дилера)
        if (GameStateData->PendingSmallBlindSeat != -1) {
            // Ищем BB, начиная СРАЗU СО СЛЕДУЮЩЕГО после SB, исключая самого SB
            GameStateData->PendingBigBlindSeat = GetNextPlayerToAct(GameStateData->PendingSmallBlindSeat, true); // <--- ИЗМЕНЕНИЕ: bExcludeStartSeat = true
        }
    }

    // Критическая проверка после определения блайндов
    if (GameStateData->PendingSmallBlindSeat == -1 ||
        GameStateData->PendingBigBlindSeat == -1 ||
        GameStateData->PendingSmallBlindSeat == GameStateData->PendingBigBlindSeat)
    {
        UE_LOG(LogTemp, Error, TEXT("StartNewHand CRITICAL: Could not determine valid and distinct SB (%d) or BB (%d) seat! NumActive: %d"),
            GameStateData->PendingSmallBlindSeat, GameStateData->PendingBigBlindSeat, NumActivePlayersThisHand);
        // Логирование состояния мест для отладки
        for (const auto& SeatDebug : GameStateData->Seats) {
            UE_LOG(LogTemp, Error, TEXT("  Seat %d: Stack %lld, Status %s, SittingIn %d, IsTurn %d"),
                SeatDebug.SeatIndex, SeatDebug.Stack, *UEnum::GetValueAsString(SeatDebug.Status), SeatDebug.bIsSittingIn, SeatDebug.bIsTurn);
        }
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("SB Seat: %d (%s), BB Seat: %d (%s)"),
        GameStateData->PendingSmallBlindSeat, *GameStateData->Seats[GameStateData->PendingSmallBlindSeat].PlayerName,
        GameStateData->PendingBigBlindSeat, *GameStateData->Seats[GameStateData->PendingBigBlindSeat].PlayerName);

    // 5. Переход к постановке малого блайнда
    GameStateData->CurrentStage = EGameStage::WaitingForSmallBlind;

    // Устанавливаем правильный статус для SB
    GameStateData->Seats[GameStateData->PendingSmallBlindSeat].Status = EPlayerStatus::MustPostSmallBlind;
    // Статус BB игрока будет изменен на MustPostBigBlind в функции RequestBigBlind

    UE_LOG(LogTemp, Log, TEXT("Hand prepared. Waiting for Small Blind from Seat %d (%s)."), GameStateData->PendingSmallBlindSeat, *GameStateData->Seats[GameStateData->PendingSmallBlindSeat].PlayerName);
    RequestPlayerAction(GameStateData->PendingSmallBlindSeat);
}

// Вспомогательная функция для постановки блайндов. Вызывается из ProcessPlayerAction.
void UOfflineGameManager::PostBlinds()
{
    if (!GameStateData) return;

    // Постановка Small Blind
    if (GameStateData->Seats.IsValidIndex(GameStateData->PendingSmallBlindSeat))
    {
        FPlayerSeatData& SBPlayer = GameStateData->Seats[GameStateData->PendingSmallBlindSeat];
        if (SBPlayer.Status == EPlayerStatus::MustPostSmallBlind) // Двойная проверка
        {
            int64 ActualSB = FMath::Min(GameStateData->SmallBlindAmount, SBPlayer.Stack);
            SBPlayer.Stack -= ActualSB;
            SBPlayer.CurrentBet = ActualSB;
            SBPlayer.bIsSmallBlind = true;
            SBPlayer.Status = EPlayerStatus::Playing; // Поставил блайнд, теперь в игре
            GameStateData->Pot += ActualSB;
            OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s posts Small Blind: %lld. Stack: %lld"), *SBPlayer.PlayerName, ActualSB, SBPlayer.Stack));
        }
    }
    else { UE_LOG(LogTemp, Error, TEXT("PostBlinds: Invalid PendingSmallBlindSeat index: %d"), GameStateData->PendingSmallBlindSeat); }

    // Постановка Big Blind
    if (GameStateData->Seats.IsValidIndex(GameStateData->PendingBigBlindSeat))
    {
        FPlayerSeatData& BBPlayer = GameStateData->Seats[GameStateData->PendingBigBlindSeat];
        if (BBPlayer.Status == EPlayerStatus::MustPostBigBlind) // Двойная проверка
        {
            int64 ActualBB = FMath::Min(GameStateData->BigBlindAmount, BBPlayer.Stack);
            BBPlayer.Stack -= ActualBB;
            BBPlayer.CurrentBet = ActualBB;
            BBPlayer.bIsBigBlind = true;
            BBPlayer.Status = EPlayerStatus::Playing;
            GameStateData->Pot += ActualBB;
            GameStateData->CurrentBetToCall = GameStateData->BigBlindAmount; // Устанавливаем начальную ставку для колла
            // Игрок, поставивший ББ, является первым агрессором в этом раунде (до других действий)
            GameStateData->LastAggressorSeatIndex = GameStateData->PendingBigBlindSeat;
            GameStateData->LastBetOrRaiseAmountInCurrentRound = GameStateData->BigBlindAmount;
            // Первый, кто должен будет походить после BB, "открывает" торги для других
            GameStateData->PlayerWhoOpenedBettingThisRound = DetermineFirstPlayerToActAtPreflop();


            OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s posts Big Blind: %lld. Stack: %lld"), *BBPlayer.PlayerName, ActualBB, BBPlayer.Stack));
        }
    }
    else { UE_LOG(LogTemp, Error, TEXT("PostBlinds: Invalid PendingBigBlindSeat index: %d"), GameStateData->PendingBigBlindSeat); }

    // Обновляем информацию о столе для UI после постановки блайндов
    FString BlindsPosterName = TEXT("Blinds"); // Общее имя для события
    if (GameStateData->Seats.IsValidIndex(GameStateData->PendingBigBlindSeat)) BlindsPosterName = GameStateData->Seats[GameStateData->PendingBigBlindSeat].PlayerName;
    else if (GameStateData->Seats.IsValidIndex(GameStateData->PendingSmallBlindSeat)) BlindsPosterName = GameStateData->Seats[GameStateData->PendingSmallBlindSeat].PlayerName;
    OnTableStateInfoDelegate.Broadcast(BlindsPosterName, GameStateData->Pot);
}

// OfflineGameManager.cpp
// ... (код из Части 1: конструктор, InitializeGame, StartNewHand, PostBlinds()) ...

// --- ЗАПРОС ДЕЙСТВИЯ У ИГРОКА ---
void UOfflineGameManager::RequestPlayerAction(int32 SeatIndex)
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(SeatIndex)) {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Invalid SeatIndex %d or GameStateData null. Broadcasting default/error state."), SeatIndex);
        OnPlayerTurnStartedDelegate.Broadcast(SeatIndex);
        OnTableStateInfoDelegate.Broadcast(TEXT("Error: Invalid State"), GameStateData ? GameStateData->Pot : 0);
        OnPlayerActionsAvailableDelegate.Broadcast({});
        OnActionUIDetailsDelegate.Broadcast(0, GameStateData ? GameStateData->BigBlindAmount : 0, 0);
        return;
    }

    // Сбрасываем флаг bIsTurn у предыдущего игрока, если он был и это не тот же самый игрок
    if (GameStateData->CurrentTurnSeat != -1 &&
        GameStateData->CurrentTurnSeat != SeatIndex &&
        GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat))
    {
        GameStateData->Seats[GameStateData->CurrentTurnSeat].bIsTurn = false;
    }
    GameStateData->CurrentTurnSeat = SeatIndex;
    FPlayerSeatData& CurrentPlayer = GameStateData->Seats[SeatIndex];
    CurrentPlayer.bIsTurn = true;

    OnPlayerTurnStartedDelegate.Broadcast(SeatIndex);
    OnTableStateInfoDelegate.Broadcast(CurrentPlayer.PlayerName, GameStateData->Pot);

    TArray<EPlayerAction> AllowedActions;
    int64 MinBetOrRaiseForAction = GameStateData->BigBlindAmount; // Базовый для Bet/Raise

    // Проверяем, может ли игрок вообще действовать
    bool bCanPlayerAct = CurrentPlayer.bIsSittingIn &&
        CurrentPlayer.Status != EPlayerStatus::Folded &&
        CurrentPlayer.Stack > 0; // Должен иметь фишки для большинства действий

    if (CurrentPlayer.Status == EPlayerStatus::AllIn) {
        // Если игрок уже олл-ин, он не может предпринимать дальнейших действий,
        // если его ставка уже максимальна или равна CurrentBetToCall.
        // Однако, если другие игроки могут еще ставить (и создавать побочные банки),
        // то для них действия еще есть. Для самого AllIn игрока - нет.
        if (GameStateData->CurrentStage >= EGameStage::Preflop && CurrentPlayer.CurrentBet >= GameStateData->CurrentBetToCall)
        {
            bCanPlayerAct = false;
        }
        // Если игрок All-In, но его ставка меньше CurrentBetToCall, и он SB/BB, он не может "PostBlind"
        if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind || GameStateData->CurrentStage == EGameStage::WaitingForBigBlind)
        {
            bCanPlayerAct = false; // Не может ставить блайнд, если уже олл-ин (этот случай должен быть обработан в PostBlinds)
        }
    }


    if (!bCanPlayerAct) {
        UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction: Seat %d (%s) cannot act (Status: %s, Stack: %lld). Broadcasting empty actions."),
            SeatIndex, *CurrentPlayer.PlayerName, *UEnum::GetValueAsString(CurrentPlayer.Status), CurrentPlayer.Stack);
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && SeatIndex == GameStateData->PendingSmallBlindSeat) {
        AllowedActions.Add(EPlayerAction::PostBlind);
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && SeatIndex == GameStateData->PendingBigBlindSeat) {
        AllowedActions.Add(EPlayerAction::PostBlind);
    }
    else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River) {
        AllowedActions.Add(EPlayerAction::Fold);

        if (CurrentPlayer.CurrentBet == GameStateData->CurrentBetToCall) {
            AllowedActions.Add(EPlayerAction::Check);
        }

        if (GameStateData->CurrentBetToCall > CurrentPlayer.CurrentBet && CurrentPlayer.Stack > 0) {
            AllowedActions.Add(EPlayerAction::Call);
        }

        // Логика для Bet и Raise
        // Минимальная сумма для бета - это Большой Блайнд
        // Минимальная сумма для чистого рейза (сверх колла) - это размер последнего бета/рейза, или ББ, если не было агрессии
        int64 MinPureRaiseAmount = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;

        if (AllowedActions.Contains(EPlayerAction::Check) && CurrentPlayer.Stack >= GameStateData->BigBlindAmount) { // Бет не меньше ББ
            AllowedActions.Add(EPlayerAction::Bet);
            MinBetOrRaiseForAction = GameStateData->BigBlindAmount;
        }

        int64 AmountToEffectivelyCall = GameStateData->CurrentBetToCall - CurrentPlayer.CurrentBet;
        if (CurrentPlayer.Stack > AmountToEffectivelyCall && // Есть больше чем на колл
            CurrentPlayer.Stack >= (AmountToEffectivelyCall + MinPureRaiseAmount)) // Хватает на колл + мин.чистый.рейз
        {
            AllowedActions.Add(EPlayerAction::Raise);
            MinBetOrRaiseForAction = MinPureRaiseAmount; // Для UI показываем размер чистого рейза
        }
        // Если игрок может пойти олл-ин, и это будет считаться валидным бетом/коллом/рейзом
        if (CurrentPlayer.Stack > 0 && !AllowedActions.Contains(EPlayerAction::Bet) && !AllowedActions.Contains(EPlayerAction::Raise)) {
            // Если не может сделать стандартный бет/рейз, но может пойти олл-ин
            if (AllowedActions.Contains(EPlayerAction::Call) && CurrentPlayer.Stack <= AmountToEffectivelyCall) {
                // Может только заколлировать олл-ином
            }
            else if (AllowedActions.Contains(EPlayerAction::Check) && CurrentPlayer.Stack < GameStateData->BigBlindAmount && CurrentPlayer.Stack > 0) {
                // Может только чекнуть, но может пойти олл-ин бетом, если бы это было валидно (редкий случай)
            }
            // Для MVP: EPlayerAction::AllIn не добавляем как отдельное действие,
            // оно будет подразумеваться, если игрок ставит весь свой стек через Bet/Call/Raise.
        }
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Unhandled stage %s for seat %d."), *UEnum::GetValueAsString(GameStateData->CurrentStage), SeatIndex);
    }
    OnPlayerActionsAvailableDelegate.Broadcast(AllowedActions);

    int64 BetToCallForUI = 0;
    int64 MinRaiseForUI = GameStateData->BigBlindAmount; // По умолчанию для ставки/рейза

    if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River) {
        BetToCallForUI = GameStateData->CurrentBetToCall - CurrentPlayer.CurrentBet;
        if (BetToCallForUI < 0) BetToCallForUI = 0;

        MinRaiseForUI = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;
        // Сумма, которую нужно добавить к CurrentBetToCall для минимального рейза
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind) {
        MinRaiseForUI = GameStateData->SmallBlindAmount; // Технически, это сумма для PostBlind
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind) {
        MinRaiseForUI = GameStateData->BigBlindAmount; // Сумма для PostBlind
    }


    OnActionUIDetailsDelegate.Broadcast(BetToCallForUI, MinRaiseForUI, CurrentPlayer.Stack);

    UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction for Seat %d (%s). Actions: %d. Stage: %s. Stack: %lld, Pot: %lld, ToCallForUI: %lld, MinRaiseForUI: %lld"),
        SeatIndex, *CurrentPlayer.PlayerName, AllowedActions.Num(),
        *UEnum::GetValueAsString(GameStateData->CurrentStage), CurrentPlayer.Stack, GameStateData->Pot, BetToCallForUI, MinRaiseForUI);
}


// --- ОБРАБОТКА ДЕЙСТВИЙ ИГРОКА ---
void UOfflineGameManager::ProcessPlayerAction(int32 ActingPlayerSeatIndex, EPlayerAction PlayerAction, int64 Amount)
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(ActingPlayerSeatIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: Invalid GameState or ActingPlayerSeatIndex %d"), ActingPlayerSeatIndex);
        return;
    }

    if (GameStateData->CurrentTurnSeat != ActingPlayerSeatIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Received action from Seat %d, but current turn is Seat %d. Requesting action again from correct player."), ActingPlayerSeatIndex, GameStateData->CurrentTurnSeat);
        RequestPlayerAction(GameStateData->CurrentTurnSeat); // Повторно запросим действие у правильного игрока
        return;
    }

    FPlayerSeatData& Player = GameStateData->Seats[ActingPlayerSeatIndex];
    FString PlayerName = Player.PlayerName;

    UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Seat %d (%s) attempts Action: %s, Amount: %lld. Stage: %s. Stack: %lld, PlayerBet: %lld, ToCall: %lld"),
        ActingPlayerSeatIndex, *PlayerName, *UEnum::GetValueAsString(PlayerAction), Amount,
        *UEnum::GetValueAsString(GameStateData->CurrentStage), Player.Stack, Player.CurrentBet, GameStateData->CurrentBetToCall);

    // Обработка Постановки Блайндов (остается такой же, как у вас)
    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeatIndex == GameStateData->PendingSmallBlindSeat) {
            PostBlinds(); // Ставит SB, обновляет Pot, Player.CurrentBet, Player.Stack
            RequestBigBlind();
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Invalid action/player for SB. Expected PostBlind from %d. Got %s from %d."), GameStateData->PendingSmallBlindSeat, *UEnum::GetValueAsString(PlayerAction), ActingPlayerSeatIndex);
            RequestPlayerAction(ActingPlayerSeatIndex);
        }
        return;
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeatIndex == GameStateData->PendingBigBlindSeat) {
            PostBlinds(); // Ставит BB, обновляет Pot, Player.CurrentBet, Player.Stack, CurrentBetToCall, LastAggressor/LastBetOrRaise
            if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Blinds posted. Current Pot: %lld"), GameStateData->Pot));
            DealHoleCardsAndStartPreflop();
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Invalid action/player for BB. Expected PostBlind from %d. Got %s from %d."), GameStateData->PendingBigBlindSeat, *UEnum::GetValueAsString(PlayerAction), ActingPlayerSeatIndex);
            RequestPlayerAction(ActingPlayerSeatIndex);
        }
        return;
    }

    // --- Обработка Игровых Действий (Preflop, Flop, Turn, River) ---
    if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        // Проверяем, может ли игрок вообще действовать
        if (Player.Status == EPlayerStatus::Folded || (Player.Status == EPlayerStatus::AllIn && Player.CurrentBet >= GameStateData->CurrentBetToCall && GameStateData->PlayerWhoOpenedBettingThisRound != -1))
        {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Player %s at Seat %d cannot act (Folded or All-In satisfied action). Finding next player."), *PlayerName, ActingPlayerSeatIndex);
            Player.bIsTurn = false; // Снимаем флаг хода
            // Сразу ищем следующего, так как этот игрок не может действовать.
            // IsBettingRoundOver() будет вызван перед переходом к следующему игроку.
        }
        else // Игрок может действовать
        {
            Player.bIsTurn = false; // Снимаем флаг хода после действия

            switch (PlayerAction)
            {
            case EPlayerAction::Fold:
                Player.Status = EPlayerStatus::Folded;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s folds."), *PlayerName));
                break;

            case EPlayerAction::Check:
                // Валидация: можно ли чекать? (CurrentBetToCall должен быть равен текущей ставке игрока)
                if (Player.CurrentBet == GameStateData->CurrentBetToCall)
                {
                    Player.Status = EPlayerStatus::Playing; // Или EPlayerStatus::Checked
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s checks."), *PlayerName));
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: %s tried to Check, but BetToCall is %lld and player's bet is %lld. Invalid action."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                    RequestPlayerAction(ActingPlayerSeatIndex); // Повторно запросить действие
                    return;
                }
                break;

            case EPlayerAction::Call:
            {
                int64 AmountNeededToCall = GameStateData->CurrentBetToCall - Player.CurrentBet;
                if (AmountNeededToCall <= 0) // Нечего коллировать или уже заколлировал
                {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: %s tried to Call, but nothing to call or already called. BetToCall: %lld, PlayerBet: %lld. Treating as Check if possible."),
                        *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                    // Если это произошло, это может быть ошибка в AllowedActions или UI
                    // Попробуем обработать как Check, если возможно
                    if (Player.CurrentBet == GameStateData->CurrentBetToCall) {
                        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s effectively checks (attempted invalid call)."), *PlayerName));
                    }
                    else {
                        RequestPlayerAction(ActingPlayerSeatIndex); return;
                    }
                    break;
                }

                int64 ActualCallAmount = FMath::Min(AmountNeededToCall, Player.Stack);
                Player.Stack -= ActualCallAmount;
                Player.CurrentBet += ActualCallAmount;
                GameStateData->Pot += ActualCallAmount;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s calls %lld. Stack: %lld"), *PlayerName, ActualCallAmount, Player.Stack));

                if (Player.Stack == 0)
                {
                    Player.Status = EPlayerStatus::AllIn;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                }
                else
                {
                    Player.Status = EPlayerStatus::Playing; // Или EPlayerStatus::Called
                }
            }
            break;

            case EPlayerAction::Bet:
                // Валидация: можно ли бетить? (CurrentBetToCall должен быть 0 или равен ставке игрока)
                if (Player.CurrentBet == GameStateData->CurrentBetToCall)
                {
                    int64 MinBet = GameStateData->BigBlindAmount;
                    if (Amount < MinBet && Amount < Player.Stack) // Нельзя ставить меньше минимума, кроме олл-ина
                    {
                        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: %s Bet amount %lld is less than MinBet %lld. Invalid action."), *PlayerName, Amount, MinBet);
                        RequestPlayerAction(ActingPlayerSeatIndex); return;
                    }
                    if (Amount > Player.Stack) { Amount = Player.Stack; } // Ставка не может быть больше стека (это All-In)

                    Player.Stack -= Amount;
                    Player.CurrentBet += Amount; // Теперь CurrentBet игрока равен Amount
                    GameStateData->Pot += Amount;
                    GameStateData->CurrentBetToCall = Player.CurrentBet; // Новая сумма для колла для других
                    GameStateData->LastBetOrRaiseAmountInCurrentRound = Amount; // Это сумма самого бета
                    GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex;
                    GameStateData->PlayerWhoOpenedBettingThisRound = ActingPlayerSeatIndex; // Этот игрок открыл торги (или сделал первую агрессию)

                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s bets %lld. Stack: %lld"), *PlayerName, Amount, Player.Stack));
                    if (Player.Stack == 0)
                    {
                        Player.Status = EPlayerStatus::AllIn;
                        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                    }
                    else { Player.Status = EPlayerStatus::Playing; } // Или EPlayerStatus::Bet
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: %s tried to Bet, but there's an uncalled bet on the table. Invalid action."), *PlayerName);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                break;

            case EPlayerAction::Raise:
            {
                int64 AmountToCallFirst = GameStateData->CurrentBetToCall - Player.CurrentBet;
                if (AmountToCallFirst < 0) AmountToCallFirst = 0; // На случай, если CurrentBet уже > CurrentBetToCall (ошибка логики)

                int64 PureRaiseAmount = Amount - AmountToCallFirst; // Чистая сумма рейза сверх колла

                // Минимальный чистый рейз должен быть не меньше предыдущего бета/рейза в этом раунде, или ББ если это первый рейз
                int64 MinValidPureRaise = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;

                if (PureRaiseAmount < MinValidPureRaise && (AmountToCallFirst + PureRaiseAmount) < Player.Stack) // Нельзя рейзить меньше минимума, кроме олл-ина
                {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: %s Raise amount %lld (pure %lld) is less than MinValidPureRaise %lld. Invalid action."), *PlayerName, Amount, PureRaiseAmount, MinValidPureRaise);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                if ((AmountToCallFirst + PureRaiseAmount) > Player.Stack) // Если общая сумма для рейза больше стека
                {
                    PureRaiseAmount = Player.Stack - AmountToCallFirst; // Игрок идет олл-ин, рейз на остаток стека
                    Amount = Player.Stack; // Общая сумма ставки = весь стек
                    if (PureRaiseAmount < 0) PureRaiseAmount = 0; // На случай если стек меньше суммы колла
                }
                else {
                    Amount = AmountToCallFirst + PureRaiseAmount; // Полная сумма, которую игрок поставит в банк
                }


                if (Amount <= AmountToCallFirst && Amount < Player.Stack) // Если итоговая ставка не больше колла (и не олл-ин)
                {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: %s Raise total amount %lld is not greater than amount to call %lld. Invalid raise."), *PlayerName, Amount, AmountToCallFirst);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }


                Player.Stack -= Amount; // Снимаем полную сумму, которую он добавляет в банк в этом действии
                GameStateData->Pot += Amount;
                Player.CurrentBet += Amount; // Обновляем общую ставку игрока в этом раунде

                GameStateData->CurrentBetToCall = Player.CurrentBet; // Новая сумма для колла
                GameStateData->LastBetOrRaiseAmountInCurrentRound = Player.CurrentBet - (GameStateData->CurrentBetToCall - Amount); // Сумма этого конкретного рейза (новая ставка - старая ставка для колла)
                // или просто PureRaiseAmount, если он валиден
                GameStateData->LastBetOrRaiseAmountInCurrentRound = PureRaiseAmount > 0 ? PureRaiseAmount : GameStateData->BigBlindAmount; // Убедимся, что это валидная сумма
                GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex;
                // PlayerWhoOpenedBettingThisRound не меняется при рейзе, он остается тем, кто сделал первый бет или BB

                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s raises to %lld (total bet: %lld). Stack: %lld"),
                    *PlayerName, Player.CurrentBet, Player.CurrentBet, Player.Stack)); // Сообщаем общую ставку, до которой дорейзил

                if (Player.Stack == 0)
                {
                    Player.Status = EPlayerStatus::AllIn;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                }
                else { Player.Status = EPlayerStatus::Playing; } // Или EPlayerStatus::Raised
            }
            break;

            default:
                UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Unknown action %s received."), *UEnum::GetValueAsString(PlayerAction));
                RequestPlayerAction(ActingPlayerSeatIndex); // Запросить снова, если действие непонятно
                return;
            }
        } // конец if (bCanPlayerAct)

        // --- Логика после действия игрока ---
        if (IsBettingRoundOver())
        {
            ProceedToNextGameStage();
        }
        else
        {
            int32 NextPlayerToAct = GetNextPlayerToAct(ActingPlayerSeatIndex, false);
            if (NextPlayerToAct != -1)
            {
                RequestPlayerAction(NextPlayerToAct);
            }
            else
            {
                // Это может произойти, если все остальные сфолдили или олл-ин, и IsBettingRoundOver() это не поймало
                // (например, если остался один не-оллиновый игрок, а остальные олл-ин с меньшими ставками)
                // Или если GetNextPlayerToAct вернул -1 из-за ошибки.
                UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: No next player to act, but betting round not flagged as over by IsBettingRoundOver. Forcing ProceedToNextGameStage. Stage: %s"), *UEnum::GetValueAsString(GameStateData->CurrentStage));
                ProceedToNextGameStage();
            }
        }
        return;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Action %s received in unhandled game stage %s."), *UEnum::GetValueAsString(PlayerAction), *UEnum::GetValueAsString(GameStateData->CurrentStage));
        // Возможно, просто запросить действие у текущего игрока снова, если стадия неизвестна
        RequestPlayerAction(ActingPlayerSeatIndex);
    }
}

// --- НОВАЯ Функция для Запроса Большого Блайнда ---
void UOfflineGameManager::RequestBigBlind()
{
    if (!GameStateData) return;

    GameStateData->CurrentStage = EGameStage::WaitingForBigBlind;
    if (GameStateData->Seats.IsValidIndex(GameStateData->PendingBigBlindSeat))
    {
        GameStateData->Seats[GameStateData->PendingBigBlindSeat].Status = EPlayerStatus::MustPostBigBlind;
        UE_LOG(LogTemp, Log, TEXT("Requesting Big Blind from Seat %d (%s)."), GameStateData->PendingBigBlindSeat, *GameStateData->Seats[GameStateData->PendingBigBlindSeat].PlayerName);
        RequestPlayerAction(GameStateData->PendingBigBlindSeat);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("RequestBigBlind: BB Seat index %d is invalid!"), GameStateData->PendingBigBlindSeat);
        // Обработка ошибки - возможно, завершить руку или игру
    }
}


// DealHoleCardsAndStartPreflop (остается как у вас)
void UOfflineGameManager::DealHoleCardsAndStartPreflop()
{
    if (!GameStateData || !Deck) { UE_LOG(LogTemp, Error, TEXT("DealHoleCards: Null GameState/Deck")); return; }
    UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Dealing hole cards..."));
    OnGameHistoryEventDelegate.Broadcast(TEXT("Dealing hole cards..."));

    int32 NumPlayersActuallyPlaying = 0;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) { if (Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing) NumPlayersActuallyPlaying++; }

    if (NumPlayersActuallyPlaying < 2) {
        UE_LOG(LogTemp, Warning, TEXT("DealHoleCards: Not enough players (%d) in Playing status to deal hole cards."), NumPlayersActuallyPlaying);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Возврат в ожидание
        // Уведомить UI
        OnPlayerTurnStartedDelegate.Broadcast(-1);
        OnTableStateInfoDelegate.Broadcast(TEXT("Error: Not enough players for deal"), GameStateData->Pot);
        OnPlayerActionsAvailableDelegate.Broadcast({});
        OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
        return;
    }

    // Определяем, с кого начинать раздачу (обычно SB или следующий активный после дилера)
    int32 StartDealingFrom = GetNextPlayerToAct(GameStateData->DealerSeat, false);
    if (NumPlayersActuallyPlaying == 2) { // В хедз-апе дилер (SB) получает карту первым
        StartDealingFrom = GameStateData->DealerSeat; // Который был PendingSmallBlindSeat
    }

    if (StartDealingFrom == -1) { // Не должно произойти, если есть активные игроки
        UE_LOG(LogTemp, Error, TEXT("DealHoleCards: Cannot determine who to deal cards to first!"));
        return;
    }

    int32 SeatToDeal = StartDealingFrom;
    for (int32 CardNum = 0; CardNum < 2; ++CardNum) { // Две карты каждому
        for (int32 i = 0; i < NumPlayersActuallyPlaying; ++i) {
            if (SeatToDeal != -1 && GameStateData->Seats.IsValidIndex(SeatToDeal) && GameStateData->Seats[SeatToDeal].Status == EPlayerStatus::Playing) {
                TOptional<FCard> DealtCardOptional = Deck->DealCard();
                if (DealtCardOptional.IsSet()) {
                    GameStateData->Seats[SeatToDeal].HoleCards.Add(DealtCardOptional.GetValue());
                }
                else {
                    UE_LOG(LogTemp, Error, TEXT("Deck ran out of cards during hole card dealing!"));
                    GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
                }
            }
            else { // Эта ветка не должна срабатывать, если NumPlayersActuallyPlaying посчитан верно и GetNextPlayerToAct работает
                UE_LOG(LogTemp, Error, TEXT("Invalid SeatToDeal %d or player not in Playing status during dealing."), SeatToDeal);
                GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
            }

            SeatToDeal = GetNextPlayerToAct(SeatToDeal, false); // Следующий активный для получения карты
            if (SeatToDeal == -1 && i < NumPlayersActuallyPlaying - 1) { // Если не нашли следующего, а карты еще не всем розданы
                UE_LOG(LogTemp, Error, TEXT("Could not find next player to deal card to while cards still need to be dealt."));
                GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
            }
        }
    }

    // Логирование розданных карт
    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.Status == EPlayerStatus::Playing && Seat.HoleCards.Num() == 2) {
            UE_LOG(LogTemp, Log, TEXT("Seat %d (%s) received: %s, %s"), Seat.SeatIndex, *Seat.PlayerName, *Seat.HoleCards[0].ToString(), *Seat.HoleCards[1].ToString());
        }
    }
    OnGameHistoryEventDelegate.Broadcast(TEXT("Hole cards dealt. Preflop betting starts."));

    // Установка стадии Preflop и определение первого ходящего
    GameStateData->CurrentStage = EGameStage::Preflop;
    GameStateData->PlayerWhoOpenedBettingThisRound = DetermineFirstPlayerToActAtPreflop(); // Кто открывает торги
    GameStateData->LastAggressorSeatIndex = GameStateData->PendingBigBlindSeat; // Последний агрессор - BB
    GameStateData->LastBetOrRaiseAmountInCurrentRound = GameStateData->BigBlindAmount; // Последняя ставка/рейз - это ББ
    // CurrentBetToCall уже установлен в BigBlindAmount после постановки BB

    int32 FirstToActSeat = GameStateData->PlayerWhoOpenedBettingThisRound;
    if (FirstToActSeat == -1) {
        UE_LOG(LogTemp, Error, TEXT("Could not determine first to act for Preflop!"));
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
    }

    UE_LOG(LogTemp, Log, TEXT("Preflop round. First to act: Seat %d (%s). CurrentBetToCall: %lld"),
        FirstToActSeat, *GameStateData->Seats[FirstToActSeat].PlayerName, GameStateData->CurrentBetToCall);
    RequestPlayerAction(FirstToActSeat);
}

// DetermineFirstPlayerToActAtPreflop (переименовано из AfterBlinds)
int32 UOfflineGameManager::DetermineFirstPlayerToActAtPreflop() const
{
    if (!GameStateData || GameStateData->PendingBigBlindSeat == -1) return -1;

    int32 NumPlayersConsideredActive = 0;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        // Считаем активными тех, кто не сфолдил и имеет фишки (или уже олл-ин, но еще в игре)
        if (Seat.bIsSittingIn && (Seat.Status == EPlayerStatus::Playing || Seat.Status == EPlayerStatus::AllIn)) {
            NumPlayersConsideredActive++;
        }
    }

    if (NumPlayersConsideredActive == 2) { // Хедз-ап, после постановки блайндов
        // Игрок, который поставил SB (и является дилером), ходит первым.
        return GameStateData->PendingSmallBlindSeat;
    }
    else {
        // Следующий активный игрок после большого блайнда.
        return GetNextPlayerToAct(GameStateData->PendingBigBlindSeat, false);
    }
}

// Вспомогательные функции (GetNextPlayerToAct, DetermineFirstPlayerToActPostflop - как у вас)
int32 UOfflineGameManager::GetNextPlayerToAct(int32 StartSeatIndex, bool bExcludeStartSeat) const
{
    if (!GameStateData || GameStateData->Seats.Num() == 0) { 
        UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: GameStateData is null or no seats."));
        return -1;
    }

    const int32 NumSeats = GameStateData->Seats.Num();
    if (NumSeats == 0) { // Дополнительная проверка
        UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: Seats array is empty."));
        return -1;
    }

    int32 CurrentIndex = StartSeatIndex;

    // Проверка валидности StartSeatIndex перед использованием в математике
    if (!GameStateData->Seats.IsValidIndex(StartSeatIndex)) {
        UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: Invalid StartSeatIndex: %d"), StartSeatIndex);
        // Можно попробовать начать с 0 или вернуть ошибку
        CurrentIndex = 0;
        // Если StartSeatIndex был результатом предыдущего GetNextPlayerToAct, который вернул -1, это проблема.
        // Но для определения ролей это может быть нормально, если DealerSeat был -1.
    }


    if (bExcludeStartSeat) {
        CurrentIndex = (CurrentIndex + 1) % NumSeats;
    }

    for (int32 i = 0; i < NumSeats; ++i) { // Цикл не более NumSeats раз для обхода всех мест
        if (!GameStateData->Seats.IsValidIndex(CurrentIndex)) {
            UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: CurrentIndex %d became invalid during loop."), CurrentIndex);
            CurrentIndex = (CurrentIndex + 1) % NumSeats; // Переходим к следующему, чтобы избежать сбоя
            continue;
        }

        const FPlayerSeatData& Seat = GameStateData->Seats[CurrentIndex];
        bool bIsEligible = false;

        if (Seat.bIsSittingIn) // Основное условие - игрок сидит за столом
        {
            // Если это этап определения ролей (дилер, SB, BB), до того как установлены MustPost... статусы,
            // или если мы просто ищем следующего "живого" игрока, например, для раздачи карт.
            // В StartNewHand мы временно ставим Status = Playing для всех, кто в игре.
            // Таким образом, эта часть условия сработает для определения Дилера, SB, BB.
            if (Seat.Status == EPlayerStatus::Playing || Seat.Status == EPlayerStatus::Waiting)
            {
                if (Seat.Stack > 0) { // Должен иметь фишки, чтобы быть дилером/блайндом или играть
                    bIsEligible = true;
                }
            }
            // Если это этап постановки Малого Блайнда
            else if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind)
            {
                bIsEligible = (Seat.Status == EPlayerStatus::MustPostSmallBlind &&
                    CurrentIndex == GameStateData->PendingSmallBlindSeat &&
                    Seat.Stack > 0);
            }
            // Если это этап постановки Большого Блайнда
            else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind)
            {
                bIsEligible = (Seat.Status == EPlayerStatus::MustPostBigBlind &&
                    CurrentIndex == GameStateData->PendingBigBlindSeat &&
                    Seat.Stack > 0);
            }
            // Если это активный раунд торгов (Preflop, Flop, Turn, River)
            else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
            {
                if (Seat.Status == EPlayerStatus::Playing && Seat.Stack > 0)
                {
                    // Игрок со статусом Playing и стеком > 0 всегда может действовать (хотя бы Fold)
                    bIsEligible = true;
                }
                else if (Seat.Status == EPlayerStatus::AllIn)
                {
                    // Игрок All-In считается "активным" для определения конца раунда,
                    // но не для запроса действия, если его ставка уже максимальна или равна CurrentBetToCall
                    // и торги уже были открыты (PlayerWhoOpenedBettingThisRound != -1).
                    // GetNextPlayerToAct ищет того, КТО СЛЕДУЮЩИЙ ХОДИТ.
                    // Игрок AllIn, чья ставка уже соответствует CurrentBetToCall (или больше), не ходит.
                    if (Seat.CurrentBet < GameStateData->CurrentBetToCall && GameStateData->PlayerWhoOpenedBettingThisRound != -1) {
                        // Этот случай редкий, обычно AllIn означает, что CurrentBet = Stack (на момент ставки).
                        // Этот AllIn может быть в ситуации, когда он поставил меньше, чем текущий CurrentBetToCall,
                        // и другие еще могут ставить. Он не будет делать ход, но учитывается.
                        // Для определения СЛЕДУЮЩЕГО ХОДЯЩЕГО, мы его пропускаем, если он не может повлиять.
                        // bIsEligible = true; // Раскомментируйте, если хотите, чтобы AllIn всегда считался для цикла хода
                    }
                    // Для простоты, если игрок AllIn, он больше не делает ходов.
                    // Его пропускаем при поиске следующего для *активного* действия.
                    bIsEligible = false;
                }
                // Игроки со статусом Folded или SittingOut не являются eligible.
            }
        } // конец if (Seat.bIsSittingIn)

        if (bIsEligible) {
            return CurrentIndex;
        }

        CurrentIndex = (CurrentIndex + 1) % NumSeats;
    }

    UE_LOG(LogTemp, Warning, TEXT("GetNextPlayerToAct: No eligible player found from Seat %d (excluded: %s), Stage: %s. NumSeats: %d"),
        StartSeatIndex, bExcludeStartSeat ? TEXT("true") : TEXT("false"),
        *UEnum::GetValueAsString(GameStateData->CurrentStage), NumSeats);
    // Дополнительное логирование состояния всех мест для отладки
    for (const auto& SeatDebug : GameStateData->Seats) {
        UE_LOG(LogTemp, Warning, TEXT("  Seat %d: Stack %lld, Status %s, SittingIn %d, IsTurn %d"),
            SeatDebug.SeatIndex, SeatDebug.Stack, *UEnum::GetValueAsString(SeatDebug.Status), SeatDebug.bIsSittingIn, SeatDebug.bIsTurn);
    }

    return -1; // Не найдено подходящих игроков
}

int32 UOfflineGameManager::DetermineFirstPlayerToActPostflop() const
{
    if (!GameStateData || GameStateData->DealerSeat == -1) return -1;
    // Первый активный игрок слева от дилера (включая SB, если он еще в игре)
    return GetNextPlayerToAct(GameStateData->DealerSeat, false);
}

bool UOfflineGameManager::IsBettingRoundOver() const
{
    if (!GameStateData|| GameStateData->Seats.Num() < 2) return true; // Недостаточно игроков

    // Если это этап постановки блайндов, раунд не закончен, пока они не поставлены
    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind || GameStateData->CurrentStage == EGameStage::WaitingForBigBlind)
    {
        return false;
    }

    int32 NumPlayersStillInHand = 0; // Игроки, которые не сфолдили
    int32 NumPlayersWhoCanStillAct = 0; // Игроки, которые не сфолдили и не олл-ин (или олл-ин на меньшую сумму)
    int32 HighestBetThisRound = GameStateData->CurrentBetToCall; // Текущая максимальная ставка для колла в этом раунде

    TArray<int32> PlayerIndicesInHand;
    for (int32 i = 0; i < GameStateData->Seats.Num(); ++i)
    {
        const FPlayerSeatData& Seat = GameStateData->Seats[i];
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded)
        {
            NumPlayersStillInHand++;
            PlayerIndicesInHand.Add(i);

            // Игрок может действовать, если он не олл-ин ИЛИ его олл-ин меньше текущей ставки для колла
            // (т.е. другие еще могут ставить поверх него) И у него есть фишки для ставки (если не олл-ин).
            // Исключаем тех, кто уже AllIn и их ставка равна CurrentBetToCall, или они просто Playing/Waiting
            // и их CurrentBet равен CurrentBetToCall, но это НЕ тот игрок, который открыл торги (или сделал последнюю агрессию)
            // и еще не все сделали ход.
            if (Seat.Status == EPlayerStatus::Playing && Seat.Stack > 0)
            {
                // Если игрок еще не уравнял текущую ставку, он может действовать
                if (Seat.CurrentBet < HighestBetThisRound)
                {
                    NumPlayersWhoCanStillAct++;
                }
                // Если игрок уравнял И это НЕ тот, кто открыл торги в этом раунде (PlayerWhoOpenedBettingThisRound)
                // ИЛИ если он LastAggressor и круг еще не дошел до него снова.
                // Это сложная часть.
                // Упрощенно: если все активные игроки имеют CurrentBet == HighestBetThisRound, и круг прошел.
            }
        }
    }

    if (NumPlayersStillInHand <= 1)
    {
        UE_LOG(LogTemp, Log, TEXT("IsBettingRoundOver: True (<=1 player left in hand)."));
        return true; // Раздача заканчивается, если остался один или ноль игроков
    }

    // Проверяем, все ли оставшиеся в игре игроки сделали свою ставку в этом раунде,
    // и их ставки равны максимальной ставке в раунде ИЛИ они олл-ин.
    bool bAllBetsMatchedOrAllIn = true;
    int32 PotentialActingPlayerCount = 0; // Сколько игроков все еще могут сделать ход (не Folded, не All-In на текущую ставку)

    for (int32 SeatIndex : PlayerIndicesInHand)
    {
        const FPlayerSeatData& Seat = GameStateData->Seats[SeatIndex];
        if (Seat.Status == EPlayerStatus::Folded) continue;

        // Игрок должен действовать, если он не All-In и его ставка меньше текущей ставки для колла
        if (Seat.Status != EPlayerStatus::AllIn && Seat.CurrentBet < HighestBetThisRound)
        {
            PotentialActingPlayerCount++;
            // UE_LOG(LogTemp, Verbose, TEXT("IsBettingRoundOver: Seat %d can still act (Bet: %lld, ToCall: %lld, Status: %s)"), SeatIndex, Seat.CurrentBet, HighestBetThisRound, *UEnum::GetValueAsString(Seat.Status));
        }
        // Игрок также должен действовать, если он открыл торги (GameStateData->PlayerWhoOpenedBettingThisRound == SeatIndex)
        // и ход еще не вернулся к нему после того, как все остальные уравняли или сфолдили.
        // ИЛИ если он был последним агрессором (GameStateData->LastAggressorSeatIndex == SeatIndex)
        // и все остальные, кто перед ним, уже сделали ход (уравняли/сфолдили).
        // ЭТО САМАЯ СЛОЖНАЯ ЧАСТЬ: определить, что круг ставок действительно замкнулся.

        // Упрощенная проверка для MVP: если никто больше не может повысить или сделать ставку,
        // и все, кто не олл-ин, имеют ставки равные CurrentBetToCall.
        if (Seat.Status != EPlayerStatus::AllIn && Seat.CurrentBet != HighestBetThisRound)
        {
            bAllBetsMatchedOrAllIn = false; // Есть игрок, который не уравнял и не олл-ин
            // UE_LOG(LogTemp, Verbose, TEXT("IsBettingRoundOver: False, Seat %d has not matched bet (Bet: %lld, ToCall: %lld)"), SeatIndex, Seat.CurrentBet, HighestBetThisRound);
            // break; // Достаточно одного такого
        }
    }

    // Если нет игроков, которые МОГУТ сделать ход (т.е. все либо уравняли, либо олл-ин, либо сфолдили)
    // И если торги уже были открыты (т.е. PlayerWhoOpenedBettingThisRound НЕ равен текущему игроку, чей ход должен быть следующим,
    // или если PlayerWhoOpenedBettingThisRound == -1, но CurrentBetToCall > 0 (т.е. BB был поставлен))
    if (PotentialActingPlayerCount == 0 && GameStateData->PlayerWhoOpenedBettingThisRound != -1) {
        // Дополнительно нужно проверить, что текущий игрок, которому должен перейти ход,
        // это не тот, кто открыл торги (или был последним агрессором), и что все между ними уже походили.
        // Самый простой способ - если PlayerWhoOpenedBettingThisRound это следующий игрок по GetNextPlayerToAct.
        int32 NextToActIfRoundContinues = GetNextPlayerToAct(GameStateData->CurrentTurnSeat, false); // кто ходил бы следующим
        if (NextToActIfRoundContinues == GameStateData->PlayerWhoOpenedBettingThisRound || // Круг дошел до того, кто открыл торги
            NextToActIfRoundContinues == GameStateData->LastAggressorSeatIndex || // Круг дошел до последнего агрессора
            (NumPlayersStillInHand > 0 && PotentialActingPlayerCount == 0) // Или просто никто больше не может ходить
            )
        {
            if (GameStateData->CurrentStage != EGameStage::Preflop || GameStateData->LastAggressorSeatIndex != GameStateData->PendingBigBlindSeat || GameStateData->CurrentBetToCall > GameStateData->BigBlindAmount || PlayerIndicesInHand.Num() <= 2)
            { // Эта проверка нужна, чтобы избежать немедленного завершения префлопа, если BB чекает (опция)
                UE_LOG(LogTemp, Log, TEXT("IsBettingRoundOver: True (PotentialActingPlayerCount is 0 and round has likely completed). NextToAct: %d, Opener: %d, Aggressor: %d"), NextToActIfRoundContinues, GameStateData->PlayerWhoOpenedBettingThisRound, GameStateData->LastAggressorSeatIndex);
                return true;
            }
        }
    }


    // UE_LOG(LogTemp, Verbose, TEXT("IsBettingRoundOver: False (Default). PotentialActingPlayers: %d. AllBetsMatchedOrAllIn: %s"), PotentialActingPlayerCount, bAllBetsMatchedOrAllIn ? TEXT("true") : TEXT("false"));
    return false;
}

void UOfflineGameManager::ProceedToNextGameStage()
{
    if (!GameStateData || !Deck) return;

    // Подсчитываем, сколько игроков еще в игре (не сфолдили)
    int32 PlayersLeftInHand = 0;
    int32 WinnerIfOneLeft = -1;
    for (const FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded)
        {
            PlayersLeftInHand++;
            WinnerIfOneLeft = Seat.SeatIndex; // Запоминаем последнего
        }
    }

    if (PlayersLeftInHand <= 1)
    {
        UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Only %d player(s) left. Awarding pot."), PlayersLeftInHand);
        if (PlayersLeftInHand == 1 && WinnerIfOneLeft != -1)
        {
            AwardPotToWinner({ WinnerIfOneLeft });
        }
        else // 0 игроков или ошибка
        {
            UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage: 0 players left or error. Pot might be split or returned (not implemented for MVP)."));
            // Для MVP можно просто начать новую руку или завершить игру
            // Если банк есть, а игроков 0 - это ошибка логики где-то.
        }
        // После AwardPotToWinner можно сразу StartNewHand() или через UI кнопку
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Готовимся к новой руке
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over"), GameStateData->Pot); // Pot должен быть 0 после AwardPot
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
        // TODO: Возможно, здесь нужно будет вызвать StartNewHand() после небольшой задержки
        return;
    }


    // Сброс ставок текущего раунда для всех активных игроков
    for (FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.Status != EPlayerStatus::Folded && Seat.Status != EPlayerStatus::SittingOut)
        {
            Seat.CurrentBet = 0; // Ставки для нового раунда начинаются с нуля
        }
    }
    GameStateData->CurrentBetToCall = 0;
    GameStateData->LastBetOrRaiseAmountInCurrentRound = 0;
    GameStateData->LastAggressorSeatIndex = -1;
    GameStateData->PlayerWhoOpenedBettingThisRound = -1; // Сбрасываем, кто открывал торги

    EGameStage NextStage = EGameStage::Showdown; // По умолчанию, если все стадии пройдены

    switch (GameStateData->CurrentStage)
    {
    case EGameStage::Preflop:
        NextStage = EGameStage::Flop;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- Proceeding to Flop ---"));
        // Deck->DealCard(); // Сжигаем карту (опционально)
        if (Deck->NumCardsLeft() < 3) { UE_LOG(LogTemp, Error, TEXT("Not enough cards for Flop!")); /* TODO: Handle error */ return; }
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Flop: %s %s %s"),
            *GameStateData->CommunityCards[0].ToString(),
            *GameStateData->CommunityCards[1].ToString(),
            *GameStateData->CommunityCards[2].ToString()));
        break;
    case EGameStage::Flop:
        NextStage = EGameStage::Turn;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- Proceeding to Turn ---"));
        // Deck->DealCard(); // Сжигаем карту
        if (Deck->NumCardsLeft() < 1) { UE_LOG(LogTemp, Error, TEXT("Not enough cards for Turn!")); /* TODO: Handle error */ return; }
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Turn: %s"), *GameStateData->CommunityCards[3].ToString()));
        break;
    case EGameStage::Turn:
        NextStage = EGameStage::River;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- Proceeding to River ---"));
        // Deck->DealCard(); // Сжигаем карту
        if (Deck->NumCardsLeft() < 1) { UE_LOG(LogTemp, Error, TEXT("Not enough cards for River!")); /* TODO: Handle error */ return; }
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("River: %s"), *GameStateData->CommunityCards[4].ToString()));
        break;
    case EGameStage::River:
        NextStage = EGameStage::Showdown; // После Ривера идем на вскрытие
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- Final betting round ended. Proceeding to Showdown ---"));
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage called from unexpected stage: %s"), *UEnum::GetValueAsString(GameStateData->CurrentStage));
        return; // Ничего не делаем, если стадия некорректна
    }

    if (OnCommunityCardsUpdatedDelegate.IsBound()) OnCommunityCardsUpdatedDelegate.Broadcast(GameStateData->CommunityCards);

    GameStateData->CurrentStage = NextStage;

    if (NextStage == EGameStage::Showdown)
    {
        ProceedToShowdown();
    }
    else // Flop, Turn, River - начинаем новый круг торгов
    {
        int32 FirstToAct = DetermineFirstPlayerToActPostflop();
        if (FirstToAct != -1)
        {
            GameStateData->PlayerWhoOpenedBettingThisRound = FirstToAct; // Первый игрок на постфлопе "открывает" торги
            RequestPlayerAction(FirstToAct);
        }
        else // Все олл-ин или сфолдили, и это не шоудаун (маловероятно, но возможно)
        {
            UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage: No player to act on %s. Advancing again."), *UEnum::GetValueAsString(NextStage));
            // Это может зациклить, если логика GetNextPlayerToAct или IsBettingRoundOver некорректна
            // Для безопасности, если нет игроков для хода, возможно, сразу на шоудаун (если есть карты)
            if (GameStateData->CommunityCards.Num() >= 3) ProceedToShowdown();
            else { /* Ошибка, не должно быть */ }
        }
    }
}

void UOfflineGameManager::ProceedToShowdown()
{
    if (!GameStateData) return;
    UE_LOG(LogTemp, Log, TEXT("--- PROCEEDING TO SHOWDOWN ---"));
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- Showdown ---"));
    GameStateData->CurrentStage = EGameStage::Showdown;

    TArray<int32> ShowdownPlayerIndices;
    TArray<FPokerHandResult> PlayerHandResults;
    TArray<int32> WinningSeatIndices;

    // 1. Определяем игроков, участвующих в шоудауне (не сфолдили)
    for (const FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded)
        {
            ShowdownPlayerIndices.Add(Seat.SeatIndex);
        }
    }

    if (ShowdownPlayerIndices.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("Showdown: No players to showdown? This shouldn't happen."));
        // TODO: Обработать эту ситуацию (возможно, остался один игрок, который забрал банк до этого)
        StartNewHand(); // Начать новую руку
        return;
    }

    if (ShowdownPlayerIndices.Num() == 1) {
        UE_LOG(LogTemp, Log, TEXT("Showdown: Only one player left, awarding pot."));
        AwardPotToWinner(ShowdownPlayerIndices); // Передаем массив с одним элементом
        StartNewHand(); // Начать новую руку
        return;
    }

    // 2. Оцениваем руки каждого участника шоудауна
    for (int32 SeatIndex : ShowdownPlayerIndices)
    {
        const FPlayerSeatData& Player = GameStateData->Seats[SeatIndex];
        FPokerHandResult HandResult = UPokerHandEvaluator::EvaluatePokerHand(Player.HoleCards, GameStateData->CommunityCards);
        PlayerHandResults.Add(HandResult); // Сохраняем результат
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s has %s"), *Player.PlayerName, *UEnum::GetValueAsString(HandResult.HandRank)));
        // TODO: Показать карты игрока UI (возможно, через новый делегат или APokerPlayerController::HandleShowdown это сделает)
    }

    // 3. Определяем победителя(ей)
    if (PlayerHandResults.Num() > 0)
    {
        FPokerHandResult BestHand = PlayerHandResults[0];
        WinningSeatIndices.Add(ShowdownPlayerIndices[0]);

        for (int32 i = 1; i < PlayerHandResults.Num(); ++i)
        {
            int32 CompareResult = UPokerHandEvaluator::CompareHandResults(PlayerHandResults[i], BestHand);
            if (CompareResult > 0) // Текущая рука лучше
            {
                BestHand = PlayerHandResults[i];
                WinningSeatIndices.Empty();
                WinningSeatIndices.Add(ShowdownPlayerIndices[i]);
            }
            else if (CompareResult == 0) // Руки равны (ничья)
            {
                WinningSeatIndices.Add(ShowdownPlayerIndices[i]);
            }
        }
    }

    // Уведомляем контроллер о шоудауне и участниках (чтобы он мог показать их карты)
    if (OnShowdownDelegate.IsBound()) OnShowdownDelegate.Broadcast(ShowdownPlayerIndices);

    // Небольшая задержка перед объявлением победителя и раздачей банка, чтобы UI успел показать карты
    // TODO: Реализовать это через таймер или передать управление UI

    // 4. Награждаем победителя(ей)
    if (WinningSeatIndices.Num() > 0)
    {
        AwardPotToWinner(WinningSeatIndices);
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("Showdown: No winners determined!"));
    }

    // 5. Подготовка к следующей руке
    // Можно добавить кнопку "Next Hand" в UI или автоматический старт через таймер
    // Пока что для теста можно сразу вызывать StartNewHand() или просто менять состояние
    // GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
    // if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
    // if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over. Click to Start New Hand."), GameStateData->Pot); // Pot должен быть 0
    // if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
    // if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
    // Для автоматического продолжения теста:
    // TODO: Рассмотреть задержку перед StartNewHand
    StartNewHand();
}

void UOfflineGameManager::AwardPotToWinner(const TArray<int32>& WinningSeatIndices)
{
    if (!GameStateData || WinningSeatIndices.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("AwardPotToWinner: No GameState or no winners. Pot: %lld"), GameStateData ? GameStateData->Pot : -1);
        return;
    }

    int64 TotalPot = GameStateData->Pot;
    if (TotalPot <= 0) {
        UE_LOG(LogTemp, Log, TEXT("AwardPotToWinner: Pot is 0 or less, nothing to award."));
        GameStateData->Pot = 0; // Убедимся, что он 0
        return;
    }

    int64 ShareOfPot = TotalPot / WinningSeatIndices.Num(); // Делим банк поровну
    int64 Remainder = TotalPot % WinningSeatIndices.Num();  // Остаток от деления

    FString WinnersString = TEXT("Winner(s): ");
    for (int32 i = 0; i < WinningSeatIndices.Num(); ++i)
    {
        int32 WinnerIdx = WinningSeatIndices[i];
        if (GameStateData->Seats.IsValidIndex(WinnerIdx))
        {
            FPlayerSeatData& Winner = GameStateData->Seats[WinnerIdx];
            int64 CurrentShare = ShareOfPot;
            if (i == 0 && Remainder > 0) // Отдаем остаток первому в списке победителей (стандартная практика)
            {
                CurrentShare += Remainder;
            }
            Winner.Stack += CurrentShare;
            WinnersString += FString::Printf(TEXT("%s (+%lld, Stack: %lld) "), *Winner.PlayerName, CurrentShare, Winner.Stack);
        }
    }
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(WinnersString);
    UE_LOG(LogTemp, Log, TEXT("AwardPotToWinner: %s. Total Pot was %lld."), *WinnersString, TotalPot);

    GameStateData->Pot = 0; // Обнуляем основной банк
    // TODO: Логика для Side Pots, если будет реализована
}