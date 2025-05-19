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

    if (OnNewHandAboutToStartDelegate.IsBound())
    {
        UE_LOG(LogTemp, Log, TEXT("StartNewHand: Broadcasting OnNewHandAboutToStartDelegate.")); // Добавьте лог
        OnNewHandAboutToStartDelegate.Broadcast();
    }

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
        Seat.bHasActedThisSubRound = false;
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
        GameStateData->DealerSeat = GetNextPlayerToAct(GameStateData->DealerSeat, false, EPlayerStatus::Playing);
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
        GameStateData->PendingBigBlindSeat = GetNextPlayerToAct(GameStateData->PendingSmallBlindSeat, true, EPlayerStatus::Playing); // <--- ИЗМЕНЕНИЕ: bExcludeStartSeat = true
    }
    else { // 3+ игроков
        GameStateData->PendingSmallBlindSeat = GetNextPlayerToAct(GameStateData->DealerSeat, true, EPlayerStatus::Playing); // <--- ИЗМЕНЕНИЕ: bExcludeStartSeat = true (следующий после дилера)
        if (GameStateData->PendingSmallBlindSeat != -1) {
            // Ищем BB, начиная СРАЗU СО СЛЕДУЮЩЕГО после SB, исключая самого SB
            GameStateData->PendingBigBlindSeat = GetNextPlayerToAct(GameStateData->PendingSmallBlindSeat, true, EPlayerStatus::Playing); // <--- ИЗМЕНЕНИЕ: bExcludeStartSeat = true
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


void UOfflineGameManager::ProcessPlayerAction(int32 ActingPlayerSeatIndex, EPlayerAction PlayerAction, int64 Amount)
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(ActingPlayerSeatIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: Invalid GameState or ActingPlayerSeatIndex %d. Cannot process action."), ActingPlayerSeatIndex);
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(GameStateData ? GameStateData->CurrentTurnSeat : -1);
        return;
    }

    if (GameStateData->CurrentTurnSeat != ActingPlayerSeatIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Action received from Seat %d, but current turn is Seat %d. Re-requesting action from correct player."), ActingPlayerSeatIndex, GameStateData->CurrentTurnSeat);
        RequestPlayerAction(GameStateData->CurrentTurnSeat);
        return;
    }

    FPlayerSeatData& Player = GameStateData->Seats[ActingPlayerSeatIndex];
    FString PlayerName = Player.PlayerName;

    UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Seat %d (%s) attempts Action: %s, Amount: %lld. Stage: %s. Stack Before: %lld, PlayerBetBefore: %lld, ToCall: %lld, LastAggressor: %d, Opener: %d, HasActedBefore: %s"),
        ActingPlayerSeatIndex, *PlayerName, *UEnum::GetValueAsString(PlayerAction), Amount,
        *UEnum::GetValueAsString(GameStateData->CurrentStage), Player.Stack, Player.CurrentBet, GameStateData->CurrentBetToCall,
        GameStateData->LastAggressorSeatIndex, GameStateData->PlayerWhoOpenedBettingThisRound, Player.bHasActedThisSubRound ? TEXT("true") : TEXT("false"));

    // Обработка Постановки Блайндов
    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeatIndex == GameStateData->PendingSmallBlindSeat) {
            PostBlinds(); // Обновляет стек SB, банк, CurrentBet SB, статус SB на Playing
            // Player.bHasActedThisSubRound не устанавливаем здесь, будет сброшено в DealHoleCards
            RequestBigBlind();
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid action/player for SB. Expected PostBlind from Seat %d. Re-requesting."), GameStateData->PendingSmallBlindSeat);
            RequestPlayerAction(ActingPlayerSeatIndex);
        }
        return;
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeatIndex == GameStateData->PendingBigBlindSeat) {
            PostBlinds(); // Обновляет стек BB, банк, CurrentBet BB, статус BB на Playing, CurrentBetToCall, LastAggressor, LastBetOrRaise
            // Player.bHasActedThisSubRound не устанавливаем.
            if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Blinds posted. Current Pot: %lld"), GameStateData->Pot));
            DealHoleCardsAndStartPreflop(); // Здесь начнется префлоп, и bHasActedThisSubRound сбросится для всех
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid action/player for BB. Expected PostBlind from Seat %d. Re-requesting."), GameStateData->PendingBigBlindSeat);
            RequestPlayerAction(ActingPlayerSeatIndex);
        }
        return;
    }

    // --- Обработка Игровых Действий (Preflop, Flop, Turn, River) ---
    if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        bool bActionCausedAggression = false;
        bool bActionValidAndPerformed = true;

        // Проверяем, может ли игрок вообще действовать (не Folded, не All-In который уже не может повлиять)
        if (Player.Status == EPlayerStatus::Folded || (Player.Status == EPlayerStatus::AllIn && Player.Stack == 0))
        {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Player %s (Seat %d) cannot act (Folded or All-In with 0 stack). This action will be skipped by IsBettingRoundOver."), *PlayerName, ActingPlayerSeatIndex);
            Player.bIsTurn = false;
            // Не устанавливаем bHasActedThisSubRound = true, так как он не совершал добровольного действия в этом под-раунде.
            // IsBettingRoundOver должна корректно обработать таких игроков (пропустить их при проверке !bHasActedThisSubRound).
        }
        else // Игрок может действовать
        {
            Player.bIsTurn = false; // Снимаем флаг хода СРАЗУ, так как действие сейчас будет обработано

            switch (PlayerAction)
            {
            case EPlayerAction::Fold:
                Player.Status = EPlayerStatus::Folded;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s folds."), *PlayerName));
                break; // bHasActedThisSubRound будет установлен ниже для всех валидных действий

            case EPlayerAction::Check:
                if (Player.CurrentBet == GameStateData->CurrentBetToCall) {
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s checks."), *PlayerName));
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Check by %s. BetToCall: %lld, PlayerBet: %lld. Re-requesting action."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                    RequestPlayerAction(ActingPlayerSeatIndex);
                    bActionValidAndPerformed = false;
                }
                break;

            case EPlayerAction::Call:
            {
                int64 AmountNeededToCallAbsolute = GameStateData->CurrentBetToCall - Player.CurrentBet;
                if (AmountNeededToCallAbsolute <= 0) {
                    if (Player.CurrentBet == GameStateData->CurrentBetToCall) { // Уже заколлировал или может чекнуть
                        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s effectively checks (attempted invalid call)."), *PlayerName));
                        // bHasActedThisSubRound будет установлен ниже
                    }
                    else {
                        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: %s Call error. BetToCall: %lld, PlayerBet: %lld. Re-requesting."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                        RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false;
                    }
                    break;
                }
                int64 ActualAmountPlayerPutsInPot = FMath::Min(AmountNeededToCallAbsolute, Player.Stack);
                Player.Stack -= ActualAmountPlayerPutsInPot;
                Player.CurrentBet += ActualAmountPlayerPutsInPot;
                GameStateData->Pot += ActualAmountPlayerPutsInPot;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s calls %lld. Stack: %lld"), *PlayerName, ActualAmountPlayerPutsInPot, Player.Stack));
                if (Player.Stack == 0) {
                    Player.Status = EPlayerStatus::AllIn;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                }
            }
            break;

            case EPlayerAction::Bet:
                if (Player.CurrentBet == GameStateData->CurrentBetToCall) {
                    int64 MinBetSize = GameStateData->BigBlindAmount;
                    if (Amount < MinBetSize && Amount < Player.Stack) {
                        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Bet by %s (Amount %lld < MinBet %lld and not All-In). Re-requesting."), *PlayerName, Amount, MinBetSize);
                        RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                    }
                    if (Amount > Player.Stack) Amount = Player.Stack;

                    Player.Stack -= Amount;
                    Player.CurrentBet += Amount;
                    GameStateData->Pot += Amount;
                    GameStateData->CurrentBetToCall = Player.CurrentBet;
                    GameStateData->LastBetOrRaiseAmountInCurrentRound = Amount;
                    GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex;
                    if (GameStateData->PlayerWhoOpenedBettingThisRound == -1) {
                        GameStateData->PlayerWhoOpenedBettingThisRound = ActingPlayerSeatIndex;
                    }
                    bActionCausedAggression = true;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s bets %lld. Stack: %lld"), *PlayerName, Amount, Player.Stack));
                    if (Player.Stack == 0) {
                        Player.Status = EPlayerStatus::AllIn;
                        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                    }
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Bet by %s (Cannot bet, CurrentBetToCall %lld > PlayerBet %lld). Re-requesting."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false;
                }
                break;

            case EPlayerAction::Raise:
            {
                // Проверяем, можно ли вообще рейзить (должна быть ставка для колла, если это не первый бет)
                if (GameStateData->CurrentBetToCall == 0) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (Cannot raise if CurrentBetToCall is 0, should be Bet). Re-requesting."), *PlayerName);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }
                if (GameStateData->CurrentBetToCall <= Player.CurrentBet) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (CurrentBetToCall %lld <= PlayerBet %lld or no valid bet to raise over). Re-requesting."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }

                // Amount от UI - это ОБЩАЯ сумма ставки, до которой игрок рейзит
                int64 TotalBetByPlayerThisAction = Amount;

                // Сколько нужно было бы доколлировать до этой общей ставки
                int64 AmountToCallFirst = GameStateData->CurrentBetToCall - Player.CurrentBet;
                if (AmountToCallFirst < 0) AmountToCallFirst = 0; // Не может быть отрицательным

                // Сколько реально фишек игрок добавляет в банк в этом действии
                // Это общая сумма его новой ставки минус то, что он уже поставил в этом раунде
                int64 AmountPlayerActuallyAddsToPotNow = TotalBetByPlayerThisAction - Player.CurrentBet;

                // Чистая сумма рейза СВЕРХ предыдущей максимальной ставки на столе
                int64 PureRaiseAmountOverPreviousHighestBet = TotalBetByPlayerThisAction - GameStateData->CurrentBetToCall;

                // Минимальный допустимый чистый рейз
                int64 MinValidPureRaise = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;

                // Валидация: чистый рейз должен быть не меньше минимального, ЕСЛИ это не олл-ин
                if (PureRaiseAmountOverPreviousHighestBet < MinValidPureRaise && TotalBetByPlayerThisAction < (Player.Stack + Player.CurrentBet)) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (PureRaise %lld < MinValidPureRaise %lld and not All-In). Re-requesting."), *PlayerName, PureRaiseAmountOverPreviousHighestBet, MinValidPureRaise);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }

                // Если запрашиваемая общая ставка больше, чем есть у игрока (с учетом уже поставленного) -> это олл-ин
                if (TotalBetByPlayerThisAction > Player.Stack + Player.CurrentBet) {
                    TotalBetByPlayerThisAction = Player.Stack + Player.CurrentBet; // Новая общая ставка = весь его стек + то что уже в поте от него
                    AmountPlayerActuallyAddsToPotNow = Player.Stack; // Он добавляет весь свой оставшийся стек
                    PureRaiseAmountOverPreviousHighestBet = TotalBetByPlayerThisAction - GameStateData->CurrentBetToCall;
                    if (PureRaiseAmountOverPreviousHighestBet < 0) PureRaiseAmountOverPreviousHighestBet = 0; // На случай, если олл-ин меньше колла
                }

                // Проверка, что итоговая ставка действительно является рейзом (больше CurrentBetToCall), если это не олл-ин на меньшую сумму
                if (TotalBetByPlayerThisAction <= GameStateData->CurrentBetToCall && AmountPlayerActuallyAddsToPotNow < Player.Stack) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (TotalBetAmount %lld not > CurrentBetToCall %lld and not All-In to cover). Re-requesting."), *PlayerName, TotalBetByPlayerThisAction, GameStateData->CurrentBetToCall);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }

                Player.Stack -= AmountPlayerActuallyAddsToPotNow;
                Player.CurrentBet = TotalBetByPlayerThisAction;
                GameStateData->Pot += AmountPlayerActuallyAddsToPotNow;

                GameStateData->CurrentBetToCall = Player.CurrentBet;
                // Сумма чистого рейза для определения следующего мин. рейза
                GameStateData->LastBetOrRaiseAmountInCurrentRound = PureRaiseAmountOverPreviousHighestBet > 0 ? PureRaiseAmountOverPreviousHighestBet : GameStateData->BigBlindAmount;
                GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex;
                bActionCausedAggression = true;

                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s raises to %lld (added %lld). Stack: %lld"),
                    *PlayerName, Player.CurrentBet, AmountPlayerActuallyAddsToPotNow, Player.Stack));
                if (Player.Stack == 0) {
                    Player.Status = EPlayerStatus::AllIn;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                }
            }
            break;
            default:
                UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Unknown action %s received."), *UEnum::GetValueAsString(PlayerAction));
                RequestPlayerAction(ActingPlayerSeatIndex);
                bActionValidAndPerformed = false;
            }

            if (!bActionValidAndPerformed) return;

            Player.bHasActedThisSubRound = true; // Устанавливаем флаг ПОСЛЕ успешного выполнения действия

        } // конец if (Player can act)

        // Если было совершено агрессивное действие (Bet или Raise),
        // сбрасываем флаг bHasActedThisSubRound для всех ДРУГИХ активных игроков.
        if (bActionCausedAggression) {
            for (FPlayerSeatData& SeatToReset : GameStateData->Seats) {
                if (SeatToReset.SeatIndex != ActingPlayerSeatIndex && // Не сам агрессор
                    SeatToReset.bIsSittingIn &&
                    SeatToReset.Status == EPlayerStatus::Playing && // Только для тех, кто еще может ходить
                    SeatToReset.Stack > 0) {                       // И у кого есть фишки
                    SeatToReset.bHasActedThisSubRound = false;
                    UE_LOG(LogTemp, Verbose, TEXT("ProcessPlayerAction: Reset bHasActedThisSubRound for Seat %d due to aggression from Seat %d."), SeatToReset.SeatIndex, ActingPlayerSeatIndex);
                }
            }
        }

        // --- Логика после действия игрока ---
        if (IsBettingRoundOver()) {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Betting round IS over. Proceeding to next stage."));
            ProceedToNextGameStage();
        }
        else {
            // Раунд ставок НЕ окончен, нужно найти следующего игрока для хода.
            // ActingPlayerSeatIndex - это тот, кто ТОЛЬКО ЧТО сделал ход.
            // Мы ищем следующего ПОСЛЕ него, поэтому bExcludeStartSeat = true.
            int32 NextPlayerToAct = GetNextPlayerToAct(ActingPlayerSeatIndex, true, EPlayerStatus::MAX_None);

            if (NextPlayerToAct != -1) {
                UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Betting round NOT over. Next player to act is Seat %d."), NextPlayerToAct);
                RequestPlayerAction(NextPlayerToAct);
            }
            else {
                UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction CRITICAL: IsBettingRoundOver() is FALSE, but GetNextPlayerToAct(excluding current) returned -1! This indicates a flaw in game state or player eligibility logic. Stage: %s. Last Actor: %d."),
                    *UEnum::GetValueAsString(GameStateData->CurrentStage), ActingPlayerSeatIndex);
                // TODO: Аварийное завершение руки или игры, так как логика зашла в тупик.
                // Для отладки можно попробовать перейти к следующей стадии, но это скроет баг.
                // ProceedToNextGameStage(); 
            }
        }
        return;
    }
    else // Неизвестная или необрабатываемая стадия игры
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Action %s received in unhandled game stage %s."),
            *UEnum::GetValueAsString(PlayerAction), *UEnum::GetValueAsString(GameStateData->CurrentStage));
        RequestPlayerAction(ActingPlayerSeatIndex); // Запросить действие у текущего игрока снова
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


void UOfflineGameManager::DealHoleCardsAndStartPreflop()
{
    if (!GameStateData || !Deck) {
        UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: GameStateData or Deck is null!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Initiating card dealing and preflop setup..."));
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("Dealing hole cards..."));

    // 1. Собираем индексы всех игроков, которые находятся в статусе 'Playing' 
    //    (этот статус должен быть установлен для SB и BB в ProcessPlayerAction -> PostBlinds).
    TArray<int32> PlayersInPlayingStatusIndices;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing) {
            PlayersInPlayingStatusIndices.Add(Seat.SeatIndex);
        }
    }
    int32 NumPlayersToReceiveCards = PlayersInPlayingStatusIndices.Num();

    if (NumPlayersToReceiveCards < 1) { // Обычно для игры нужно >= 2
        UE_LOG(LogTemp, Warning, TEXT("DealHoleCardsAndStartPreflop: Not enough players (%d) in 'Playing' status to deal cards."), NumPlayersToReceiveCards);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Error: Not enough players for deal"), GameStateData->Pot);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
        return;
    }

    // 2. Определяем, с кого начинать раздачу карт
    int32 DealerSeatIndex = GameStateData->DealerSeat;
    if (!GameStateData->Seats.IsValidIndex(DealerSeatIndex)) {
        UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Invalid DealerSeatIndex %d! Cannot proceed."), DealerSeatIndex);
        return;
    }

    int32 StartDealingFromSeat;
    // В хедз-апе (2 игрока) дилер (который является SB) получает карту первым.
    // В 3+ игроков, первый слева от дилера (SB) получает карту первым.
    if (NumPlayersToReceiveCards == 2) {
        StartDealingFromSeat = DealerSeatIndex;
    }
    else {
        StartDealingFromSeat = GetNextPlayerToAct(DealerSeatIndex, false, EPlayerStatus::Playing);
    }

    // Проверка, что StartDealingFromSeat валиден и находится среди тех, кто должен получить карты
    if (StartDealingFromSeat == -1 || !PlayersInPlayingStatusIndices.Contains(StartDealingFromSeat)) {
        UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: CRITICAL - Could not determine a valid StartDealingFromSeat (%d) from 'Playing' players. NumPlayersToReceive: %d, Dealer: %d."),
            StartDealingFromSeat, NumPlayersToReceiveCards, DealerSeatIndex);
        // Пытаемся взять первого из списка Playing как аварийный вариант, но это указывает на проблему
        if (PlayersInPlayingStatusIndices.Num() > 0) {
            StartDealingFromSeat = PlayersInPlayingStatusIndices[0];
            UE_LOG(LogTemp, Warning, TEXT("DealHoleCardsAndStartPreflop: Fallback applied for StartDealingFromSeat, using first available playing player: Seat %d."), StartDealingFromSeat);
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Fallback failed, no players in playing status. Aborting deal."));
            return;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Dealer: Seat %d. StartDealingFrom: Seat %d. NumPlayersToReceiveCards: %d"),
        DealerSeatIndex, StartDealingFromSeat, NumPlayersToReceiveCards);

    // 3. Формируем точный порядок раздачи (DealOrderResolved)
    TArray<int32> DealOrderResolved;
    DealOrderResolved.Reserve(NumPlayersToReceiveCards);
    int32 CurrentSeatForOrderLoop = StartDealingFromSeat;

    for (int32 i = 0; i < NumPlayersToReceiveCards; ++i) {
        // Проверяем, что текущий выбранный для добавления в порядок все еще валиден и должен получить карты
        if (GameStateData->Seats.IsValidIndex(CurrentSeatForOrderLoop) &&
            PlayersInPlayingStatusIndices.Contains(CurrentSeatForOrderLoop)) {
            DealOrderResolved.Add(CurrentSeatForOrderLoop);
            if (i < NumPlayersToReceiveCards - 1) { // Ищем следующего, только если это не последний игрок в порядке раздачи
                // ИСПОЛЬЗУЕМ bExcludeStartSeat = true, чтобы найти СЛЕДУЮЩЕГО УНИКАЛЬНОГО игрока
                CurrentSeatForOrderLoop = GetNextPlayerToAct(CurrentSeatForOrderLoop, true, EPlayerStatus::Playing);
                if (CurrentSeatForOrderLoop == -1) {
                    UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: CRITICAL - Next player not found while building DealOrder. Iteration %d. Order so far has %d players."), i, DealOrderResolved.Num());
                    return;
                }
                if (DealOrderResolved.Contains(CurrentSeatForOrderLoop)) { // Защита от зацикливания
                    UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: CRITICAL - Duplicate player %d detected in DealOrder. This should not happen with bExcludeStartSeat=true. Logic error."), CurrentSeatForOrderLoop);
                    return;
                }
            }
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: CRITICAL - CurrentSeatForOrderLoop %d is invalid or not in Playing list during DealOrder construction. Iteration %d."), CurrentSeatForOrderLoop, i);
            return;
        }
    }

    if (DealOrderResolved.Num() != NumPlayersToReceiveCards) {
        UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Mismatch DealOrderResolved.Num() (%d) vs NumPlayersToReceiveCards (%d). THIS IS A CRITICAL LOGIC FLAW IN DEAL ORDERING."), DealOrderResolved.Num(), NumPlayersToReceiveCards);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Deal order determined (%d players):"), DealOrderResolved.Num());
    for (int32 SeatIdx : DealOrderResolved) { UE_LOG(LogTemp, Log, TEXT("  -> Seat %d (%s)"), SeatIdx, *GameStateData->Seats[SeatIdx].PlayerName); }

    // 4. Раздаем карты: два круга по сформированному DealOrderResolved
    for (int32 CardPass = 0; CardPass < 2; ++CardPass) {
        UE_LOG(LogTemp, Log, TEXT("Dealing Card Pass #%d"), CardPass + 1);
        for (int32 SeatIndexToDeal : DealOrderResolved) {
            // Дополнительно проверяем, что игрок все еще в статусе Playing и у него меньше 2 карт
            if (GameStateData->Seats.IsValidIndex(SeatIndexToDeal) &&
                GameStateData->Seats[SeatIndexToDeal].Status == EPlayerStatus::Playing &&
                GameStateData->Seats[SeatIndexToDeal].HoleCards.Num() < 2) {
                TOptional<FCard> DealtCardOptional = Deck->DealCard();
                if (DealtCardOptional.IsSet()) {
                    FCard DealtCard = DealtCardOptional.GetValue();
                    GameStateData->Seats[SeatIndexToDeal].HoleCards.Add(DealtCard);
                    UE_LOG(LogTemp, Log, TEXT("  Dealt card %s to Seat %d (%s). Total cards now: %d"),
                        *DealtCard.ToString(), SeatIndexToDeal,
                        *GameStateData->Seats[SeatIndexToDeal].PlayerName,
                        GameStateData->Seats[SeatIndexToDeal].HoleCards.Num());
                }
                else {
                    UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Deck ran out of cards on pass %d for seat %d!"), CardPass + 1, SeatIndexToDeal);
                    GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
                }
            }
            else {
                // Этот лог может сработать для второго круга раздачи, если игрок уже получил 2 карты (что не должно быть)
                // или если его статус изменился (что тоже не должно быть на этом этапе).
                UE_LOG(LogTemp, Warning, TEXT("DealHoleCardsAndStartPreflop: Skipped dealing card (pass %d) to Seat %d (%s). Status: %s, NumCards: %d, SittingIn: %d"),
                    CardPass + 1, SeatIndexToDeal, *GameStateData->Seats[SeatIndexToDeal].PlayerName,
                    *UEnum::GetValueAsString(GameStateData->Seats[SeatIndexToDeal].Status), GameStateData->Seats[SeatIndexToDeal].HoleCards.Num(),
                    GameStateData->Seats[SeatIndexToDeal].bIsSittingIn);
            }
        }
    }

    // 5. Финальная проверка и логирование розданных карт
    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (PlayersInPlayingStatusIndices.Contains(Seat.SeatIndex)) { // Логируем только для тех, кто должен был получить карты
            if (Seat.HoleCards.Num() == 2) {
                UE_LOG(LogTemp, Log, TEXT("Seat %d (%s) FINAL cards: %s, %s. Status: %s"),
                    Seat.SeatIndex, *Seat.PlayerName,
                    *Seat.HoleCards[0].ToString(), *Seat.HoleCards[1].ToString(),
                    *UEnum::GetValueAsString(Seat.Status));
            }
            else {
                UE_LOG(LogTemp, Error, TEXT("Seat %d (%s) is Playing/Eligible but has %d hole cards! THIS IS A DEALING LOGIC ERROR."),
                    Seat.SeatIndex, *Seat.PlayerName, Seat.HoleCards.Num());
            }
        }
    }
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("Hole cards dealt. Preflop betting starts."));

    // 6. Уведомляем контроллер, что карты розданы, ПЕРЕД запросом первого действия на префлопе
    if (OnActualHoleCardsDealtDelegate.IsBound()) {
        UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Broadcasting OnActualHoleCardsDealtDelegate."));
        OnActualHoleCardsDealtDelegate.Broadcast();
    }

    // 7. СБРОС ФЛАГОВ bHasActedThisSubRound ДЛЯ НАЧАЛА ПРЕФЛОП-ТОРГОВ
    for (FPlayerSeatData& Seat : GameStateData->Seats) {
        // Сбрасываем для всех, кто не сфолдил и не сидит аут.
        // Игроки All-In тоже должны иметь этот флаг сброшенным, если они еще могут быть частью определения конца раунда.
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded && Seat.Status != EPlayerStatus::SittingOut) {
            Seat.bHasActedThisSubRound = false;
            UE_LOG(LogTemp, Verbose, TEXT("DealHoleCardsAndStartPreflop: Reset bHasActedThisSubRound for Seat %d (%s) to false."), Seat.SeatIndex, *Seat.PlayerName);
        }
    }

    // 8. Установка стадии Preflop и определение ключевых игроков для раунда ставок
    GameStateData->CurrentStage = EGameStage::Preflop;

    // Игрок, который "открыл" торги на префлопе (на ком должно замкнуться действие, если не было рейзов) - это BB.
    GameStateData->PlayerWhoOpenedBettingThisRound = GameStateData->PendingBigBlindSeat;
    // Последний "агрессор" на начало префлопа - это BB (его обязательная ставка).
    GameStateData->LastAggressorSeatIndex = GameStateData->PendingBigBlindSeat;
    GameStateData->LastBetOrRaiseAmountInCurrentRound = GameStateData->BigBlindAmount; // Сумма этой "агрессии".
    // CurrentBetToCall уже должен быть равен BigBlindAmount (установлен в ProcessPlayerAction при постановке BB).

    // Определяем, кто делает первое ДОБРОВОЛЬНОЕ действие на префлопе
    int32 FirstToMakeVoluntaryActionSeat = DetermineFirstPlayerToActAtPreflop();
    if (FirstToMakeVoluntaryActionSeat == -1) {
        UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: Could not determine first player to make a voluntary action for Preflop!"));
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; return;
    }

    UE_LOG(LogTemp, Log, TEXT("Preflop round. First to make voluntary action: Seat %d (%s). CurrentBetToCall: %lld. OpenerThisRound (BB): %d, LastAggressor (BB): %d"),
        FirstToMakeVoluntaryActionSeat, *GameStateData->Seats[FirstToMakeVoluntaryActionSeat].PlayerName, GameStateData->CurrentBetToCall,
        GameStateData->PlayerWhoOpenedBettingThisRound, GameStateData->LastAggressorSeatIndex);

    RequestPlayerAction(FirstToMakeVoluntaryActionSeat);
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
        return GetNextPlayerToAct(GameStateData->PendingBigBlindSeat, false, EPlayerStatus::MAX_None);
    }
}

int32 UOfflineGameManager::GetNextPlayerToAct(int32 StartSeatIndex, bool bExcludeStartSeat, EPlayerStatus RequiredStatus) const
{
    if (!GameStateData || GameStateData->Seats.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: GameStateData is null or Seats array is empty. Cannot find next player."));
        return -1;
    }

    const int32 NumSeats = GameStateData->Seats.Num();
    int32 CurrentIndexToTest = StartSeatIndex;

    // 1. Обработка начального индекса и флага исключения
    if (!GameStateData->Seats.IsValidIndex(StartSeatIndex))
    {
        // Если StartSeatIndex невалиден (например, -1), и мы НЕ исключаем его,
        // то это обычно означает, что мы ищем первого подходящего с самого начала (с индекса 0).
        if (StartSeatIndex == -1 && !bExcludeStartSeat) {
            CurrentIndexToTest = 0;
            // bExcludeStartSeat уже false, так что мы проверим и место 0
            UE_LOG(LogTemp, Verbose, TEXT("GetNextPlayerToAct: StartSeatIndex was -1 and not excluded, starting search from Seat 0 including Seat 0."));
        }
        // Если StartSeatIndex невалиден, и мы ДОЛЖНЫ его исключить (что странно для невалидного индекса),
        // то все равно начнем с 0, но технически "исключение" уже произошло.
        else if (StartSeatIndex == -1 && bExcludeStartSeat) {
            CurrentIndexToTest = 0;
            // bExcludeStartSeat можно оставить true, но это не будет иметь эффекта, если мы и так начинаем с 0.
            // Логичнее, если StartSeatIndex = -1, то bExcludeStartSeat игнорируется, и мы всегда проверяем с 0.
            // Для большей ясности:
            bExcludeStartSeat = false; // Если начинаем с 0 из-за плохого StartSeatIndex, то 0 не исключаем.
            UE_LOG(LogTemp, Verbose, TEXT("GetNextPlayerToAct: StartSeatIndex was -1 and was to be excluded, starting search from Seat 0 including Seat 0."));
        }
        else // StartSeatIndex не -1, но невалидный (например, > NumSeats)
        {
            UE_LOG(LogTemp, Warning, TEXT("GetNextPlayerToAct: Invalid StartSeatIndex: %d. Defaulting to search from Seat 0, including Seat 0."), StartSeatIndex);
            CurrentIndexToTest = 0;
            bExcludeStartSeat = false;
        }
    }

    // Если StartSeatIndex валиден и его нужно исключить, переходим к следующему
    if (bExcludeStartSeat && GameStateData->Seats.IsValidIndex(CurrentIndexToTest))
    {
        CurrentIndexToTest = (CurrentIndexToTest + 1) % NumSeats;
    }
    // Если StartSeatIndex был невалиден, bExcludeStartSeat уже должен быть false по логике выше.

    // 2. Цикл поиска следующего подходящего игрока
    // Проходим не более NumSeats раз, чтобы гарантированно обойти всех один раз
    for (int32 i = 0; i < NumSeats; ++i)
    {
        // CurrentIndexToTest всегда будет в диапазоне [0, NumSeats-1] из-за операции %
        const FPlayerSeatData& Seat = GameStateData->Seats[CurrentIndexToTest];

        bool bIsEligible = false;
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded)
        {
            // Проверяем соответствие требуемому статусу
            if (RequiredStatus == EPlayerStatus::MAX_None) // PlayerStatus_MAX - флаг "любой, кто может продолжать игру"
            {
                // Игрок может продолжать игру/быть следующим для хода, если он:
                // - Playing И имеет стек > 0 (может делать ставки)
                // - ИЛИ AllIn (уже не может делать ставки, но его карты играют, и другие могут ставить в побочный банк)
                if ((Seat.Status == EPlayerStatus::Playing && Seat.Stack > 0) || Seat.Status == EPlayerStatus::AllIn)
                {
                    bIsEligible = true;
                }
            }
            else // Ищем игрока с конкретным RequiredStatus
            {
                if (Seat.Status == RequiredStatus)
                {
                    // Если ищем игрока для активного действия (например, Playing, MustPost...), он должен иметь стек, если он не AllIn.
                    // Если RequiredStatus == EPlayerStatus::AllIn, проверка стека не нужна, он уже AllIn.
                    if (RequiredStatus == EPlayerStatus::Playing ||
                        RequiredStatus == EPlayerStatus::MustPostSmallBlind ||
                        RequiredStatus == EPlayerStatus::MustPostBigBlind)
                    {
                        if (Seat.Stack > 0) // Для этих статусов нужен стек, чтобы действовать
                        {
                            bIsEligible = true;
                        }
                        // Если Stack == 0, но статус Playing - это ошибка состояния, он должен быть AllIn или Folded/SittingOut.
                        // Если статус MustPost... и стек 0, он все равно должен попытаться поставить (и пойдет AllIn).
                        // Поэтому, если статус MustPost..., разрешаем, PostBlinds разберется с AllIn.
                        else if (RequiredStatus == EPlayerStatus::MustPostSmallBlind || RequiredStatus == EPlayerStatus::MustPostBigBlind)
                        {
                            bIsEligible = true; // Позволяем ему попытаться поставить блайнд, даже если стек 0 (это будет AllIn)
                        }

                    }
                    else { // Для других статусов (например, Waiting, если мы его ищем) проверка стека может быть не нужна
                        bIsEligible = true;
                    }
                }
            }
        }

        if (bIsEligible)
        {
            UE_LOG(LogTemp, Verbose, TEXT("GetNextPlayerToAct: Found eligible player at Seat %d (%s). OriginalStartIdx: %d, ExcludedStart: %s, ReqStatus: %s"),
                CurrentIndexToTest, *Seat.PlayerName, StartSeatIndex, bExcludeStartSeat ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(RequiredStatus));
            return CurrentIndexToTest;
        }

        CurrentIndexToTest = (CurrentIndexToTest + 1) % NumSeats; // Переходим к следующему месту
    }

    UE_LOG(LogTemp, Warning, TEXT("GetNextPlayerToAct: No eligible player found after full circle. OriginalStartIdx: %d, ExcludedStart: %s, ReqStatus: %s"),
        StartSeatIndex, bExcludeStartSeat ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(RequiredStatus));
    return -1; // Не найдено подходящих игроков после полного круга
}


int32 UOfflineGameManager::DetermineFirstPlayerToActPostflop() const
{
    if (!GameStateData || GameStateData->DealerSeat == -1) return -1;
    // Первый активный игрок слева от дилера (включая SB, если он еще в игре)
    return GetNextPlayerToAct(GameStateData->DealerSeat, false, EPlayerStatus::MAX_None);
}

bool UOfflineGameManager::IsBettingRoundOver() const
{
    if (!GameStateData || GameStateData->Seats.IsEmpty()) { return true; }
    if (GameStateData->CurrentStage < EGameStage::Preflop || GameStateData->CurrentStage > EGameStage::River) { return false; }

    UE_LOG(LogTemp, Log, TEXT("--- IsBettingRoundOver CHECK --- Stage: %s, PlayerWhoJustActed (CurrentTurnSeat): %d, ToCall: %lld, LastAggressor: %d, OpenerThisRound: %d"),
        *UEnum::GetValueAsString(GameStateData->CurrentStage), GameStateData->CurrentTurnSeat,
        GameStateData->CurrentBetToCall, GameStateData->LastAggressorSeatIndex, GameStateData->PlayerWhoOpenedBettingThisRound);

    TArray<int32> PlayersStillInHandIndices; // Игроки, которые не сфолдили и сидят в игре
    int32 NumPlayersWithChipsAndNotFolded = 0; // Игроки, которые не сфолдили, сидят И имеют фишки (не All-In с 0 стеком)

    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded) {
            PlayersStillInHandIndices.Add(Seat.SeatIndex);
            if (Seat.Stack > 0 || Seat.Status == EPlayerStatus::AllIn) { // Считаем и тех, кто AllIn, но еще в игре
                // Точнее, тех, кто не Folded и имеет право на часть банка
            }
            if (Seat.Stack > 0 && Seat.Status == EPlayerStatus::Playing) { // Те, кто еще может активно ставить
                NumPlayersWithChipsAndNotFolded++;
            }
        }
    }

    if (PlayersStillInHandIndices.Num() <= 1) {
        UE_LOG(LogTemp, Log, TEXT("IsBettingRoundOver: True (<=1 player left not folded)."));
        return true;
    }

    // Если никто не делал агрессивных действий (бет/рейз) в этом раунде,
    // то раунд заканчивается, когда все, кто мог, прочекали, и ход вернулся к открывшему торги.
    if (GameStateData->LastAggressorSeatIndex == -1) {
        // Проверяем, все ли активные игроки (не AllIn с 0 стеком) уже походили (bHasActedThisSubRound == true)
        // И их ставка равна CurrentBetToCall (которая должна быть 0 на постфлопе или BB на префлопе)
        for (int32 SeatIndex : PlayersStillInHandIndices) {
            const FPlayerSeatData& Seat = GameStateData->Seats[SeatIndex];
            if (Seat.Status == EPlayerStatus::Playing && Seat.Stack > 0) { // Только те, кто мог бы ходить
                if (!Seat.bHasActedThisSubRound) {
                    // Если это BB на префлопе, и CurrentBetToCall = BB, и это его ход - он еще не действовал (опция)
                    if (GameStateData->CurrentStage == EGameStage::Preflop &&
                        SeatIndex == GameStateData->PendingBigBlindSeat &&
                        GameStateData->CurrentBetToCall == GameStateData->BigBlindAmount) {
                        UE_LOG(LogTemp, Verbose, TEXT("  IsBettingRoundOver: False (BB Option: Seat %d hasn't acted post-blinds)."), SeatIndex);
                        return false;
                    }
                    // Если это постфлоп, CurrentBetToCall=0, и это первый игрок, он еще не чекнул
                    if (GameStateData->CurrentStage > EGameStage::Preflop &&
                        GameStateData->CurrentBetToCall == 0 &&
                        SeatIndex == GameStateData->PlayerWhoOpenedBettingThisRound) {
                        UE_LOG(LogTemp, Verbose, TEXT("  IsBettingRoundOver: False (Postflop Opener %d hasn't acted in check round)."), SeatIndex);
                        return false;
                    }
                    // Если просто кто-то еще не походил в круге чеков/коллов
                    UE_LOG(LogTemp, Verbose, TEXT("  IsBettingRoundOver: False (No aggression, but Seat %d has not acted this sub-round)."), SeatIndex);
                    return false;
                }
            }
        }
        // Если все походили и не было агрессии, раунд окончен
        UE_LOG(LogTemp, Log, TEXT("IsBettingRoundOver: True (No aggression, and all eligible players have acted)."));
        return true;
    }

    // Если была агрессия (LastAggressorSeatIndex != -1)
    // Раунд окончен, если ВСЕ ОСТАЛЬНЫЕ активные игроки (кроме самого LastAggressor)
    // уже сделали ход (bHasActedThisSubRound == true) И их ставка равна CurrentBetToCall (или они AllIn/Folded).
    // И ход должен был бы вернуться к LastAggressor.
    for (int32 SeatIndex : PlayersStillInHandIndices) {
        const FPlayerSeatData& Seat = GameStateData->Seats[SeatIndex];

        if (Seat.Status == EPlayerStatus::AllIn && Seat.Stack == 0) continue; // Пропускаем тех, кто уже не может действовать

        if (Seat.Status == EPlayerStatus::Playing) { // Рассматриваем тех, кто еще может ставить
            if (Seat.CurrentBet < GameStateData->CurrentBetToCall && Seat.Stack > 0) {
                UE_LOG(LogTemp, Verbose, TEXT("  IsBettingRoundOver: False (Seat %d needs to meet call of %lld. Bet: %lld)."), SeatIndex, GameStateData->CurrentBetToCall, Seat.CurrentBet);
                return false; // Кто-то еще не уравнял
            }
            // Если CurrentBet == CurrentBetToCall, но bHasActedThisSubRound == false,
            // это означает, что он еще не отреагировал на последний рейз.
            if (!Seat.bHasActedThisSubRound) {
                UE_LOG(LogTemp, Verbose, TEXT("  IsBettingRoundOver: False (Seat %d matched bet, but HasActedThisSubRound is false)."), SeatIndex);
                return false;
            }
        }
    }

    // Если мы дошли сюда, все ставки уравнены, и все активные игроки уже сделали ход в этом под-раунде.
    UE_LOG(LogTemp, Log, TEXT("IsBettingRoundOver: True (Aggression occurred, and all subsequent players have acted and matched bets)."));
    return true;
}


void UOfflineGameManager::ProceedToNextGameStage()
{
    if (!GameStateData || !Deck) {
        UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: GameStateData or Deck is null!"));
        return;
    }

    EGameStage StageBeforeAdvance = GameStateData->CurrentStage; // Сохраняем текущую стадию для логики
    UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Attempting to advance from stage: %s"), *UEnum::GetValueAsString(StageBeforeAdvance));

    // 1. Подсчитываем, сколько игроков еще в игре (не сфолдили)
    int32 PlayersLeftInHand = 0;
    int32 WinnerIfOneLeft = -1;
    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded) {
            PlayersLeftInHand++;
            WinnerIfOneLeft = Seat.SeatIndex; // Запоминаем последнего не сфолдившего
        }
    }

    // 2. Проверяем, не закончилась ли рука из-за фолдов или не пора ли на шоудаун (если текущая стадия была Ривер)
    if (PlayersLeftInHand <= 1 || StageBeforeAdvance == EGameStage::River) {
        if (PlayersLeftInHand == 1 && WinnerIfOneLeft != -1) {
            UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Only one player (%s, Seat %d) left. Awarding pot."),
                *GameStateData->Seats[WinnerIfOneLeft].PlayerName, WinnerIfOneLeft);
            AwardPotToWinner({ WinnerIfOneLeft });
        }
        else if (PlayersLeftInHand > 1 && StageBeforeAdvance == EGameStage::River) {
            UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: River betting complete, %d players left. Proceeding to Showdown."), PlayersLeftInHand);
            ProceedToShowdown(); // Переходим к шоудауну
            return; // ProceedToShowdown сам завершит руку или начнет новую
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage: Hand ends due to %d players left or unexpected state. Pot: %lld"), PlayersLeftInHand, GameStateData->Pot);
            if (PlayersLeftInHand == 0 && GameStateData->Pot > 0) {
                // Ситуация, когда банк не пуст, но игроков не осталось (ошибка или очень специфический сценарий)
                // Можно добавить логику возврата банка или его переноса. Для MVP пока просто обнуляем.
                UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: 0 players left but pot is %lld! This should not happen."), GameStateData->Pot);
            }
            AwardPotToWinner({}); // Попытка отдать банк, если есть (может быть разделен или возвращен, если 0 победителей)
        }

        // После завершения руки (победитель один или шоудаун уже вызван)
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Готовимся к новой руке
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over"), GameStateData->Pot); // Pot должен быть 0 после AwardPot
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
        // TODO: В будущем здесь может быть задержка и автоматический вызов StartNewHand()
        // или ожидание команды от UI для начала новой руки.
        return;
    }

    // --- Если рука продолжается, готовимся к НОВОМУ РАУНДУ СТАВОК на следующей улице ---
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- Betting round ended. Proceeding to next street. ---"));

    // 3. Сброс состояния для нового раунда ставок
    GameStateData->CurrentBetToCall = 0;
    GameStateData->LastBetOrRaiseAmountInCurrentRound = 0;
    GameStateData->LastAggressorSeatIndex = -1;
    // PlayerWhoOpenedBettingThisRound будет установлен ниже, после определения первого ходящего на новой улице.
    GameStateData->PlayerWhoOpenedBettingThisRound = -1;

    for (FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing) { // Сбрасываем только для тех, кто еще активно играет
            Seat.CurrentBet = 0;
            Seat.bHasActedThisSubRound = false; // КЛЮЧЕВОЙ СБРОС
        }
        // Для AllIn игроков CurrentBet не сбрасываем, их ставка уже зафиксирована.
        // bHasActedThisSubRound для них не так важен, так как они не будут делать новых ходов.
    }

    EGameStage NextStageToSet = GameStateData->CurrentStage; // Инициализируем на случай непредвиденного CurrentStage

    // 4. Раздача карт для следующей улицы
    switch (StageBeforeAdvance) // Используем сохраненную стадию ДО ее изменения
    {
    case EGameStage::Preflop:
        NextStageToSet = EGameStage::Flop;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Dealing %s ---"), *UEnum::GetValueAsString(NextStageToSet)));
        // Deck->DealCard(); // Сжигание карты - опционально
        if (Deck->NumCardsLeft() < 3) { UE_LOG(LogTemp, Error, TEXT("Not enough cards for Flop!")); /* TODO: Обработка ошибки, возможно, завершить руку */ return; }
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Flop: %s %s %s"),
            *GameStateData->CommunityCards[0].ToString(), *GameStateData->CommunityCards[1].ToString(), *GameStateData->CommunityCards[2].ToString()));
        break;
    case EGameStage::Flop:
        NextStageToSet = EGameStage::Turn;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Dealing %s ---"), *UEnum::GetValueAsString(NextStageToSet)));
        if (Deck->NumCardsLeft() < 1) { UE_LOG(LogTemp, Error, TEXT("Not enough cards for Turn!")); /* TODO: Обработка ошибки */ return; }
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Turn: %s"), *GameStateData->CommunityCards.Last().ToString()));
        break;
    case EGameStage::Turn:
        NextStageToSet = EGameStage::River;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Dealing %s ---"), *UEnum::GetValueAsString(NextStageToSet)));
        if (Deck->NumCardsLeft() < 1) { UE_LOG(LogTemp, Error, TEXT("Not enough cards for River!")); /* TODO: Обработка ошибки */ return; }
        GameStateData->CommunityCards.Add(Deck->DealCard().Get(FCard()));
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("River: %s"), *GameStateData->CommunityCards.Last().ToString()));
        break;
    default:
        UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage called from invalid or unexpected stage to deal new cards: %s."), *UEnum::GetValueAsString(StageBeforeAdvance));
        // Эта ветка не должна достигаться, если логика ProcessPlayerAction и IsBettingRoundOver верна,
        // и если условие (PlayersLeftInHand <= 1 || GameStateData->CurrentStage == EGameStage::River) в начале отработало.
        return;
    }

    // 5. Уведомляем UI об обновлении ВСЕХ общих карт
    if (OnCommunityCardsUpdatedDelegate.IsBound()) {
        OnCommunityCardsUpdatedDelegate.Broadcast(GameStateData->CommunityCards);
    }

    // 6. Устанавливаем новую стадию игры
    GameStateData->CurrentStage = NextStageToSet;
    UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: New stage is %s."), *UEnum::GetValueAsString(NextStageToSet));

    // 7. Начинаем новый круг торгов
    int32 FirstToActOnNewStreet = DetermineFirstPlayerToActPostflop();
    if (FirstToActOnNewStreet != -1) {
        GameStateData->PlayerWhoOpenedBettingThisRound = FirstToActOnNewStreet;
        UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: First to act on %s is Seat %d (%s). Opener set."),
            *UEnum::GetValueAsString(NextStageToSet), FirstToActOnNewStreet, *GameStateData->Seats[FirstToActOnNewStreet].PlayerName);
        RequestPlayerAction(FirstToActOnNewStreet);
    }
    else {
        // Если не осталось игроков для хода (например, все, кроме одного, олл-ин на предыдущей улице,
        // а этот один не может ставить, или все оставшиеся олл-ин).
        UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage: No player to act on %s. PlayersLeftInHand: %d. This may mean all remaining are all-in."),
            *UEnum::GetValueAsString(NextStageToSet), PlayersLeftInHand);

        // Если это еще не ривер, и есть несколько игроков (но они все олл-ин),
        // мы должны автоматически раздать оставшиеся карты до ривера и перейти к шоудауну.
        if (NextStageToSet < EGameStage::River && PlayersLeftInHand > 1) {
            UE_LOG(LogTemp, Warning, TEXT("   Auto-advancing to next stage again as no one can act."));
            ProceedToNextGameStage(); // Рекурсивный вызов для раздачи следующей улицы
        }
        else if (PlayersLeftInHand > 1) { // Уже на ривере (или ошибка), и никто не может ходить -> шоудаун
            ProceedToShowdown();
        }
        else {
            // Если остался 1 игрок, это должно было обработаться в самом начале функции.
            // Эта ветка - аварийный случай.
            UE_LOG(LogTemp, Error, TEXT("   Unexpected state in ProceedToNextGameStage fallback (No one to act, but PlayersLeftInHand is not >1 or stage is not < River)."));
            GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
            // ... (уведомление UI о завершении руки) ...
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