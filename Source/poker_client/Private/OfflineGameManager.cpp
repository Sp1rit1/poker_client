#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "PokerBotAI.h"
#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h" 
#include "PokerHandEvaluator.h"  


UOfflineGameManager::UOfflineGameManager()
{

}

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

    if (!BotAIInstance)
    {
        BotAIInstance = NewObject<UPokerBotAI>(this);
        if (BotAIInstance)
        {
            UE_LOG(LogTemp, Log, TEXT("InitializeGame: BotAIInstance created successfully."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("InitializeGame: Failed to create BotAIInstance! Bots will not function."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("InitializeGame: BotAIInstance already exists."));
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
    TArray<FBotPersonalitySettings> BotPersonalitiesFromGI; 

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetOuter());
    if (GI)
    {
        if (GI->bIsLoggedIn || GI->bIsInOfflineMode)
        {
            PlayerActualName = GI->LoggedInUsername.IsEmpty() ? TEXT("Player") : GI->LoggedInUsername;
            PlayerActualId = GI->LoggedInUserId;
        }
        BotPersonalitiesFromGI = GI->PendingBotPersonalities; 
    }
    else { UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Could not get GameInstance. Using default player name and bot personalities.")); }


    GameStateData->Seats.Empty();
    GameStateData->Seats.Reserve(TotalActivePlayers);
    for (int32 i = 0; i < TotalActivePlayers; ++i) {
        FPlayerSeatData Seat;
        Seat.SeatIndex = i;
        Seat.bIsBot = (i >= NumRealPlayers);
        Seat.PlayerName = Seat.bIsBot ? FString::Printf(TEXT("Bot %d"), (i - NumRealPlayers) + 1) : PlayerActualName;
        Seat.PlayerId = Seat.bIsBot ? -1 : PlayerActualId;
        Seat.Stack = InitialStack;
        Seat.bIsSittingIn = true;
        Seat.Status = EPlayerStatus::Waiting;

        if (Seat.bIsBot)
        {
            int32 BotArrayIndex = i - NumRealPlayers; // Индекс бота в массиве личностей (0 для первого бота, 1 для второго и т.д.)
            if (BotPersonalitiesFromGI.IsValidIndex(BotArrayIndex))
            {
                Seat.BotPersonality = BotPersonalitiesFromGI[BotArrayIndex];
                UE_LOG(LogTemp, Log, TEXT("InitializeGame: Bot %s (Seat %d) initialized with custom personality - Aggro: %.2f, Bluff: %.2f, Tight: %.2f"),
                    *Seat.PlayerName, Seat.SeatIndex,
                    Seat.BotPersonality.Aggressiveness, Seat.BotPersonality.BluffFrequency, Seat.BotPersonality.Tightness);
            }
            else
            {
                Seat.BotPersonality = FBotPersonalitySettings(); 
                UE_LOG(LogTemp, Warning, TEXT("InitializeGame: Bot %s (Seat %d) initialized with DEFAULT personality (settings not found in GameInstance for bot index %d). Using defaults: Aggro: %.2f, Bluff: %.2f, Tight: %.2f"),
                    *Seat.PlayerName, Seat.SeatIndex, BotArrayIndex,
                    Seat.BotPersonality.Aggressiveness, Seat.BotPersonality.BluffFrequency, Seat.BotPersonality.Tightness);
            }
        }

        GameStateData->Seats.Add(Seat);
    }
    BuildTurnOrderMap();
    GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
    GameStateData->DealerSeat = -1;

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
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("--- Начало Новой Раздачи ---"));

    if (OnNewHandAboutToStartDelegate.IsBound())
    {
        UE_LOG(LogTemp, Log, TEXT("StartNewHand: Broadcasting OnNewHandAboutToStartDelegate.")); 
        OnNewHandAboutToStartDelegate.Broadcast();
    }

    StacksAtHandStart_Internal.Empty();
    if (GameStateData) 
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

        if (Seat.bIsSittingIn && Seat.Stack > 0) {

            Seat.Status = EPlayerStatus::Playing;
            NumActivePlayersThisHand++;
        }
        else if (Seat.Stack <= 0 && Seat.bIsSittingIn) {
            Seat.Status = EPlayerStatus::SittingOut;
        }
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

        GameStateData->DealerSeat = GetNextPlayerToAct(GameStateData->DealerSeat, true, EPlayerStatus::Playing);
    }

    if (GameStateData->DealerSeat == -1) {
        UE_LOG(LogTemp, Error, TEXT("StartNewHand CRITICAL: Could not determine new dealer seat!"));
        return;
    }
    GameStateData->Seats[GameStateData->DealerSeat].bIsDealer = true;
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Диллер - %s (Место: %d)"), *GameStateData->Seats[GameStateData->DealerSeat].PlayerName, GameStateData->DealerSeat));

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
        GameStateData->PendingBigBlindSeat = GetNextPlayerToAct(GameStateData->PendingSmallBlindSeat, true, EPlayerStatus::Playing); 
    }
    else { // 3+ игроков
        GameStateData->PendingSmallBlindSeat = GetNextPlayerToAct(GameStateData->DealerSeat, true, EPlayerStatus::Playing); 
        if (GameStateData->PendingSmallBlindSeat != -1) {
            // Ищем BB, начиная СРАЗU СО СЛЕДУЮЩЕГО после SB, исключая самого SB
            GameStateData->PendingBigBlindSeat = GetNextPlayerToAct(GameStateData->PendingSmallBlindSeat, true, EPlayerStatus::Playing); 
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

    const TArray<int32> FullFixedOrderNinePlayers = { 0, 2, 4, 6, 7, 8, 5, 3, 1 };

    // 3. Создаем актуальный порядок хода для текущей игры, фильтруя FullFixedOrder
    //    и оставляя только тех, кто есть в PlayersConsideredForTurnOrder.
    TArray<int32> ActualTurnOrderInThisGame;
    for (int32 OrderedSeatIndex : FullFixedOrderNinePlayers)
    {
        if (PlayersConsideredForTurnOrder.Contains(OrderedSeatIndex))
        {

            if (GameStateData->Seats.IsValidIndex(OrderedSeatIndex) && GameStateData->Seats[OrderedSeatIndex].bIsSittingIn)
            {
                ActualTurnOrderInThisGame.Add(OrderedSeatIndex);
            }
        }
    }

    if (ActualTurnOrderInThisGame.Num() < 2)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTurnOrderMap: Could not establish a valid turn order for %d active players based on fixed sequence. Players considered: %d. Filtered order count: %d."),
            PlayersConsideredForTurnOrder.Num(), PlayersConsideredForTurnOrder.Num(), ActualTurnOrderInThisGame.Num());
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
    FPlayerSeatData& PlayerToAct = GameStateData->Seats[SeatIndex]; 
    PlayerToAct.bIsTurn = true;

    if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(SeatIndex);
    if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(PlayerToAct.PlayerName, GameStateData->Pot);


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

        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({}); // Пустой массив
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, PlayerToAct.Stack, PlayerToAct.CurrentBet);

        return; // Выходим, так как бот будет действовать по таймеру
    }

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

    UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Seat %d (%s) attempts Action: %s, Amount (total bet for Bet/Raise): %lld. Stage: %s. Stack: %lld, PBetInRound: %lld, Pot: %lld, ToCallOnTable: %lld, LstAggr: %d (Amt %lld), Opener: %d, HasActed: %s"),
        ActingPlayerSeatIndex, *PlayerName, *UEnum::GetValueAsString(PlayerAction), Amount,
        *UEnum::GetValueAsString(GameStateData->CurrentStage), Player.Stack, Player.CurrentBet, GameStateData->Pot, GameStateData->CurrentBetToCall,
        GameStateData->LastAggressorSeatIndex, GameStateData->LastBetOrRaiseAmountInCurrentRound, GameStateData->PlayerWhoOpenedBettingThisRound,
        Player.bHasActedThisSubRound ? TEXT("true") : TEXT("false"));

    if (PlayerAction == EPlayerAction::PostBlind) 
    {
        bool bBlindPostedSuccessfully = false;
        if (GameStateData->CurrentStage == EGameStage::WaitingForSmallBlind && ActingPlayerSeatIndex == GameStateData->PendingSmallBlindSeat)
        {
            FPlayerSeatData& SBPlayer = Player; 
            int64 ActualSBToPost = FMath::Min(GameStateData->SmallBlindAmount, SBPlayer.Stack);
            SBPlayer.Stack -= ActualSBToPost;
            SBPlayer.CurrentBet = ActualSBToPost;
            SBPlayer.bIsSmallBlind = true;
            GameStateData->Pot += ActualSBToPost;

            if (SBPlayer.Stack == 0) {
                SBPlayer.Status = EPlayerStatus::AllIn;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s идёт All-In на малом блайнде (%lld)."), *PlayerName, ActualSBToPost));
            }
            else {
                SBPlayer.Status = EPlayerStatus::Playing;
            }
            if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s поставил Small Blind: %lld. Стек: %lld"), *PlayerName, ActualSBToPost, SBPlayer.Stack));

            bBlindPostedSuccessfully = true;
            Player.bHasActedThisSubRound = true; // SB сделал свое обязательное действие
            Player.bIsTurn = false; // Его ход на этом этапе завершен
            RequestBigBlind(); // Переходим к запросу BB
        }
        else if (GameStateData->CurrentStage == EGameStage::WaitingForBigBlind && ActingPlayerSeatIndex == GameStateData->PendingBigBlindSeat)
        {
            FPlayerSeatData& BBPlayer = Player;
            int64 ActualBBToPost = FMath::Min(GameStateData->BigBlindAmount, BBPlayer.Stack);
            BBPlayer.Stack -= ActualBBToPost;
            BBPlayer.CurrentBet = ActualBBToPost;
            BBPlayer.bIsBigBlind = true;
            GameStateData->Pot += ActualBBToPost;

            // Устанавливаем состояние для начала префлоп-торгов
            GameStateData->CurrentBetToCall = GameStateData->BigBlindAmount;
            GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex; // BB - последний "агрессор"
            GameStateData->LastBetOrRaiseAmountInCurrentRound = GameStateData->BigBlindAmount;
            GameStateData->PlayerWhoOpenedBettingThisRound = ActingPlayerSeatIndex; // BB "открыл" торги

            if (BBPlayer.Stack == 0) {
                BBPlayer.Status = EPlayerStatus::AllIn;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s идёт All-In на большом блайнде (%lld)."), *PlayerName, ActualBBToPost));
            }
            else {
                BBPlayer.Status = EPlayerStatus::Playing;
            }
            if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s поставил Big Blind: %lld. Стек: %lld"), *PlayerName, ActualBBToPost, BBPlayer.Stack));
            if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Блайнды поставлены. Банк: %lld"), GameStateData->Pot));

            bBlindPostedSuccessfully = true;
            Player.bHasActedThisSubRound = true; // BB сделал свое обязательное действие
            Player.bIsTurn = false; // Его ход на этом этапе завершен
            DealHoleCardsAndStartPreflop(); // Начинаем раздачу карт и префлоп
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid action/player/stage for PostBlind. Player: %s (Seat %d), Action: PostBlind, Stage: %s. Re-requesting action."),
                *PlayerName, ActingPlayerSeatIndex, *UEnum::GetValueAsString(GameStateData->CurrentStage));
            RequestPlayerAction(ActingPlayerSeatIndex); // Запросить действие у текущего игрока снова
        }
        if (bBlindPostedSuccessfully && OnTableStateInfoDelegate.IsBound()) {
            OnTableStateInfoDelegate.Broadcast(PlayerName, GameStateData->Pot);
        }
        return; // Выход из ProcessPlayerAction после обработки PostBlind
    }

    // --- Обработка Игровых Действий (Preflop, Flop, Turn, River) ---
    if (GameStateData->CurrentStage >= EGameStage::Preflop && GameStateData->CurrentStage <= EGameStage::River)
    {
        bool bActionCausedAggression = false;
        bool bActionValidAndPerformed = true; // Флаг, что действие было валидным и выполнено

        // Проверяем, может ли игрок вообще действовать (не Folded, не All-In который уже не может повлиять на банк)
        // Исключение: если игрок All-In, но его ставка меньше CurrentBetToCall, он не может действовать, но IsBettingRoundOver это учтет.
        if (Player.Status == EPlayerStatus::Folded ||
            (Player.Status == EPlayerStatus::AllIn && Player.Stack == 0 && Player.CurrentBet >= GameStateData->CurrentBetToCall))
        {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Player %s (Seat %d) is Folded or unabled All-In. Action processing skipped for them."), *PlayerName, ActingPlayerSeatIndex);
            bActionValidAndPerformed = false; // Действие не было выполнено этим игроком сейчас
        }
        else // Игрок может действовать
        {
            switch (PlayerAction)
            {
            case EPlayerAction::Fold:
                Player.Status = EPlayerStatus::Folded;
                // Player.bHasActedThisSubRound = true; // Устанавливается ниже для всех валидных действий
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s пасует (fold)."), *PlayerName));
                break;

            case EPlayerAction::Check:
                if (Player.CurrentBet == GameStateData->CurrentBetToCall) {
                    // Player.bHasActedThisSubRound = true;
                    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s пропускает (check)."), *PlayerName));
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Check by %s. BetToCall: %lld, PlayerBet: %lld. Re-requesting action."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                    RequestPlayerAction(ActingPlayerSeatIndex);
                    bActionValidAndPerformed = false; // Действие не было выполнено
                }
                break;

            case EPlayerAction::Call:
            {
                int64 AmountNeededToCallAbsolute = GameStateData->CurrentBetToCall - Player.CurrentBet;
                if (AmountNeededToCallAbsolute <= 0) { // Нечего коллировать или уже заколлировано
                    if (Player.CurrentBet == GameStateData->CurrentBetToCall) { // Можно было чекнуть
                        UE_LOG(LogTemp, Verbose, TEXT("ProcessPlayerAction: %s attempted Call but could Check. Treating as Check."), *PlayerName);
                        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s пропускает (check)."), *PlayerName));
                        // Player.bHasActedThisSubRound = true; (будет установлено ниже)
                    }
                    else {
                        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Call by %s. BetToCall: %lld, PlayerBet: %lld. Re-requesting."), *PlayerName, GameStateData->CurrentBetToCall, Player.CurrentBet);
                        RequestPlayerAction(ActingPlayerSeatIndex);
                        bActionValidAndPerformed = false;
                    }
                    break;
                }
                int64 ActualAmountPlayerPutsInPot = FMath::Min(AmountNeededToCallAbsolute, Player.Stack);
                Player.Stack -= ActualAmountPlayerPutsInPot;
                Player.CurrentBet += ActualAmountPlayerPutsInPot;
                GameStateData->Pot += ActualAmountPlayerPutsInPot;
                // Player.bHasActedThisSubRound = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s отвечает (call): %lld. Стек: %lld"), *PlayerName, ActualAmountPlayerPutsInPot, Player.Stack));
                if (Player.Stack == 0) { Player.Status = EPlayerStatus::AllIn; if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s идёт All-In."), *PlayerName)); }
            }
            break;

            case EPlayerAction::Bet: // Бет возможен, только если CurrentBetToCall равен текущей ставке игрока (обычно 0)
            {
                if (Player.CurrentBet != GameStateData->CurrentBetToCall) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Bet by %s (Cannot Bet, must Call/Raise/Fold). CurrentBet: %lld, ToCall: %lld. Re-requesting."), *PlayerName, Player.CurrentBet, GameStateData->CurrentBetToCall);
                    RequestPlayerAction(ActingPlayerSeatIndex);
                    bActionValidAndPerformed = false;
                    break;
                }
                // Amount здесь - это чистая сумма бета (сколько добавляется сверх Player.CurrentBet, которое должно быть 0)
                // Но AI передает ОБЩУЮ сумму ставки. Для первого бета Amount == чистый бет.
                int64 ActualBetAmountPlayerAdds = Amount; // Если Player.CurrentBet = 0
                if (ActualBetAmountPlayerAdds <= 0) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Bet by %s (Amount %lld must be > 0). Re-requesting."), *PlayerName, ActualBetAmountPlayerAdds);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }
                int64 MinBetSize = GameStateData->BigBlindAmount;
                if (ActualBetAmountPlayerAdds < MinBetSize && ActualBetAmountPlayerAdds < Player.Stack) { // Нельзя ставить меньше минимума, кроме олл-ина
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Bet by %s (Amount %lld < MinBet %lld and not All-In). Re-requesting."), *PlayerName, ActualBetAmountPlayerAdds, MinBetSize);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }
                if (ActualBetAmountPlayerAdds > Player.Stack) ActualBetAmountPlayerAdds = Player.Stack; // Коррекция на олл-ин

                Player.Stack -= ActualBetAmountPlayerAdds;
                Player.CurrentBet += ActualBetAmountPlayerAdds; // Теперь Player.CurrentBet = ActualBetAmountPlayerAdds
                GameStateData->Pot += ActualBetAmountPlayerAdds;
                GameStateData->CurrentBetToCall = Player.CurrentBet; // Новая сумма для колла
                GameStateData->LastBetOrRaiseAmountInCurrentRound = ActualBetAmountPlayerAdds; // Это была первая ставка, ее размер
                GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex;
                if (GameStateData->PlayerWhoOpenedBettingThisRound == -1) GameStateData->PlayerWhoOpenedBettingThisRound = ActingPlayerSeatIndex;
                bActionCausedAggression = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s ставит (bet): %lld. Стек: %lld"), *PlayerName, ActualBetAmountPlayerAdds, Player.Stack));
                if (Player.Stack == 0) { Player.Status = EPlayerStatus::AllIn; if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s идёт All-In."), *PlayerName)); }
            }
            break;

            case EPlayerAction::Raise:
            {
                int64 TotalNewBetByPlayer = Amount;

                // 1. Проверка, есть ли вообще что рейзить (CurrentBetToCall должен быть > 0)
                if (GameStateData->CurrentBetToCall == 0) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (No bet to raise, should be Bet). Re-requesting."), *PlayerName);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }
                // 2. Проверка, что новая общая ставка БОЛЬШЕ текущей ставки для колла
                if (TotalNewBetByPlayer <= GameStateData->CurrentBetToCall && TotalNewBetByPlayer < (Player.CurrentBet + Player.Stack)) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s. TotalBet %lld not > CurrentBetToCall %lld (and not a smaller All-In). Re-requesting."),
                        *PlayerName, TotalNewBetByPlayer, GameStateData->CurrentBetToCall);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }

                // Сколько игрок реально добавляет фишек в банк в этом действии
                int64 AmountPlayerActuallyAdds = TotalNewBetByPlayer - Player.CurrentBet;
                if (AmountPlayerActuallyAdds <= 0 && TotalNewBetByPlayer < (Player.CurrentBet + Player.Stack)) { // Должен добавить > 0, если не олл-ин
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (Adds %lld, not >0, and not All-In). Re-requesting."), *PlayerName, AmountPlayerActuallyAdds);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }

                // Если запрашиваемая общая ставка больше, чем есть у игрока (с учетом уже поставленного) -> это олл-ин
                if (AmountPlayerActuallyAdds > Player.Stack) {
                    AmountPlayerActuallyAdds = Player.Stack;
                    TotalNewBetByPlayer = Player.CurrentBet + Player.Stack; // Корректируем общую ставку до олл-ина
                }

                // Чистая сумма рейза СВЕРХ предыдущей максимальной ставки на столе
                int64 PureRaiseAmount = TotalNewBetByPlayer - GameStateData->CurrentBetToCall;
                // Минимальный допустимый чистый рейз (размер предыдущего бета/рейза, или ББ если это первый рейз после лимпов/блайндов)
                int64 MinValidPureRaise = GameStateData->LastBetOrRaiseAmountInCurrentRound > 0 ? GameStateData->LastBetOrRaiseAmountInCurrentRound : GameStateData->BigBlindAmount;

                // Валидация: чистый рейз должен быть не меньше минимального, ЕСЛИ это не олл-ин на меньшую сумму
                if (PureRaiseAmount < MinValidPureRaise && AmountPlayerActuallyAdds < Player.Stack) {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Invalid Raise by %s (PureRaise %lld < MinValidPureRaise %lld and not All-In). Re-requesting."), *PlayerName, PureRaiseAmount, MinValidPureRaise);
                    RequestPlayerAction(ActingPlayerSeatIndex); bActionValidAndPerformed = false; break;
                }

                Player.Stack -= AmountPlayerActuallyAdds;
                Player.CurrentBet = TotalNewBetByPlayer; // Обновляем общую ставку игрока в этом раунде
                GameStateData->Pot += AmountPlayerActuallyAdds;

                GameStateData->CurrentBetToCall = Player.CurrentBet; // Новая сумма для колла
                GameStateData->LastBetOrRaiseAmountInCurrentRound = PureRaiseAmount > 0 ? PureRaiseAmount : MinValidPureRaise; // Если PureRaise <0 из-за олл-ина, берем MinValidPureRaise
                GameStateData->LastAggressorSeatIndex = ActingPlayerSeatIndex;
                bActionCausedAggression = true;
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s поднимает (raise) до %lld (добавил %lld). Стек: %lld"), *PlayerName, Player.CurrentBet, AmountPlayerActuallyAdds, Player.Stack));
                if (Player.Stack == 0) { Player.Status = EPlayerStatus::AllIn; if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s идёт All-In."), *PlayerName)); }
            }
            break;

            default:
                UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Unknown action %s received."), *UEnum::GetValueAsString(PlayerAction));
                RequestPlayerAction(ActingPlayerSeatIndex);
                bActionValidAndPerformed = false; // Неизвестное действие не выполнено
            }
        }

        if (!bActionValidAndPerformed) {
            return;
        }

        // Если действие было валидным (Fold, Check, Call, Bet, Raise) и игрок не был пропущен
        if (Player.Status != EPlayerStatus::Folded && !(Player.Status == EPlayerStatus::AllIn && Player.Stack == 0 && Player.CurrentBet >= GameStateData->CurrentBetToCall))
        {
            Player.bHasActedThisSubRound = true;
        }

        Player.bIsTurn = false;

        if (bActionCausedAggression) {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Aggression from Seat %d. Resetting bHasActedThisSubRound for others."), ActingPlayerSeatIndex);
            for (FPlayerSeatData& SeatToReset : GameStateData->Seats) {
                if (SeatToReset.SeatIndex != ActingPlayerSeatIndex &&
                    SeatToReset.bIsSittingIn &&
                    SeatToReset.Status == EPlayerStatus::Playing && // Только для тех, кто еще может ходить
                    SeatToReset.Stack > 0) {                       // И у кого есть фишки
                    SeatToReset.bHasActedThisSubRound = false;
                    UE_LOG(LogTemp, Verbose, TEXT("  Reset bHasActedThisSubRound for Seat %d"), SeatToReset.SeatIndex);
                }
            }
        }

        // --- Логика после действия игрока ---
        int32 ActivePlayersStillInHand = 0; // Игроки, которые не сфолдили и не сидят аут
        for (const FPlayerSeatData& Seat : GameStateData->Seats) {
            if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded && Seat.Status != EPlayerStatus::SittingOut) {
                ActivePlayersStillInHand++;
            }
        }

        if (ActivePlayersStillInHand <= 1)
        {
            UE_LOG(LogTemp, Log, TEXT("ProcessPlayerAction: Hand ends with %d active player(s). Proceeding to Showdown logic to finalize."), ActivePlayersStillInHand);
            ProceedToShowdown();
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
                // Это аварийный переход, если логика зашла в тупик. Должно быть очень редким.
                ProceedToNextGameStage();
            }
        }
        return; // Явный выход из функции после обработки действия и определения следующего шага
    }
    else 
    {
        UE_LOG(LogTemp, Warning, TEXT("ProcessPlayerAction: Action %s received in unhandled game stage %s."),
            *UEnum::GetValueAsString(PlayerAction), *UEnum::GetValueAsString(GameStateData->CurrentStage));
        RequestPlayerAction(ActingPlayerSeatIndex); // Запросить действие у текущего игрока снова
    }
}

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
    }
}


void UOfflineGameManager::DealHoleCardsAndStartPreflop()
{
    if (!GameStateData || !Deck) {
        UE_LOG(LogTemp, Error, TEXT("DealHoleCardsAndStartPreflop: GameStateData or Deck is null!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Initiating card dealing and preflop setup..."));
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("Раздача карманных карт..."));

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
    if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(TEXT("Карманные карты разданы. Начинается прием ставок на префлопе."));

    // 6. Уведомляем контроллер, что карты розданы, ПЕРЕД запросом первого действия на префлопе
    if (OnActualHoleCardsDealtDelegate.IsBound()) {
        UE_LOG(LogTemp, Log, TEXT("DealHoleCardsAndStartPreflop: Broadcasting OnActualHoleCardsDealtDelegate."));
        OnActualHoleCardsDealtDelegate.Broadcast();
    }

    // 7. СБРОС ФЛАГОВ bHasActedThisSubRound ДЛЯ НАЧАЛА ПРЕФЛОП-ТОРГОВ
    for (FPlayerSeatData& Seat : GameStateData->Seats) {
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


int32 UOfflineGameManager::DetermineFirstPlayerToActAtPreflop() const
{
    if (!GameStateData || !GameStateData->Seats.IsValidIndex(GameStateData->PendingBigBlindSeat) || !GameStateData->Seats.IsValidIndex(GameStateData->PendingSmallBlindSeat))
    {
        UE_LOG(LogTemp, Error, TEXT("DetermineFirstPlayerToActAtPreflop: GameStateData is null, or PendingBigBlindSeat/PendingSmallBlindSeat is invalid."));
        return -1;
    }

    int32 NumPlayersActuallyInHandForBetting = 0;
    for (const FPlayerSeatData& Seat : GameStateData->Seats)
    {
        if (Seat.bIsSittingIn &&
            (Seat.Status == EPlayerStatus::Playing || Seat.Status == EPlayerStatus::AllIn ||
                Seat.Status == EPlayerStatus::MustPostSmallBlind || Seat.Status == EPlayerStatus::MustPostBigBlind))
        {
            NumPlayersActuallyInHandForBetting++;
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("DetermineFirstPlayerToActAtPreflop: NumPlayersActuallyInHandForBetting: %d. SB Seat: %d, BB Seat: %d"),
        NumPlayersActuallyInHandForBetting, GameStateData->PendingSmallBlindSeat, GameStateData->PendingBigBlindSeat);

    if (NumPlayersActuallyInHandForBetting < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("DetermineFirstPlayerToActAtPreflop: Less than 2 players actually in hand for betting. Returning -1."));
        return -1;
    }

    if (NumPlayersActuallyInHandForBetting == 2)
    {
        // Хедз-ап: первым ходит SB (который также может быть дилером).
        // Важно, чтобы PendingSmallBlindSeat был корректно определен в StartNewHand для хедз-апа.
        UE_LOG(LogTemp, Log, TEXT("DetermineFirstPlayerToActAtPreflop: Heads-up detected. SB (Seat %d) acts first."), GameStateData->PendingSmallBlindSeat);
        return GameStateData->PendingSmallBlindSeat;
    }
    else // 3+ игроков в раздаче
    {
        int32 FirstToAct = GetNextPlayerToAct(GameStateData->PendingBigBlindSeat, true, EPlayerStatus::MAX_None);
        UE_LOG(LogTemp, Log, TEXT("DetermineFirstPlayerToActAtPreflop: 3+ players. Player to the left of BB (Seat %d) is Seat %d."), GameStateData->PendingBigBlindSeat, FirstToAct);
        return FirstToAct;
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
        return -1;
    }

    int32 InitialSearchIndex = StartSeatIndex;
    bool bInitialIndexValidAndInMap = GameStateData->Seats.IsValidIndex(InitialSearchIndex) && CurrentTurnOrderMap_Internal.Contains(InitialSearchIndex);

    // 1. Определяем, с какого индекса реально начинать поиск в CurrentTurnOrderMap_Internal
    if (!bInitialIndexValidAndInMap)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetNextPlayerToAct: StartSeatIndex %d is invalid or not in CurrentTurnOrderMap_Internal. Attempting to find a valid starting point."), StartSeatIndex);

        if (CurrentTurnOrderMap_Internal.Num() > 0) {
            InitialSearchIndex = CurrentTurnOrderMap_Internal.begin().Key(); 
            bExcludeStartSeat = false; 
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
                    if (RequiredStatus == EPlayerStatus::Playing) {
                        if (Seat.Stack > 0) bIsEligible = true;
                    }
                    else if (RequiredStatus == EPlayerStatus::MustPostSmallBlind || RequiredStatus == EPlayerStatus::MustPostBigBlind) {
                        bIsEligible = true; // Позволяем ему попытаться поставить блайнд, даже если стек 0 (это будет AllIn)
                    }
                    else if (RequiredStatus == EPlayerStatus::AllIn) {
                        bIsEligible = true;
                    }
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
    if (!GameStateData || GameStateData->DealerSeat == -1 || GameStateData->Seats.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("DetermineFirstPlayerToActPostflop: Invalid preconditions (GameState, DealerSeat, or Seats empty)."));
        return -1;
    }

    if (CurrentTurnOrderMap_Internal.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("DetermineFirstPlayerToActPostflop: CurrentTurnOrderMap_Internal is empty! Cannot determine action order."));
        return -1;
    }


    int32 FirstToAct = GetNextPlayerToAct(GameStateData->DealerSeat, true, EPlayerStatus::MAX_None);


    if (FirstToAct == -1)
    {
        UE_LOG(LogTemp, Warning, TEXT("DetermineFirstPlayerToActPostflop: GetNextPlayerToAct returned -1 (no active player found starting after dealer %d). This might happen if only dealer is left or map error."), GameStateData->DealerSeat);
    }

    UE_LOG(LogTemp, Log, TEXT("DetermineFirstPlayerToActPostflop: Dealer is Seat %d. First to act on postflop determined as Seat %d."), GameStateData->DealerSeat, FirstToAct);
    return FirstToAct;
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
        OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Раунд ставок на %s окончен. ---"), *UEnum::GetValueAsString(StageBeforeAdvance)));
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
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Раздача на стадии Flop ---")));
        for (int i = 0; i < 3; ++i) {
            TOptional<FCard> DealtCardOpt = Deck->DealCard();
            if (DealtCardOpt.IsSet()) { GameStateData->CommunityCards.Add(DealtCardOpt.GetValue()); }
            else { bDealingErrorOccurred = true; UE_LOG(LogTemp, Error, TEXT("Deck ran out for Flop card %d"), i + 1); break; }
        }
        if (!bDealingErrorOccurred && GameStateData->CommunityCards.Num() >= 3 && OnGameHistoryEventDelegate.IsBound()) {
            OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Общие карты на Flop: %s %s %s"),
                *GameStateData->CommunityCards[0].ToRussianString(), *GameStateData->CommunityCards[1].ToRussianString(), *GameStateData->CommunityCards[2].ToRussianString()));
        }
    }
    break;
    case EGameStage::Flop:
    {
        NextStageToSet = EGameStage::Turn;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Раздача на стадии Turn ---")));
        TOptional<FCard> TurnCardOpt = Deck->DealCard();
        if (TurnCardOpt.IsSet()) {
            GameStateData->CommunityCards.Add(TurnCardOpt.GetValue());
            if (GameStateData->CommunityCards.Num() >= 4 && OnGameHistoryEventDelegate.IsBound()) {
                OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Общие карты на Turn: %s"), *GameStateData->CommunityCards.Last().ToRussianString()));
            }
        }
        else { bDealingErrorOccurred = true; UE_LOG(LogTemp, Error, TEXT("Deck ran out for Turn")); }
    }
    break;
    case EGameStage::Turn:
    {
        NextStageToSet = EGameStage::River;
        if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Раздача на стадии River ---")));
        TOptional<FCard> RiverCardOpt = Deck->DealCard();
        if (RiverCardOpt.IsSet()) {
            GameStateData->CommunityCards.Add(RiverCardOpt.GetValue());
            if (GameStateData->CommunityCards.Num() >= 5 && OnGameHistoryEventDelegate.IsBound()) {
                OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Общие карты на River: %s"), *GameStateData->CommunityCards.Last().ToRussianString()));
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
            if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("--- Автоматическая раздача на %s ---"), *UEnum::GetValueAsString(StageToDealNext)));

            TOptional<FCard> NextCardOpt = Deck->DealCard();
            if (NextCardOpt.IsSet()) {
                GameStateData->CommunityCards.Add(NextCardOpt.GetValue());
                if (OnGameHistoryEventDelegate.IsBound()) OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s: %s"), *UEnum::GetValueAsString(StageToDealNext), *GameStateData->CommunityCards.Last().ToRussianString()));
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
    if (!GameStateData || !Deck) // Добавил проверку Deck на всякий случай, хотя здесь он не используется
    {
        UE_LOG(LogTemp, Error, TEXT("ProceedToShowdown: GameStateData or Deck is null!"));
        // Безопасное уведомление UI, если возможно
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
        OnGameHistoryEventDelegate.Broadcast(TEXT("--- Вскрытие карт ---"));
        if (GameStateData->CommunityCards.Num() > 0) {
            FString CommunityCardsString = TEXT("Общие карты: ");
            for (int32 i = 0; i < GameStateData->CommunityCards.Num(); ++i) {
                CommunityCardsString += GameStateData->CommunityCards[i].ToRussianString();
                if (i < GameStateData->CommunityCards.Num() - 1) CommunityCardsString += TEXT(" ");
            }
            OnGameHistoryEventDelegate.Broadcast(CommunityCardsString);
        }
        else {
            OnGameHistoryEventDelegate.Broadcast(TEXT("Общие карты на стол не выкладывались."));
        }
    }
    GameStateData->CurrentStage = EGameStage::Showdown;
    GameStateData->CurrentTurnSeat = -1; // Явно указываем, что активного хода нет
    if (OnPlayerTurnStartedDelegate.IsBound()) OnPlayerTurnStartedDelegate.Broadcast(-1); // Уведомляем UI

    // Структура для хранения данных для оценки и последующего использования
    struct FPlayerHandEvaluation
    {
        int32 SeatIndex;
        FPokerHandResult HandResult;
        FPlayerSeatData PlayerDataAtShowdown; // Снимок состояния игрока на момент шоудауна (стек до выигрыша)
        bool bEligibleToWinPot; // Флаг, может ли игрок претендовать на банк (не сфолдил)
    };

    TArray<FPlayerHandEvaluation> PlayerEvaluations; // Будем хранить здесь всех, кто дошел до вскрытия или был AllIn

    // 1. Собираем данные и оцениваем руки игроков, которые НЕ СФОЛДИЛИ и УЧАСТВУЮТ В ИГРЕ
    UE_LOG(LogTemp, Log, TEXT("Showdown: Evaluating hands of eligible players..."));
    for (const FPlayerSeatData& CurrentSeatState : GameStateData->Seats)
    {
        if (CurrentSeatState.bIsSittingIn && CurrentSeatState.Status != EPlayerStatus::Waiting && CurrentSeatState.HoleCards.Num() == 2)
        {
            FPokerHandResult EvaluatedResult = UPokerHandEvaluator::EvaluatePokerHand(CurrentSeatState.HoleCards, GameStateData->CommunityCards);
            bool bIsEligible = (CurrentSeatState.Status != EPlayerStatus::Folded);

            PlayerEvaluations.Add({ CurrentSeatState.SeatIndex, EvaluatedResult, CurrentSeatState, bIsEligible });

            UE_LOG(LogTemp, Verbose, TEXT("  Evaluated Seat %d (%s): HandRank %s, EligibleToWin: %s, Status: %s"),
                CurrentSeatState.SeatIndex, *CurrentSeatState.PlayerName,
                *UEnum::GetDisplayValueAsText(EvaluatedResult.HandRank).ToString(),
                bIsEligible ? TEXT("Yes") : TEXT("No"),
                *UEnum::GetDisplayValueAsText(CurrentSeatState.Status).ToString());
        }
    }

    if (PlayerEvaluations.Num() == 0) { // Если нет рук для оценки (все сфолдили до шоудауна, что должно было обработаться раньше)
        UE_LOG(LogTemp, Warning, TEXT("Showdown: No hands could be evaluated. Hand likely ended before showdown."));
        GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
        FString Announcement = TEXT("Hand ended before showdown.");
        TArray<FShowdownPlayerInfo> EmptyResults;
        if (OnShowdownResultsDelegate.IsBound()) OnShowdownResultsDelegate.Broadcast(EmptyResults, Announcement);
        if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Hand Over"), GameStateData->Pot);
        if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
        if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0, 0);
        return;
    }

    // 2. Определяем победителя(ей) ТОЛЬКО среди тех, кто ELIGIBLE TO WIN (не сфолдил)
    TArray<FPlayerHandEvaluation> EligibleHandsToCompare;
    for (const auto& Eval : PlayerEvaluations) {
        if (Eval.bEligibleToWinPot) {
            EligibleHandsToCompare.Add(Eval);
        }
    }

    TArray<int32> ActualWinningSeatIndices;
    if (EligibleHandsToCompare.Num() == 1) {
        ActualWinningSeatIndices.Add(EligibleHandsToCompare[0].SeatIndex);
        UE_LOG(LogTemp, Log, TEXT("Showdown: One eligible player (Seat %d), wins by default."), EligibleHandsToCompare[0].SeatIndex);
    }
    else if (EligibleHandsToCompare.Num() > 1) {
        // Сортируем руки от лучшей к худшей
        EligibleHandsToCompare.Sort([](const FPlayerHandEvaluation& A, const FPlayerHandEvaluation& B) {
            return UPokerHandEvaluator::CompareHandResults(A.HandResult, B.HandResult) > 0; // >0 A лучше B
            });

        FPokerHandResult BestEligibleHand = EligibleHandsToCompare[0].HandResult;
        ActualWinningSeatIndices.Add(EligibleHandsToCompare[0].SeatIndex);

        for (int32 i = 1; i < EligibleHandsToCompare.Num(); ++i) {
            if (UPokerHandEvaluator::CompareHandResults(EligibleHandsToCompare[i].HandResult, BestEligibleHand) == 0) {
                ActualWinningSeatIndices.Add(EligibleHandsToCompare[i].SeatIndex); // Ничья
            }
            else {
                break; // Остальные руки слабее
            }
        }
        UE_LOG(LogTemp, Log, TEXT("Showdown: Comparison complete. Best HandRank: %s. Winners: %d"),
            *UEnum::GetDisplayValueAsText(BestEligibleHand.HandRank).ToString(), ActualWinningSeatIndices.Num());
    }
    else 
    {
        UE_LOG(LogTemp, Warning, TEXT("Showdown: No eligible players to compare hands, though PlayerEvaluations was not empty. This indicates an issue."));
    }


    // 3. Награждаем победителя(ей) - это ИЗМЕНИТ стеки в GameStateData->Seats
    TMap<int32, int64> AmountsWonByPlayers = AwardPotToWinner(ActualWinningSeatIndices);

    // 4. Формируем финальные данные для UI и строку объявления
    TArray<FShowdownPlayerInfo> FinalShowdownResultsData;
    FString FinalWinnerAnnouncementString;

    // Собираем информацию для ВСЕХ, кто участвовал в шоудауне (PlayerEvaluations)
    for (const FPlayerHandEvaluation& EvalEntry : PlayerEvaluations)
    {
        FShowdownPlayerInfo Info;
        Info.SeatIndex = EvalEntry.SeatIndex;
        Info.PlayerName = EvalEntry.PlayerDataAtShowdown.PlayerName; // Используем имя из снимка
        Info.HoleCards = EvalEntry.PlayerDataAtShowdown.HoleCards;   // Карты из снимка
        Info.HandResult = EvalEntry.HandResult;                      // Оцененный результат
        Info.PlayerStatusAtShowdown = EvalEntry.PlayerDataAtShowdown.Status; // Статус на момент шоудауна (Folded, AllIn, Playing)

        Info.bIsWinner = ActualWinningSeatIndices.Contains(EvalEntry.SeatIndex);
        Info.AmountWon = AmountsWonByPlayers.Contains(EvalEntry.SeatIndex) ? AmountsWonByPlayers.FindChecked(EvalEntry.SeatIndex) : 0;

        if (GameStateData->Seats.IsValidIndex(EvalEntry.SeatIndex) && StacksAtHandStart_Internal.Contains(EvalEntry.SeatIndex))
        {
            Info.NetResult = GameStateData->Seats[EvalEntry.SeatIndex].Stack - StacksAtHandStart_Internal.FindChecked(EvalEntry.SeatIndex);
        }
        else
        {
            Info.NetResult = Info.AmountWon; // Запасной вариант, если начальный стек не найден
            UE_LOG(LogTemp, Warning, TEXT("ProceedToShowdown: Could not find StackAtHandStart_Internal for Seat %d (%s). NetResult for UI defaults to AmountWon."), EvalEntry.SeatIndex, *Info.PlayerName);
        }
        FinalShowdownResultsData.Add(Info);

        // Логирование для истории игры (можно немного упростить, т.к. основное объявление будет ниже)
        if (OnGameHistoryEventDelegate.IsBound()) {
            FString HandDesc = *PokerRankToRussianString(Info.HandResult.HandRank); 
            FString ActionDesc = TEXT("");
            if (Info.PlayerStatusAtShowdown == EPlayerStatus::Folded) ActionDesc = TEXT("(Спасовал)");
            else if (Info.bIsWinner) ActionDesc = FString::Printf(TEXT("(Выиграл %lld)"), Info.AmountWon);

            OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("%s (Место: %d) показывает %s %s - %s %s"),
                *Info.PlayerName, Info.SeatIndex,
                Info.HoleCards.IsValidIndex(0) ? *Info.HoleCards[0].ToRussianString() : TEXT("??"),
                Info.HoleCards.IsValidIndex(1) ? *Info.HoleCards[1].ToRussianString() : TEXT("??"),
                *HandDesc, *ActionDesc
            ));
        }
    }

    // Формируем строку объявления победителя(ей)
    if (ActualWinningSeatIndices.Num() > 0) {
        if (ActualWinningSeatIndices.Num() == 1) {
            int32 WinnerIdx = ActualWinningSeatIndices[0];
            // Находим информацию о победителе в FinalShowdownResultsData для корректного имени и руки
            const FShowdownPlayerInfo* WinnerInfoPtr = FinalShowdownResultsData.FindByPredicate(
                [WinnerIdx](const FShowdownPlayerInfo& info) { return info.SeatIndex == WinnerIdx; });
            if (WinnerInfoPtr) {
                FinalWinnerAnnouncementString = FString::Printf(TEXT("%s выиграл %lld с комбинацией: %s!"),
                    *WinnerInfoPtr->PlayerName, WinnerInfoPtr->AmountWon, *PokerRankToRussianString(WinnerInfoPtr->HandResult.HandRank));
            }
            else { FinalWinnerAnnouncementString = TEXT("Победитель определён, но информации нет"); }
        }
        else {
            FinalWinnerAnnouncementString = TEXT("Банк разделили: ");
            for (int32 i = 0; i < ActualWinningSeatIndices.Num(); ++i) {
                int32 WinnerIdx = ActualWinningSeatIndices[i];
                const FShowdownPlayerInfo* WinnerInfoPtr = FinalShowdownResultsData.FindByPredicate(
                    [WinnerIdx](const FShowdownPlayerInfo& info) { return info.SeatIndex == WinnerIdx; });
                if (WinnerInfoPtr) {
                    FinalWinnerAnnouncementString += FString::Printf(TEXT("%s (%s, +%lld)"),
                        *WinnerInfoPtr->PlayerName, *PokerRankToRussianString(WinnerInfoPtr->HandResult.HandRank), WinnerInfoPtr->AmountWon);
                    if (i < ActualWinningSeatIndices.Num() - 1) FinalWinnerAnnouncementString += TEXT("; ");
                }
            }
        }
    }
    else {
        FinalWinnerAnnouncementString = TEXT("В этой раздаче нет определенного победителя");
    }
    if (OnGameHistoryEventDelegate.IsBound() && !FinalWinnerAnnouncementString.IsEmpty()) OnGameHistoryEventDelegate.Broadcast(FinalWinnerAnnouncementString);


    if (OnShowdownResultsDelegate.IsBound())
    {
        UE_LOG(LogTemp, Log, TEXT("Showdown: Broadcasting OnShowdownResultsDelegate with %d results. Announcement: '%s'"), FinalShowdownResultsData.Num(), *FinalWinnerAnnouncementString);
        OnShowdownResultsDelegate.Broadcast(FinalShowdownResultsData, FinalWinnerAnnouncementString);
    }

    // 5. Рука завершена
    UE_LOG(LogTemp, Log, TEXT("--- HAND OVER (After Showdown Finalization) ---"));
    GameStateData->CurrentStage = EGameStage::WaitingForPlayers;
    if (OnTableStateInfoDelegate.IsBound()) OnTableStateInfoDelegate.Broadcast(TEXT("Раздача завершена"), GameStateData->Pot); // Pot должен быть 0
    if (OnPlayerActionsAvailableDelegate.IsBound()) OnPlayerActionsAvailableDelegate.Broadcast({});
    if (OnActionUIDetailsDelegate.IsBound()) OnActionUIDetailsDelegate.Broadcast(0, GameStateData->BigBlindAmount, 0, 0); // Добавлен 5-й параметр

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
            OnGameHistoryEventDelegate.Broadcast(FString::Printf(TEXT("Банк в размере %lld остается неразыгранным (победители не указаны)"), TotalPotToAward));
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
            WinnersLogString += FString::Printf(TEXT("%s (Место %d) +%lld (Обновлённый стек: %lld)"),
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

    FString FinalLogMessage = FString::Printf(TEXT("Банк в размере %lld достаётся %s"), TotalPotToAward, *WinnersLogString);
    UE_LOG(LogTemp, Log, TEXT("AwardPotToWinner: %s"), *FinalLogMessage);
    if (OnGameHistoryEventDelegate.IsBound()) {
        OnGameHistoryEventDelegate.Broadcast(FinalLogMessage);
    }

    GameStateData->Pot = 0; 

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
    // 1. Проверка предусловий
    if (!GameStateData || !BotAIInstance || !GameStateData->Seats.IsValidIndex(BotSeatIndex) || !GameStateData->Seats[BotSeatIndex].bIsBot)
    {
        UE_LOG(LogTemp, Error, TEXT("TriggerBotDecision: Prerequisites not met for BotSeatIndex %d. (GameState: %d, BotAI: %d, ValidIndex: %d, IsBot: %d)"),
            BotSeatIndex,
            GameStateData.Get() ? 1 : 0,                       // bool -> int (1 for true, 0 for false)
            BotAIInstance.Get() ? 1 : 0,                       // bool -> int (1 for true, 0 for false)
            (GameStateData.Get() && GameStateData->Seats.IsValidIndex(BotSeatIndex)) ? 1 : 0, // bool -> int
            (GameStateData.Get() && GameStateData->Seats.IsValidIndex(BotSeatIndex) && GameStateData->Seats[BotSeatIndex].bIsBot) ? 1 : 0 // bool -> int
        );
        // Если что-то не так, и у нас есть текущий ход, лучше его не терять, а запросить снова
        // или обработать как ошибку, чтобы игра не зависла.
        if (GameStateData && GameStateData->CurrentTurnSeat != -1) {
            RequestPlayerAction(GameStateData->CurrentTurnSeat);
        }
        return;
    }

    // 2. Проверяем, действительно ли сейчас ход этого бота
    if (GameStateData->CurrentTurnSeat != BotSeatIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("TriggerBotDecision: CurrentTurnSeat (%d) is not BotSeatIndex (%d). Bot will not act now. Timer might have fired late."),
            GameStateData->CurrentTurnSeat, BotSeatIndex);
        // Не вызываем ProcessPlayerAction, так как ход уже мог перейти к другому
        // или ситуация изменилась. RequestPlayerAction для CurrentTurnSeat должен был быть уже вызван.
        return;
    }

    const FPlayerSeatData& BotPlayer = GameStateData->Seats[BotSeatIndex];

    // 3. Получаем контекст действий для бота
    FActionDecisionContext BotActionContext = GetActionContextForSeat(BotSeatIndex);

    // 4. Проверяем, есть ли у бота доступные действия
    // (Учитываем, что AllIn игрок может не иметь действий, но раунд еще не окончен для других)
    if (BotActionContext.AvailableActions.IsEmpty() && BotPlayer.Status != EPlayerStatus::AllIn)
    {
        UE_LOG(LogTemp, Log, TEXT("TriggerBotDecision: Bot %s (Seat %d) has no available actions (and not AllIn). Current Status: %s. Attempting to advance game state."),
            *BotPlayer.PlayerName, BotSeatIndex, *UEnum::GetValueAsString(BotPlayer.Status));

        // Если у бота нет действий (и он не AllIn), это может означать, что он должен был бы
        // автоматически пропустить ход (например, уже сфолдил, или это ошибка в GetActionContextForSeat).
        // Пытаемся определить, окончен ли раунд, или передать ход следующему.
        if (IsBettingRoundOver()) {
            ProceedToNextGameStage();
        }
        else {
            // Если раунд не окончен, но у текущего бота нет действий (что странно, если он не AllIn/Folded)
            // пытаемся передать ход следующему. GetNextPlayerToAct должен пропустить этого бота.
            int32 NextPlayer = GetNextPlayerToAct(BotSeatIndex, true);
            if (NextPlayer != -1) {
                RequestPlayerAction(NextPlayer);
            }
            else {
                // Это критическая ситуация, если раунд не окончен, а следующего игрока нет.
                UE_LOG(LogTemp, Error, TEXT("TriggerBotDecision: No available actions for bot %d, IsBettingRoundOver is false, but no next player found! Forcing stage advance."), BotSeatIndex);
                ProceedToNextGameStage(); // Аварийный переход
            }
        }
        return;
    }
    BotAIInstance->SetPersonalityFactors(BotPlayer.BotPersonality);

    int64 BotChosenAmount = 0; // Сумма, которую бот решит поставить (общая сумма для Bet/Raise)

    EPlayerAction ChosenAction = EPlayerAction::None; // Инициализируем

    // Если бот AllIn и не может больше делать ставки, его действие по сути "Check" или "None"
    // Однако, если он еще не поставил блайнд и у него All-In, то его действие - PostBlind (All-In).
    bool bBotIsEffectivelyAllInAndCannotActFurther =
        (BotPlayer.Status == EPlayerStatus::AllIn &&
            BotPlayer.Stack == 0 &&
            BotPlayer.CurrentBet >= BotActionContext.CurrentBetToCallOnTable && // Его ставка уже покрывает или равна текущей
            GameStateData->CurrentStage >= EGameStage::Preflop); // Не на стадии постановки блайндов

    if (bBotIsEffectivelyAllInAndCannotActFurther)
    {
        UE_LOG(LogTemp, Log, TEXT("TriggerBotDecision: Bot %s (Seat %d) is All-In and cannot act further. Treating as effective Check/Pass."),
            *BotPlayer.PlayerName, BotSeatIndex);
        ChosenAction = BotActionContext.AvailableActions.Contains(EPlayerAction::Check) ? EPlayerAction::Check : EPlayerAction::None;
        // Если Check не доступен (т.е. есть ставка для колла, которую он уже покрыл своим олл-ином),
        // то его действие фактически "None" для ProcessPlayerAction.
        // ProcessPlayerAction должен корректно обработать EPlayerAction::None для такого игрока (пропустить ход).
    }
    else if (BotActionContext.AvailableActions.Num() > 0) // Если есть доступные действия
    {
        ChosenAction = BotAIInstance->GetBestAction(
            GameStateData.Get(),
            BotPlayer,
            BotActionContext.AvailableActions,
            BotActionContext.CurrentBetToCallOnTable,
            BotActionContext.MinPureRaiseUI,
            BotChosenAmount
        );
    }
    else // Нет доступных действий, но он и не AllIn, который не может влиять (странная ситуация)
    {
        UE_LOG(LogTemp, Warning, TEXT("TriggerBotDecision: Bot %s (Seat %d) has NO available actions, but not clearly All-In/Done. Status: %s. Defaulting to Fold if possible."),
            *BotPlayer.PlayerName, BotSeatIndex, *UEnum::GetValueAsString(BotPlayer.Status));
        ChosenAction = EPlayerAction::Fold; // Безопасное действие по умолчанию в странной ситуации
    }


    UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) AI decided: %s with Amount (total bet): %lld"),
        *BotPlayer.PlayerName, BotSeatIndex, *UEnum::GetValueAsString(ChosenAction), BotChosenAmount);

    int64 AmountForProcessAction = 0;
    if (ChosenAction == EPlayerAction::Bet || ChosenAction == EPlayerAction::Raise)
    {
        AmountForProcessAction = BotChosenAmount; // Передаем ОБЩУЮ сумму ставки
    }
    // Для PostBlind, Call, Check, Fold, None - Amount = 0, т.к. ProcessPlayerAction сам рассчитает/проигнорирует.

    ProcessPlayerAction(BotSeatIndex, ChosenAction, AmountForProcessAction);
}

UPokerBotAI* UOfflineGameManager::GetBotAIInstance() const
{
    return BotAIInstance.Get();
}

UDeck* UOfflineGameManager::GetDeck() const
{
    return Deck.Get();
}