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

bool UOfflineGameManager::CanStartNewHand(FString& OutReasonIfNotPossible)
{
    OutReasonIfNotPossible = TEXT(""); // Очищаем причину
    if (!GameStateData || !GameStateData) {
        OutReasonIfNotPossible = TEXT("Внутренняя ошибка: Состояние игры не инициализировано.");
        UE_LOG(LogTemp, Error, TEXT("CanStartNewHand: GameStateData is null!"));
        return false;
    }

    // 1. Проверяем, что текущая рука действительно завершена.
    // Рука считается завершенной, если стадия WaitingForPlayers или Showdown (и мы не в середине шоудауна).
    // ИЛИ если нет активного хода (CurrentTurnSeat == -1) И нет игроков в активных игровых статусах.
    bool bHandEffectivelyOver = false;
    if (GameStateData->CurrentStage == EGameStage::WaitingForPlayers) {
        bHandEffectivelyOver = true;
    }
    else if (GameStateData->CurrentStage == EGameStage::Showdown) {
        // После AwardPotToWinner в ProceedToShowdown, мы должны были бы перейти в WaitingForPlayers.
        // Если мы все еще в Showdown, значит, процесс еще не завершен.
        // Но для кнопки "Next Hand" после шоудауна это ОК, если других активных ходов нет.
        // Более точная проверка: нет активного CurrentTurnSeat.
        if (GameStateData->CurrentTurnSeat == -1) {
            bHandEffectivelyOver = true;
        }
    }

    if (!bHandEffectivelyOver) { // Если стадия не WaitingForPlayers/Showdown, проверяем детальнее
        bool bActivePlayerFound = false;
        if (GameStateData->CurrentTurnSeat != -1 && GameStateData->Seats.IsValidIndex(GameStateData->CurrentTurnSeat) &&
            GameStateData->Seats[GameStateData->CurrentTurnSeat].Status != EPlayerStatus::Folded &&
            GameStateData->Seats[GameStateData->CurrentTurnSeat].Status != EPlayerStatus::SittingOut &&
            (GameStateData->Seats[GameStateData->CurrentTurnSeat].Stack > 0 || GameStateData->Seats[GameStateData->CurrentTurnSeat].Status == EPlayerStatus::AllIn))
        {
            bActivePlayerFound = true; // Есть игрок, чей сейчас ход
        }
        else // Если CurrentTurnSeat сброшен, проверим, есть ли кто-то еще, кто должен ходить
        {
            for (const FPlayerSeatData& Seat : GameStateData->Seats) {
                if (Seat.bIsSittingIn &&
                    (Seat.Status == EPlayerStatus::Playing ||
                        Seat.Status == EPlayerStatus::MustPostSmallBlind ||
                        Seat.Status == EPlayerStatus::MustPostBigBlind) &&
                    !Seat.bHasActedThisSubRound) { // Важно: кто-то еще не походил в этом под-раунде
                    bActivePlayerFound = true;
                    break;
                }
            }
        }

        if (bActivePlayerFound) {
            OutReasonIfNotPossible = TEXT("Текущая рука еще не окончена!");
            UE_LOG(LogTemp, Warning, TEXT("CanStartNewHand: false - Hand is still in progress (Stage: %s, Turn: %d)."),
                *UEnum::GetValueAsString(GameStateData->CurrentStage), GameStateData->CurrentTurnSeat);
            return false;
        }
    }

    // 2. Проверяем, достаточно ли игроков с фишками для начала новой руки.
    int32 PlayersWithEnoughForBigBlind = 0;
    int64 RequiredStackForBB = GameStateData->BigBlindAmount;
    if (RequiredStackForBB <= 0) RequiredStackForBB = 1; // Хотя бы 1 фишка, если ББ = 0 (не должно быть)

    for (const FPlayerSeatData& Seat : GameStateData->Seats)
    {
        // Считаем только тех, кто bIsSittingIn (активно участвует в игре)
        if (Seat.bIsSittingIn && Seat.Stack >= RequiredStackForBB)
        {
            PlayersWithEnoughForBigBlind++;
        }
    }

    if (PlayersWithEnoughForBigBlind < 2)
    {
        OutReasonIfNotPossible = FString::Printf(TEXT("Недостаточно игроков с деньгами для новой игры (нужно хотя бы двое со стеком >= %lld). Найдено: %d"), RequiredStackForBB, PlayersWithEnoughForBigBlind);
        UE_LOG(LogTemp, Warning, TEXT("CanStartNewHand: false - Not enough players with chips for BB. Found: %d, Needed Stack: %lld"), PlayersWithEnoughForBigBlind, RequiredStackForBB);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("CanStartNewHand: true - Can start new hand."));
    return true;
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
    if (!GameStateData || !GameStateData || !GameStateData->Seats.IsValidIndex(ActingPlayerSeatIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: Invalid GameState or ActingPlayerSeatIndex %d. Cannot process action."), ActingPlayerSeatIndex);
        if (OnPlayerTurnStartedDelegate.IsBound() && GameStateData) OnPlayerTurnStartedDelegate.Broadcast(GameStateData->CurrentTurnSeat);
        return;
    }

    if (GameStateData->CurrentTurnSeat != ActingPlayerSeatIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Action from Seat %d, but turn is Seat %d. Re-requesting."), ActingPlayerSeatIndex, GameStateData->CurrentTurnSeat);
        RequestPlayerAction(GameStateData->CurrentTurnSeat);
        return;
    }

    FPlayerSeatData& Player = GameStateData->Seats[ActingPlayerSeatIndex];
    FString PlayerName = Player.PlayerName;

    UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Seat %d (%s) attempts Action: %s, Amount: %lld. Stage: %s. Stack: %lld, PBet: %lld, Pot: %lld, ToCall: %lld, LstAggr: %d (Amt %lld), Opnr: %d, HasActed: %s"),
        ActingPlayerSeatIndex, *PlayerName, *UEnum::GetValueAsString(PlayerAction), Amount,
        *UEnum::GetValueAsString(GameStateData->CurrentStage), Player.Stack, Player.CurrentBet, GameStateData->Pot, GameStateData->CurrentBetToCall,
        GameStateData->LastAggressorSeatIndex, GameStateData->LastBetOrRaiseAmountInCurrentRound, GameStateData->PlayerWhoOpenedBettingThisRound,
        Player.bHasActedThisSubRound ? TEXT("true") : TEXT("false"));

    // --- Обработка Постановки Блайндов (этот блок остается неизменным) ---
    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeatIndex == GameStateData->PendingSmallBlindSeat) {
            PostBlinds(); RequestBigBlind();
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid action/player for SB. Re-requesting."));
            RequestPlayerAction(ActingPlayerSeatIndex);
        }
        return;
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeatIndex == GameStateData->PendingBigBlindSeat) {
            PostBlinds();
            if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Blinds posted. Current Pot: %lld"), GameStateData->Pot));
            DealHoleCardsAndStartPreflop();
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid action/player for BB. Re-requesting."));
            RequestPlayerAction(ActingPlayerSeatIndex);
        }
        return;
    }

    // --- Обработка Игровых Действий (Preflop, Flop, Turn, River) ---
    if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        bool bActionCausedAggression = false;
        bool bActionWasValidAndPerformed = true; // Флаг, чтобы не переходить к след. ходу, если действие было невалидным

        // Проверяем, может ли игрок вообще действовать (не Folded, не All-In который уже не может повлиять)
        if (Player.Status == EPlayerStatus::Folded || (Player.Status == EPlayerStatus::AllIn && Player.Stack == 0))
        {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Player %s (Seat %d) cannot make a new voluntary action (Folded or All-In with 0 stack)."), *PlayerName, ActingPlayerSeatIndex);
            // Этот игрок будет пропущен логикой IsBettingRoundOver / GetNextPlayerToAct.
            // Снимаем флаг хода, так как он не действовал.
            Player.bIsTurn = false;
            // Не устанавливаем bHasActedThisSubRound, так как он не действовал.
            // Сразу переходим к проверке конца раунда, как если бы он пропустил ход.
            bActionWasValidAndPerformed = true; // Технически, действие "пропуск хода" было выполнено
        }
        else // Игрок может действовать
        {
            // Флаг хода будет снят с этого игрока и установлен следующему в RequestPlayerAction,
            // или если раунд/рука заканчивается, CurrentTurnSeat будет сброшен.
            // Player.bIsTurn = false; // Пока не сбрасываем, если действие невалидно, ход его.

            switch (PlayerAction)
            {
            case EPlayerAction::Fold:
                Player.Status = EPlayerStatus::Folded;
                Player.bHasActedThisSubRound = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s folds."), *PlayerName));
                break;

            case EPlayerAction::Check:
                if (Player.CurrentBet == GameStateData->CurrentBetToCall) {
                    Player.bHasActedThisSubRound = true;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s checks."), *PlayerName));
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Check by %s. BetToCall: %lld, PlayerBet: %lld. Re-requesting action."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                    RequestPlayerAction(ActingPlayerSeatIndex);
                    return; // Действие невалидно, выходим
                }
                break;

            case EPlayerAction::Call:
            {
                int64 AmountNeededToCallAbsolute = GameStateData->CurrentBetToCall - Player.CurrentBet;
                if (AmountNeededToCallAbsolute <= 0) {
                    if (Player.CurrentBet == GameStateData->CurrentBetToCall) {
                        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s effectively checks (attempted invalid call)."), *PlayerName));
                        Player.bHasActedThisSubRound = true;
                    }
                    else {
                        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: %s Call error. BetToCall: %lld, PlayerBet: %lld. Re-requesting."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                        RequestPlayerAction(ActingPlayerSeatIndex); return;
                    }
                    break;
                }
                int64 ActualAmountPlayerPutsInPot = FMath::Min(AmountNeededToCallAbsolute, Player.Stack);
                Player.Stack -= ActualAmountPlayerPutsInPot;
                Player.CurrentBet += ActualAmountPlayerPutsInPot;
                GameStateData->Pot += ActualAmountPlayerPutsInPot;
                Player.bHasActedThisSubRound = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s calls %lld. Stack: %lld"), *PlayerName, ActualAmountPlayerPutsInPot, Player.Stack));
                if (Player.Stack == 0) {
                    Player.Status = EPlayerStatus::AllIn;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                }
            }
            break;

            case EPlayerAction::Bet:
            {
                if (Player.CurrentBet != GameStateData->CurrentBetToCall) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Bet by %s (Must Call/Check). Re-requesting."), *PlayerName);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                int64 MinBetSize = GameStateData->BigBlindAmount;
                if ((Amount < MinBetSize && Amount < Player.Stack) || Amount <= 0) { // Не олл-ин и меньше мин.бета ИЛИ ставка <=0
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Bet by %s (Amount %lld invalid vs MinBet %lld or Stack %lld). Re-requesting."), *PlayerName, Amount, MinBetSize, Player.Stack);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
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
                Player.bHasActedThisSubRound = true;
                bActionCausedAggression = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s bets %lld. Stack: %lld"), *PlayerName, Amount, Player.Stack));
                if (Player.Stack == 0) {
                    Player.Status = EPlayerStatus::AllIn;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                }
            }
            break;

            case EPlayerAction::Raise:
            {
                if (GameStateData->CurrentBetToCall == 0 || Player.CurrentBet >= GameStateData->CurrentBetToCall) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (No valid bet to raise over or already acted). Re-requesting."), *PlayerName);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }

                int64 TotalNewBetByPlayer = Amount; // Amount от UI - это ОБЩАЯ сумма, до которой игрок рейзит
                int64 AmountPlayerActuallyAddsToPotNow = TotalNewBetByPlayer - Player.CurrentBet;

                if (AmountPlayerActuallyAddsToPotNow <= 0 && TotalNewBetByPlayer < (Player.CurrentBet + Player.Stack)) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (Adds %lld to pot, <=0, not all-in). Re-requesting."), *PlayerName, AmountPlayerActuallyAddsToPotNow);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                if (AmountPlayerActuallyAddsToPotNow > Player.Stack) {
                    AmountPlayerActuallyAddsToPotNow = Player.Stack;
                    TotalNewBetByPlayer = Player.CurrentBet + Player.Stack;
                }

                int64 PureRaiseAmountOverPreviousHighestBet = TotalNewBetByPlayer - GameStateData->CurrentBetToCall;
                int64 MinValidPureRaise = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;

                if (PureRaiseAmountOverPreviousHighestBet < MinValidPureRaise && AmountPlayerActuallyAddsToPotNow < Player.Stack) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (PureRaise %lld < MinValidPureRaise %lld and not All-In). Re-requesting."), *PlayerName, PureRaiseAmountOverPreviousHighestBet, MinValidPureRaise);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                if (TotalNewBetByPlayer <= GameStateData->CurrentBetToCall && AmountPlayerActuallyAddsToPotNow < Player.Stack) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (TotalBet %lld not > ToCall %lld and not All-In). Re-requesting."), *PlayerName, TotalNewBetByPlayer, GameStateData->CurrentBetToCall);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }

                Player.Stack -= AmountPlayerActuallyAddsToPotNow;
                Player.CurrentBet = TotalNewBetByPlayer;
                GameStateData->Pot += AmountPlayerActuallyAddsToPotNow;
                GameStateData->CurrentBetToCall = Player.CurrentBet;
                GameStateData->LastBetOrRaiseAmountInCurrentRound = PureRaiseAmountOverPreviousHighestBet > 0 ? PureRaiseAmountOverPreviousHighestBet : MinValidPureRaise;
                GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex;
                Player.bHasActedThisSubRound = true;
                bActionCausedAggression = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s raises to %lld (added %lld). Stack: %lld"), *PlayerName, Player.CurrentBet, AmountPlayerActuallyAddsToPotNow, Player.Stack));
                if (Player.Stack == 0) {
                    Player.Status = EPlayerStatus::AllIn;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName));
                }
            }
            break;

            default:
                UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Unknown action %s received."), *UEnum::GetValueAsString(PlayerAction));
                RequestPlayerAction(ActingPlayerSeatIndex);
                return;
            }
            Player.bIsTurn = false; // Снимаем флаг хода после успешного действия
        } // конец if (Player can act)

        // Если было совершено агрессивное действие (Bet или Raise),
        // сбрасываем флаг bHasActedThisSubRound для всех ДРУГИХ активных игроков.
        if (bActionCausedAggression) {
            for (FPlayerSeatData& SeatToReset : GameStateData->Seats) {
                if (SeatToReset.SeatIndex != ActingPlayerSeatIndex &&
                    SeatToReset.bIsSittingIn &&
                    SeatToReset.Status != EPlayerStatus::Folded &&
                    SeatToReset.Status != EPlayerStatus::AllIn) { // All-In игроки не будут ходить снова в этом раунде
                    SeatToReset.bHasActedThisSubRound = false;
                }
            }
        }

        // --- Логика после действия игрока ---
        // Сначала проверяем, не остался ли один игрок после этого действия (особенно после Fold)
        int32 ActivePlayersStillInHand = 0;
        int32 LastManStandingIndex = -1;
        for (const FPlayerSeatData& Seat : GameStateData->Seats) {
            if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded) {
                ActivePlayersStillInHand++;
                LastManStandingIndex = Seat.SeatIndex;
            }
        }

        if (ActivePlayersStillInHand <= 1) {
            if (ActivePlayersStillInHand == 1 && LastManStandingIndex != -1) {
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s wins the pot of %lld."), *GameStateData->Seats[LastManStandingIndex].PlayerName, GameStateData->Pot));
                AwardPotToWinner({ LastManStandingIndex });
            }
            else {
                UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Hand ends with %d players after action. Pot: %lld"), ActivePlayersStillInHand, GameStateData->Pot);
                if (GameStateData->Pot > 0) AwardPotToWinner({});
            }
            GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
            GameStateData->CurrentTurnSeat = -1; // Сбрасываем текущий ход
            if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
            if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over"), GameStateData->Pot);
            if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
            if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
            // НЕ ВЫЗЫВАЕМ StartNewHand() автоматически
            return;
        }

        // Если игроков больше одного, проверяем, завершен ли раунд ставок
        if (IsBettingRoundOver()) {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Betting round IS over. Proceeding to next stage."));
            ProceedToNextGameStage();
        }
        else {
            int32 NextPlayerToAct = GetNextPlayerToAct(ActingPlayerSeatIndex, true, EPlayerStatus::MAX_None);
            if (NextPlayerToAct != -1) {
                UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Betting round NOT over. Next player to act is Seat %d."), NextPlayerToAct);
                RequestPlayerAction(NextPlayerToAct);
            }
            else {
                UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction CRITICAL: IsBettingRoundOver() is FALSE, but GetNextPlayerToAct returned -1! Stage: %s. Last Actor: %d. Attempting to advance stage to prevent stall."),
                    *UEnum::GetValueAsString(GameStateData->CurrentStage), ActingPlayerSeatIndex);
                ProceedToNextGameStage(); // Аварийный переход
            }
        }
        return;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Action %s received in unhandled game stage %s."),
            *UEnum::GetValueAsString(PlayerAction), *UEnum::GetValueAsString(GameStateData->CurrentStage));
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
    if (!GameStateData || !GameStateData || !Deck || !Deck) {
        UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: GameStateData or Deck is null!"));
        // Попытка безопасно завершить, если возможно
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Error: Game State Null"), GameStateData ? GameStateData->Pot : 0);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData ? GameStateData->BigBlindAmount : 0, 0);
        return;
    }

    EGameStage StageBeforeAdvance = GameStateData->CurrentStage;
    UE_LOG(LogTemp, Log, TEXT("--- ProceedToNextGameStage: Advancing from %s ---"), *UEnum::GetValueAsString(StageBeforeAdvance));
    if (OnGameHistoryEventDelegate.IsBound()) {
        OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Betting round on %s ended. ---"), *UEnum::GetValueAsString(StageBeforeAdvance)));
    }

    // 1. Проверяем, сколько игроков еще в игре (не сфолдили и могут претендовать на банк).
    // Это дублирует проверку из ProcessPlayerAction, но здесь она важна перед раздачей карт.
    TArray<int32> PlayersLeftInPotIndices;
    int32 NumPlayersAbleToMakeFurtherBets = 0;

    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded) {
            PlayersLeftInPotIndices.Add(Seat.SeatIndex);
            // Считаем тех, кто не сфолдил И имеет стек > 0 И не AllIn (или AllIn, но может влиять на побочные банки - для MVP считаем только тех, кто может ставить в основной банк)
            if (Seat.Stack > 0 && Seat.Status != EPlayerStatus::AllIn) { // Игрок еще может делать ставки
                NumPlayersAbleToMakeFurtherBets++;
            }
        }
    }

    // Если остался только один игрок, не сфолдивший, он выигрывает банк (это должно было обработаться в ProcessPlayerAction)
    if (PlayersLeftInPotIndices.Num() <= 1) {
        if (PlayersLeftInPotIndices.Num() == 1) {
            UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Only one player (%s, Seat %d) left. Awarding pot."), *GameStateData->Seats[PlayersLeftInPotIndices[0]].PlayerName, PlayersLeftInPotIndices[0]);
            AwardPotToWinner(PlayersLeftInPotIndices);
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage: 0 players left in pot. Pot: %lld"), GameStateData->Pot);
            if (GameStateData->Pot > 0) AwardPotToWinner({});
        }
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over"), GameStateData->Pot);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
        // НЕ ЗАПУСКАЕМ StartNewHand() здесь, ждем команды от UI
        return;
    }

    // Если это была последняя улица (Ривер), и раунд ставок завершен, переходим к Шоудауну
    if (StageBeforeAdvance == EGameStage::River) {
        UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: River betting complete with %d players. Proceeding to Showdown."), PlayersLeftInPotIndices.Num());
        ProceedToShowdown();
        return;
    }

    // --- Готовимся к НОВОМУ РАУНДУ СТАВОК на следующей улице ---
    UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Preparing for next betting round. Players in pot: %d, Able to bet: %d"), PlayersLeftInPotIndices.Num(), NumPlayersAbleToMakeFurtherBets);

    // 2. Сброс состояния для нового раунда ставок
    GameStateData->CurrentBetToCall = 0;
    GameStateData->LastBetOrRaiseAmountInCurrentRound = 0;
    GameStateData->LastAggressorSeatIndex = -1;
    GameStateData->PlayerWhoOpenedBettingThisRound = -1; // Будет установлен первым ходящим на новой улице

    for (FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded) { // Для всех, кто еще в игре
            Seat.CurrentBet = 0; // Ставки предыдущего раунда уже в банке, обнуляем для нового раунда
            // Сбрасываем bHasActedThisSubRound только для тех, кто НЕ AllIn и НЕ Folded
            // Игроки AllIn уже не будут ходить, их флаг можно считать "отработанным"
            if (Seat.Status != EPlayerStatus::AllIn) {
                Seat.bHasActedThisSubRound = false;
                UE_LOG(LogTemp, Verbose, TEXT("ProceedToNextGameStage: Reset bHasActedThisSubRound for Seat %d."), Seat.SeatIndex);
            }
        }
    }

    EGameStage NextStageToSet = GameStateData->CurrentStage;
    bool bDealingErrorOccurred = false;

    // 3. Раздача карт для следующей улицы
    switch (StageBeforeAdvance)
    {
    case EGameStage::Preflop:
    { // Используем блок для локализации переменных
        NextStageToSet = EGameStage::Flop;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Dealing %s ---"), *UEnum::GetValueAsString(NextStageToSet)));
        // Deck->DealCard(); // Опциональное сжигание карты
        for (int i = 0; i < 3; ++i) {
            TOptional<FCard> DealtCardOpt = Deck->DealCard();
            if (DealtCardOpt.IsSet()) {
                GameStateData->CommunityCards.Add(DealtCardOpt.GetValue());
            }
            else {
                UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: Deck ran out for Flop card %d!"), i + 1);
                bDealingErrorOccurred = true; break;
            }
        }
        if (!bDealingErrorOccurred && GameStateData->CommunityCards.Num() >= 3 && OnGameHistoryEventDelegate.IsBound()) {
            OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Flop: %s %s %s"),
                *GameStateData->CommunityCards[0].ToString(), *GameStateData->CommunityCards[1].ToString(), *GameStateData->CommunityCards[2].ToString()));
        }
    }
    break;

    case EGameStage::Flop:
    {
        NextStageToSet = EGameStage::Turn;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Dealing %s ---"), *UEnum::GetValueAsString(NextStageToSet)));
        // Deck->DealCard(); // Сжигание карты
        TOptional<FCard> TurnCardOpt = Deck->DealCard();
        if (TurnCardOpt.IsSet()) {
            GameStateData->CommunityCards.Add(TurnCardOpt.GetValue());
            if (GameStateData->CommunityCards.Num() >= 4 && OnGameHistoryEventDelegate.IsBound()) {
                OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Turn: %s"), *GameStateData->CommunityCards.Last().ToString()));
            }
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: Deck ran out for Turn card!"));
            bDealingErrorOccurred = true;
        }
    }
    break;

    case EGameStage::Turn:
    {
        NextStageToSet = EGameStage::River;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Dealing %s ---"), *UEnum::GetValueAsString(NextStageToSet)));
        // Deck->DealCard(); // Сжигание карты
        TOptional<FCard> RiverCardOpt = Deck->DealCard();
        if (RiverCardOpt.IsSet()) {
            GameStateData->CommunityCards.Add(RiverCardOpt.GetValue());
            if (GameStateData->CommunityCards.Num() >= 5 && OnGameHistoryEventDelegate.IsBound()) {
                OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("River: %s"), *GameStateData->CommunityCards.Last().ToString()));
            }
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: Deck ran out for River card!"));
            bDealingErrorOccurred = true;
        }
    }
    break;

    default:
        UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: Called from invalid stage to deal community cards: %s."), *UEnum::GetValueAsString(StageBeforeAdvance));
        return; // Прерываем, если стадия некорректна для раздачи общих карт
    }

    // Если произошла ошибка при раздаче (кончились карты)
    if (bDealingErrorOccurred) {
        UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: Error during community card dealing. Hand cannot continue as planned. Proceeding to showdown with current cards."));
        // Переходим к шоудауну с тем, что есть на столе
        ProceedToShowdown();
        return;
    }

    // 4. Уведомляем UI об обновлении ВСЕХ общих карт
    if (OnCommunityCardsUpdatedDelegate.IsBound()) {
        OnCommunityCardsUpdatedDelegate.Broadcast(GameStateData->CommunityCards);
    }

    // 5. Устанавливаем новую стадию игры
    GameStateData->CurrentStage = NextStageToSet;
    UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: New stage is %s. Community Cards Count: %d"), *UEnum::GetValueAsString(NextStageToSet), GameStateData->CommunityCards.Num());

    // 6. Начинаем новый круг торгов, ЕСЛИ ЕСТЬ ХОТЯ БЫ ДВА ИГРОКА, СПОСОБНЫХ ДЕЛАТЬ СТАВКИ
    if (NumPlayersAbleToMakeFurtherBets >= 2) {
        int32 FirstToActOnNewStreet = DetermineFirstPlayerToActPostflop();
        if (FirstToActOnNewStreet != -1) {
            GameStateData->PlayerWhoOpenedBettingThisRound = FirstToActOnNewStreet; // Первый ходящий открывает торги на новой улице
            UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Starting new betting round. First to act on %s is Seat %d (%s). Opener set."),
                *UEnum::GetValueAsString(NextStageToSet), FirstToActOnNewStreet, *GameStateData->Seats[FirstToActOnNewStreet].PlayerName);
            RequestPlayerAction(FirstToActOnNewStreet);
        }
        else {
            // Эта ситуация (NumPlayersAbleToMakeFurtherBets >= 2, но FirstToActOnNewStreet == -1) не должна возникать,
            // если GetNextPlayerToAct работает корректно.
            UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage CRITICAL: %d players able to bet, but no first actor found for %s! Advancing to prevent stall."),
                NumPlayersAbleToMakeFurtherBets, *UEnum::GetValueAsString(NextStageToSet));
            // Аварийный переход (если это не ривер, то на следующую улицу, если ривер - то шоудаун)
            if (NextStageToSet < EGameStage::River) { ProceedToNextGameStage(); }
            else { ProceedToShowdown(); }
        }
    }
    else { // Меньше двух игроков могут делать ставки (например, один может, остальные олл-ин, или все олл-ин)
        UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Less than 2 players able to make further bets (%d). Auto-advancing or showdown."), NumPlayersAbleToMakeFurtherBets);
        if (NextStageToSet < EGameStage::River && PlayersLeftInPotIndices.Num() > 1) { // Если есть >1 игрока в поте, но они не могут ставить
            ProceedToNextGameStage(); // Раздаем оставшиеся карты до ривера автоматически
        }
        else if (PlayersLeftInPotIndices.Num() > 1) { // Мы на ривере (или была ошибка) и >1 игрока в поте -> шоудаун
            ProceedToShowdown();
        }
        else {
            // Если <=1 игрока в поте, это должно было обработаться в начале функции.
            // Но если мы здесь, значит, что-то пошло не так.
            UE_LOG(LogTemp, Warning, TEXT("ProceedToNextGameStage: Fallback - Hand ending due to few players left (%d) after stage change."), PlayersLeftInPotIndices.Num());
            if (PlayersLeftInPotIndices.Num() == 1) AwardPotToWinner(PlayersLeftInPotIndices); else AwardPotToWinner({});
            GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
            // ... (уведомление UI о конце руки) ...
        }
    }
}

void UOfflineGameManager::ProceedToShowdown()
{
    if (!GameStateData || !GameStateData) {
        UE_LOG(LogTemp, Error, TEXT("ProceedToShowdown: GameStateData is null!"));
        // Попытка безопасно завершить, если возможно
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Error: Game State Null"), GameStateData ? GameStateData->Pot : 0);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData ? GameStateData->BigBlindAmount : 0, 0);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("--- PROCEEDING TO SHOWDOWN ---"));
    if (OnGameHistoryEventDelegate.IsBound()) {
        OnGameHistoryEventDelegate.Broadcast(TEXT("--- Showdown ---"));
        FString CommunityCardsString = TEXT("Community Cards: ");
        for (int32 i = 0; i < GameStateData->CommunityCards.Num(); ++i) {
            CommunityCardsString += GameStateData->CommunityCards[i].ToString();
            if (i < GameStateData->CommunityCards.Num() - 1) CommunityCardsString += TEXT(" ");
        }
        OnGameHistoryEventDelegate.Broadcast(CommunityCardsString);
    }
    GameStateData->CurrentStage = EGameStage::Showdown;
    GameStateData->CurrentTurnSeat = -1; // На шоудауне нет активного хода для ставок
    if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1); // Уведомляем UI

    TArray<int32> ShowdownPlayerIndices;
    // Структура для хранения индекса игрока и результата его руки для сортировки и определения победителя
    struct FPlayerHandEvaluation
    {
        int32 SeatIndex;
        FPokerHandResult HandResult;
        // Можно добавить ссылку на FPlayerSeatData, если нужно больше информации об игроке
    };
    TArray<FPlayerHandEvaluation> EvaluatedHands;

    // 1. Определяем игроков, участвующих в шоудауне (не сфолдили и имеют право на банк)
    for (const FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded)
        {
            // Для MVP считаем, что все не сфолдившие участвуют в основном банке.
            // Логика побочных банков (side pots) потребует более сложного определения участников.
            ShowdownPlayerIndices.Add(Seat.SeatIndex);
        }
    }

    if (ShowdownPlayerIndices.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("Showdown: No players to showdown? This shouldn't happen if hand reached here with >1 active player. Pot: %lld"), GameStateData->Pot);
        if (GameStateData->Pot > 0) AwardPotToWinner({}); // Попытка обработать банк
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers; // Готовимся к новой руке
        // Уведомляем UI, что рука окончена и можно нажать "Next Hand"
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over (No Showdown Players)"), GameStateData->Pot);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
        return;
    }

    // Если только один игрок дошел до шоудауна (остальные сфолдили на предыдущих улицах), он выигрывает.
    // Эта проверка также была в ProcessPlayerAction и ProceedToNextGameStage.
    if (ShowdownPlayerIndices.Num() == 1) {
        UE_LOG(LogTemp, Log, TEXT("Showdown: Only one player (%s) in showdown. Awarding pot."), *GameStateData->Seats[ShowdownPlayerIndices[0]].PlayerName);
        AwardPotToWinner(ShowdownPlayerIndices);
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over"), GameStateData->Pot);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);
        return;
    }

    // Уведомляем контроллер о шоудауне и участниках, чтобы он мог ПОКАЗАТЬ ИХ КАРТЫ в UI
    if (OnShowdownDelegate.IsBound()) {
        UE_LOG(LogTemp, Log, TEXT("Showdown: Broadcasting OnShowdownDelegate for %d players to reveal cards."), ShowdownPlayerIndices.Num());
        OnShowdownDelegate.Broadcast(ShowdownPlayerIndices);
    }
    // ВАЖНО: Здесь UI должен показать карты. В реальной игре была бы пауза.
    // Мы продолжим выполнение, предполагая, что UI обновился или обновится асинхронно.

    // 2. Оцениваем руки каждого участника шоудауна
    UE_LOG(LogTemp, Log, TEXT("Showdown: Evaluating hands of %d players..."), ShowdownPlayerIndices.Num());
    for (int32 SeatIndex : ShowdownPlayerIndices)
    {
        const FPlayerSeatData& Player = GameStateData->Seats[SeatIndex];
        // Убедимся, что у игрока действительно есть карты для оценки
        // (он мог быть олл-ин до раздачи всех своих карт, но это редкий случай и должен обрабатываться ранее)
        if (Player.HoleCards.Num() == 2)
        {
            FPokerHandResult HandResult = UPokerHandEvaluator::EvaluatePokerHand(Player.HoleCards, GameStateData->CommunityCards);
            EvaluatedHands.Add({ SeatIndex, HandResult });

            if (OnGameHistoryEventDelegate.IsBound()) {
                FString KickersStr;
                for (ECardRank Kicker : HandResult.Kickers) { KickersStr += UEnum::GetDisplayValueAsText(Kicker).ToString() + TEXT(" "); }
                KickersStr = KickersStr.TrimEnd();
                OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s (Seat %d) shows: %s. Kickers: [%s]"),
                    *Player.PlayerName, SeatIndex, *UEnum::GetDisplayValueAsText(HandResult.HandRank).ToString(), *KickersStr));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Showdown: Player %s (Seat %d) has %d hole cards, cannot evaluate hand properly. Skipping for evaluation."),
                *Player.PlayerName, SeatIndex, Player.HoleCards.Num());
            // Такого игрока можно либо исключить, либо присвоить ему самую слабую из возможных рук, если он все еще в поте.
            // Для простоты MVP, если у него нет 2 карт, но он дошел до вскрытия (что странно), его рука не оценивается.
        }
    }

    if (EvaluatedHands.Num() == 0 && ShowdownPlayerIndices.Num() > 0) {
        UE_LOG(LogTemp, Error, TEXT("Showdown: No hands could be evaluated, but there were players in showdown. Awarding pot to all showdown players if pot > 0."));
        if (GameStateData->Pot > 0) AwardPotToWinner(ShowdownPlayerIndices); else AwardPotToWinner({});
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        // ... (уведомление UI о конце руки) ...
        return;
    }
    if (EvaluatedHands.Num() == 0 && ShowdownPlayerIndices.Num() == 0) { // Двойная проверка, если вдруг ShowdownPlayerIndices был пуст
        UE_LOG(LogTemp, Warning, TEXT("Showdown: No hands evaluated and no players in showdown. Hand ends."));
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        // ... (уведомление UI о конце руки) ...
        return;
    }


    // 3. Определяем победителя(ей) среди тех, чьи руки были оценены
    TArray<int32> WinningSeatIndices;
    if (EvaluatedHands.Num() > 0)
    {
        // Сортируем оцененные руки от лучшей к худшей
        EvaluatedHands.Sort([](const FPlayerHandEvaluation& A, const FPlayerHandEvaluation& B) {
            return UPokerHandEvaluator::CompareHandResults(A.HandResult, B.HandResult) > 0;
            });

        // Первая рука в отсортированном массиве - лучшая
        FPokerHandResult BestHand = EvaluatedHands[0].HandResult;
        WinningSeatIndices.Add(EvaluatedHands[0].SeatIndex);

        // Проверяем на ничьи с этой лучшей рукой
        for (int32 i = 1; i < EvaluatedHands.Num(); ++i)
        {
            if (UPokerHandEvaluator::CompareHandResults(EvaluatedHands[i].HandResult, BestHand) == 0) // Руки равны
            {
                WinningSeatIndices.Add(EvaluatedHands[i].SeatIndex);
            }
            else // Руки слабее, дальше можно не проверять, так как массив отсортирован
            {
                break;
            }
        }
    }

    // 4. Награждаем победителя(ей)
    if (WinningSeatIndices.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Showdown: Awarding pot to %d winner(s)."), WinningSeatIndices.Num());
        AwardPotToWinner(WinningSeatIndices); // Эта функция обнулит GameStateData->Pot
    }
    else if (GameStateData->Pot > 0) // Если были участники, но не смогли определить победителя (ошибка в логике)
    {
        UE_LOG(LogTemp, Error, TEXT("Showdown: No winners determined but pot exists and players were in showdown! This is a critical error. Pot: %lld. Attempting to split among all showdown players."), GameStateData->Pot);
        AwardPotToWinner(ShowdownPlayerIndices); // Пытаемся разделить между всеми, кто дошел до вскрытия
    }

    // 5. Рука завершена, устанавливаем состояние ожидания для UI кнопки "Next Hand"
    UE_LOG(LogTemp, Log, TEXT("--- HAND OVER (After Showdown) ---"));
    GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
    GameStateData->CurrentTurnSeat = -1;
    // Уведомляем UI
    if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
    if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over. Press 'Next Hand'."), GameStateData->Pot); // Pot должен быть 0
    if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
    if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0);

    // НЕ ВЫЗЫВАЕМ StartNewHand() автоматически
}

void UOfflineGameManager::AwardPotToWinner(const TArray<int32>& WinningSeatIndices)
{
    if (!GameStateData || !GameStateData) {
        UE_LOG(LogTemp, Error, TEXT("AwardPotToWinner: GameStateData is null!"));
        return;
    }

    int64 TotalPotToAward = GameStateData->Pot; // TODO: Это основной банк. Логика побочных банков будет сложнее.

    if (TotalPotToAward <= 0) {
        UE_LOG(LogTemp, Log, TEXT("AwardPotToWinner: Pot is %lld, nothing to award."), TotalPotToAward);
        GameStateData->Pot = 0; // Убедимся, что он 0
        return;
    }

    if (WinningSeatIndices.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("AwardPotToWinner: No winners provided, but pot is %lld. Pot will be reset (or should be handled differently)."), TotalPotToAward);
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Pot of %lld remains unawarded (no winners)."), TotalPotToAward));
        GameStateData->Pot = 0; // Сбрасываем банк
        return;
    }

    int64 SharePerWinner = TotalPotToAward / WinningSeatIndices.Num();
    int64 RemainderChips = TotalPotToAward % WinningSeatIndices.Num(); // Остаток фишек

    FString WinnersLogString = TEXT("Winner(s): ");
    bool bFirstWinnerRemainder = true;

    for (int32 WinnerIdx : WinningSeatIndices)
    {
        if (GameStateData->Seats.IsValidIndex(WinnerIdx))
        {
            FPlayerSeatData& Winner = GameStateData->Seats[WinnerIdx];
            int64 CurrentShare = SharePerWinner;
            if (bFirstWinnerRemainder && RemainderChips > 0)
            {
                CurrentShare += RemainderChips; // Отдаем остаток первому в списке
                RemainderChips = 0; // Остаток отдан
                bFirstWinnerRemainder = false;
            }
            Winner.Stack += CurrentShare;
            WinnersLogString += FString::Printf(TEXT("%s (Seat %d) +%lld (Stack: %lld) "), *Winner.PlayerName, Winner.SeatIndex, CurrentShare, Winner.Stack);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("AwardPotToWinner: Invalid winner seat index %d found in WinningSeatIndices!"), WinnerIdx);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("AwardPotToWinner: %s. Total Pot awarded was %lld."), *WinnersLogString, TotalPotToAward);
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(WinnersLogString);

    GameStateData->Pot = 0; // Обнуляем основной банк
}