#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "PokerBotAI.h"
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

    // --- НОВОЕ: Инициализация BotAIInstance ---
    if (!BotAIInstance) // Создаем, только если еще не создан (на случай повторного вызова InitializeGame)
    {
        BotAIInstance = NewObject<UPokerBotAI>(this); // "this" (OfflineGameManager) будет владельцем
        if (BotAIInstance)
        {
            UE_LOG(LogTemp, Log, TEXT("InitializeGame: BotAIInstance created successfully."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("InitializeGame: Failed to create BotAIInstance! Bots will not function."));
            // Решите, является ли это критической ошибкой. Для MVP без ботов можно продолжать.
            // Если боты обязательны, можно здесь return;
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("InitializeGame: BotAIInstance already exists."));
    }
    // --- КОНЕЦ НОВОГО ---

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
        if (NumRealPlayers > TotalActivePlayers) NumRealPlayers = TotalActivePlayers; // Убедимся, что реальных игроков не больше, чем всего мест
        NumBots = TotalActivePlayers - NumRealPlayers;
        if (NumBots < 0) NumBots = 0; // На всякий случай
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
    GameStateData->Seats.Empty(); // Очищаем на случай повторной инициализации
    GameStateData->Seats.Reserve(TotalActivePlayers);
    for (int32 i = 0; i < TotalActivePlayers; ++i) {
        FPlayerSeatData Seat;
        Seat.SeatIndex = i;
        Seat.bIsBot = (i >= NumRealPlayers); // Первые NumRealPlayers - это реальные игроки
        Seat.PlayerName = Seat.bIsBot ? FString::Printf(TEXT("Bot %d"), (i - NumRealPlayers) + 1) : PlayerActualName; // Нумерация ботов с 1
        Seat.PlayerId = Seat.bIsBot ? -1 : PlayerActualId;
        Seat.Stack = InitialStack;
        Seat.bIsSittingIn = true; // Все игроки начинают "в игре"
        Seat.Status = EPlayerStatus::Waiting; // Начальный статус до первой раздачи
        GameStateData->Seats.Add(Seat);
    }
    BuildTurnOrderMap();
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

    BuildTurnOrderMap();

    UE_LOG(LogTemp, Log, TEXT("--- STARTING NEW HAND ---"));
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- New Hand Starting ---"));

    if (OnNewHandAboutToStartDelegate.IsBound())
    {
        UE_LOG(LogTemp, Log, TEXT("StartNewHand: Broadcasting OnNewHandAboutToStartDelegate.")); // Добавьте лог
        OnNewHandAboutToStartDelegate.Broadcast();
    }

    StacksAtHandStart_Internal.Empty();
    if (GameStateData) // Добавим проверку, хотя она должна быть выше
    {
        for (const FPlayerSeatData& Seat : GameStateData->Seats)
        {
            if (Seat.bIsSittingIn) // Сохраняем для всех, кто сидит за столом (даже если стек 0, чтобы не было ошибок)
            {
                StacksAtHandStart_Internal.Add(Seat.SeatIndex, Seat.Stack);
                UE_LOG(LogTemp, Verbose, TEXT("StartNewHand: Recorded StartStack for Seat %d: %lld"), Seat.SeatIndex, Seat.Stack);
            }
        }
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
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0, 0);
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


void UOfflineGameManager::BuildTurnOrderMap()
{
    CurrentTurnOrderMap_Internal.Empty(); // Очищаем старую карту

    if (!GameStateData || GameStateData->Seats.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("BuildTurnOrderMap: Not enough players in GameStateData->Seats (%d) or GameStateData is null. Turn order map will be empty."),
            GameStateData ? GameStateData->Seats.Num() : 0);
        return;
    }

    // 1. Собираем индексы всех игроков, которые СЕЙЧАС сидят за столом (bIsSittingIn)
    // На этом этапе нам не так важен их Stack > 0 или Status, так как порядок хода
    // определяется рассадкой. GetNextPlayerToAct уже будет проверять, может ли игрок ходить.
    TArray<int32> PlayersConsideredForTurnOrder;
    for (const FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.bIsSittingIn)
        {
            PlayersConsideredForTurnOrder.Add(Seat.SeatIndex);
        }
    }

    if (PlayersConsideredForTurnOrder.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("BuildTurnOrderMap: Less than 2 players are SittingIn (%d). Turn order map will be empty."), PlayersConsideredForTurnOrder.Num());
        return;
    }

    // 2. Ваша "идеальная" полная последовательность для 9 игроков, движущихся по часовой стрелке (слева от предыдущего)
    // 0 -> 2 -> 4 -> 6 -> 7 -> 8 -> 5 -> 3 -> 1 -> (снова 0)
    const TArray<int32> FullFixedOrderNinePlayers = { 0, 2, 4, 6, 7, 8, 5, 3, 1 };

    // 3. Создаем актуальный порядок хода для текущей игры, фильтруя FullFixedOrder
    //    и оставляя только тех, кто есть в PlayersConsideredForTurnOrder.
    TArray<int32> ActualTurnOrderInThisGame;
    for (int32 OrderedSeatIndex : FullFixedOrderNinePlayers)
    {
        // Проверяем, присутствует ли игрок из "идеального порядка" среди тех, кто сейчас сидит за столом
        if (PlayersConsideredForTurnOrder.Contains(OrderedSeatIndex))
        {
            // Дополнительно убедимся, что такой индекс действительно есть в GameStateData->Seats,
            // хотя PlayersConsideredForTurnOrder уже должен содержать только валидные индексы из Seats.
            if (GameStateData->Seats.IsValidIndex(OrderedSeatIndex) && GameStateData->Seats[OrderedSeatIndex].bIsSittingIn)
            {
                ActualTurnOrderInThisGame.Add(OrderedSeatIndex);
            }
        }
    }

    if (ActualTurnOrderInThisGame.Num() < 2)
    {
        // Это может произойти, если PlayersConsideredForTurnOrder содержит индексы,
        // которых нет в FullFixedOrderNinePlayers, или если совпадений слишком мало.
        // Например, если играют только игроки с индексами, которых нет в вашей стандартной рассадке.
        // Или если TotalActivePlayers в InitializeGame было > 9, и создались места с индексами > 8.
        UE_LOG(LogTemp, Error, TEXT("BuildTurnOrderMap: Could not establish a valid turn order for %d active players based on fixed sequence. Players considered: %d. Filtered order count: %d."),
            PlayersConsideredForTurnOrder.Num(), PlayersConsideredForTurnOrder.Num(), ActualTurnOrderInThisGame.Num());
        // В этом случае можно попробовать построить простой круговой порядок на основе PlayersConsideredForTurnOrder,
        // но это нарушит вашу "особую" рассадку. Для отладки лучше остановиться здесь.
        return;
    }

    // 4. Теперь строим карту: CurrentTurnOrderMap_Internal[текущий_индекс] = следующий_индекс
    UE_LOG(LogTemp, Log, TEXT("BuildTurnOrderMap: Building map for %d players in actual turn order:"), ActualTurnOrderInThisGame.Num());
    for (int32 i = 0; i < ActualTurnOrderInThisGame.Num(); ++i)
    {
        int32 CurrentPlayerActualIndex = ActualTurnOrderInThisGame[i];
        int32 NextPlayerActualIndex = ActualTurnOrderInThisGame[(i + 1) % ActualTurnOrderInThisGame.Num()]; // Зацикливаем на начало

        CurrentTurnOrderMap_Internal.Add(CurrentPlayerActualIndex, NextPlayerActualIndex);
        UE_LOG(LogTemp, Verbose, TEXT("  Turn Order Map: Seat %d -> Next Seat %d"), CurrentPlayerActualIndex, NextPlayerActualIndex);
    }
    UE_LOG(LogTemp, Log, TEXT("BuildTurnOrderMap: Turn order map successfully built with %d entries."), CurrentTurnOrderMap_Internal.Num());
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

void UOfflineGameManager::RequestPlayerAction(int32 SeatIndex)
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(SeatIndex)) {
        UE_LOG(LogTemp, Warning, TEXT("RequestPlayerAction: Invalid SeatIndex %d or GameStateData null. Broadcasting default/error state."), SeatIndex);
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(SeatIndex);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Error: Invalid State"), GameStateData ? GameStateData->Pot : 0);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData ? GameStateData->BigBlindAmount : 0, 0, 0);
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
    FPlayerSeatData& PlayerToAct = GameStateData->Seats[SeatIndex]; // Переименовали для ясности
    PlayerToAct.bIsTurn = true;

    // Вызываем первые два делегата НЕЗАВИСИМО от того, бот это или игрок,
    // чтобы UI всегда знал, чей ход и какой банк.
    if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(SeatIndex);
    if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(PlayerToAct.PlayerName, GameStateData->Pot);

    // --- НОВАЯ ЛОГИКА ДЛЯ БОТОВ ---
    if (PlayerToAct.bIsBot)
    {
        // Это ход бота, запускаем таймер для принятия решения
        float Delay = FMath::FRandRange(BotActionDelayMin, BotActionDelayMax);
        UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction: It's Bot %s (Seat %d)'s turn. Delaying action by %f seconds."), *PlayerToAct.PlayerName, SeatIndex, Delay);

        // Очищаем предыдущий таймер, если он был активен (на всякий случай)
        if (GetWorld()) // Таймеру нужен мир
        {
            GetWorld()->GetTimerManager().ClearTimer(BotActionTimerHandle);

            FTimerDelegate BotTimerDelegate;
            // Привязываем нашу новую функцию TriggerBotDecision к таймеру
            BotTimerDelegate.BindUFunction(this, FName("TriggerBotDecision"), SeatIndex);
            GetWorld()->GetTimerManager().SetTimer(BotActionTimerHandle, BotTimerDelegate, Delay, false);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("RequestPlayerAction: GetWorld() returned null for Bot %s (Seat %d). Cannot set timer."), *PlayerToAct.PlayerName, SeatIndex);
            // Если мира нет, бот не сможет действовать по таймеру.
            // Можно либо вызвать TriggerBotDecision немедленно, либо обработать как ошибку.
            // Для простоты пока оставим так, но в реальном проекте это нужно обработать.
        }

        // Для бота мы не отправляем AllowedActions и ActionUIDetails,
        // так как локальный игрок не должен видеть кнопки действий для бота.
        // Вместо этого мы отправляем "пустые" или дефолтные значения,
        // чтобы APokerPlayerController мог вызвать DisableButtons у HUD.
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({}); // Пустой массив
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, PlayerToAct.Stack, PlayerToAct.CurrentBet);

        return; // Выходим, так как бот будет действовать по таймеру
    }

    // --- СУЩЕСТВУЮЩАЯ ЛОГИКА ДЛЯ РЕАЛЬНОГО ИГРОКА (ТЕПЕРЬ ИСПОЛЬЗУЕТ GetActionContextForSeat) ---
    // Эта часть выполняется, только если PlayerToAct.bIsBot == false

    UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction: It's Human Player %s (Seat %d)'s turn."), *PlayerToAct.PlayerName, SeatIndex);

    FActionDecisionContext PlayerContext = GetActionContextForSeat(SeatIndex);

    if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast(PlayerContext.AvailableActions);
    if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(
        PlayerContext.AmountToCallUI,
        PlayerContext.MinPureRaiseUI,
        PlayerContext.PlayerCurrentStack,
        PlayerContext.PlayerCurrentBetInRound
    );

    // Финальное логирование для реального игрока
    UE_LOG(LogTemp, Log, TEXT("RequestPlayerAction (Human) for Seat %d (%s). Actions: %d. Stage: %s. Stack: %lld, PBet: %lld, Pot: %lld, ActualToCallUI: %lld, MinPureRaiseUI: %lld"),
        SeatIndex, *PlayerToAct.PlayerName, PlayerContext.AvailableActions.Num(),
        *UEnum::GetValueAsString(GameStateData->CurrentStage), PlayerContext.PlayerCurrentStack, PlayerContext.PlayerCurrentBetInRound,
        GameStateData->Pot, PlayerContext.AmountToCallUI, PlayerContext.MinPureRaiseUI);
}

void UOfflineGameManager::ProcessPlayerAction(int32 ActingPlayerSeatIndex, EPlayerAction PlayerAction, int64 Amount)
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(ActingPlayerSeatIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction: Invalid GameState or ActingPlayerSeatIndex %d. Cannot process action."), ActingPlayerSeatIndex);
        if (OnPlayerTurnStartedDelegate.IsBound() && GameStateData) OnPlayerTurnStartedDelegate.Broadcast(GameStateData->CurrentTurnSeat);
        return;
    }

    if (GameStateData->CurrentTurnSeat != ActingPlayerSeatIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Action from Seat %d, but turn is Seat %d. Re-requesting action from correct player."), ActingPlayerSeatIndex, GameStateData->CurrentTurnSeat);
        RequestPlayerAction(GameStateData->CurrentTurnSeat);
        return;
    }

    FPlayerSeatData& Player = GameStateData->Seats[ActingPlayerSeatIndex];
    FString PlayerName = Player.PlayerName;
    // int64 InitialPlayerStackForLog = Player.Stack; // Можно раскомментировать, если нужно для более детального лога

    UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Seat %d (%s) attempts Action: %s, Amount: %lld. Stage: %s. Stack: %lld, PBet: %lld, Pot: %lld, ToCall: %lld, LstAggr: %d (Amt %lld), Opnr: %d, HasActed: %s"),
        ActingPlayerSeatIndex, *PlayerName, *UEnum::GetValueAsString(PlayerAction), Amount,
        *UEnum::GetValueAsString(GameStateData->CurrentStage), Player.Stack, Player.CurrentBet, GameStateData->Pot, GameStateData->CurrentBetToCall,
        GameStateData->LastAggressorSeatIndex, GameStateData->LastBetOrRaiseAmountInCurrentRound, GameStateData->PlayerWhoOpenedBettingThisRound,
        Player.bHasActedThisSubRound ? TEXT("true") : TEXT("false"));

    // --- Обработка Постановки Блайндов ---
    if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind) {
        if (PlayerAction == EPlayerAction::PostBlind && ActingPlayerSeatIndex == GameStateData->PendingSmallBlindSeat) {
            PostBlinds();
            RequestBigBlind();
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid action/player for SB stage. Re-requesting."));
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
            UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid action/player for BB stage. Re-requesting."));
            RequestPlayerAction(ActingPlayerSeatIndex);
        }
        return;
    }

    // --- Обработка Игровых Действий (Preflop, Flop, Turn, River) ---
    if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        bool bActionCausedAggression = false;

        // Проверяем, может ли игрок вообще действовать (не Folded, не All-In который уже не может повлиять на банк)
        if (Player.Status == EPlayerStatus::Folded || (Player.Status == EPlayerStatus::AllIn && Player.Stack == 0 && Player.CurrentBet >= GameStateData->CurrentBetToCall))
        {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Player %s (Seat %d) is already Folded or All-In and cannot make new voluntary actions. Will be skipped by betting round logic."), *PlayerName, ActingPlayerSeatIndex);
            // Player.bIsTurn = false; // Снимаем флаг хода ниже, после общей логики
            // Не устанавливаем bHasActedThisSubRound, так как он не действовал добровольно в этом под-раунде.
            // Его пропустит GetNextPlayerToAct или IsBettingRoundOver.
        }
        else // Игрок может действовать
        {
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
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
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
                        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Call by %s. BetToCall: %lld, PlayerBet: %lld. Re-requesting."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
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
                if (Player.Stack == 0) { Player.Status = EPlayerStatus::AllIn; if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName)); }
            }
            break;

            case EPlayerAction::Bet:
            {
                if (Player.CurrentBet != GameStateData->CurrentBetToCall) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Bet by %s (Must Call/Check/Raise). CurrentBet: %lld, ToCall: %lld. Re-requesting."), *PlayerName, Player.CurrentBet, GameStateData->CurrentBetToCall);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                int64 MinBetSize = GameStateData->BigBlindAmount;
                if ((Amount < MinBetSize && Amount < Player.Stack) || Amount <= 0) {
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
                if (GameStateData->PlayerWhoOpenedBettingThisRound == -1) GameStateData->PlayerWhoOpenedBettingThisRound = ActingPlayerSeatIndex;
                Player.bHasActedThisSubRound = true;
                bActionCausedAggression = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s bets %lld. Stack: %lld"), *PlayerName, Amount, Player.Stack));
                if (Player.Stack == 0) { Player.Status = EPlayerStatus::AllIn; if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName)); }
            }
            break;

            case EPlayerAction::Raise:
            {
                if (GameStateData->CurrentBetToCall == 0) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (No bet to raise, should be Bet). Re-requesting."), *PlayerName);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                if (Player.CurrentBet >= GameStateData->CurrentBetToCall && Player.Stack > 0) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (Already met/exceeded current bet or cannot raise self). Re-requesting."), *PlayerName);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }

                int64 TotalNewBetByPlayer = Amount;
                int64 AmountPlayerMustAdd = TotalNewBetByPlayer - Player.CurrentBet;

                if (AmountPlayerMustAdd <= 0 && TotalNewBetByPlayer < (Player.CurrentBet + Player.Stack)) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (Adds %lld, not >0, and not All-In). Re-requesting."), *PlayerName, AmountPlayerMustAdd);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                if (AmountPlayerMustAdd > Player.Stack) {
                    AmountPlayerMustAdd = Player.Stack;
                    TotalNewBetByPlayer = Player.CurrentBet + Player.Stack;
                }

                int64 PureRaiseAmount = TotalNewBetByPlayer - GameStateData->CurrentBetToCall;
                int64 MinValidPureRaise = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;

                if (PureRaiseAmount < MinValidPureRaise && AmountPlayerMustAdd < Player.Stack) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (PureRaise %lld < MinValidPureRaise %lld and not All-In). Re-requesting."), *PlayerName, PureRaiseAmount, MinValidPureRaise);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }
                if (TotalNewBetByPlayer <= GameStateData->CurrentBetToCall && AmountPlayerMustAdd < Player.Stack) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (TotalBet %lld not > ToCall %lld and not All-In). Re-requesting."), *PlayerName, TotalNewBetByPlayer, GameStateData->CurrentBetToCall);
                    RequestPlayerAction(ActingPlayerSeatIndex); return;
                }

                Player.Stack -= AmountPlayerMustAdd;
                Player.CurrentBet = TotalNewBetByPlayer;
                GameStateData->Pot += AmountPlayerMustAdd;
                GameStateData->CurrentBetToCall = Player.CurrentBet;
                GameStateData->LastBetOrRaiseAmountInCurrentRound = PureRaiseAmount > 0 ? PureRaiseAmount : MinValidPureRaise;
                GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex;
                Player.bHasActedThisSubRound = true;
                bActionCausedAggression = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s raises to %lld (added %lld). Stack: %lld"), *PlayerName, Player.CurrentBet, AmountPlayerMustAdd, Player.Stack));
                if (Player.Stack == 0) { Player.Status = EPlayerStatus::AllIn; if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s is All-In."), *PlayerName)); }
            }
            break;

            default:
                UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Unknown action %s received."), *UEnum::GetValueAsString(PlayerAction));
                RequestPlayerAction(ActingPlayerSeatIndex); return;
            }
        } // конец if (Player can act)

        Player.bIsTurn = false; // Снимаем флаг хода с текущего игрока после его действия или пропуска

        if (bActionCausedAggression) {
            for (FPlayerSeatData& SeatToReset : GameStateData->Seats) {
                if (SeatToReset.SeatIndex != ActingPlayerSeatIndex &&
                    SeatToReset.bIsSittingIn &&
                    SeatToReset.Status == EPlayerStatus::Playing &&
                    SeatToReset.Stack > 0) {
                    SeatToReset.bHasActedThisSubRound = false;
                }
            }
        }

        // --- Логика после действия игрока ---
        int32 ActivePlayersStillInHand = 0;
        for (const FPlayerSeatData& Seat : GameStateData->Seats) {
            if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded) {
                ActivePlayersStillInHand++;
            }
        }

        if (ActivePlayersStillInHand <= 1)
        {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Hand ends with %d active player(s). Proceeding to Showdown logic to finalize."), ActivePlayersStillInHand);
            ProceedToShowdown(); // ProceedToShowdown теперь сам обработает награждение и уведомление UI
            return;
        }

        if (IsBettingRoundOver()) {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Betting round IS over. Proceeding to next stage."));
            ProceedToNextGameStage();
        }
        else {
            int32 NextPlayerToAct = GetNextPlayerToAct(ActingPlayerSeatIndex, true, EPlayerStatus::MAX_None);
            if (NextPlayerToAct != -1) {
                UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Betting round NOT over. Next player to act is Seat %d (%s)."), NextPlayerToAct, *GameStateData->Seats[NextPlayerToAct].PlayerName);
                RequestPlayerAction(NextPlayerToAct);
            }
            else {
                UE_LOG(LogTemp, Error, TEXT("ProcessPlayerAction CRITICAL: IsBettingRoundOver() is FALSE, but GetNextPlayerToAct returned -1! Stage: %s. Forcing stage advance."),
                    *UEnum::GetValueAsString(GameStateData->CurrentStage));
                ProceedToNextGameStage();
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
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0, 0);
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
        UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: GameStateData is null or Seats array is empty."));
        return -1;
    }

    if (CurrentTurnOrderMap_Internal.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: CurrentTurnOrderMap_Internal is empty! Call BuildTurnOrderMap() first."));
        // Это критическая ошибка, если карта не построена, мы не можем определить порядок.
        // Можно попытаться найти первого активного игрока по обычному кругу, но это нарушит кастомный порядок.
        // Лучше вернуть -1 и разбираться, почему карта не построена.
        return -1;
    }

    int32 InitialSearchIndex = StartSeatIndex;
    bool bInitialIndexValidAndInMap = GameStateData->Seats.IsValidIndex(InitialSearchIndex) && CurrentTurnOrderMap_Internal.Contains(InitialSearchIndex);

    // 1. Определяем, с какого индекса реально начинать поиск в CurrentTurnOrderMap_Internal
    if (!bInitialIndexValidAndInMap)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetNextPlayerToAct: StartSeatIndex %d is invalid or not in CurrentTurnOrderMap_Internal. Attempting to find a valid starting point."), StartSeatIndex);
        // Если стартовый индекс плохой, попробуем взять первый ключ из карты как точку отсчета.
        // Это не идеальный обход, но лучше, чем ничего, если BuildTurnOrderMap был вызван.
        if (CurrentTurnOrderMap_Internal.Num() > 0) {
            InitialSearchIndex = CurrentTurnOrderMap_Internal.begin().Key(); // Берем первый попавшийся ключ
            bExcludeStartSeat = false; // Так как мы выбрали новый "стартовый", его не исключаем из первой проверки
            UE_LOG(LogTemp, Warning, TEXT("   Fallback: Using Seat %d as a new starting point for search."), InitialSearchIndex);
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: No players in CurrentTurnOrderMap_Internal to start search from."));
            return -1; // Карта пуста, искать негде
        }
    }

    int32 CurrentIndexToTest = InitialSearchIndex;

    // Если нужно исключить стартовый, сразу переходим к следующему по нашей карте
    if (bExcludeStartSeat)
    {
        // Проверяем, что CurrentIndexToTest (который равен InitialSearchIndex) есть в карте перед поиском следующего
        if (CurrentTurnOrderMap_Internal.Contains(CurrentIndexToTest))
        {
            CurrentIndexToTest = CurrentTurnOrderMap_Internal.FindChecked(CurrentIndexToTest);
        }
        else
        {
            // Этого не должно произойти, если InitialSearchIndex был валидирован выше как находящийся в карте
            UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: Logic error - StartSeatIndex %d (to be excluded) was not found in TurnOrderMap after validation. This is unexpected."), StartSeatIndex);
            return -1;
        }
    }

    // 2. Цикл поиска следующего подходящего игрока
    // Проходим по нашему кастомному порядку не более чем количество игроков в этом порядке
    // (CurrentTurnOrderMap_Internal.Num() гарантирует, что мы не зациклимся бесконечно, если все не подходят)
    for (int32 i = 0; i < CurrentTurnOrderMap_Internal.Num(); ++i)
    {
        if (!GameStateData->Seats.IsValidIndex(CurrentIndexToTest))
        {
            UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: CurrentIndexToTest %d became invalid during loop! Map might be corrupted or out of sync with Seats."), CurrentIndexToTest);
            return -1;
        }

        const FPlayerSeatData& Seat = GameStateData->Seats[CurrentIndexToTest];
        bool bIsEligible = false;

        // Логика проверки на возможность действовать (bIsSittingIn, не Folded)
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded)
        {
            // Проверяем соответствие требуемому статусу или общим критериям для хода
            if (RequiredStatus == EPlayerStatus::MAX_None) // Ищем любого, кто может продолжать ИГРУ (не обязательно делать ставку)
            {
                // Игрок может продолжать игру/быть следующим для хода, если он:
                // - Playing И имеет стек > 0 (может делать ставки)
                // - ИЛИ AllIn (уже не может делать ставки, но его карты играют)
                // - ИЛИ MustPostSmallBlind / MustPostBigBlind (должен поставить блайнд, даже если стек 0)
                if (((Seat.Status == EPlayerStatus::Playing || Seat.Status == EPlayerStatus::MustPostSmallBlind || Seat.Status == EPlayerStatus::MustPostBigBlind) && Seat.Stack > 0) ||
                    Seat.Status == EPlayerStatus::AllIn ||
                    ((Seat.Status == EPlayerStatus::MustPostSmallBlind || Seat.Status == EPlayerStatus::MustPostBigBlind) && Seat.Stack == 0) // Разрешаем All-in на блайндах
                    )
                {
                    bIsEligible = true;
                }
            }
            else // Ищем игрока с конкретным RequiredStatus
            {
                if (Seat.Status == RequiredStatus)
                {
                    // Если ищем игрока для активного действия (Playing, MustPost...), он должен иметь стек,
                    // если только это не AllIn или MustPost... (где All-In на блайнде допустим).
                    if (RequiredStatus == EPlayerStatus::Playing) {
                        if (Seat.Stack > 0) bIsEligible = true;
                    }
                    else if (RequiredStatus == EPlayerStatus::MustPostSmallBlind || RequiredStatus == EPlayerStatus::MustPostBigBlind) {
                        bIsEligible = true; // Позволяем ему попытаться поставить блайнд, даже если стек 0 (это будет AllIn)
                    }
                    else if (RequiredStatus == EPlayerStatus::AllIn) {
                        bIsEligible = true;
                    }
                    // Для других статусов (Waiting, SittingOut), если они когда-либо будут в RequiredStatus,
                    // проверка стека может быть не нужна.
                    else {
                        bIsEligible = true; // По умолчанию для других специфичных статусов
                    }
                }
            }
        }

        if (bIsEligible)
        {
            UE_LOG(LogTemp, Verbose, TEXT("GetNextPlayerToAct (Custom Order): Found eligible player at Seat %d (%s). OriginalStartIdx: %d, ExcludedStart: %s, ReqStatus: %s"),
                CurrentIndexToTest, *Seat.PlayerName, StartSeatIndex, bExcludeStartSeat ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(RequiredStatus));
            return CurrentIndexToTest;
        }

        // Переходим к следующему игроку согласно нашей карте порядка
        if (CurrentTurnOrderMap_Internal.Contains(CurrentIndexToTest))
        {
            CurrentIndexToTest = CurrentTurnOrderMap_Internal.FindChecked(CurrentIndexToTest);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("GetNextPlayerToAct: CurrentIndexToTest %d not found in TurnOrderMap_Internal during loop! Cannot determine next player. Map size: %d. Searched from %d."),
                CurrentIndexToTest, CurrentTurnOrderMap_Internal.Num(), InitialSearchIndex);
            // Это может произойти, если CurrentTurnOrderMap_Internal не полная или если CurrentIndexToTest как-то вышел из нее.
            return -1;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("GetNextPlayerToAct (Custom Order): No eligible player found after full circle search of %d players. OriginalStartIdx: %d, ExcludedStart: %s, ReqStatus: %s"),
        CurrentTurnOrderMap_Internal.Num(), StartSeatIndex, bExcludeStartSeat ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(RequiredStatus));
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
        // Безопасное уведомление UI, если возможно
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        FString ErrorMsg = TEXT("Error: Game State or Deck uninitialized for Stage Change");
        TArray<FShowdownPlayerInfo> EmptyResults;
        if (OnShowdownResultsDelegate.IsBound()) OnShowdownResultsDelegate.Broadcast(EmptyResults, ErrorMsg);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(ErrorMsg, GameStateData ? GameStateData->Pot : 0);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData ? GameStateData->BigBlindAmount : 0, 0, 0);
        if (GameStateData) GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        return;
    }

    EGameStage StageBeforeAdvance = GameStateData->CurrentStage;
    UE_LOG(LogTemp, Log, TEXT("--- ProceedToNextGameStage: Advancing from %s ---"), *UEnum::GetValueAsString(StageBeforeAdvance));
    if (OnGameHistoryEventDelegate.IsBound()) {
        OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Betting round on %s ended. ---"), *UEnum::GetValueAsString(StageBeforeAdvance)));
    }

    // 1. Проверяем, сколько игроков еще не сфолдило
    TArray<int32> PlayersLeftInPotIndices;
    int32 NumPlayersAbleToMakeFurtherBets = 0; // Игроки, кто не сфолдил И может еще ставить (не All-In с 0 стеком)

    for (const FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded) {
            PlayersLeftInPotIndices.Add(Seat.SeatIndex);
            if (Seat.Stack > 0 && Seat.Status != EPlayerStatus::AllIn) {
                NumPlayersAbleToMakeFurtherBets++;
            }
        }
    }

    // ЕСЛИ РУКА ЗАКОНЧЕНА (<=1 игрока в поте ИЛИ это был Ривер и в поте есть игроки) -> ПЕРЕХОДИМ К ШОУДАУНУ
    // ProceedToShowdown() сам обработает случай с одним или нулем игроков.
    if (PlayersLeftInPotIndices.Num() <= 1 || StageBeforeAdvance == EGameStage::River) {
        if (PlayersLeftInPotIndices.Num() <= 1) {
            UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: <=1 player left in pot (%d). Proceeding to Showdown logic to finalize hand."), PlayersLeftInPotIndices.Num());
        }
        else { // StageBeforeAdvance == EGameStage::River && PlayersLeftInPotIndices.Num() > 1
            UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: River betting complete with %d players. Proceeding to Showdown."), PlayersLeftInPotIndices.Num());
        }
        ProceedToShowdown(); // Пусть ProceedToShowdown обрабатывает все эти случаи завершения руки
        return;
    }

    // --- ЕСЛИ РУКА ПРОДОЛЖАЕТСЯ НА НОВУЮ УЛИЦУ (И ЭТО НЕ РИВЕР, И ИГРОКОВ > 1) ---
    UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Preparing for next betting round. Players in pot: %d, Able to bet: %d"), PlayersLeftInPotIndices.Num(), NumPlayersAbleToMakeFurtherBets);

    // 2. Сброс состояния для нового раунда ставок
    GameStateData->CurrentBetToCall = 0;
    GameStateData->LastBetOrRaiseAmountInCurrentRound = 0;
    GameStateData->LastAggressorSeatIndex = -1;
    GameStateData->PlayerWhoOpenedBettingThisRound = -1;

    for (FPlayerSeatData& Seat : GameStateData->Seats) {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded) {
            Seat.CurrentBet = 0;
            if (Seat.Status != EPlayerStatus::AllIn) { // Игроки AllIn не сбрасывают bHasActed, т.к. они не будут ходить
                Seat.bHasActedThisSubRound = false;
            }
        }
    }

    EGameStage NextStageToSet = GameStateData->CurrentStage;
    bool bDealingErrorOccurred = false;

    // 3. Раздача карт для следующей улицы (Флоп, Терн, Ривер)
    switch (StageBeforeAdvance)
    {
    case EGameStage::Preflop:
    {
        NextStageToSet = EGameStage::Flop;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Dealing %s ---"), *UEnum::GetValueAsString(NextStageToSet)));
        // Deck->DealCard(); // Опциональное сжигание карты
        for (int i = 0; i < 3; ++i) {
            TOptional<FCard> DealtCardOpt = Deck->DealCard();
            if (DealtCardOpt.IsSet()) { GameStateData->CommunityCards.Add(DealtCardOpt.GetValue()); }
            else { bDealingErrorOccurred = true; UE_LOG(LogTemp, Error, TEXT("Deck ran out for Flop card %d"), i + 1); break; }
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
        TOptional<FCard> TurnCardOpt = Deck->DealCard();
        if (TurnCardOpt.IsSet()) {
            GameStateData->CommunityCards.Add(TurnCardOpt.GetValue());
            if (GameStateData->CommunityCards.Num() >= 4 && OnGameHistoryEventDelegate.IsBound()) {
                OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Turn: %s"), *GameStateData->CommunityCards.Last().ToString()));
            }
        }
        else { bDealingErrorOccurred = true; UE_LOG(LogTemp, Error, TEXT("Deck ran out for Turn")); }
    }
    break;
    case EGameStage::Turn:
    {
        NextStageToSet = EGameStage::River;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Dealing %s ---"), *UEnum::GetValueAsString(NextStageToSet)));
        TOptional<FCard> RiverCardOpt = Deck->DealCard();
        if (RiverCardOpt.IsSet()) {
            GameStateData->CommunityCards.Add(RiverCardOpt.GetValue());
            if (GameStateData->CommunityCards.Num() >= 5 && OnGameHistoryEventDelegate.IsBound()) {
                OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("River: %s"), *GameStateData->CommunityCards.Last().ToString()));
            }
        }
        else { bDealingErrorOccurred = true; UE_LOG(LogTemp, Error, TEXT("Deck ran out for River")); }
    }
    break;
    default:
        UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: Called from invalid stage to deal community cards: %s."), *UEnum::GetValueAsString(StageBeforeAdvance));
        return;
    }

    if (bDealingErrorOccurred) {
        UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: Error during community card dealing. Proceeding to showdown with current cards."));
        ProceedToShowdown(); // Переходим к шоудауну с тем, что есть
        return;
    }

    if (OnCommunityCardsUpdatedDelegate.IsBound()) {
        OnCommunityCardsUpdatedDelegate.Broadcast(GameStateData->CommunityCards);
    }
    GameStateData->CurrentStage = NextStageToSet;
    UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: New stage is %s. Community Cards: %d"), *UEnum::GetValueAsString(NextStageToSet), GameStateData->CommunityCards.Num());

    // 6. Начинаем новый круг торгов, ЕСЛИ ЕСТЬ ХОТЯ БЫ ДВА ИГРОКА, СПОСОБНЫХ ДЕЛАТЬ СТАВКИ
    if (NumPlayersAbleToMakeFurtherBets >= 2) {
        int32 FirstToActOnNewStreet = DetermineFirstPlayerToActPostflop();
        if (FirstToActOnNewStreet != -1) {
            GameStateData->PlayerWhoOpenedBettingThisRound = FirstToActOnNewStreet;
            RequestPlayerAction(FirstToActOnNewStreet);
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage CRITICAL: %d players able to bet, but no first actor found for %s! Auto-advancing/Showdown."), NumPlayersAbleToMakeFurtherBets, *UEnum::GetValueAsString(NextStageToSet));
            // Эта ветка не должна достигаться, если PlayersLeftInPotIndices.Num() > 1
            // Но на всякий случай, если логика NumPlayersAbleToMakeFurtherBets и PlayersLeftInPotIndices разошлась
            ProceedToShowdown(); // Безопаснее всего перейти к шоудауну
        }
    }
    else { // Меньше двух игроков могут делать ставки (например, все остальные олл-ин)
        UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Less than 2 players able to make further bets (%d). Auto-advancing to showdown logic."), NumPlayersAbleToMakeFurtherBets);
        // Если ставок больше быть не может, а игроков в поте > 1, то это сразу шоудаун, пропустив оставшиеся улицы (если они есть)
        // ProceedToShowdown() должен быть вызван после того, как все карты розданы (если возможно)
        // Эта логика означает, что если мы здесь, то это НЕ Ривер (обработано выше), но и ставить больше некому.
        // Значит, нужно раздать оставшиеся карты и перейти к шоудауну.
        EGameStage CurrentDealingStage = NextStageToSet; // Стадия, на которой мы СЕЙЧАС (уже после раздачи карт этой улицы)
        while (CurrentDealingStage < EGameStage::River && PlayersLeftInPotIndices.Num() > 1 && !bDealingErrorOccurred)
        {
            // Симулируем переход на следующую улицу для раздачи карт
            EGameStage StageToDealNext = EGameStage::WaitingForPlayers; // заглушка
            switch (CurrentDealingStage)
            {
            case EGameStage::Flop: StageToDealNext = EGameStage::Turn; break;
            case EGameStage::Turn: StageToDealNext = EGameStage::River; break;
            default: bDealingErrorOccurred = true; break; // Не должно произойти
            }
            if (bDealingErrorOccurred) break;

            UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Auto-dealing for stage %s"), *UEnum::GetValueAsString(StageToDealNext));
            if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Auto-Dealing %s ---"), *UEnum::GetValueAsString(StageToDealNext)));

            TOptional<FCard> NextCardOpt = Deck->DealCard();
            if (NextCardOpt.IsSet()) {
                GameStateData->CommunityCards.Add(NextCardOpt.GetValue());
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s: %s"), *UEnum::GetValueAsString(StageToDealNext), *GameStateData->CommunityCards.Last().ToString()));
            }
            else {
                bDealingErrorOccurred = true;
                UE_LOG(LogTemp, Error, TEXT("ProceedToNextGameStage: Deck ran out during auto-deal for %s"), *UEnum::GetValueAsString(StageToDealNext));
                break;
            }
            CurrentDealingStage = StageToDealNext; // Обновляем для следующей итерации цикла
        }

        if (OnCommunityCardsUpdatedDelegate.IsBound() && GameStateData->CommunityCards.Num() > 0) { // Отправляем финальный набор общих карт
            OnCommunityCardsUpdatedDelegate.Broadcast(GameStateData->CommunityCards);
        }

        UE_LOG(LogTemp, Log, TEXT("ProceedToNextGameStage: Auto-dealing complete or error. Proceeding to showdown."));
        ProceedToShowdown(); // После автоматической раздачи всех карт (или ошибки) -> шоудаун
    }
}

void UOfflineGameManager::ProceedToShowdown()
{
    if (!GameStateData || !Deck) {
        UE_LOG(LogTemp, Error, TEXT("ProceedToShowdown: GameStateData or Deck is null!"));
        if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
        FString ErrorMsg = TEXT("Error: Game State or Deck uninitialized for Showdown");
        TArray<FShowdownPlayerInfo> EmptyResults;
        if (OnShowdownResultsDelegate.IsBound()) OnShowdownResultsDelegate.Broadcast(EmptyResults, ErrorMsg);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(ErrorMsg, GameStateData ? GameStateData->Pot : 0);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData ? GameStateData->BigBlindAmount : 0, 0, 0);
        if (GameStateData) GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("--- PROCEEDING TO SHOWDOWN (NetResult relative to Hand Start, Full Reveal) ---"));
    if (OnGameHistoryEventDelegate.IsBound()) {
        OnGameHistoryEventDelegate.Broadcast(TEXT("--- Showdown ---"));
        if (GameStateData->CommunityCards.Num() > 0) {
            FString CommunityCardsString = TEXT("Community Cards: ");
            for (int32 i = 0; i < GameStateData->CommunityCards.Num(); ++i) {
                CommunityCardsString += GameStateData->CommunityCards[i].ToString(); // Используем ToString для логов
                if (i < GameStateData->CommunityCards.Num() - 1) CommunityCardsString += TEXT(" ");
            }
            OnGameHistoryEventDelegate.Broadcast(CommunityCardsString);
        }
        else {
            OnGameHistoryEventDelegate.Broadcast(TEXT("No Community Cards on table for Showdown."));
        }
    }
    GameStateData->CurrentStage = EGameStage::Showdown;
    GameStateData->CurrentTurnSeat = -1;
    if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);

    struct FPlayerHandEvaluation
    {
        int32 SeatIndex;
        FPokerHandResult HandResult;
        FPlayerSeatData PlayerDataSnapshot; // Снимок данных игрока на момент начала шоудауна
    };
    TArray<FPlayerHandEvaluation> AllPlayersDealtInEvaluations;
    TArray<int32> PlayersEligibleToWinPotIndices; // Только те, кто не сфолдил

    // 1. Собираем данные и оцениваем руки ВСЕХ, кто получил карты и участвовал
    UE_LOG(LogTemp, Log, TEXT("Showdown: Evaluating hands and using PlayerDataSnapshot..."));
    for (const FPlayerSeatData& CurrentSeatStateInGS : GameStateData->Seats) // Используем другое имя, чтобы не путать со снимком
    {
        if (CurrentSeatStateInGS.bIsSittingIn && CurrentSeatStateInGS.HoleCards.Num() == 2 &&
            (CurrentSeatStateInGS.Status != EPlayerStatus::Waiting && CurrentSeatStateInGS.Status != EPlayerStatus::SittingOut))
        {
            FPokerHandResult EvaluatedResult = UPokerHandEvaluator::EvaluatePokerHand(CurrentSeatStateInGS.HoleCards, GameStateData->CommunityCards);
            // Сохраняем CurrentSeatStateInGS как PlayerDataSnapshot
            AllPlayersDealtInEvaluations.Add({ CurrentSeatStateInGS.SeatIndex, EvaluatedResult, CurrentSeatStateInGS });

            if (CurrentSeatStateInGS.Status != EPlayerStatus::Folded)
            {
                PlayersEligibleToWinPotIndices.Add(CurrentSeatStateInGS.SeatIndex);
            }
        }
    }

    if (AllPlayersDealtInEvaluations.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("Showdown: No hands could be evaluated. Hand ends."));
        AwardPotToWinner({});
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        FString ErrorAnnouncement = TEXT("Showdown Error: No players' hands to evaluate.");
        TArray<FShowdownPlayerInfo> EmptyResultsOnError;
        if (OnShowdownResultsDelegate.IsBound()) OnShowdownResultsDelegate.Broadcast(EmptyResultsOnError, ErrorAnnouncement);
        // ... (остальные делегаты для сброса UI)
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over"), GameStateData->Pot);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0, 0);
        return;
    }

    // 2. Определяем победителя(ей) ТОЛЬКО среди тех, кто НЕ СФОЛДИЛ
    TArray<int32> ActualWinningSeatIndices;
    if (PlayersEligibleToWinPotIndices.Num() == 1) {
        ActualWinningSeatIndices.Add(PlayersEligibleToWinPotIndices[0]);
    }
    else if (PlayersEligibleToWinPotIndices.Num() > 1) {
        TArray<FPlayerHandEvaluation> EligibleHandsToCompare;
        for (const auto& Eval : AllPlayersDealtInEvaluations) { // Итерируем по AllPlayersDealtInEvaluations
            if (PlayersEligibleToWinPotIndices.Contains(Eval.SeatIndex)) { // Проверяем, есть ли этот игрок в списке тех, кто может выиграть
                EligibleHandsToCompare.Add(Eval);
            }
        }
        if (EligibleHandsToCompare.Num() > 0) {
            EligibleHandsToCompare.Sort([](const FPlayerHandEvaluation& A, const FPlayerHandEvaluation& B) {
                return UPokerHandEvaluator::CompareHandResults(A.HandResult, B.HandResult) > 0;
                });
            FPokerHandResult BestEligibleHand = EligibleHandsToCompare[0].HandResult;
            ActualWinningSeatIndices.Add(EligibleHandsToCompare[0].SeatIndex);
            for (int32 i = 1; i < EligibleHandsToCompare.Num(); ++i) {
                if (UPokerHandEvaluator::CompareHandResults(EligibleHandsToCompare[i].HandResult, BestEligibleHand) == 0) {
                    ActualWinningSeatIndices.Add(EligibleHandsToCompare[i].SeatIndex);
                }
                else { break; }
            }
        }
    }

    // 3. Награждаем победителя(ей) - это ИЗМЕНИТ стеки в GameStateData->Seats
    TMap<int32, int64> AmountsWonByPlayers = AwardPotToWinner(ActualWinningSeatIndices);

    // 4. Формируем финальные данные для UI и строку объявления
    TArray<FShowdownPlayerInfo> FinalShowdownResultsData;
    FString FinalWinnerAnnouncementString;

    for (const FPlayerHandEvaluation& EvalEntry : AllPlayersDealtInEvaluations)
    {
        FShowdownPlayerInfo Info;
        Info.SeatIndex = EvalEntry.SeatIndex;
        // Используем данные из PlayerDataSnapshot, который был сохранен в FPlayerHandEvaluation
        Info.PlayerName = EvalEntry.PlayerDataSnapshot.PlayerName;
        Info.HoleCards = EvalEntry.PlayerDataSnapshot.HoleCards;
        Info.HandResult = EvalEntry.HandResult;
        Info.PlayerStatusAtShowdown = EvalEntry.PlayerDataSnapshot.Status;
        Info.bIsWinner = ActualWinningSeatIndices.Contains(EvalEntry.SeatIndex);
        Info.AmountWon = AmountsWonByPlayers.FindRef(EvalEntry.SeatIndex);

        // Рассчитываем NetResult ОТНОСИТЕЛЬНО САМОГО НАЧАЛА РУКИ
        if (GameStateData->Seats.IsValidIndex(EvalEntry.SeatIndex) && StacksAtHandStart_Internal.Contains(EvalEntry.SeatIndex))
        {
            // GameStateData->Seats[EvalEntry.SeatIndex].Stack -- это стек ПОСЛЕ присуждения банка
            // StacksAtHandStart_Internal.FindChecked(EvalEntry.SeatIndex) -- это стек В САМОМ НАЧАЛЕ РУКИ (до блайндов)
            Info.NetResult = GameStateData->Seats[EvalEntry.SeatIndex].Stack - StacksAtHandStart_Internal.FindChecked(EvalEntry.SeatIndex);
        }
        else
        {
            Info.NetResult = Info.AmountWon; // Запасной вариант
            UE_LOG(LogTemp, Warning, TEXT("ProceedToShowdown: Could not find StackAtHandStart_Internal for Seat %d. NetResult for UI defaults to AmountWon."), EvalEntry.SeatIndex);
        }

        FinalShowdownResultsData.Add(Info);

        // Логирование для истории игры
        if (OnGameHistoryEventDelegate.IsBound()) {
            FString KickersStr;
            for (ECardRank Kicker : Info.HandResult.Kickers) { KickersStr += UEnum::GetDisplayValueAsText(Kicker).ToString() + TEXT(" "); }
            KickersStr = KickersStr.TrimEnd();
            FString StatusTextForLog = (Info.PlayerStatusAtShowdown == EPlayerStatus::Folded) ? TEXT("(Folded)") :
                (Info.bIsWinner ? FString::Printf(TEXT("(Wins %lld, Net: %+lld)"), Info.AmountWon, Info.NetResult) : FString::Printf(TEXT("(Net: %+lld)"), Info.NetResult));
            OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s (Seat %d) shows: %s %s. Hand: %s. Kickers: [%s] %s"),
                *Info.PlayerName, Info.SeatIndex,
                Info.HoleCards.IsValidIndex(0) ? *Info.HoleCards[0].ToString() : TEXT("N/A"),
                Info.HoleCards.IsValidIndex(1) ? *Info.HoleCards[1].ToString() : TEXT("N/A"),
                *UEnum::GetDisplayValueAsText(Info.HandResult.HandRank).ToString(),
                *KickersStr,
                *StatusTextForLog
            ));
        }
    }

    // Формируем строку объявления победителя(ей) (БЕЗ NetResult в главном объявлении)
    if (ActualWinningSeatIndices.Num() > 0) {
        if (ActualWinningSeatIndices.Num() == 1) {
            int32 WinnerIdx = ActualWinningSeatIndices[0];
            const FShowdownPlayerInfo* WinnerInfoPtr = FinalShowdownResultsData.FindByPredicate(
                [WinnerIdx](const FShowdownPlayerInfo& info) { return info.SeatIndex == WinnerIdx; });
            if (WinnerInfoPtr) {
                FinalWinnerAnnouncementString = FString::Printf(TEXT("Winner: %s (Hand: %s) wins %lld!"),
                    *WinnerInfoPtr->PlayerName,
                    *UEnum::GetDisplayValueAsText(WinnerInfoPtr->HandResult.HandRank).ToString(),
                    WinnerInfoPtr->AmountWon);
            }
            else { FinalWinnerAnnouncementString = TEXT("Winner determined, but info not found for announcement."); }
        }
        else {
            FinalWinnerAnnouncementString = TEXT("Split Pot! Winners: ");
            for (int32 i = 0; i < ActualWinningSeatIndices.Num(); ++i) {
                int32 WinnerIdx = ActualWinningSeatIndices[i];
                const FShowdownPlayerInfo* WinnerInfoPtr = FinalShowdownResultsData.FindByPredicate(
                    [WinnerIdx](const FShowdownPlayerInfo& info) { return info.SeatIndex == WinnerIdx; });
                if (WinnerInfoPtr) {
                    FinalWinnerAnnouncementString += FString::Printf(TEXT("%s (Hand: %s, Wins: %lld)"),
                        *WinnerInfoPtr->PlayerName,
                        *UEnum::GetDisplayValueAsText(WinnerInfoPtr->HandResult.HandRank).ToString(),
                        WinnerInfoPtr->AmountWon
                    );
                    if (i < ActualWinningSeatIndices.Num() - 1) FinalWinnerAnnouncementString += TEXT(", ");
                }
            }
        }
    }
    else {
        FinalWinnerAnnouncementString = TEXT("No winner for the pot this hand.");
    }

    // Записываем основное объявление победителя в историю еще раз, если оно не пустое (на случай, если предыдущее было более детальным для каждого игрока)
    if (OnGameHistoryEventDelegate.IsBound() && !FinalWinnerAnnouncementString.IsEmpty() && !FinalWinnerAnnouncementString.Contains(TEXT("Showdown Results:"))) {
        OnGameHistoryEventDelegate.Broadcast(FinalWinnerAnnouncementString);
    }

    if (OnShowdownResultsDelegate.IsBound())
    {
        UE_LOG(LogTemp, Log, TEXT("Showdown: Broadcasting OnShowdownResultsDelegate with %d results. Announcement: '%s'"), FinalShowdownResultsData.Num(), *FinalWinnerAnnouncementString);
        OnShowdownResultsDelegate.Broadcast(FinalShowdownResultsData, FinalWinnerAnnouncementString);
    }

    // 5. Рука завершена
    UE_LOG(LogTemp, Log, TEXT("--- HAND OVER (After Showdown Finalization) ---"));
    GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
    if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1);
    if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over. Press 'Next Hand'."), GameStateData->Pot);
    if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
    if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0, 0);
}


TMap<int32, int64> UOfflineGameManager::AwardPotToWinner(const TArray<int32>& WinningSeatIndices)
{
    TMap<int32, int64> AmountsWonByPlayers; // Карта для возврата: SeatIndex -> AmountWon

    if (!GameStateData) {
        UE_LOG(LogTemp, Error, TEXT("AwardPotToWinner: GameStateData is null!"));
        return AmountsWonByPlayers; // Возвращаем пустую карту
    }

    int64 TotalPotToAward = GameStateData->Pot;

    if (TotalPotToAward <= 0) {
        UE_LOG(LogTemp, Log, TEXT("AwardPotToWinner: Pot is %lld, nothing to award."), TotalPotToAward);
        GameStateData->Pot = 0; // Убедимся, что он 0, если был отрицательным или нулевым
        // Если победителей не было, но банк был (например, все сфолдили одновременно - редкий случай),
        // то каждый победитель в WinningSeatIndices (если они есть) получит 0.
        for (int32 WinnerIdx : WinningSeatIndices) {
            AmountsWonByPlayers.Add(WinnerIdx, 0);
        }
        return AmountsWonByPlayers;
    }

    if (WinningSeatIndices.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("AwardPotToWinner: No winners provided, but pot is %lld. Pot will be reset and not awarded."), TotalPotToAward);
        if (OnGameHistoryEventDelegate.IsBound()) {
            OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Pot of %lld remains unawarded (no winners specified)."), TotalPotToAward));
        }
        GameStateData->Pot = 0; // Сбрасываем банк
        return AmountsWonByPlayers; // Возвращаем пустую карту, так как нет явных победителей для записи выигрыша
    }

    // Рассчитываем долю каждого победителя и остаток
    int64 SharePerWinner = TotalPotToAward / WinningSeatIndices.Num();
    int64 RemainderChips = TotalPotToAward % WinningSeatIndices.Num();

    FString WinnersLogString = TEXT(""); // Будем формировать строку для истории и логов
    bool bFirstWinnerForRemainder = true; // Флаг для корректного распределения остатка

    for (int32 WinnerIdx : WinningSeatIndices)
    {
        if (GameStateData->Seats.IsValidIndex(WinnerIdx))
        {
            FPlayerSeatData& Winner = GameStateData->Seats[WinnerIdx];
            int64 CurrentPlayerShare = SharePerWinner;

            // Отдаем остаток фишек первому победителю в списке (стандартная практика)
            if (bFirstWinnerForRemainder && RemainderChips > 0)
            {
                CurrentPlayerShare += RemainderChips;
                RemainderChips = 0; // Весь остаток отдан
                bFirstWinnerForRemainder = false;
            }

            Winner.Stack += CurrentPlayerShare; // Начисляем выигрыш на стек
            AmountsWonByPlayers.Add(WinnerIdx, CurrentPlayerShare); // Сохраняем сумму выигрыша для этого игрока

            // Формируем часть строки для лога/истории
            if (!WinnersLogString.IsEmpty()) {
                WinnersLogString += TEXT(", ");
            }
            WinnersLogString += FString::Printf(TEXT("%s (Seat %d) +%lld (New Stack: %lld)"),
                *Winner.PlayerName,
                Winner.SeatIndex,
                CurrentPlayerShare,
                Winner.Stack);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("AwardPotToWinner: Invalid winner seat index %d found in WinningSeatIndices! This share of pot (%lld) will be lost if not redistributed."), WinnerIdx, SharePerWinner + (bFirstWinnerForRemainder && RemainderChips > 0 ? RemainderChips : 0));
            // В этом случае доля банка для невалидного индекса "сгорит", если не добавить логику перераспределения.
            // Для MVP это допустимо, но в продакшене нужно обрабатывать.
        }
    }

    FString FinalLogMessage = FString::Printf(TEXT("Pot of %lld awarded. %s"), TotalPotToAward, *WinnersLogString);
    UE_LOG(LogTemp, Log, TEXT("AwardPotToWinner: %s"), *FinalLogMessage);
    if (OnGameHistoryEventDelegate.IsBound()) {
        OnGameHistoryEventDelegate.Broadcast(FinalLogMessage);
    }

    GameStateData->Pot = 0; // Обнуляем основной банк после распределения
    // TODO: В будущем здесь будет логика для Side Pots, если они будут реализованы.
    // Каждый побочный банк должен будет обрабатываться отдельно со своим списком претендентов.

    return AmountsWonByPlayers; // Возвращаем карту с суммами выигрышей
}


UOfflineGameManager::FActionDecisionContext UOfflineGameManager::GetActionContextForSeat(int32 SeatIndex) const
{
    FActionDecisionContext Context; // Инициализируется значениями по умолчанию (пустой массив, нули)

    if (!GameStateData || !GameStateData->Seats.IsValidIndex(SeatIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("GetActionContextForSeat: Invalid SeatIndex %d or GameStateData is null. Returning empty context."), SeatIndex);
        return Context; // Возвращаем пустой/дефолтный контекст
    }

    const FPlayerSeatData& Player = GameStateData->Seats[SeatIndex];

    Context.PlayerCurrentStack = Player.Stack;
    Context.PlayerCurrentBetInRound = Player.CurrentBet;
    Context.CurrentBetToCallOnTable = GameStateData->CurrentBetToCall;

    // Логика определения AllowedActions (почти идентична той, что была в RequestPlayerAction)
    bool bCanPlayerActuallyAct = Player.bIsSittingIn &&
        Player.Status != EPlayerStatus::Folded &&
        Player.Stack > 0;
    if (Player.Status == EPlayerStatus::AllIn) {
        if ((GameStateData->CurrentStage >= EGameStage::Preflop && Player.CurrentBet >= GameStateData->CurrentBetToCall) ||
            GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind ||
            GameStateData->CurrentStage == EGameStage::WaitingForBigBlind)
        {
            bCanPlayerActuallyAct = false;
        }
    }

    if (!bCanPlayerActuallyAct) {
        // Игрок не может действовать, AllowedActions остается пустым
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && SeatIndex == GameStateData->PendingSmallBlindSeat && Player.Status == EPlayerStatus::MustPostSmallBlind) {
        Context.AvailableActions.Add(EPlayerAction::PostBlind);
        Context.MinPureRaiseUI = GameStateData->SmallBlindAmount; // Сумма для постановки блайнда
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && SeatIndex == GameStateData->PendingBigBlindSeat && Player.Status == EPlayerStatus::MustPostBigBlind) {
        Context.AvailableActions.Add(EPlayerAction::PostBlind);
        Context.MinPureRaiseUI = GameStateData->BigBlindAmount; // Сумма для постановки блайнда
    }
    else if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River) {
        Context.AvailableActions.Add(EPlayerAction::Fold);

        if (Player.CurrentBet == GameStateData->CurrentBetToCall) {
            Context.AvailableActions.Add(EPlayerAction::Check);
        }

        if (GameStateData->CurrentBetToCall > Player.CurrentBet && Player.Stack > 0) {
            Context.AvailableActions.Add(EPlayerAction::Call);
        }

        // Bet
        if (Context.AvailableActions.Contains(EPlayerAction::Check) && Player.Stack >= GameStateData->BigBlindAmount) {
            Context.AvailableActions.Add(EPlayerAction::Bet);
        }

        // Raise
        int64 AmountToEffectivelyCallForRaiseCheck = GameStateData->CurrentBetToCall - Player.CurrentBet;
        if (AmountToEffectivelyCallForRaiseCheck < 0) AmountToEffectivelyCallForRaiseCheck = 0;
        int64 MinPureRaiseForRaiseCalc = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;

        if (GameStateData->CurrentBetToCall > 0 && // Должна быть ставка для рейза (иначе это Bet)
            Player.Stack > AmountToEffectivelyCallForRaiseCheck &&
            Player.Stack >= (AmountToEffectivelyCallForRaiseCheck + MinPureRaiseForRaiseCalc))
        {
            Context.AvailableActions.Add(EPlayerAction::Raise);
        }

        // Расчет значений для UI (которые также полезны для ИИ)
        Context.AmountToCallUI = GameStateData->CurrentBetToCall - Player.CurrentBet;
        if (Context.AmountToCallUI < 0) Context.AmountToCallUI = 0;

        Context.MinPureRaiseUI = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;
    }
    // Для этапов блайндов AmountToCallUI будет 0, MinPureRaiseUI будет суммой блайнда
    else if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind)
    {
        Context.AmountToCallUI = 0;
        Context.MinPureRaiseUI = GameStateData->SmallBlindAmount;
    }
    else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind)
    {
        Context.AmountToCallUI = 0;
        Context.MinPureRaiseUI = GameStateData->BigBlindAmount;
    }


    return Context;
}


// --- НОВАЯ Приватная Функция для Вызова Решения Бота по Таймеру ---
void UOfflineGameManager::TriggerBotDecision(int32 BotSeatIndex)
{
    if (!GameStateData || !BotAIInstance || !GameStateData->Seats.IsValidIndex(BotSeatIndex) || !GameStateData->Seats[BotSeatIndex].bIsBot)
    {
        UE_LOG(LogTemp, Error, TEXT("TriggerBotDecision: Prerequisites not met for BotSeatIndex %d. (GameState: %d, BotAI: %d, ValidIndex: %d, IsBot: %d)"),
            BotSeatIndex, IsValid(GameStateData.Get()), IsValid(BotAIInstance.Get()),
            GameStateData ? GameStateData->Seats.IsValidIndex(BotSeatIndex) : 0,
            (GameStateData && GameStateData->Seats.IsValidIndex(BotSeatIndex)) ? GameStateData->Seats[BotSeatIndex].bIsBot : 0
        );
        return;
    }

    // Проверяем, действительно ли сейчас ход этого бота, на случай если состояние изменилось, пока тикал таймер
    if (GameStateData->CurrentTurnSeat != BotSeatIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("TriggerBotDecision: CurrentTurnSeat (%d) is not BotSeatIndex (%d). Bot will not act."), GameStateData->CurrentTurnSeat, BotSeatIndex);
        return;
    }

    const FPlayerSeatData& BotPlayer = GameStateData->Seats[BotSeatIndex];

    // Получаем контекст действий для бота
    FActionDecisionContext BotActionContext = GetActionContextForSeat(BotSeatIndex);

    if (BotActionContext.AvailableActions.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("TriggerBotDecision: Bot %s (Seat %d) has no available actions. Betting round might be over or player cannot act."),
            *BotPlayer.PlayerName, BotSeatIndex);
        // Если нет действий, возможно, нужно проверить конец раунда или передать ход.
        // Но ProcessPlayerAction с EPlayerAction::None или пропуск хода должны быть обработаны в основной логике.
        // Можно просто залогировать и ничего не делать, полагаясь, что ProcessPlayerAction разберется.
        // На данный момент, если IsBettingRoundOver() вызывается после каждого ProcessPlayerAction, это должно быть нормально.
        // Это может случиться, если бот уже AllIn и не может дальше влиять на ставки.
        if (IsBettingRoundOver()) {
            ProceedToNextGameStage();
        }
        else {
            // Этого не должно происходить, если GetActionContextForSeat правильно определяет, что игрок не может ходить
            // и возвращает пустой AllowedActions, который потом не приведет к вызову ProcessPlayerAction.
            // Но на всякий случай, если мы сюда попали, и раунд не окончен, передаем ход.
            RequestPlayerAction(GetNextPlayerToAct(BotSeatIndex, true));
        }
        return;
    }

    int64 BotChosenAmount = 0; // Сумма, которую бот решит поставить (общая сумма для Bet/Raise, 0 для Call/Check/Fold/PostBlind)

    EPlayerAction ChosenAction = BotAIInstance->GetBestAction(
        GameStateData.Get(),
        BotPlayer,
        BotActionContext.AvailableActions,
        BotActionContext.CurrentBetToCallOnTable, // Это GameStateData->CurrentBetToCall
        BotActionContext.MinPureRaiseUI,          // Минимальный чистый рейз или мин. бет
        BotChosenAmount                           // Выходной параметр суммы
    );

    UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) AI decided: %s with Amount: %lld"),
        *BotPlayer.PlayerName, BotSeatIndex, *UEnum::GetValueAsString(ChosenAction), BotChosenAmount);

    // Для PostBlind и Call, Amount передаваемый в ProcessPlayerAction должен быть 0,
    // так как сама функция ProcessPlayerAction вычисляет нужную сумму.
    // Для Bet и Raise, Amount должен быть итоговой суммой ставки.
    int64 AmountForProcessAction = 0;
    if (ChosenAction == EPlayerAction::Bet || ChosenAction == EPlayerAction::Raise)
    {
        AmountForProcessAction = BotChosenAmount;
    }
    // Для PostBlind сумма будет взята из GameStateData->Small/BigBlindAmount внутри ProcessPlayerAction->PostBlinds.
    // Для Call сумма будет вычислена в ProcessPlayerAction на основе CurrentBetToCall.

    ProcessPlayerAction(BotSeatIndex, ChosenAction, AmountForProcessAction);
}