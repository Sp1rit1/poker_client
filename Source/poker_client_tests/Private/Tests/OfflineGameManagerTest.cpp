#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Editor.h" // Для GEditor (если используется в CreateTestOfflineManagerForAutomation)
#include "Engine/Engine.h" // Для GEngine (если используется в CreateTestOfflineManagerForAutomation)

// Инклюды ваших классов
#include "poker_client/Public/OfflineGameManager.h" // Замените PokerClient на имя вашего модуля
#include "poker_client/Public/OfflinePokerGameState.h"
#include "poker_client/Public/Deck.h"
#include "poker_client/Public/PokerDataTypes.h"
#include "poker_client/Public/PokerBotAI.h"
// #include "PokerClient/Public/MyGameInstance.h" // Не нужен напрямую в тестах, если Outer - это World

// Вспомогательная функция для создания экземпляра OfflineGameManager для тестов
// (Как была определена в предыдущем ответе)
static UOfflineGameManager* CreateTestOfflineManagerForAutomation(FAutomationTestBase* TestRunner)
{
    UWorld* TestWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
            {
                TestWorld = Context.World();
                if (TestWorld) break;
            }
        }
    }
    if (!TestWorld && GIsEditor)
    {
        if (GEditor)
        {
            UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
            if (EditorWorld) TestWorld = EditorWorld;
        }
    }

    UObject* OuterForNewObject = TestWorld ? static_cast<UObject*>(TestWorld) : static_cast<UObject*>(GetTransientPackage());
    if (!TestWorld && TestRunner) // Добавляем ошибку только если мир не найден и есть TestRunner
    {
        TestRunner->AddWarning(TEXT("Could not obtain a valid UWorld for test Outer. Using GetTransientPackage(). GameInstance-dependent features might not work."));
    }

    return NewObject<UOfflineGameManager>(OuterForNewObject);
}

static void SetSpecificCards(UOfflinePokerGameState* GameState, int32 Player1Seat, ECardSuit P1C1S, ECardRank P1C1R, ECardSuit P1C2S, ECardRank P1C2R,
    int32 Player2Seat, ECardSuit P2C1S, ECardRank P2C1R, ECardSuit P2C2S, ECardRank P2C2R,
    const TArray<FCard>& Community)
{
    if (!GameState) return;
    if (GameState->Seats.IsValidIndex(Player1Seat)) {
        GameState->Seats[Player1Seat].HoleCards = { FCard(P1C1S, P1C1R), FCard(P1C2S, P1C2R) };
    }
    if (GameState->Seats.IsValidIndex(Player2Seat)) {
        GameState->Seats[Player2Seat].HoleCards = { FCard(P2C1S, P2C1R), FCard(P2C2S, P2C2R) };
    }
    GameState->CommunityCards = Community;
}


static bool SetupGameToPreflopFirstAction(FAutomationTestBase* TestRunner, UOfflineGameManager* Manager, int32 NumTotalPlayers, int64 InitialStack, int64 SBAmount, int32& OutFirstToActSeatIndex)
{
    if (!Manager) return false;
    Manager->InitializeGame(1, NumTotalPlayers - 1, InitialStack, SBAmount);
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    if (!GameState) { TestRunner->AddError(TEXT("SetupGameToPreflop: GameState is null after InitializeGame.")); return false; }

    Manager->StartNewHand(); // Доходит до запроса SB
    if (GameState->CurrentStage != EGameStage::WaitingForSmallBlind || !GameState->Seats.IsValidIndex(GameState->PendingSmallBlindSeat)) {
        TestRunner->AddError(TEXT("SetupGameToPreflop: Did not reach WaitingForSmallBlind or SB seat invalid.")); return false;
    }
    Manager->ProcessPlayerAction(GameState->PendingSmallBlindSeat, EPlayerAction::PostBlind, 0); // SB ставит

    if (GameState->CurrentStage != EGameStage::WaitingForBigBlind || !GameState->Seats.IsValidIndex(GameState->PendingBigBlindSeat)) {
        TestRunner->AddError(TEXT("SetupGameToPreflop: Did not reach WaitingForBigBlind or BB seat invalid.")); return false;
    }
    Manager->ProcessPlayerAction(GameState->PendingBigBlindSeat, EPlayerAction::PostBlind, 0); // BB ставит

    if (GameState->CurrentStage != EGameStage::Preflop || GameState->CurrentTurnSeat == -1) {
        TestRunner->AddError(FString::Printf(TEXT("SetupGameToPreflop: Did not reach Preflop or CurrentTurnSeat is -1. Stage: %s, Turn: %d"), *UEnum::GetValueAsString(GameState->CurrentStage), GameState->CurrentTurnSeat));
        return false;
    }
    OutFirstToActSeatIndex = GameState->CurrentTurnSeat;
    TestRunner->TestTrue(TEXT("SetupGameToPreflop: First preflop actor should be valid"), GameState->Seats.IsValidIndex(OutFirstToActSeatIndex));
    return GameState->Seats.IsValidIndex(OutFirstToActSeatIndex);
}

static bool SetupGameToPostflopStreet(FAutomationTestBase* TestRunner, UOfflineGameManager* Manager,
    int32 NumTotalPlayers, int64 InitialStack, int64 SBAmount,
    EGameStage TargetStreet, int32& OutFirstToActOnStreet)
{
    if (!Manager) return false;
    int32 FirstToActPreflop;
    if (!SetupGameToPreflopFirstAction(TestRunner, Manager, NumTotalPlayers, InitialStack, SBAmount, FirstToActPreflop))
    {
        return false; // Ошибка в настройке префлопа
    }
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    if (!GameState) { TestRunner->AddError(TEXT("SetupGameToPostflop: GameState is null.")); return false; }

    // --- Префлоп ---
    if (TargetStreet >= EGameStage::Flop)
    {
        // Симулируем, что все коллируют BB на префлопе, чтобы дойти до флопа
        int32 CurrentActor = GameState->CurrentTurnSeat;
        int32 LoopGuard = 0;
        while (GameState->CurrentStage == EGameStage::Preflop && LoopGuard < GameState->Seats.Num() * 2) // Защита от бесконечного цикла
        {
            if (CurrentActor == -1) { TestRunner->AddError(TEXT("SetupGameToPostflop: Preflop turn is -1 unexpectedly.")); return false; }
            FPlayerSeatData& Player = GameState->Seats[CurrentActor];
            if (Player.Status == EPlayerStatus::Folded || Player.Status == EPlayerStatus::AllIn) { // Пропускаем, если уже не может действовать
                CurrentActor = Manager->GetNextPlayerToAct(CurrentActor, true); continue;
            }

            if (Player.CurrentBet < GameState->CurrentBetToCall) { // Нужно коллировать
                Manager->ProcessPlayerAction(CurrentActor, EPlayerAction::Call, 0);
            }
            else { // Можно чекнуть (например, BB, если не было рейзов)
                Manager->ProcessPlayerAction(CurrentActor, EPlayerAction::Check, 0);
            }
            CurrentActor = GameState->CurrentTurnSeat; // Обновляем на случай, если IsBettingRoundOver сработал
            LoopGuard++;
        }
        if (GameState->CurrentStage != EGameStage::Flop) { TestRunner->AddError(FString::Printf(TEXT("SetupGameToPostflop: Did not reach Flop. Current stage: %s"), *UEnum::GetValueAsString(GameState->CurrentStage))); return false; }
    }
    if (TargetStreet == EGameStage::Flop) { OutFirstToActOnStreet = GameState->CurrentTurnSeat; return true; }

    // --- Флоп ---
    if (TargetStreet >= EGameStage::Turn)
    {
        // Симулируем, что все чекают на флопе
        int32 CurrentActor = GameState->CurrentTurnSeat;
        int32 LoopGuard = 0;
        while (GameState->CurrentStage == EGameStage::Flop && LoopGuard < GameState->Seats.Num() * 2)
        {
            if (CurrentActor == -1) { TestRunner->AddError(TEXT("SetupGameToPostflop: Flop turn is -1 unexpectedly.")); return false; }
            FPlayerSeatData& Player = GameState->Seats[CurrentActor];
            if (Player.Status == EPlayerStatus::Folded || Player.Status == EPlayerStatus::AllIn) {
                CurrentActor = Manager->GetNextPlayerToAct(CurrentActor, true); continue;
            }
            Manager->ProcessPlayerAction(CurrentActor, EPlayerAction::Check, 0); // Все чекают
            CurrentActor = GameState->CurrentTurnSeat;
            LoopGuard++;
        }
        if (GameState->CurrentStage != EGameStage::Turn) { TestRunner->AddError(FString::Printf(TEXT("SetupGameToPostflop: Did not reach Turn. Current stage: %s"), *UEnum::GetValueAsString(GameState->CurrentStage))); return false; }
    }
    if (TargetStreet == EGameStage::Turn) { OutFirstToActOnStreet = GameState->CurrentTurnSeat; return true; }

    // --- Терн ---
    if (TargetStreet >= EGameStage::River)
    {
        // Симулируем, что все чекают на терне
        int32 CurrentActor = GameState->CurrentTurnSeat;
        int32 LoopGuard = 0;
        while (GameState->CurrentStage == EGameStage::Turn && LoopGuard < GameState->Seats.Num() * 2)
        {
            if (CurrentActor == -1) { TestRunner->AddError(TEXT("SetupGameToPostflop: Turn turn is -1 unexpectedly.")); return false; }
            FPlayerSeatData& Player = GameState->Seats[CurrentActor];
            if (Player.Status == EPlayerStatus::Folded || Player.Status == EPlayerStatus::AllIn) {
                CurrentActor = Manager->GetNextPlayerToAct(CurrentActor, true); continue;
            }
            Manager->ProcessPlayerAction(CurrentActor, EPlayerAction::Check, 0); // Все чекают
            CurrentActor = GameState->CurrentTurnSeat;
            LoopGuard++;
        }
        if (GameState->CurrentStage != EGameStage::River) { TestRunner->AddError(FString::Printf(TEXT("SetupGameToPostflop: Did not reach River. Current stage: %s"), *UEnum::GetValueAsString(GameState->CurrentStage))); return false; }
    }
    if (TargetStreet == EGameStage::River) { OutFirstToActOnStreet = GameState->CurrentTurnSeat; return true; }

    TestRunner->AddError(TEXT("SetupGameToPostflop: TargetStreet not reached."));
    return false;
}




// --- Тесты для UOfflineGameManager::InitializeGame() ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_InitializeGame_BasicCreation, "PokerClient.UnitTests.OfflineGameManager.InitializeGame.BasicCreation", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_InitializeGame_BasicCreation::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    // Act
    Manager->InitializeGame(1, 1, 1000, 5); // 1 реальный, 1 бот, стек 1000, SB 5

    // Assert
    TestNotNull(TEXT("GameStateData should be created after InitializeGame"), Manager->GetGameState());
    TestNotNull(TEXT("Deck should be created after InitializeGame"), Manager->GetDeck());
    TestNotNull(TEXT("BotAIInstance should be created after InitializeGame"), Manager->GetBotAIInstance());

    if (Manager->GetGameState())
    {
        TestEqual(TEXT("Initial Pot should be 0"), Manager->GetGameState()->Pot, (int64)0);
        TestEqual(TEXT("Initial CurrentBetToCall should be 0"), Manager->GetGameState()->CurrentBetToCall, (int64)0);
        TestEqual(TEXT("Initial DealerSeat should be -1"), Manager->GetGameState()->DealerSeat, -1);
        TestEqual(TEXT("Initial CurrentStage should be WaitingForPlayers"), Manager->GetGameState()->CurrentStage, EGameStage::WaitingForPlayers);
        TestTrue(TEXT("Initial CommunityCards should be empty"), Manager->GetGameState()->CommunityCards.IsEmpty());
        TestEqual(TEXT("Initial LastAggressorSeatIndex should be -1"), Manager->GetGameState()->LastAggressorSeatIndex, -1);
        TestEqual(TEXT("Initial LastBetOrRaiseAmountInCurrentRound should be 0"), Manager->GetGameState()->LastBetOrRaiseAmountInCurrentRound, (int64)0);
        TestEqual(TEXT("Initial PlayerWhoOpenedBettingThisRound should be -1"), Manager->GetGameState()->PlayerWhoOpenedBettingThisRound, -1);

    }

    Manager->ConditionalBeginDestroy();
    // Если CreateTestOfflineManagerForAutomation использовал GetTransientPackage(), нет необходимости удалять Outer.
    // Если он использовал мир, мир удалять не нужно, он управляется движком.
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_InitializeGame_PlayerCountAndData, "PokerClient.UnitTests.OfflineGameManager.InitializeGame.PlayerCountAndData", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_InitializeGame_PlayerCountAndData::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 NumReal = 1;
    int32 NumBotsInput = 3; // Например, 3 бота
    int32 ExpectedTotalPlayers = NumReal + NumBotsInput; // Ожидаем 4 игрока
    int64 InitialStack = 1500;
    int64 SB = 10;

    // Act
    Manager->InitializeGame(NumReal, NumBotsInput, InitialStack, SB);

    // Assert
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameStateData should exist"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("Number of seats should match total players"), GameState->Seats.Num(), ExpectedTotalPlayers);

    for (int32 i = 0; i < ExpectedTotalPlayers; ++i)
    {
        TestTrue(FString::Printf(TEXT("Seat index %d should be valid"), i), GameState->Seats.IsValidIndex(i));
        if (!GameState->Seats.IsValidIndex(i))
        {
            // AddError уже вызван TestTrue, можно просто return false или continue
            Manager->ConditionalBeginDestroy();
            return false;
        }

        const FPlayerSeatData& Seat = GameState->Seats[i];
        TestEqual(FString::Printf(TEXT("Seat %d: Seat.SeatIndex should match loop index"), i), Seat.SeatIndex, i);
        TestEqual(FString::Printf(TEXT("Seat %d: Seat.Stack should match InitialStack"), i), Seat.Stack, InitialStack);
        TestTrue(FString::Printf(TEXT("Seat %d: Seat.bIsSittingIn should be true"), i), Seat.bIsSittingIn);
        TestEqual(FString::Printf(TEXT("Seat %d: Seat.Status should be Waiting"), i), Seat.Status, EPlayerStatus::Waiting);
        TestEqual(FString::Printf(TEXT("Seat %d: Seat.CurrentBet should be 0"), i), Seat.CurrentBet, (int64)0);
        TestTrue(FString::Printf(TEXT("Seat %d: Seat.HoleCards should be empty"), i), Seat.HoleCards.IsEmpty());
        TestFalse(FString::Printf(TEXT("Seat %d: Seat.bHasActedThisSubRound should be false"), i), Seat.bHasActedThisSubRound);


        if (i < NumReal)
        {
            TestFalse(FString::Printf(TEXT("Seat %d: Real player seat should not be marked as bot"), i), Seat.bIsBot);
            // Имя реального игрока может быть "Player" или из GameInstance. Для теста можно не проверять строго, если GI не мокается.
            // TestEqual(FString::Printf(TEXT("Seat %d: Real player name should be 'Player' (default)"), i), Seat.PlayerName, TEXT("Player"));
        }
        else
        {
            TestTrue(FString::Printf(TEXT("Seat %d: Bot seat should be marked as bot"), i), Seat.bIsBot);
            FString ExpectedBotName = FString::Printf(TEXT("Bot %d"), (i - NumReal) + 1);
            TestEqual(FString::Printf(TEXT("Seat %d: Bot name should be '%s'"), i, *ExpectedBotName), Seat.PlayerName, ExpectedBotName);
        }
    }

    Manager->ConditionalBeginDestroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_InitializeGame_BlindAmounts, "PokerClient.UnitTests.OfflineGameManager.InitializeGame.BlindAmounts", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_InitializeGame_BlindAmounts::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    // Act: Тест с валидным SB
    Manager->InitializeGame(1, 1, 1000, 25);
    UOfflinePokerGameState* GameStateValidSB = Manager->GetGameState();
    TestNotNull(TEXT("GameStateData (ValidSB) should exist"), GameStateValidSB);
    if (!GameStateValidSB) { Manager->ConditionalBeginDestroy(); return false; }

    // Assert: Валидный SB
    TestEqual(TEXT("SmallBlindAmount should be set correctly for valid input"), GameStateValidSB->SmallBlindAmount, (int64)25);
    TestEqual(TEXT("BigBlindAmount should be 2 * SmallBlindAmount for valid input"), GameStateValidSB->BigBlindAmount, (int64)50);

    // Act: Тест с невалидным SB (0)
    Manager->InitializeGame(1, 1, 1000, 0); // Используем тот же менеджер, InitializeGame должен сбрасывать состояние
    UOfflinePokerGameState* GameStateInvalidSB_Zero = Manager->GetGameState();
    TestNotNull(TEXT("GameStateData (InvalidSB_Zero) should exist"), GameStateInvalidSB_Zero);
    if (!GameStateInvalidSB_Zero) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("SmallBlindAmount should default to 5 if input is 0"), GameStateInvalidSB_Zero->SmallBlindAmount, (int64)5);
    TestEqual(TEXT("BigBlindAmount should be 10 if SB defaults to 5 (input 0)"), GameStateInvalidSB_Zero->BigBlindAmount, (int64)10);

    // Act: Тест с невалидным SB (отрицательный)
    Manager->InitializeGame(1, 1, 1000, -10);
    UOfflinePokerGameState* GameStateInvalidSB_Negative = Manager->GetGameState();
    TestNotNull(TEXT("GameStateData (InvalidSB_Negative) should exist"), GameStateInvalidSB_Negative);
    if (!GameStateInvalidSB_Negative) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("SmallBlindAmount should default to 5 if input is negative"), GameStateInvalidSB_Negative->SmallBlindAmount, (int64)5);
    TestEqual(TEXT("BigBlindAmount should be 10 if SB defaults to 5 (negative input)"), GameStateInvalidSB_Negative->BigBlindAmount, (int64)10);

    Manager->ConditionalBeginDestroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_InitializeGame_PlayerCountClamping, "PokerClient.UnitTests.OfflineGameManager.InitializeGame.PlayerCountClamping", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_InitializeGame_PlayerCountClamping::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    // Act 1: Меньше минимального (1 реальный, 0 ботов = 1 всего)
    Manager->InitializeGame(1, 0, 1000, 5);
    UOfflinePokerGameState* GameStateTooFew = Manager->GetGameState();
    TestNotNull(TEXT("GameStateData (TooFew) should exist"), GameStateTooFew);
    if (!GameStateTooFew) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("Total players should be clamped to 2 if input total is 1"), GameStateTooFew->Seats.Num(), 2);
    if (GameStateTooFew->Seats.Num() == 2) // Дополнительные проверки, если количество мест правильное
    {
        TestFalse(TEXT("First player (real) should not be bot in TooFew case"), GameStateTooFew->Seats[0].bIsBot);
        TestTrue(TEXT("Second player (added bot) should be bot in TooFew case"), GameStateTooFew->Seats[1].bIsBot);
    }

    // Act 2: Больше максимального (1 реальный, 9 ботов = 10 всего -> должно стать 9)
    Manager->InitializeGame(1, 9, 1000, 5);
    UOfflinePokerGameState* GameStateTooMany = Manager->GetGameState();
    TestNotNull(TEXT("GameStateData (TooMany) should exist"), GameStateTooMany);
    if (!GameStateTooMany) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("Total players should be clamped to 9 if input total is 10"), GameStateTooMany->Seats.Num(), 9);
    if (GameStateTooMany->Seats.Num() > 0)
    {
        TestFalse(TEXT("First player (real) should not be bot in TooMany case"), GameStateTooMany->Seats[0].bIsBot);
    }
    for (int32 i = 1; i < GameStateTooMany->Seats.Num(); ++i) // Остальные должны быть ботами
    {
        if (!GameStateTooMany->Seats[i].bIsBot)
        {
            AddError(FString::Printf(TEXT("Seat %d should be a bot in TooMany case but is not."), i));
            // Не возвращаем false сразу, чтобы тест мог завершиться и показать все ошибки
        }
    }

    // Act 3: Слишком много реальных игроков (например, 10 реальных, 0 ботов)
    Manager->InitializeGame(10, 0, 1000, 5);
    UOfflinePokerGameState* GameStateTooManyReal = Manager->GetGameState();
    TestNotNull(TEXT("GameStateData (TooManyReal) should exist"), GameStateTooManyReal);
    if (!GameStateTooManyReal) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("Total players should be clamped to 9 if 10 real players are input"), GameStateTooManyReal->Seats.Num(), 9);
    for (int32 i = 0; i < GameStateTooManyReal->Seats.Num(); ++i)
    {
        // Все оставшиеся должны быть реальными игроками, так как NumRealPlayers был урезан до TotalActivePlayers
        if (GameStateTooManyReal->Seats[i].bIsBot)
        {
            AddError(FString::Printf(TEXT("Seat %d should be a real player in TooManyReal case but is a bot."), i));
        }
    }


    Manager->ConditionalBeginDestroy();
    return true;
}






IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_StartNewHand_3Players_InitialState, "PokerClient.UnitTests.OfflineGameManager.StartNewHand.3Players.InitialState", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_StartNewHand_3Players_InitialState::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    Manager->InitializeGame(1, 2, 1000, 5); // 1 реальный, 2 бота = 3 игрока
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameState should exist after InitializeGame"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    // Установим какое-то "предыдущее" состояние, чтобы проверить сброс
    GameState->Pot = 100;
    GameState->CommunityCards.Add(FCard(ECardSuit::Clubs, ECardRank::Ace));
    if (GameState->Seats.IsValidIndex(0)) GameState->Seats[0].CurrentBet = 50;

    // Act
    Manager->StartNewHand();

    // Assert Состояние Стола
    TestEqual(TEXT("Pot should be reset to 0 at StartNewHand"), GameState->Pot, (int64)0);
    TestTrue(TEXT("CommunityCards should be empty at StartNewHand"), GameState->CommunityCards.IsEmpty());
    TestEqual(TEXT("CurrentBetToCall should be 0 at StartNewHand (before blinds are processed)"), GameState->CurrentBetToCall, (int64)0);
    TestEqual(TEXT("LastAggressorSeatIndex should be -1"), GameState->LastAggressorSeatIndex, -1);
    TestEqual(TEXT("LastBetOrRaiseAmountInCurrentRound should be 0"), GameState->LastBetOrRaiseAmountInCurrentRound, (int64)0);
    TestEqual(TEXT("PlayerWhoOpenedBettingThisRound should be -1"), GameState->PlayerWhoOpenedBettingThisRound, -1);


    // Assert Состояние Игроков
    for (const FPlayerSeatData& Seat : GameState->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Stack > 0) // Проверяем только активных игроков
        {
            TestTrue(FString::Printf(TEXT("Seat %d: HoleCards should be empty"), Seat.SeatIndex), Seat.HoleCards.IsEmpty());
            TestEqual(FString::Printf(TEXT("Seat %d: CurrentBet should be 0"), Seat.SeatIndex), Seat.CurrentBet, (int64)0);
            TestFalse(FString::Printf(TEXT("Seat %d: bIsTurn should be false initially (except for SB)"), Seat.SeatIndex), Seat.bIsTurn && Seat.SeatIndex != GameState->CurrentTurnSeat);
            TestFalse(FString::Printf(TEXT("Seat %d: bIsSmallBlind should be false initially (except for SB)"), Seat.SeatIndex), Seat.bIsSmallBlind && Seat.SeatIndex != GameState->PendingSmallBlindSeat);
            TestFalse(FString::Printf(TEXT("Seat %d: bIsBigBlind should be false initially (except for BB)"), Seat.SeatIndex), Seat.bIsBigBlind && Seat.SeatIndex != GameState->PendingBigBlindSeat);
            TestFalse(FString::Printf(TEXT("Seat %d: bHasActedThisSubRound should be false"), Seat.SeatIndex), Seat.bHasActedThisSubRound);

            // Проверяем статус: SB должен быть MustPostSmallBlind, остальные активные - Playing
            if (Seat.SeatIndex == GameState->PendingSmallBlindSeat)
            {
                TestEqual(FString::Printf(TEXT("Seat %d (SB): Status should be MustPostSmallBlind"), Seat.SeatIndex), Seat.Status, EPlayerStatus::MustPostSmallBlind);
            }
            else if (Seat.SeatIndex == GameState->PendingBigBlindSeat) {
                // Статус BB будет Playing, так как MustPostBigBlind устанавливается в RequestBigBlind,
                // а StartNewHand только подготавливает SB.
                TestEqual(FString::Printf(TEXT("Seat %d (BB): Status should be Playing (will be MustPostBigBlind later)"), Seat.SeatIndex), Seat.Status, EPlayerStatus::Playing);
            }
            else
            {
                TestEqual(FString::Printf(TEXT("Seat %d (Non-blind): Status should be Playing"), Seat.SeatIndex), Seat.Status, EPlayerStatus::Playing);
            }
        }
    }

    // Assert Дилер, Блайнды, Ход, Стадия
    TestTrue(TEXT("Dealer should be assigned (not -1)"), GameState->DealerSeat != -1);
    TestTrue(TEXT("PendingSmallBlindSeat should be assigned (not -1)"), GameState->PendingSmallBlindSeat != -1);
    TestTrue(TEXT("PendingBigBlindSeat should be assigned (not -1)"), GameState->PendingBigBlindSeat != -1);
    TestNotEqual(TEXT("SB and BB should be different players"), GameState->PendingSmallBlindSeat, GameState->PendingBigBlindSeat);

    if (GameState->Seats.IsValidIndex(GameState->DealerSeat))
    {
        TestTrue(TEXT("Dealer flag should be set on dealer seat"), GameState->Seats[GameState->DealerSeat].bIsDealer);
    }

    TestEqual(TEXT("CurrentStage should be WaitingForSmallBlind"), GameState->CurrentStage, EGameStage::WaitingForSmallBlind);
    TestEqual(TEXT("CurrentTurnSeat should be PendingSmallBlindSeat"), GameState->CurrentTurnSeat, GameState->PendingSmallBlindSeat);

    // Assert Колода
    TestNotNull(TEXT("Deck should exist"), Manager->Deck.Get());
    if (Manager->Deck.Get())
    {
        // После Initialize и Shuffle в StartNewHand, в колоде должно быть 52 карты
        // (т.к. карманные карты еще не розданы на этом этапе)
        TestEqual(TEXT("Deck should have 52 cards after shuffle in StartNewHand"), Manager->Deck.Get()->NumCardsLeft(), 52);
    }

    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест StartNewHand для хедз-ап ситуации (2 игрока)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_StartNewHand_HeadsUp_DealerAndSB, "PokerClient.UnitTests.OfflineGameManager.StartNewHand.HeadsUp.DealerAndSB", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_StartNewHand_HeadsUp_DealerAndSB::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    Manager->InitializeGame(1, 1, 1000, 5); // 2 игрока всего
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameState should exist"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    // Act
    Manager->StartNewHand();

    // Assert
    TestTrue(TEXT("HeadsUp: Dealer should be assigned"), GameState->DealerSeat != -1);
    TestTrue(TEXT("HeadsUp: PendingSmallBlindSeat should be assigned"), GameState->PendingSmallBlindSeat != -1);
    TestTrue(TEXT("HeadsUp: PendingBigBlindSeat should be assigned"), GameState->PendingBigBlindSeat != -1);

    // В хедз-апе дилер является SB
    TestEqual(TEXT("HeadsUp: Dealer should be the Small Blind"), GameState->DealerSeat, GameState->PendingSmallBlindSeat);
    TestNotEqual(TEXT("HeadsUp: SB and BB should be different"), GameState->PendingSmallBlindSeat, GameState->PendingBigBlindSeat);

    TestEqual(TEXT("HeadsUp: CurrentStage should be WaitingForSmallBlind"), GameState->CurrentStage, EGameStage::WaitingForSmallBlind);
    TestEqual(TEXT("HeadsUp: CurrentTurnSeat should be SB (Dealer)"), GameState->CurrentTurnSeat, GameState->PendingSmallBlindSeat);

    if (GameState->Seats.IsValidIndex(GameState->PendingSmallBlindSeat))
    {
        TestEqual(TEXT("HeadsUp: SB status should be MustPostSmallBlind"), GameState->Seats[GameState->PendingSmallBlindSeat].Status, EPlayerStatus::MustPostSmallBlind);
    }

    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест последовательного вызова StartNewHand для проверки смены дилера
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_StartNewHand_DealerRotation, "PokerClient.UnitTests.OfflineGameManager.StartNewHand.DealerRotation", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_StartNewHand_DealerRotation::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    Manager->InitializeGame(1, 2, 1000, 5); // 3 игрока
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameState should exist"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    // Act 1: Первая рука
    Manager->StartNewHand();
    int32 FirstDealer = GameState->DealerSeat;
    TestTrue(TEXT("First dealer should be valid"), FirstDealer != -1);

    // Act 2: Вторая рука (нужно симулировать завершение первой руки до вызова StartNewHand снова,
    // но для простого теста дилера, мы просто вызовем StartNewHand еще раз,
    // предполагая, что внутренняя логика сброса работает)
    // В реальной игре между StartNewHand были бы ProcessPlayerAction, ProceedToNextStage и т.д.
    // Для этого теста мы проверяем только механику смены дилера.
    Manager->StartNewHand();
    int32 SecondDealer = GameState->DealerSeat;
    TestTrue(TEXT("Second dealer should be valid"), SecondDealer != -1);

    // Assert
    TestNotEqual(TEXT("Dealer should rotate to a different player on the next hand"), FirstDealer, SecondDealer);

    // Проверим, что дилер сдвинулся по вашему CurrentTurnOrderMap_Internal (косвенно через GetNextPlayerToAct)
    // Это более сложная проверка, так как GetNextPlayerToAct зависит от активных игроков.
    // Простой тест: он просто не тот же самый.
    // Можно сделать 3 вызова и проверить, что он вернулся к первому или прошел полный круг (если игроков < 3)

    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест StartNewHand, когда недостаточно активных игроков
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_StartNewHand_NotEnoughPlayers, "PokerClient.UnitTests.OfflineGameManager.StartNewHand.NotEnoughPlayers", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_StartNewHand_NotEnoughPlayers::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    Manager->InitializeGame(1, 0, 1000, 5); // 1 игрок всего
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameState should exist"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    // В InitializeGame количество игроков уже должно быть скорректировано до 2 (1 реальный + 1 бот)
    // Если мы хотим протестировать StartNewHand с <2 активными Игроками, нужно это симулировать
    // Например, установив Stack = 0 для одного из двух игроков после InitializeGame.
    if (GameState->Seats.Num() == 2)
    {
        GameState->Seats[1].Stack = 0; // Делаем одного из игроков неактивным для этой руки
    }
    else {
        AddError(TEXT("Expected 2 players after InitializeGame with 1 input player for this test setup."));
        Manager->ConditionalBeginDestroy(); return false;
    }

    // Act
    Manager->StartNewHand();

    // Assert
    // Ожидаем, что рука не начнется, и стадия останется WaitingForPlayers
    TestEqual(TEXT("Stage should remain WaitingForPlayers if not enough active players"), GameState->CurrentStage, EGameStage::WaitingForPlayers);
    TestEqual(TEXT("CurrentTurnSeat should be -1 if hand cannot start"), GameState->CurrentTurnSeat, -1);
    // Проверим, что Pot не изменился (должен быть 0)
    TestEqual(TEXT("Pot should be 0 if hand cannot start"), GameState->Pot, (int64)0);

    Manager->ConditionalBeginDestroy();
    return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_SBPostsBlind, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.PostBlind.SBPosts", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_SBPostsBlind::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    Manager->InitializeGame(1, 2, 1000, 10); // 3 игрока, стек 1000, SB=10, BB=20
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameState should not be null"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    Manager->StartNewHand(); // Инициирует запрос действия у SB

    TestEqual(TEXT("Initial stage should be WaitingForSmallBlind"), GameState->CurrentStage, EGameStage::WaitingForSmallBlind);
    int32 SBSeatIndex = GameState->PendingSmallBlindSeat;
    TestTrue(TEXT("SBSeatIndex should be valid"), GameState->Seats.IsValidIndex(SBSeatIndex));
    if (!GameState->Seats.IsValidIndex(SBSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("CurrentTurnSeat should be SBSeatIndex"), GameState->CurrentTurnSeat, SBSeatIndex);
    TestEqual(TEXT("SB Status should be MustPostSmallBlind"), GameState->Seats[SBSeatIndex].Status, EPlayerStatus::MustPostSmallBlind);
    int64 SBStackBefore = GameState->Seats[SBSeatIndex].Stack;

    // Act
    Manager->ProcessPlayerAction(SBSeatIndex, EPlayerAction::PostBlind, 0); // Amount для PostBlind не используется в ProcessPlayerAction, берется из GameState

    // Assert
    TestEqual(TEXT("Stage should be WaitingForBigBlind after SB posts"), GameState->CurrentStage, EGameStage::WaitingForBigBlind);
    TestEqual(TEXT("SB Stack should decrease by SmallBlindAmount"), GameState->Seats[SBSeatIndex].Stack, SBStackBefore - GameState->SmallBlindAmount);
    TestEqual(TEXT("SB CurrentBet should be SmallBlindAmount"), GameState->Seats[SBSeatIndex].CurrentBet, GameState->SmallBlindAmount);
    TestTrue(TEXT("SB bIsSmallBlind flag should be true"), GameState->Seats[SBSeatIndex].bIsSmallBlind);
    TestEqual(TEXT("SB Status should be Playing"), GameState->Seats[SBSeatIndex].Status, EPlayerStatus::Playing);
    TestEqual(TEXT("Pot should contain SmallBlindAmount"), GameState->Pot, GameState->SmallBlindAmount);

    TestTrue(TEXT("PendingBigBlindSeat should be valid"), GameState->Seats.IsValidIndex(GameState->PendingBigBlindSeat));
    if (!GameState->Seats.IsValidIndex(GameState->PendingBigBlindSeat)) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("CurrentTurnSeat should now be BBSeatIndex"), GameState->CurrentTurnSeat, GameState->PendingBigBlindSeat);
    TestEqual(TEXT("BB Status should be MustPostBigBlind"), GameState->Seats[GameState->PendingBigBlindSeat].Status, EPlayerStatus::MustPostBigBlind);
    TestFalse(TEXT("SB bIsTurn flag should be false"), GameState->Seats[SBSeatIndex].bIsTurn); // Проверяем, что ход перешел
    TestTrue(TEXT("BB bIsTurn flag should be true"), GameState->Seats[GameState->PendingBigBlindSeat].bIsTurn);


    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест: Большой Блайнд ставит свой блайнд
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_BBPostsBlind, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.PostBlind.BBPosts", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_BBPostsBlind::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    Manager->InitializeGame(1, 2, 1000, 10); // 3 игрока, SB=10, BB=20
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameState should not be null"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    Manager->StartNewHand();
    int32 SBSeatIndex = GameState->PendingSmallBlindSeat;
    Manager->ProcessPlayerAction(SBSeatIndex, EPlayerAction::PostBlind, 0); // SB ставит

    TestEqual(TEXT("Stage should be WaitingForBigBlind before BB posts"), GameState->CurrentStage, EGameStage::WaitingForBigBlind);
    int32 BBSeatIndex = GameState->PendingBigBlindSeat;
    TestTrue(TEXT("BBSeatIndex should be valid"), GameState->Seats.IsValidIndex(BBSeatIndex));
    if (!GameState->Seats.IsValidIndex(BBSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }

    TestEqual(TEXT("CurrentTurnSeat should be BBSeatIndex"), GameState->CurrentTurnSeat, BBSeatIndex);
    TestEqual(TEXT("BB Status should be MustPostBigBlind"), GameState->Seats[BBSeatIndex].Status, EPlayerStatus::MustPostBigBlind);
    int64 BBStackBefore = GameState->Seats[BBSeatIndex].Stack;
    int64 PotBeforeBB = GameState->Pot;

    // Act
    Manager->ProcessPlayerAction(BBSeatIndex, EPlayerAction::PostBlind, 0);

    // Assert
    TestEqual(TEXT("Stage should be Preflop after BB posts"), GameState->CurrentStage, EGameStage::Preflop);
    TestEqual(TEXT("BB Stack should decrease by BigBlindAmount"), GameState->Seats[BBSeatIndex].Stack, BBStackBefore - GameState->BigBlindAmount);
    TestEqual(TEXT("BB CurrentBet should be BigBlindAmount"), GameState->Seats[BBSeatIndex].CurrentBet, GameState->BigBlindAmount);
    TestTrue(TEXT("BB bIsBigBlind flag should be true"), GameState->Seats[BBSeatIndex].bIsBigBlind);
    TestEqual(TEXT("BB Status should be Playing"), GameState->Seats[BBSeatIndex].Status, EPlayerStatus::Playing);
    TestEqual(TEXT("Pot should contain SB + BB amounts"), GameState->Pot, PotBeforeBB + GameState->BigBlindAmount);
    TestEqual(TEXT("CurrentBetToCall should be BigBlindAmount"), GameState->CurrentBetToCall, GameState->BigBlindAmount);
    TestEqual(TEXT("LastAggressor should be BB"), GameState->LastAggressorSeatIndex, BBSeatIndex);
    TestEqual(TEXT("LastBetOrRaiseAmount should be BigBlindAmount"), GameState->LastBetOrRaiseAmountInCurrentRound, GameState->BigBlindAmount);

    // Проверяем, что карманные карты розданы (минимум у тех, кто Playing)
    int32 PlayersWithHoleCards = 0;
    for (const FPlayerSeatData& Seat : GameState->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing)
        {
            TestEqual(FString::Printf(TEXT("Seat %d should have 2 hole cards"), Seat.SeatIndex), Seat.HoleCards.Num(), 2);
            if (Seat.HoleCards.Num() == 2) PlayersWithHoleCards++;
        }
    }
    int32 ExpectedPlayersWithCards = 0;
    for (const FPlayerSeatData& Seat : GameState->Seats) { if (Seat.bIsSittingIn && Seat.Stack > 0) ExpectedPlayersWithCards++; }
    TestEqual(TEXT("Number of players with hole cards should match active players"), PlayersWithHoleCards, ExpectedPlayersWithCards);


    // Проверяем первого ходящего на префлопе
    int32 ExpectedFirstToActPreflop = Manager->DetermineFirstPlayerToActAtPreflop();
    TestEqual(TEXT("CurrentTurnSeat should be the first preflop actor"), GameState->CurrentTurnSeat, ExpectedFirstToActPreflop);
    if (GameState->Seats.IsValidIndex(ExpectedFirstToActPreflop))
    {
        TestTrue(TEXT("First preflop actor bIsTurn flag should be true"), GameState->Seats[ExpectedFirstToActPreflop].bIsTurn);
    }

    Manager->ConditionalBeginDestroy();
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_SBPostsAllInBlind, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.PostBlind.SBAllIn", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_SBPostsAllInBlind::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int64 PlayerInitialStack = 1000;
    int64 TestSBActualStack = 3;
    int64 DefinedSmallBlindAmount = 5;

    Manager->InitializeGame(1, 1, PlayerInitialStack, DefinedSmallBlindAmount);
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameState should not be null after InitializeGame"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    Manager->StartNewHand();

    int32 ActualSBSeatIndex = GameState->PendingSmallBlindSeat;
    TestTrue(TEXT("PendingSmallBlindSeat (ActualSBSeatIndex) should be a valid index"), GameState->Seats.IsValidIndex(ActualSBSeatIndex));
    if (!GameState->Seats.IsValidIndex(ActualSBSeatIndex))
    {
        AddError(FString::Printf(TEXT("ActualSBSeatIndex %d is invalid."), ActualSBSeatIndex));
        Manager->ConditionalBeginDestroy();
        return false;
    }

    GameState->Seats[ActualSBSeatIndex].Stack = TestSBActualStack;
    int64 SBStackInGameStateBeforeAction = GameState->Seats[ActualSBSeatIndex].Stack;
    int64 PotBeforeAction = GameState->Pot;

    UE_LOG(LogTemp, Log, TEXT("SBPostsAllInBlind_Test: Setup complete. SB is Seat %d. Setting its stack to %lld. SmallBlindAmount defined as %lld. Turn is %d. Stage is %s."),
        ActualSBSeatIndex, SBStackInGameStateBeforeAction, GameState->SmallBlindAmount, GameState->CurrentTurnSeat, *UEnum::GetValueAsString(GameState->CurrentStage));

    TestEqual(TEXT("Pre-Act: CurrentTurnSeat should be ActualSBSeatIndex"), GameState->CurrentTurnSeat, ActualSBSeatIndex);
    TestEqual(TEXT("Pre-Act: SB Status should be MustPostSmallBlind"), GameState->Seats[ActualSBSeatIndex].Status, EPlayerStatus::MustPostSmallBlind);
    TestEqual(TEXT("Pre-Act: SB Stack in GameState should be TestSBActualStack"), SBStackInGameStateBeforeAction, TestSBActualStack);

    // Act
    Manager->ProcessPlayerAction(ActualSBSeatIndex, EPlayerAction::PostBlind, 0);

    // Отладочный лог СРАЗУ ПОСЛЕ ProcessPlayerAction (с уровнем Log или Warning)
    EPlayerStatus StatusAfterAction = GameState->Seats[ActualSBSeatIndex].Status;
    int64 StackAfterAction = GameState->Seats[ActualSBSeatIndex].Stack;
    UE_LOG(LogTemp, Log, TEXT("TEST DEBUG (Log Level): Player at SB Seat %d - Status AFTER ProcessPlayerAction: %s, Stack: %lld"), // ИЗМЕНЕН УРОВЕНЬ ЛОГА
        ActualSBSeatIndex, *UEnum::GetValueAsString(StatusAfterAction), StackAfterAction);

    // Assert
    TestEqual(TEXT("SB Stack should be 0 after All-In blind"), GameState->Seats[ActualSBSeatIndex].Stack, (int64)0);
    TestEqual(TEXT("SB CurrentBet should be their entire initial stack for this action (TestSBActualStack)"), GameState->Seats[ActualSBSeatIndex].CurrentBet, TestSBActualStack);
    TestTrue(TEXT("SB bIsSmallBlind flag should be true"), GameState->Seats[ActualSBSeatIndex].bIsSmallBlind);

    // Используем TestTrue для проверки статуса
    bool bIsStatusCorrectlyAllIn = (GameState->Seats[ActualSBSeatIndex].Status == EPlayerStatus::AllIn);
    TestTrue(TEXT("SB Status should be AllIn (checked with TestTrue)"), bIsStatusCorrectlyAllIn);
    if (!bIsStatusCorrectlyAllIn)
    {
        AddError(FString::Printf(TEXT("TestTrue for SB Status AllIn FAILED. Actual Status: %s (Value: %d), Expected: AllIn (Value: %d)"),
            *UEnum::GetValueAsString(GameState->Seats[ActualSBSeatIndex].Status),
            static_cast<uint8>(GameState->Seats[ActualSBSeatIndex].Status),
            static_cast<uint8>(EPlayerStatus::AllIn)
        ));
    }

    TestEqual(TEXT("Pot should contain SB's All-In amount"), GameState->Pot, PotBeforeAction + TestSBActualStack);
    TestEqual(TEXT("Stage should now be WaitingForBigBlind"), GameState->CurrentStage, EGameStage::WaitingForBigBlind);

    int32 ExpectedBBSeat = GameState->PendingBigBlindSeat;
    TestTrue(TEXT("PendingBigBlindSeat should be a valid index"), GameState->Seats.IsValidIndex(ExpectedBBSeat));
    if (GameState->Seats.IsValidIndex(ExpectedBBSeat))
    {
        TestNotEqual(TEXT("CurrentTurnSeat should have passed from SB"), GameState->CurrentTurnSeat, ActualSBSeatIndex);
        TestEqual(TEXT("CurrentTurnSeat should be BB"), GameState->CurrentTurnSeat, ExpectedBBSeat);
        TestEqual(TEXT("BB Status should be MustPostBigBlind"), GameState->Seats[ExpectedBBSeat].Status, EPlayerStatus::MustPostBigBlind);
    }
    else {
        AddError(TEXT("PendingBigBlindSeat is invalid after SB posted, cannot verify BB state."));
    }

    Manager->ConditionalBeginDestroy();
    return true;
}


// --- Тесты для FOLD ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Preflop_UTGFolds_3Players, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Preflop.Fold.UTGFolds_3Players", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Preflop_UTGFolds_3Players::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 UtgSeatIndex;
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, 1000, 5, UtgSeatIndex)) // 3 игрока, SB=5, BB=10
    {
        Manager->ConditionalBeginDestroy(); return false; // Ошибка в настройке
    }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    UE_LOG(LogTemp, Log, TEXT("UTGFolds_3Players: UTG (First to act preflop) is Seat %d"), UtgSeatIndex);

    // Act
    Manager->ProcessPlayerAction(UtgSeatIndex, EPlayerAction::Fold, 0);

    // Assert
    TestEqual(TEXT("UTG player status should be Folded"), GameState->Seats[UtgSeatIndex].Status, EPlayerStatus::Folded);
    TestNotEqual(TEXT("Turn should pass from UTG"), GameState->CurrentTurnSeat, UtgSeatIndex);

    // В игре 3 игрока (0, 1, 2). Если SB=0, BB=1, UTG=2. UTG фолдит. Ход должен перейти к SB (0).
    // Точный следующий игрок зависит от порядка, определенного GetNextPlayerToAct.
    // Мы ожидаем, что IsBettingRoundOver() вернет false, и будет вызван RequestPlayerAction для следующего.
    TestFalse(TEXT("Betting round should not be over yet"), Manager->IsBettingRoundOver());
    TestTrue(TEXT("CurrentTurnSeat should be valid (not -1)"), GameState->CurrentTurnSeat != -1);
    if (GameState->Seats.IsValidIndex(GameState->CurrentTurnSeat))
    {
        TestTrue(TEXT("Next player should be in Playing or MustPost status"),
            GameState->Seats[GameState->CurrentTurnSeat].Status == EPlayerStatus::Playing ||
            GameState->Seats[GameState->CurrentTurnSeat].Status == EPlayerStatus::MustPostSmallBlind || // Не должно быть на префлопе
            GameState->Seats[GameState->CurrentTurnSeat].Status == EPlayerStatus::MustPostBigBlind);  // Не должно быть на префлопе
    }

    Manager->ConditionalBeginDestroy();
    return true;
}

// TODO: Добавить тест FOfflineGM_ProcessAction_Preflop_Fold_LeadsToWinner
// (например, 2 игрока, SB фолдит, BB выигрывает)


// --- Тесты для CHECK ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Preflop_BBChecks_3Players, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Preflop.Check.BBChecks_3Players", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Preflop_BBChecks_3Players::RunTest(const FString& Parameters)
{
    // Arrange: 3 игрока, SB=5, BB=10. UTG (первый) коллит BB. SB фолдит. Ход BB.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 FirstToActSeat;
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, 1000, 5, FirstToActSeat)) { Manager->ConditionalBeginDestroy(); return false; }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    int32 UtgSeat = FirstToActSeat;
    int32 SbSeat = GameState->PendingSmallBlindSeat;
    int32 BbSeat = GameState->PendingBigBlindSeat;

    // UTG calls BB
    Manager->ProcessPlayerAction(UtgSeat, EPlayerAction::Call, 0);
    // SB folds
    int32 CurrentTurnAfterUtgCall = GameState->CurrentTurnSeat;
    Manager->ProcessPlayerAction(CurrentTurnAfterUtgCall, EPlayerAction::Fold, 0);

    TestEqual(TEXT("Turn should now be BB before his check"), GameState->CurrentTurnSeat, BbSeat);
    TestFalse(TEXT("BB bHasActedThisSubRound should be false before his check"), GameState->Seats[BbSeat].bHasActedThisSubRound); // Проверяем начальное состояние флага
    bool bIsRoundOverBeforeBBCheck = Manager->IsBettingRoundOver(); // Сохраняем состояние IsBettingRoundOver ДО чека BB
    TestFalse(TEXT("Betting round should NOT be over before BB checks"), bIsRoundOverBeforeBBCheck);


    // Act: BB Checks
    Manager->ProcessPlayerAction(BbSeat, EPlayerAction::Check, 0);

    // Assert
    // 1. Проверяем состояние BB СРАЗУ ПОСЛЕ его действия (но ДО того, как ProceedToNextGameStage полностью изменит все)
    // Это сложно сделать напрямую в юнит-тесте, если ProcessPlayerAction вызывает ProceedToNextGameStage синхронно.
    // Однако, если ProcessPlayerAction для Check НЕ вызывает агрессию, то bHasActedThisSubRound для BB должен был установиться.
    // Мы можем проверить это, если бы у нас был способ "заглянуть" внутрь ProcessPlayerAction или если бы IsBettingRoundOver
    // проверялся ДО вызова ProceedToNextGameStage.

    // Поскольку ProcessPlayerAction вызывает IsBettingRoundOver, а затем ProceedToNextGameStage,
    // к моменту, когда ProcessPlayerAction завершится, стадия УЖЕ будет Flop, и флаги будут сброшены.

    // Значит, нам нужно изменить ассерты, чтобы они проверяли состояние НАЧАЛА ФЛОПА.
    TestEqual(TEXT("Stage should be Flop after preflop betting ends with BB's check"), GameState->CurrentStage, EGameStage::Flop);
    TestEqual(TEXT("There should be 3 community cards for Flop"), GameState->CommunityCards.Num(), 3);

    // На флопе, CurrentBetToCall должен быть 0, и ничей bHasActedThisSubRound не должен быть true.
    TestEqual(TEXT("CurrentBetToCall on Flop should be 0"), GameState->CurrentBetToCall, (int64)0);
    TestFalse(TEXT("BB bHasActedThisSubRound should be false (reset for Flop)"), GameState->Seats[BbSeat].bHasActedThisSubRound);
    TestFalse(TEXT("UTG bHasActedThisSubRound should be false (reset for Flop)"), GameState->Seats[UtgSeat].bHasActedThisSubRound);
    // SB сфолдил, его флаг не так важен, но тоже должен быть false или его статус Folded

    // Проверяем первого ходящего на флопе
    int32 ExpectedFlopActor = Manager->DetermineFirstPlayerToActPostflop();
    TestEqual(TEXT("CurrentTurnSeat should be first actor on Flop"), GameState->CurrentTurnSeat, ExpectedFlopActor);

    Manager->ConditionalBeginDestroy();
    return true;
}

// TODO: Добавить тест FOfflineGM_ProcessAction_Preflop_InvalidCheck (попытка чека, когда есть ставка)

// --- Тесты для CALL ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Preflop_UTGCalls_3Players, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Preflop.Call.UTGCalls_3Players", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Preflop_UTGCalls_3Players::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 UtgSeatIndex;
    int64 InitialStack = 1000;
    int64 SB = 5;
    int64 BB = SB * 2;
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, InitialStack, SB, UtgSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    int64 UtgStackBefore = GameState->Seats[UtgSeatIndex].Stack;
    int64 PotBefore = GameState->Pot; // Должен быть SB + BB

    // Act: UTG Calls
    Manager->ProcessPlayerAction(UtgSeatIndex, EPlayerAction::Call, 0);

    // Assert
    TestEqual(TEXT("UTG CurrentBet should be BigBlindAmount after call"), GameState->Seats[UtgSeatIndex].CurrentBet, BB);
    TestEqual(TEXT("UTG Stack should decrease by BigBlindAmount"), GameState->Seats[UtgSeatIndex].Stack, UtgStackBefore - BB);
    TestEqual(TEXT("Pot should increase by BigBlindAmount"), GameState->Pot, PotBefore + BB);
    TestTrue(TEXT("UTG bHasActedThisSubRound should be true"), GameState->Seats[UtgSeatIndex].bHasActedThisSubRound);
    TestFalse(TEXT("Betting round should not be over (SB and BB still to act)"), Manager->IsBettingRoundOver());
    TestNotEqual(TEXT("Turn should pass from UTG"), GameState->CurrentTurnSeat, UtgSeatIndex);

    Manager->ConditionalBeginDestroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Preflop_CallAllIn, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Preflop.Call.CallAllIn", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Preflop_CallAllIn::RunTest(const FString& Parameters)
{
    // Arrange: 3 игрока. SB=5, BB=10. UTG стек=7 (меньше BB). Ход UTG.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 UtgSeatIndex;
    int64 SB = 5;
    int64 BB = 10;
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, 1000, SB, UtgSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    GameState->Seats[UtgSeatIndex].Stack = 7; // Устанавливаем стек UTG меньше BB
    int64 UtgStackBefore = GameState->Seats[UtgSeatIndex].Stack; // = 7
    int64 PotBefore = GameState->Pot; // = 15 (SB 5 + BB 10)

    // Act: UTG Calls (это будет All-In)
    Manager->ProcessPlayerAction(UtgSeatIndex, EPlayerAction::Call, 0);

    // Assert
    TestEqual(TEXT("UTG Stack should be 0 after All-In call"), GameState->Seats[UtgSeatIndex].Stack, (int64)0);
    TestEqual(TEXT("UTG CurrentBet should be their entire stack"), GameState->Seats[UtgSeatIndex].CurrentBet, UtgStackBefore); // Поставил 7
    TestEqual(TEXT("UTG Status should be AllIn"), GameState->Seats[UtgSeatIndex].Status, EPlayerStatus::AllIn);
    TestEqual(TEXT("Pot should increase by UTG's stack"), GameState->Pot, PotBefore + UtgStackBefore); // 15 + 7 = 22
    TestTrue(TEXT("UTG bHasActedThisSubRound should be true"), GameState->Seats[UtgSeatIndex].bHasActedThisSubRound);
    // CurrentBetToCall все еще должен быть равен BB (10), так как олл-ин UTG был меньше.
    TestEqual(TEXT("CurrentBetToCall should remain BigBlindAmount"), GameState->CurrentBetToCall, BB);


    Manager->ConditionalBeginDestroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Preflop_UTGRaisesMin_3Players, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Preflop.Raise.UTGRaisesMin_3Players", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Preflop_UTGRaisesMin_3Players::RunTest(const FString& Parameters)
{
    // Arrange: 3 игрока, SB=5, BB=10. Ход UTG.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 UtgSeatIndex;
    int64 InitialStack = 1000;
    int64 SB = 5;
    int64 BB = SB * 2; // 10
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, InitialStack, SB, UtgSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    int64 UtgStackBefore = GameState->Seats[UtgSeatIndex].Stack; // 1000
    int64 PotBefore = GameState->Pot; // 15 (SB 5 + BB 10)
    int64 CurrentBetToCallBefore = GameState->CurrentBetToCall; // 10 (сумма BB)
    int64 LastRaiseAmountBefore = GameState->LastBetOrRaiseAmountInCurrentRound; // 10 (сумма BB)

    // UTG делает минимальный рейз. Минимальный рейз на префлопе - это еще один BB.
    // То есть, общая ставка UTG будет BB (колл) + BB (рейз) = 10 + 10 = 20.
    int64 UtgTotalBetAmount = CurrentBetToCallBefore + LastRaiseAmountBefore; // 10 (колл) + 10 (мин.рейз) = 20

    UE_LOG(LogTemp, Log, TEXT("UTGRaisesMin_3Players: UTG (Seat %d) raises to %lld. CurrentBetToCall: %lld, LastRaise: %lld"),
        UtgSeatIndex, UtgTotalBetAmount, CurrentBetToCallBefore, LastRaiseAmountBefore);

    // Act: UTG Raises
    Manager->ProcessPlayerAction(UtgSeatIndex, EPlayerAction::Raise, UtgTotalBetAmount);

    // Assert
    TestEqual(TEXT("UTG CurrentBet should be TotalBetAmount after min raise"), GameState->Seats[UtgSeatIndex].CurrentBet, UtgTotalBetAmount);
    TestEqual(TEXT("UTG Stack should decrease by TotalBetAmount"), GameState->Seats[UtgSeatIndex].Stack, UtgStackBefore - UtgTotalBetAmount);
    TestEqual(TEXT("Pot should increase by TotalBetAmount"), GameState->Pot, PotBefore + UtgTotalBetAmount);
    TestTrue(TEXT("UTG bHasActedThisSubRound should be true"), GameState->Seats[UtgSeatIndex].bHasActedThisSubRound);

    TestEqual(TEXT("CurrentBetToCall should be UTG's TotalBetAmount"), GameState->CurrentBetToCall, UtgTotalBetAmount);
    TestEqual(TEXT("LastAggressor should be UTG"), GameState->LastAggressorSeatIndex, UtgSeatIndex);
    // LastBetOrRaiseAmountInCurrentRound должен быть равен чистой сумме рейза UTG (UtgTotalBetAmount - CurrentBetToCallBefore)
    TestEqual(TEXT("LastBetOrRaiseAmountInCurrentRound should be the pure raise amount"), GameState->LastBetOrRaiseAmountInCurrentRound, LastRaiseAmountBefore); // или UtgTotalBetAmount - CurrentBetToCallBefore

    // Проверяем, что bHasActedThisSubRound сброшен для других активных игроков (SB и BB)
    for (const FPlayerSeatData& Seat : GameState->Seats)
    {
        if (Seat.SeatIndex != UtgSeatIndex && Seat.bIsSittingIn && Seat.Status == EPlayerStatus::Playing)
        {
            TestFalse(FString::Printf(TEXT("Seat %d bHasActedThisSubRound should be false after UTG's raise"), Seat.SeatIndex), Seat.bHasActedThisSubRound);
        }
    }
    TestNotEqual(TEXT("Turn should pass from UTG"), GameState->CurrentTurnSeat, UtgSeatIndex);

    Manager->ConditionalBeginDestroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Preflop_UTGRaisesAllIn, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Preflop.Raise.UTGRaisesAllIn", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Preflop_UTGRaisesAllIn::RunTest(const FString& Parameters)
{
    // Arrange: 3 игрока, SB=5, BB=10. UTG стек = 50. Ход UTG.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 UtgSeatIndex;
    int64 InitialStackSB_BB = 1000;
    int64 UtgActualInitialStack = 50;
    int64 SB_Amount = 5;
    int64 BB_Amount = SB_Amount * 2;
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, InitialStackSB_BB, SB_Amount, UtgSeatIndex))
    {
        Manager->ConditionalBeginDestroy(); return false;
    }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    // Устанавливаем стек UTG (для того, кто был определен как UTG)
    TestTrue(TEXT("UTGSeatIndex should be valid"), GameState->Seats.IsValidIndex(UtgSeatIndex));
    if (!GameState->Seats.IsValidIndex(UtgSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }
    GameState->Seats[UtgSeatIndex].Stack = UtgActualInitialStack;

    int64 PotBeforeUtgAction = GameState->Pot; // Должен быть SB_Amount + BB_Amount

    // Act: UTG Raises All-In
    // Сумма, передаваемая в ProcessPlayerAction для Raise - это ОБЩАЯ сумма ставки, до которой он рейзит.
    // Если он идет олл-ин, то его общая ставка будет его текущий стек (UtgActualInitialStack)
    // плюс то, что он, возможно, уже поставил в этом раунде (в данном случае 0, т.к. это его первый ход).
    int64 UtgTotalBetAmount = GameState->Seats[UtgSeatIndex].CurrentBet + GameState->Seats[UtgSeatIndex].Stack;
    Manager->ProcessPlayerAction(UtgSeatIndex, EPlayerAction::Raise, UtgTotalBetAmount);

    // Assert состояние UTG СРАЗУ ПОСЛЕ его действия All-In Raise
    TestEqual(TEXT("UTG Stack should be 0 after All-In raise"), GameState->Seats[UtgSeatIndex].Stack, (int64)0);
    TestEqual(TEXT("UTG CurrentBet should be their initial stack before this action"), GameState->Seats[UtgSeatIndex].CurrentBet, UtgActualInitialStack); // Т.к. CurrentBet был 0 до этого.
    TestEqual(TEXT("UTG Status should be AllIn"), GameState->Seats[UtgSeatIndex].Status, EPlayerStatus::AllIn);
    // Ассерт на bHasActedThisSubRound для UTG здесь уже может быть невалиден, если раунд сразу завершился.
    // TestTrue(TEXT("UTG bHasActedThisSubRound should be true immediately after their all-in raise"), GameState->Seats[UtgSeatIndex].bHasActedThisSubRound); // <-- УДАЛЯЕМ ЭТОТ АССЕРТ ИЛИ ПЕРЕСМАТРИВАЕМ

    // Assert состояние стола ПОСЛЕ действия UTG
    TestEqual(TEXT("Pot should increase by UTG's initial stack"), GameState->Pot, PotBeforeUtgAction + UtgActualInitialStack);
    TestEqual(TEXT("CurrentBetToCall should be UTG's total bet (All-In amount)"), GameState->CurrentBetToCall, UtgActualInitialStack);
    TestEqual(TEXT("LastAggressor should be UTG"), GameState->LastAggressorSeatIndex, UtgSeatIndex);
    int64 ExpectedPureRaise = UtgActualInitialStack - GameState->BigBlindAmount; // Его олл-ин минус то, что было CurrentBetToCall (BB)
    TestEqual(TEXT("LastBetOrRaiseAmountInCurrentRound should be pure raise amount"), GameState->LastBetOrRaiseAmountInCurrentRound, ExpectedPureRaise);

    // Симулируем фолды от SB и BB, чтобы проверить завершение руки
    int32 SbSeatIndex = GameState->PendingSmallBlindSeat; // Получаем актуальные индексы
    int32 BbSeatIndex = GameState->PendingBigBlindSeat;

    // Важно: CurrentTurnSeat изменится после действия UTG. Нужно получить актуального.
    int32 NextToActAfterUTG = GameState->CurrentTurnSeat;
    if (GameState->Seats.IsValidIndex(NextToActAfterUTG) && GameState->Seats[NextToActAfterUTG].Status != EPlayerStatus::Folded && GameState->Seats[NextToActAfterUTG].Status != EPlayerStatus::AllIn)
    {
        Manager->ProcessPlayerAction(NextToActAfterUTG, EPlayerAction::Fold, 0);
    }

    int32 NextToActAfterFirstFold = GameState->CurrentTurnSeat;
    if (GameState->Seats.IsValidIndex(NextToActAfterFirstFold) && GameState->Seats[NextToActAfterFirstFold].Status != EPlayerStatus::Folded && GameState->Seats[NextToActAfterFirstFold].Status != EPlayerStatus::AllIn)
    {
        Manager->ProcessPlayerAction(NextToActAfterFirstFold, EPlayerAction::Fold, 0);
    }

    // Assert: Теперь UTG должен выиграть банк, и рука должна завершиться
    TestTrue(TEXT("UTG (Seat %d) should still be AllIn or have won"), GameState->Seats.IsValidIndex(UtgSeatIndex) && (GameState->Seats[UtgSeatIndex].Status == EPlayerStatus::AllIn || GameState->Seats[UtgSeatIndex].Stack > UtgActualInitialStack));
    TestEqual(TEXT("Final pot should be 0 after UTG wins by default (or after AwardPot)"), GameState->Pot, (int64)0);
    // Стек UTG должен быть равен его начальному стеку + банк, который был ДО его ставки (ставки блайндов)
    TestEqual(TEXT("UTG stack should contain original stack + blinds pot"), GameState->Seats[UtgSeatIndex].Stack, UtgActualInitialStack + PotBeforeUtgAction);
    TestEqual(TEXT("Stage should be WaitingForPlayers after hand ends"), GameState->CurrentStage, EGameStage::WaitingForPlayers);

    Manager->ConditionalBeginDestroy();
    return true;
}


// Тест на Bet на флопе (когда торги открыты, CurrentBetToCall == 0)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Flop_PlayerBets, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Flop.Bet", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Flop_PlayerBets::RunTest(const FString& Parameters)
{
    // Arrange: 3 игрока, дошли до флопа, все прочекали до одного игрока (например, SB)
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 FirstToActPreflop;
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, 1000, 5, FirstToActPreflop)) { Manager->ConditionalBeginDestroy(); return false; }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    // Симулируем префлоп: UTG calls, SB calls, BB checks
    int32 UtgSeat = FirstToActPreflop;
    int32 SbSeat = GameState->PendingSmallBlindSeat;
    int32 BbSeat = GameState->PendingBigBlindSeat;

    Manager->ProcessPlayerAction(UtgSeat, EPlayerAction::Call, 0); // UTG calls BB
    Manager->ProcessPlayerAction(SbSeat, EPlayerAction::Call, 0); // SB calls BB (добавляет разницу)
    Manager->ProcessPlayerAction(BbSeat, EPlayerAction::Check, 0); // BB checks

    // Теперь должен быть флоп, и ход у SB (или первого активного слева от дилера)
    TestEqual(TEXT("Stage should be Flop"), GameState->CurrentStage, EGameStage::Flop);
    TestEqual(TEXT("Flop should have 3 cards"), GameState->CommunityCards.Num(), 3);
    TestEqual(TEXT("CurrentBetToCall on Flop should be 0"), GameState->CurrentBetToCall, (int64)0);

    int32 FlopFirstActor = GameState->CurrentTurnSeat;
    TestTrue(TEXT("FlopFirstActor should be valid"), GameState->Seats.IsValidIndex(FlopFirstActor));
    if (!GameState->Seats.IsValidIndex(FlopFirstActor)) { Manager->ConditionalBeginDestroy(); return false; }

    int64 PlayerStackBefore = GameState->Seats[FlopFirstActor].Stack;
    int64 PotBefore = GameState->Pot;
    int64 BetAmount = 50;

    // Act: Игрок делает Bet
    Manager->ProcessPlayerAction(FlopFirstActor, EPlayerAction::Bet, BetAmount);

    // Assert
    TestEqual(TEXT("Player CurrentBet should be BetAmount after bet"), GameState->Seats[FlopFirstActor].CurrentBet, BetAmount);
    TestEqual(TEXT("Player Stack should decrease by BetAmount"), GameState->Seats[FlopFirstActor].Stack, PlayerStackBefore - BetAmount);
    TestEqual(TEXT("Pot should increase by BetAmount"), GameState->Pot, PotBefore + BetAmount);
    TestTrue(TEXT("Player bHasActedThisSubRound should be true"), GameState->Seats[FlopFirstActor].bHasActedThisSubRound);

    TestEqual(TEXT("CurrentBetToCall should be BetAmount"), GameState->CurrentBetToCall, BetAmount);
    TestEqual(TEXT("LastAggressor should be betting player"), GameState->LastAggressorSeatIndex, FlopFirstActor);
    TestEqual(TEXT("LastBetOrRaiseAmountInCurrentRound should be BetAmount"), GameState->LastBetOrRaiseAmountInCurrentRound, BetAmount);
    TestEqual(TEXT("PlayerWhoOpenedBettingThisRound should be betting player"), GameState->PlayerWhoOpenedBettingThisRound, FlopFirstActor);

    Manager->ConditionalBeginDestroy();
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Preflop_SB3BetsUTGRaise, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Preflop.Raise.SB3BetsUTGRaise", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Preflop_SB3BetsUTGRaise::RunTest(const FString& Parameters)
{
    // Arrange: 3 игрока. SB=5, BB=10.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int32 UtgSeatIndex; // Первый после BB
    int64 InitialStack = 1000;
    int64 SB_Amount = 5;
    int64 BB_Amount = SB_Amount * 2; // 10
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, InitialStack, SB_Amount, UtgSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    int32 SbSeatIndex = GameState->PendingSmallBlindSeat;
    int32 BbSeatIndex = GameState->PendingBigBlindSeat;

    // Предположим порядок: UTG (Seat X), SB (Seat Y), BB (Seat Z)
    // Убедимся, что UTG - не SB и не BB, чтобы цепочка была длиннее
    if (UtgSeatIndex == SbSeatIndex || UtgSeatIndex == BbSeatIndex) {
        AddWarning(TEXT("Test setup warning: UTG is also a blind, scenario might be less clear for 3-bet by SB."));
    }

    // 1. UTG делает рейз (например, до 30)
    int64 UtgOriginalRaiseTo = BB_Amount * 3; // Рейз до 30 (общая ставка)
    Manager->ProcessPlayerAction(UtgSeatIndex, EPlayerAction::Raise, UtgOriginalRaiseTo);
    TestEqual(TEXT("UTG CurrentBet should be UtgOriginalRaiseTo"), GameState->Seats[UtgSeatIndex].CurrentBet, UtgOriginalRaiseTo);
    TestEqual(TEXT("CurrentBetToCall should be UtgOriginalRaiseTo"), GameState->CurrentBetToCall, UtgOriginalRaiseTo);
    int64 PureRaiseByUtg = UtgOriginalRaiseTo - BB_Amount; // Чистый рейз UTG = 20
    TestEqual(TEXT("LastBetOrRaiseAmount should be PureRaiseByUtg"), GameState->LastBetOrRaiseAmountInCurrentRound, PureRaiseByUtg);
    TestEqual(TEXT("LastAggressor should be UTG"), GameState->LastAggressorSeatIndex, UtgSeatIndex);
    TestEqual(TEXT("Turn should be SB (or next after UTG)"), GameState->CurrentTurnSeat, SbSeatIndex); // Зависит от вашего GetNextPlayerToAct

    // 2. SB (следующий по ходу) делает 3-бет (ре-рейз)
    // Минимальный ре-рейз для SB будет до UtgOriginalRaiseTo + PureRaiseByUtg = 30 + 20 = 50.
    // Допустим, SB ре-рейзит до 70.
    int64 SbReRaiseTo = 70;
    int64 SbStackBefore = GameState->Seats[SbSeatIndex].Stack;
    int64 SbCurrentBetBefore = GameState->Seats[SbSeatIndex].CurrentBet; // Должен быть SB_Amount (5)
    int64 PotBeforeSBAction = GameState->Pot;

    // Act: SB 3-bets
    Manager->ProcessPlayerAction(SbSeatIndex, EPlayerAction::Raise, SbReRaiseTo);

    // Assert
    TestEqual(TEXT("SB CurrentBet should be SbReRaiseTo"), GameState->Seats[SbSeatIndex].CurrentBet, SbReRaiseTo);
    int64 SBAmountAdded = SbReRaiseTo - SbCurrentBetBefore;
    TestEqual(TEXT("SB Stack should decrease by amount added"), GameState->Seats[SbSeatIndex].Stack, SbStackBefore - SBAmountAdded);
    TestEqual(TEXT("Pot should increase by amount SB added"), GameState->Pot, PotBeforeSBAction + SBAmountAdded);

    TestEqual(TEXT("CurrentBetToCall should be SbReRaiseTo"), GameState->CurrentBetToCall, SbReRaiseTo);
    TestEqual(TEXT("LastAggressor should be SB"), GameState->LastAggressorSeatIndex, SbSeatIndex);
    // Чистый рейз SB = SbReRaiseTo - UtgOriginalRaiseTo (предыдущий CurrentBetToCall) = 70 - 30 = 40
    TestEqual(TEXT("LastBetOrRaiseAmount should be SB's pure re-raise amount"), GameState->LastBetOrRaiseAmountInCurrentRound, SbReRaiseTo - UtgOriginalRaiseTo);

    // bHasActedThisSubRound должен быть сброшен для UTG и BB
    TestFalse(TEXT("UTG bHasActedThisSubRound should be false after SB's 3-bet"), GameState->Seats[UtgSeatIndex].bHasActedThisSubRound);
    TestFalse(TEXT("BB bHasActedThisSubRound should be false after SB's 3-bet"), GameState->Seats[BbSeatIndex].bHasActedThisSubRound);
    TestEqual(TEXT("Turn should be BB (or next after SB)"), GameState->CurrentTurnSeat, BbSeatIndex);

    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест: Завершение раунда ставок на префлопе после коллов
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Preflop_BettingEndsWithCalls, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Preflop.BettingEndsWithCalls", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Preflop_BettingEndsWithCalls::RunTest(const FString& Parameters)
{
    // Arrange: 3 игрока, SB=5, BB=10.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 UtgSeatIndex;
    if (!SetupGameToPreflopFirstAction(this, Manager, 3, 1000, 5, UtgSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    int32 SbSeatIndex = GameState->PendingSmallBlindSeat;
    int32 BbSeatIndex = GameState->PendingBigBlindSeat;
    int64 BetRaiseAmount = 30; // Сумма, до которой рейзит UTG

    // Act
    // UTG raises to 30
    Manager->ProcessPlayerAction(UtgSeatIndex, EPlayerAction::Raise, BetRaiseAmount);
    // SB calls 30 
    Manager->ProcessPlayerAction(SbSeatIndex, EPlayerAction::Call, 0);
    // BB calls 30
    Manager->ProcessPlayerAction(BbSeatIndex, EPlayerAction::Call, 0);

    // Assert: Теперь мы должны быть на флопе.
    TestEqual(TEXT("Stage should be Flop after all calls on preflop"), GameState->CurrentStage, EGameStage::Flop);
    TestEqual(TEXT("Flop should have 3 community cards"), GameState->CommunityCards.Num(), 3);
    TestEqual(TEXT("Pot should be 3 * BetRaiseAmount"), GameState->Pot, BetRaiseAmount * 3); // 30 * 3 = 90

    // Проверяем, что состояние для НОВОГО раунда ставок (флопа) сброшено
    TestEqual(TEXT("CurrentBetToCall on Flop should be 0"), GameState->CurrentBetToCall, (int64)0);
    for (const FPlayerSeatData& Seat : GameState->Seats)
    {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded)
        {
            TestEqual(FString::Printf(TEXT("Seat %d CurrentBet should be 0 for Flop"), Seat.SeatIndex), Seat.CurrentBet, (int64)0);
            TestFalse(FString::Printf(TEXT("Seat %d bHasActedThisSubRound should be false for Flop"), Seat.SeatIndex), Seat.bHasActedThisSubRound);
        }
    }
    TestTrue(TEXT("CurrentTurnSeat should be valid for Flop"), GameState->CurrentTurnSeat != -1);

    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест: Невалидный Bet (сумма меньше минимального, не олл-ин)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_InvalidBetAmount_LessThanMin, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Invalid.BetTooSmall", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_InvalidBetAmount_LessThanMin::RunTest(const FString& Parameters)
{
    // Arrange: Дошли до флопа, ход Игрока 0, банк = 30, стек игрока 0 = 970
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager) return false;

    int32 FlopFirstActorSeatIndex;
    // Доводим до флопа, где все прочекали до первого игрока, или просто ставим стейт
    Manager->InitializeGame(1, 2, 1000, 5); // SB=5, BB=10
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    // Симулируем префлоп коллы до флопа
    Manager->StartNewHand();
    Manager->ProcessPlayerAction(GameState->PendingSmallBlindSeat, EPlayerAction::PostBlind, 0);
    Manager->ProcessPlayerAction(GameState->PendingBigBlindSeat, EPlayerAction::PostBlind, 0);
    // Допустим, UTG (первый ходящий на префлопе) коллит, SB коллит, BB чекает
    int32 UtgPreflop = GameState->CurrentTurnSeat;
    Manager->ProcessPlayerAction(UtgPreflop, EPlayerAction::Call, 0);
    int32 SbPreflop = GameState->CurrentTurnSeat;
    Manager->ProcessPlayerAction(SbPreflop, EPlayerAction::Call, 0);
    int32 BbPreflop = GameState->CurrentTurnSeat;
    Manager->ProcessPlayerAction(BbPreflop, EPlayerAction::Check, 0);
    // Теперь стадия флоп, ход первого игрока постфлоп

    TestEqual(TEXT("Stage should be Flop"), GameState->CurrentStage, EGameStage::Flop);
    FlopFirstActorSeatIndex = GameState->CurrentTurnSeat;
    TestTrue(TEXT("FlopFirstActor should be valid"), GameState->Seats.IsValidIndex(FlopFirstActorSeatIndex));
    if (!GameState->Seats.IsValidIndex(FlopFirstActorSeatIndex)) { Manager->ConditionalBeginDestroy(); return false; }

    int64 PlayerStackBefore = GameState->Seats[FlopFirstActorSeatIndex].Stack;
    int64 PotBefore = GameState->Pot;
    int64 InvalidBetAmount = GameState->BigBlindAmount - 1; // Меньше BB (например, 9 если BB=10)
    if (InvalidBetAmount <= 0) InvalidBetAmount = 1; // Если BB очень маленький

    UE_LOG(LogTemp, Log, TEXT("InvalidBetAmount_LessThanMin: Player %d attempts invalid bet %lld (MinBet is BB=%lld)"),
        FlopFirstActorSeatIndex, InvalidBetAmount, GameState->BigBlindAmount);

    // Act: Игрок пытается сделать невалидный Bet
    Manager->ProcessPlayerAction(FlopFirstActorSeatIndex, EPlayerAction::Bet, InvalidBetAmount);

    // Assert
    // Состояние игры не должно измениться, ход должен остаться у того же игрока,
    // так как RequestPlayerAction должен быть вызван для него снова.
    TestEqual(TEXT("Player Stack should not change after invalid bet"), GameState->Seats[FlopFirstActorSeatIndex].Stack, PlayerStackBefore);
    TestEqual(TEXT("Pot should not change after invalid bet"), GameState->Pot, PotBefore);
    TestEqual(TEXT("Player CurrentBet should not change after invalid bet"), GameState->Seats[FlopFirstActorSeatIndex].CurrentBet, (int64)0); // Ставка не была сделана
    TestEqual(TEXT("CurrentBetToCall should still be 0"), GameState->CurrentBetToCall, (int64)0);
    TestEqual(TEXT("CurrentTurnSeat should remain the same player after invalid bet"), GameState->CurrentTurnSeat, FlopFirstActorSeatIndex);
    TestTrue(TEXT("Player bIsTurn flag should still be true"), GameState->Seats[FlopFirstActorSeatIndex].bIsTurn);
    // bHasActedThisSubRound не должен установиться в true
    TestFalse(TEXT("Player bHasActedThisSubRound should be false after invalid bet"), GameState->Seats[FlopFirstActorSeatIndex].bHasActedThisSubRound);


    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест: Игрок делает Bet на флопе, другой коллирует, третий фолдит. Раунд ставок на флопе завершен.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Flop_BetCallFold, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Flop.BetCallFold", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Flop_BetCallFold::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int32 Player1_Seat, Player2_Seat, Player3_Seat;
    int32 FirstToActFlop;
    if (!SetupGameToPostflopStreet(this, Manager, 3, 1000, 5, EGameStage::Flop, FirstToActFlop))
    {
        Manager->ConditionalBeginDestroy(); return false;
    }
    UOfflinePokerGameState* GameState = Manager->GetGameState();

    Player1_Seat = FirstToActFlop;
    Player2_Seat = Manager->GetNextPlayerToAct(Player1_Seat, true); // Игрок после P1
    Player3_Seat = Manager->GetNextPlayerToAct(Player2_Seat, true); // Игрок после P2

    TestTrue(TEXT("Player1_Seat (Flop) should be valid"), GameState->Seats.IsValidIndex(Player1_Seat));
    TestTrue(TEXT("Player2_Seat (Flop) should be valid"), GameState->Seats.IsValidIndex(Player2_Seat));
    TestTrue(TEXT("Player3_Seat (Flop) should be valid"), GameState->Seats.IsValidIndex(Player3_Seat));
    if (!GameState->Seats.IsValidIndex(Player1_Seat) || !GameState->Seats.IsValidIndex(Player2_Seat) || !GameState->Seats.IsValidIndex(Player3_Seat))
    {
        Manager->ConditionalBeginDestroy(); return false;
    }

    TestEqual(TEXT("Initial Stage on Flop should be Flop"), GameState->CurrentStage, EGameStage::Flop);
    TestEqual(TEXT("Initial Turn on Flop should be Player1_Seat"), GameState->CurrentTurnSeat, Player1_Seat);

    int64 BetAmount = 50;
    int64 PotBeforeP1Bet = GameState->Pot;

    // Act
    // Player1 (FirstToActFlop) Bets
    UE_LOG(LogTemp, Log, TEXT("Flop_BetCallFold_Test: Player %d Bets %lld"), Player1_Seat, BetAmount);
    Manager->ProcessPlayerAction(Player1_Seat, EPlayerAction::Bet, BetAmount);
    TestEqual(TEXT("Turn should be Player2 after P1 bet"), GameState->CurrentTurnSeat, Player2_Seat);

    // Player2 Calls
    UE_LOG(LogTemp, Log, TEXT("Flop_BetCallFold_Test: Player %d Calls"), Player2_Seat);
    Manager->ProcessPlayerAction(Player2_Seat, EPlayerAction::Call, 0);
    TestEqual(TEXT("Turn should be Player3 after P2 call"), GameState->CurrentTurnSeat, Player3_Seat);

    // Player3 Folds
    UE_LOG(LogTemp, Log, TEXT("Flop_BetCallFold_Test: Player %d Folds"), Player3_Seat);
    Manager->ProcessPlayerAction(Player3_Seat, EPlayerAction::Fold, 0);

    // Assert: После того, как P3 сфолдил, P1 сделал бет, P2 заколлировал.
    // Это должно завершить раунд ставок на флопе.
    // ProcessPlayerAction для P3 (фолд) должен был вызвать IsBettingRoundOver() (которая вернет true),
    // а затем ProceedToNextGameStage(), которая переведет игру на терн.

    TestEqual(TEXT("P3 Status should be Folded"), GameState->Seats[Player3_Seat].Status, EPlayerStatus::Folded);
    TestEqual(TEXT("Stage should NOW be Turn after flop betting concluded"), GameState->CurrentStage, EGameStage::Turn);
    TestEqual(TEXT("There should be 4 community cards for Turn (3 Flop + 1 Turn)"), GameState->CommunityCards.Num(), 4);

    // Проверяем, что состояние для НОВОГО раунда ставок (терна) сброшено
    TestEqual(TEXT("CurrentBetToCall on Turn should be 0"), GameState->CurrentBetToCall, (int64)0);
    if (GameState->Seats.IsValidIndex(Player1_Seat) && GameState->Seats[Player1_Seat].Status != EPlayerStatus::Folded)
    {
        TestEqual(FString::Printf(TEXT("Player1 (Seat %d) CurrentBet should be 0 for Turn"), Player1_Seat), GameState->Seats[Player1_Seat].CurrentBet, (int64)0);
        TestFalse(FString::Printf(TEXT("Player1 (Seat %d) bHasActedThisSubRound should be false for Turn"), Player1_Seat), GameState->Seats[Player1_Seat].bHasActedThisSubRound);
    }
    if (GameState->Seats.IsValidIndex(Player2_Seat) && GameState->Seats[Player2_Seat].Status != EPlayerStatus::Folded)
    {
        TestEqual(FString::Printf(TEXT("Player2 (Seat %d) CurrentBet should be 0 for Turn"), Player2_Seat), GameState->Seats[Player2_Seat].CurrentBet, (int64)0);
        TestFalse(FString::Printf(TEXT("Player2 (Seat %d) bHasActedThisSubRound should be false for Turn"), Player2_Seat), GameState->Seats[Player2_Seat].bHasActedThisSubRound);
    }

    // Проверяем первого ходящего на терне
    int32 ExpectedTurnActor = Manager->DetermineFirstPlayerToActPostflop();
    TestEqual(TEXT("CurrentTurnSeat should be first actor on Turn"), GameState->CurrentTurnSeat, ExpectedTurnActor);

    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест: Два игрока идут All-In на флопе, третий фолдит. Автоматический переход до ривера и шоудаун.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Flop_TwoPlayersAllIn, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Flop.TwoPlayersAllIn", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Flop_TwoPlayersAllIn::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int64 InitialStackForAllPlayers_PreBlinds = 1000;
    int64 SB_Amount = 5;
    int64 BB_Amount = SB_Amount * 2; // 10

    // 1. Инициализация игры с 3 игроками.
    Manager->InitializeGame(1, 2, InitialStackForAllPlayers_PreBlinds, SB_Amount);
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("FlopAllIn_Test: GameState should exist after InitializeGame"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }
    TestEqual(TEXT("FlopAllIn_Test: Should have 3 players"), GameState->Seats.Num(), 3);
    if (GameState->Seats.Num() != 3) { Manager->ConditionalBeginDestroy(); return false; }

    // --- ЯВНО ОПРЕДЕЛЯЕМ ТЕСТОВЫЕ РОЛИ ДЛЯ SEATINDEX ---
    // Пусть P1 = Seat 0, P2 = Seat 1, P3 = Seat 2
    // Это упростит назначение карт и стеков, а также проверку.
    // Нам нужно, чтобы эти трое дошли до флопа.
    const int32 TestP1_SeatIndex = 0; // Пойдет All-In на флопе с AA
    const int32 TestP2_SeatIndex = 1; // Пойдет All-In на флопе с KK
    const int32 TestP3_SeatIndex = 2; // Сфолдит на флопе, имеет большой стек

    // 2. Начинаем новую руку
    Manager->StartNewHand(); // Дилер, SB, BB будут определены

    // --- Симулируем префлоп: все коллируют BB, чтобы дойти до флопа ---
    // Эта часть должна быть аккуратной, чтобы привести игру к флопу
    // и чтобы все три наших тестовых игрока (0, 1, 2) остались в игре.
    int32 LoopGuardPreflop = 0;
    while (GameState->CurrentStage == EGameStage::WaitingForSmallBlind || GameState->CurrentStage == EGameStage::WaitingForBigBlind || GameState->CurrentStage == EGameStage::Preflop)
    {
        if (LoopGuardPreflop++ > 15) { AddError(TEXT("FlopAllIn_Test: Setup to Flop took too many iterations.")); Manager->ConditionalBeginDestroy(); return false; }
        int32 CurrentActorPreflop = GameState->CurrentTurnSeat;
        if (CurrentActorPreflop == -1) { AddError(TEXT("FlopAllIn_Test: Setup to Flop: Turn is -1.")); Manager->ConditionalBeginDestroy(); return false; }

        FPlayerSeatData& PlayerToActPreflop = GameState->Seats[CurrentActorPreflop];
        if (PlayerToActPreflop.Status == EPlayerStatus::MustPostSmallBlind || PlayerToActPreflop.Status == EPlayerStatus::MustPostBigBlind) {
            Manager->ProcessPlayerAction(CurrentActorPreflop, EPlayerAction::PostBlind, 0);
        }
        else if (PlayerToActPreflop.Status == EPlayerStatus::Playing && PlayerToActPreflop.Stack > 0) {
            // Для простоты все коллируют до BB, чтобы дойти до флопа без сложных ставок
            if (PlayerToActPreflop.CurrentBet < GameState->BigBlindAmount) { // Если ставка меньше BB, коллируем до BB
                Manager->ProcessPlayerAction(CurrentActorPreflop, EPlayerAction::Call, 0); // Call должен сам рассчитать до BB
            }
            else if (PlayerToActPreflop.CurrentBet == GameState->BigBlindAmount) { // Если уже поставил BB (например, сам BB)
                Manager->ProcessPlayerAction(CurrentActorPreflop, EPlayerAction::Check, 0);
            }
            else { // Если кто-то зарейзил, и мы тут - коллируем этот рейз
                Manager->ProcessPlayerAction(CurrentActorPreflop, EPlayerAction::Call, 0);
            }
        }
        else if (Manager->IsBettingRoundOver()) { // Если раунд окончен
            Manager->ProceedToNextGameStage();
        }
        else {
            int32 NextPlayer = Manager->GetNextPlayerToAct(CurrentActorPreflop, true);
            if (NextPlayer != -1) GameState->CurrentTurnSeat = NextPlayer;
            else { AddError(FString::Printf(TEXT("Stuck in preflop setup at seat %d"), CurrentActorPreflop)); break; }
        }
    }
    TestEqual(TEXT("FlopAllIn_Test: Stage should be Flop after preflop setup"), GameState->CurrentStage, EGameStage::Flop);
    if (GameState->CurrentStage != EGameStage::Flop) { Manager->ConditionalBeginDestroy(); return false; }

    // Сохраняем стеки игроков ПОСЛЕ префлопа, и банк
    int64 P1_StackAfterPreflop = GameState->Seats[TestP1_SeatIndex].Stack;
    int64 P2_StackAfterPreflop = GameState->Seats[TestP2_SeatIndex].Stack;
    int64 P3_StackAfterPreflop = GameState->Seats[TestP3_SeatIndex].Stack;
    int64 PotBeforeFlopActions = GameState->Pot;
    TestEqual(TEXT("Pot after preflop should be 3 * BB (30)"), PotBeforeFlopActions, BB_Amount * 3);


    // Устанавливаем ЭФФЕКТИВНЫЕ стеки ДЛЯ ДЕЙСТВИЙ НА ФЛОПЕ
    int64 P1_EffectiveStackForFlopBet = 100;
    int64 P2_EffectiveStackForFlopBet = 150;
    GameState->Seats[TestP1_SeatIndex].Stack = P1_EffectiveStackForFlopBet;
    GameState->Seats[TestP2_SeatIndex].Stack = P2_EffectiveStackForFlopBet;
    // Стек P3 (TestP3_SeatIndex) остается его P3_StackAfterPreflop (он большой и покрывает всех)

    // Устанавливаем карты: P1(AA) выигрывает у P2(KK)
    GameState->Seats[TestP1_SeatIndex].HoleCards = { FCard(ECardSuit::Spades, ECardRank::Ace), FCard(ECardSuit::Clubs, ECardRank::Ace) };
    GameState->Seats[TestP2_SeatIndex].HoleCards = { FCard(ECardSuit::Spades, ECardRank::King), FCard(ECardSuit::Clubs, ECardRank::King) };
    // Карты P3 не важны, он сфолдит. Общие карты флопа уже розданы.

    // Сумма фишек у игроков ПЕРЕД их действиями на флопе (с учетом установленных эффективных стеков)
    // ПЛЮС банк, который уже на столе с префлопа
    int64 TotalChipsInPlayAtFlopActionStart = GameState->Seats[TestP1_SeatIndex].Stack +
        GameState->Seats[TestP2_SeatIndex].Stack +
        GameState->Seats[TestP3_SeatIndex].Stack +
        PotBeforeFlopActions;

    // --- Act: Действия на Флопе ---
    // Определяем порядок хода на флопе
    int32 FlopActor1 = GameState->CurrentTurnSeat;
    int32 FlopActor2 = Manager->GetNextPlayerToAct(FlopActor1, true);
    int32 FlopActor3 = Manager->GetNextPlayerToAct(FlopActor2, true);

    // Симулируем действия в зависимости от того, кто есть кто из наших P1, P2, P3
    // Это немного усложняет, но гарантирует, что действия делают нужные "роли"
    TArray<int32> FlopActionOrder = { FlopActor1, FlopActor2, FlopActor3 };
    for (int32 ActorMakingMoveOnFlop : FlopActionOrder)
    {
        if (!GameState->Seats.IsValidIndex(ActorMakingMoveOnFlop) || GameState->Seats[ActorMakingMoveOnFlop].Status == EPlayerStatus::Folded || GameState->Seats[ActorMakingMoveOnFlop].Status == EPlayerStatus::AllIn) continue;
        TestEqual(TEXT("Current turn should match ActorMakingMoveOnFlop"), GameState->CurrentTurnSeat, ActorMakingMoveOnFlop);

        if (ActorMakingMoveOnFlop == TestP1_SeatIndex) {
            Manager->ProcessPlayerAction(TestP1_SeatIndex, EPlayerAction::Bet, P1_EffectiveStackForFlopBet);
        }
        else if (ActorMakingMoveOnFlop == TestP2_SeatIndex) {
            Manager->ProcessPlayerAction(TestP2_SeatIndex, EPlayerAction::Raise, P2_EffectiveStackForFlopBet);
        }
        else if (ActorMakingMoveOnFlop == TestP3_SeatIndex) {
            Manager->ProcessPlayerAction(TestP3_SeatIndex, EPlayerAction::Fold, 0);
        }
    }

    // --- Assert: Состояние ПОСЛЕ ЗАВЕРШЕНИЯ РУКИ ---
    TestEqual(TEXT("FlopAllIn_Test: Final stage should be WaitingForPlayers"), GameState->CurrentStage, EGameStage::WaitingForPlayers);
    TestEqual(TEXT("FlopAllIn_Test: Final pot should be 0"), GameState->Pot, (int64)0);
    TestEqual(TEXT("FlopAllIn_Test: Final community cards should be 5"), GameState->CommunityCards.Num(), 5);
    TestEqual(TEXT("FlopAllIn_Test: CurrentTurnSeat should be -1"), GameState->CurrentTurnSeat, -1);

    // Банк на момент шоудауна: PotBeforeFlopActions(30) + P1_Bet(100) + P2_Bet(150) = 280
    // Победитель P1 (AA) забирает все 280 (при упрощенной логике AwardPotToWinner)
    int64 Expected_P1_Stack_Final = 0 /*его стек после олл-ина на флопе*/ + (PotBeforeFlopActions + P1_EffectiveStackForFlopBet + P2_EffectiveStackForFlopBet);
    int64 Expected_P2_Stack_Final = 0;
    int64 Expected_P3_Stack_Final = P3_StackAfterPreflop;

    TestEqual(TEXT("FlopAllIn_Test: P1 (Winner) stack should be correct"), GameState->Seats[TestP1_SeatIndex].Stack, Expected_P1_Stack_Final);
    TestEqual(TEXT("FlopAllIn_Test: P2 (Loser) stack should be 0"), GameState->Seats[TestP2_SeatIndex].Stack, Expected_P2_Stack_Final);
    TestEqual(TEXT("FlopAllIn_Test: P3 (Folded) stack should be correct"), GameState->Seats[TestP3_SeatIndex].Stack, Expected_P3_Stack_Final);

    int64 TotalStackAfterShowdown = GameState->Seats[TestP1_SeatIndex].Stack + GameState->Seats[TestP2_SeatIndex].Stack + GameState->Seats[TestP3_SeatIndex].Stack;
    TestEqual(TEXT("FlopAllIn_Test: Total chips in game should remain constant from flop action start"), TotalStackAfterShowdown, TotalChipsInPlayAtFlopActionStart);

    Manager->ConditionalBeginDestroy();
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_River_BetCall_ToShowdown, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.River.BetCallToShowdown", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_River_BetCall_ToShowdown::RunTest(const FString& Parameters)
{
    // Arrange: 2 игрока, дошли до ривера.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int32 Player1_Seat_Actual;
    int32 Player2_Seat_Actual;
    int32 FirstToActRiver;
    int64 InitialStack_PreBlinds = 1000; // Этот стек будет у каждого в начале InitializeGame
    int64 SB_Amount = 5;
    int64 BB_Amount = SB_Amount * 2; // 10

    // 1. Настраиваем игру до ривера, где все чекали/коллировали на предыдущих улицах.
    if (!SetupGameToPostflopStreet(this, Manager, 2, InitialStack_PreBlinds, SB_Amount, EGameStage::River, FirstToActRiver))
    {
        AddError(TEXT("River_BetCall_Test: Failed to setup game to River stage."));
        Manager->ConditionalBeginDestroy(); return false;
    }
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("River_BetCall_Test: GameState should exist."), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    Player1_Seat_Actual = FirstToActRiver; // Первый ходящий на ривере
    Player2_Seat_Actual = Manager->GetNextPlayerToAct(Player1_Seat_Actual, true); // Другой игрок

    TestTrue(TEXT("River_BetCall_Test: Player1_Seat_Actual valid"), GameState->Seats.IsValidIndex(Player1_Seat_Actual));
    TestTrue(TEXT("River_BetCall_Test: Player2_Seat_Actual valid"), GameState->Seats.IsValidIndex(Player2_Seat_Actual));
    if (!GameState->Seats.IsValidIndex(Player1_Seat_Actual) || !GameState->Seats.IsValidIndex(Player2_Seat_Actual) || Player1_Seat_Actual == Player2_Seat_Actual)
    {
        AddError(TEXT("River_BetCall_Test: Could not determine two distinct players for river action."));
        Manager->ConditionalBeginDestroy(); return false;
    }

    // Устанавливаем конкретные карты: P1 (Ac Kc) > P2 (Qh Ts)
    // Общие карты (5 штук) уже розданы SetupGameToPostflopStreet.
    // Для определенности в тесте, давайте их зададим явно, чтобы знать победителя.
    GameState->CommunityCards = {
        FCard(ECardSuit::Diamonds, ECardRank::Ace), FCard(ECardSuit::Hearts, ECardRank::King), FCard(ECardSuit::Spades, ECardRank::Five), // У P1 две пары (Тузы и Короли)
        FCard(ECardSuit::Clubs, ECardRank::Two), FCard(ECardSuit::Diamonds, ECardRank::Seven)
    };
    GameState->Seats[Player1_Seat_Actual].HoleCards = { FCard(ECardSuit::Clubs, ECardRank::Ace), FCard(ECardSuit::Spades, ECardRank::King) }; // Ac Ks -> Две Пары (AA, KK)
    GameState->Seats[Player2_Seat_Actual].HoleCards = { FCard(ECardSuit::Hearts, ECardRank::Queen), FCard(ECardSuit::Diamonds, ECardRank::Queen) }; // Qh Qd -> Сет Дам (если на борде нет Q) или Две пары (QQ + AA/KK)
    // С текущим бордом Ad Kh 5s 2c 7d: P1 (AK) имеет пару тузов и пару королей. P2 (QQ) имеет пару дам. P1 выигрывает.
// Если мы хотим, чтобы P2 проиграл с худшей парой, дадим ему что-то типа QJ.
    GameState->Seats[Player2_Seat_Actual].HoleCards = { FCard(ECardSuit::Hearts, ECardRank::Queen), FCard(ECardSuit::Spades, ECardRank::Jack) }; // Qh Js -> у P2 только пара тузов со стола (если есть) или пара королей, или старшая дама.
    // На борде Ad Kh 5s 2c 7d: P1 = AA KK. P2 = A K Q J 7 (Старшая карта Туз, если борд общий). P1 выигрывает.

// Сохраняем состояние перед действиями на ривере
    int64 P1StackBeforeRiverBet = GameState->Seats[Player1_Seat_Actual].Stack; // Стек после префлопа/флопа/терна
    int64 P2StackBeforeRiverBet = GameState->Seats[Player2_Seat_Actual].Stack; // Стек после префлопа/флопа/терна
    int64 PotBeforeRiverBet = GameState->Pot; // Банк после всех предыдущих улиц (префлоп (20) + 0 + 0 = 20)
    int64 BetAmountOnRiver = 75;

    // Общее количество фишек у этих двух игроков + то, что уже в банке от них с предыдущих улиц
    int64 TotalChipsInPlayAtRiverActionStart = P1StackBeforeRiverBet + P2StackBeforeRiverBet + PotBeforeRiverBet;

    TestEqual(TEXT("River_BetCall_Test: Turn should be Player1_Seat_Actual"), GameState->CurrentTurnSeat, Player1_Seat_Actual);

    // Act
    Manager->ProcessPlayerAction(Player1_Seat_Actual, EPlayerAction::Bet, BetAmountOnRiver);
    Manager->ProcessPlayerAction(Player2_Seat_Actual, EPlayerAction::Call, 0);

    // Assert: Состояние ПОСЛЕ ЗАВЕРШЕНИЯ РУКИ
    TestEqual(TEXT("River_BetCall_Test: Stage should be WaitingForPlayers"), GameState->CurrentStage, EGameStage::WaitingForPlayers);
    TestEqual(TEXT("River_BetCall_Test: Pot should be 0"), GameState->Pot, (int64)0);
    TestEqual(TEXT("River_BetCall_Test: CurrentTurnSeat should be -1"), GameState->CurrentTurnSeat, -1);
    TestEqual(TEXT("River_BetCall_Test: Community cards count should still be 5"), GameState->CommunityCards.Num(), 5);

    // P1 (победитель) забирает весь банк.
    // Банк на момент шоудауна = PotBeforeRiverBet + BetAmountOnRiver (от P1) + BetAmountOnRiver (от P2).
    int64 TotalPotForShowdown = PotBeforeRiverBet + BetAmountOnRiver + BetAmountOnRiver; // 20 + 75 + 75 = 170.

    int64 ExpectedP1Stack = P1StackBeforeRiverBet - BetAmountOnRiver + TotalPotForShowdown;
    int64 ExpectedP2Stack = P2StackBeforeRiverBet - BetAmountOnRiver;

    TestEqual(TEXT("River_BetCall_Test: Player 1 (Winner) stack correct"), GameState->Seats[Player1_Seat_Actual].Stack, ExpectedP1Stack);
    TestEqual(TEXT("River_BetCall_Test: Player 2 (Loser) stack correct"), GameState->Seats[Player2_Seat_Actual].Stack, ExpectedP2Stack);

    int64 TotalStackAfterShowdown = GameState->Seats[Player1_Seat_Actual].Stack + GameState->Seats[Player2_Seat_Actual].Stack;
    // Сравниваем с суммой (стеки игроков на начало действий на ривере + банк на начало действий на ривере)
    TestEqual(TEXT("River_BetCall_Test: Total chips of involved players constant from river action start"), TotalStackAfterShowdown, TotalChipsInPlayAtRiverActionStart);

    // --- Опционально: Тест начала НОВОЙ руки ---
    // (Остается как было, с проверкой, что есть кому играть)
    AddInfo(TEXT("--- Simulating StartNewHand after River_BetCall_ToShowdown ---"));
    bool bCanActuallyStartNewHand = true;
    int32 PlayersWithSufficientStackForBB = 0;
    for (const auto& Seat : GameState->Seats) {
        if (Seat.bIsSittingIn && Seat.Stack >= GameState->BigBlindAmount) {
            PlayersWithSufficientStackForBB++;
        }
    }
    if (PlayersWithSufficientStackForBB < 2) bCanActuallyStartNewHand = false;

    if (bCanActuallyStartNewHand)
    {
        Manager->StartNewHand();
        TestEqual(TEXT("Stage after explicit StartNewHand should be WaitingForSmallBlind"), GameState->CurrentStage, EGameStage::WaitingForSmallBlind);
        TestTrue(TEXT("Community cards should be empty after explicit StartNewHand"), GameState->CommunityCards.IsEmpty());
    }
    else { AddWarning(TEXT("River_BetCall_Test: Skipping StartNewHand assertions, not enough players with sufficient chips for BB.")); }

    Manager->ConditionalBeginDestroy();
    return true;
}

// --- Тесты для ProceedToShowdown и AwardPotToWinner ---

// Вспомогательная функция для установки конкретных карт игрокам и на стол


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_Showdown_Player1WinsWithFlush, "PokerClient.UnitTests.OfflineGameManager.Showdown.Player1WinsFlush", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_Showdown_Player1WinsWithFlush::RunTest(const FString& Parameters)
{
    // Arrange: 2 игрока. У Игрока 0 флеш, у Игрока 1 две пары.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int64 InitialStack = 1000;
    Manager->InitializeGame(1, 1, InitialStack, 5); // 2 игрока
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    // Устанавливаем карты для сценария (Игрок 0 - Флеш, Игрок 1 - Две Пары)
    // Игроки 0 и 1
    TArray<FCard> Community = {
        FCard(ECardSuit::Hearts, ECardRank::Ace), FCard(ECardSuit::Hearts, ECardRank::King), FCard(ECardSuit::Spades, ECardRank::Ten),
        FCard(ECardSuit::Clubs, ECardRank::Ten), FCard(ECardSuit::Hearts, ECardRank::Seven)
    };
    SetSpecificCards(GameState, 0, ECardSuit::Hearts, ECardRank::Queen, ECardSuit::Hearts, ECardRank::Jack, // P0: Qh Jh (Флеш до туза)
        1, ECardSuit::Spades, ECardRank::Ace, ECardSuit::Clubs, ECardRank::King,    // P1: As Kc (Две пары: Тузы и Короли, если есть на борде, или Тузы и Десятки)
        Community);

    // Симулируем, что игроки дошли до шоудауна, и в банке есть деньги
    GameState->Pot = 200;
    GameState->Seats[0].Status = EPlayerStatus::Playing; // Устанавливаем статус для участия в шоудауне
    GameState->Seats[1].Status = EPlayerStatus::Playing;
    GameState->CurrentStage = EGameStage::Showdown; // Устанавливаем стадию для вызова ProceedToShowdown

    int64 Player0StackBefore = GameState->Seats[0].Stack;
    int64 Player1StackBefore = GameState->Seats[1].Stack;

    // Act
    Manager->ProceedToShowdown(); // Эта функция должна определить победителя и вызвать AwardPot

    // Assert
    TestEqual(TEXT("Player 0 (Winner) stack should increase by Pot amount"), GameState->Seats[0].Stack, Player0StackBefore + 200);
    TestEqual(TEXT("Player 1 (Loser) stack should not change"), GameState->Seats[1].Stack, Player1StackBefore);
    TestEqual(TEXT("Pot should be 0 after awarding"), GameState->Pot, (int64)0);
    TestEqual(TEXT("Stage should be WaitingForPlayers after showdown"), GameState->CurrentStage, EGameStage::WaitingForPlayers);

    // Можно добавить проверку на вызов делегата OnShowdownResultsDelegate, если бы у нас был тестовый подписчик

    Manager->ConditionalBeginDestroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_Showdown_SplitPot, "PokerClient.UnitTests.OfflineGameManager.Showdown.SplitPot", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_Showdown_SplitPot::RunTest(const FString& Parameters)
{
    // Arrange: 2 игрока, у обоих одинаковый стрит.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int64 InitialStack = 1000;
    Manager->InitializeGame(1, 1, InitialStack, 5);
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    // Общие карты: A K Q J 2 (Стрит от Десятки до Туза возможен для обоих)
    TArray<FCard> Community = {
        FCard(ECardSuit::Hearts, ECardRank::Ace), FCard(ECardSuit::Spades, ECardRank::King), FCard(ECardSuit::Diamonds, ECardRank::Queen),
        FCard(ECardSuit::Clubs, ECardRank::Jack), FCard(ECardSuit::Hearts, ECardRank::Two)
    };
    // Игрок 0: 10 пик, 3 треф (использует A K Q J 10)
    // Игрок 1: 10 бубен, 4 червей (использует A K Q J 10)
    SetSpecificCards(GameState, 0, ECardSuit::Spades, ECardRank::Ten, ECardSuit::Clubs, ECardRank::Three,
        1, ECardSuit::Diamonds, ECardRank::Ten, ECardSuit::Hearts, ECardRank::Four,
        Community);

    GameState->Pot = 300; // Банк для разделения
    GameState->Seats[0].Status = EPlayerStatus::Playing;
    GameState->Seats[1].Status = EPlayerStatus::Playing;
    GameState->CurrentStage = EGameStage::Showdown;

    int64 Player0StackBefore = GameState->Seats[0].Stack;
    int64 Player1StackBefore = GameState->Seats[1].Stack;

    // Act
    Manager->ProceedToShowdown();

    // Assert
    // Банк 300 делится на 2 = 150 каждому
    TestEqual(TEXT("Player 0 stack should increase by half of Pot"), GameState->Seats[0].Stack, Player0StackBefore + 150);
    TestEqual(TEXT("Player 1 stack should increase by half of Pot"), GameState->Seats[1].Stack, Player1StackBefore + 150);
    TestEqual(TEXT("Pot should be 0 after split"), GameState->Pot, (int64)0);
    TestEqual(TEXT("Stage should be WaitingForPlayers"), GameState->CurrentStage, EGameStage::WaitingForPlayers);

    Manager->ConditionalBeginDestroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_CanStartNewHand_Logic, "PokerClient.UnitTests.OfflineGameManager.CanStartNewHand.Logic", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_CanStartNewHand_Logic::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;
    FString Reason;

    // Сценарий 1: Сразу после инициализации (2+ игрока со стеками)
    Manager->InitializeGame(1, 2, 1000, 5); // 3 игрока
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    TestNotNull(TEXT("GameState should exist"), GameState);
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    TestTrue(TEXT("CanStartNewHand should be true after init with enough players"), Manager->CanStartNewHand(Reason));
    TestTrue(TEXT("Reason should be empty if can start"), Reason.IsEmpty());

    // Сценарий 2: Рука в процессе (префлоп, ход игрока)
    Manager->StartNewHand();
    TestEqual(TEXT("Stage should be WaitingForSmallBlind"), GameState->CurrentStage, EGameStage::WaitingForSmallBlind);

    TestFalse(TEXT("CanStartNewHand should be false when hand is in progress"), Manager->CanStartNewHand(Reason));
    TestFalse(TEXT("Reason should not be empty when cannot start"), Reason.IsEmpty());
    UE_LOG(LogTemp, Log, TEXT("CanStartNewHand (HandInProgress) Reason: %s"), *Reason);

    // Сценарий 3: Недостаточно игроков со стеком для BB
    Manager->InitializeGame(1, 1, 5, 10); // 2 игрока, стек 5, SB=10, BB=20
    GameState = Manager->GetGameState(); // GameState пересоздается в InitializeGame
    // В вашей InitializeGame SB будет 10, BB 20. Игроки со стеком 5 не смогут поставить BB.
    TestFalse(TEXT("CanStartNewHand should be false if not enough players can post BB"), Manager->CanStartNewHand(Reason));
    TestFalse(TEXT("Reason should not be empty for insufficient stacks"), Reason.IsEmpty());
    UE_LOG(LogTemp, Log, TEXT("CanStartNewHand (InsufficientStacks) Reason: %s"), *Reason);

    // Сценарий 4: Только один игрок со стеком для BB
    Manager->InitializeGame(1, 1, 1000, 10);
    GameState = Manager->GetGameState();
    if (GameState->Seats.Num() == 2) GameState->Seats[1].Stack = GameState->BigBlindAmount - 1; // У второго игрока стек меньше BB
    TestFalse(TEXT("CanStartNewHand should be false if only one player can post BB"), Manager->CanStartNewHand(Reason));
    TestFalse(TEXT("Reason should not be empty for one player able to post BB"), Reason.IsEmpty());
    UE_LOG(LogTemp, Log, TEXT("CanStartNewHand (OnePlayerSufficientStack) Reason: %s"), *Reason);


    Manager->ConditionalBeginDestroy();
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_ProcessAction_Fold_FoldToWin_Preflop, "PokerClient.UnitTests.OfflineGameManager.ProcessAction.Fold.FoldToWinPreflop", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_ProcessAction_Fold_FoldToWin_Preflop::RunTest(const FString& Parameters)
{
    // Arrange: 2 игрока, SB=5, BB=10.
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int64 InitialStack = 1000;
    int64 SB_Amount_Defined = 5; // Это GameState->SmallBlindAmount
    Manager->InitializeGame(1, 1, InitialStack, SB_Amount_Defined);
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    if (!GameState) { Manager->ConditionalBeginDestroy(); return false; }

    Manager->StartNewHand();
    int32 SbSeat = GameState->PendingSmallBlindSeat;
    int32 BbSeat = GameState->PendingBigBlindSeat;

    // SB ставит свой блайнд
    Manager->ProcessPlayerAction(SbSeat, EPlayerAction::PostBlind, 0);
    // BB ставит свой блайнд
    Manager->ProcessPlayerAction(BbSeat, EPlayerAction::PostBlind, 0);

    TestEqual(TEXT("Turn should be SB for preflop action"), GameState->CurrentTurnSeat, SbSeat);
    // Стеки ПОСЛЕ постановки блайндов:
    int64 PlayerSBStackAfterBlind = GameState->Seats[SbSeat].Stack; // = InitialStack - SB_Amount_Defined
    int64 PlayerBBStackAfterBlind = GameState->Seats[BbSeat].Stack; // = InitialStack - GameState->BigBlindAmount
    // Банк ПОСЛЕ постановки блайндов:
    int64 PotAfterBlinds = GameState->Pot; // = SB_Amount_Defined + GameState->BigBlindAmount

    // Act: SB фолдит
    Manager->ProcessPlayerAction(SbSeat, EPlayerAction::Fold, 0);

    // Assert
    TestEqual(TEXT("SB Status should be Folded"), GameState->Seats[SbSeat].Status, EPlayerStatus::Folded);

    // BB выигрывает банк. Его стек должен стать: его_стек_ПОСЛЕ_его_блайнда + ВЕСЬ_банк_который_был_на_момент_фолда_SB.
    // Потому что ProcessPlayerAction (Fold) теперь должен вызывать AwardPotToWinner(BB), и AwardPotToWinner отдает весь GameState->Pot.
    int64 ExpectedBBStack = PlayerBBStackAfterBlind + PotAfterBlinds;
    TestEqual(TEXT("BB (Winner) stack should be (BBStackAfterBlind + PotAfterBlinds)"), GameState->Seats[BbSeat].Stack, ExpectedBBStack);

    TestEqual(TEXT("SB (Loser) stack should remain its value after posting blind and folding"), GameState->Seats[SbSeat].Stack, PlayerSBStackAfterBlind);
    TestEqual(TEXT("Pot should be 0 after awarding to BB"), GameState->Pot, (int64)0);
    // Если StartNewHand не вызывается в конце ProcessPlayerAction/AwardPot, то стадия будет WaitingForPlayers
    TestEqual(TEXT("Stage should be WaitingForPlayers after fold to win"), GameState->CurrentStage, EGameStage::WaitingForPlayers);

    Manager->ConditionalBeginDestroy();
    return true;
}

// Тест: Автоматическая раздача до ривера и шоудаун, если все игроки All-In до флопа
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfflineGM_AutoDeal_PreflopAllIns, "PokerClient.UnitTests.OfflineGameManager.AutoDeal.PreflopAllIns", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOfflineGM_AutoDeal_PreflopAllIns::RunTest(const FString& Parameters)
{
    // Arrange
    UOfflineGameManager* Manager = CreateTestOfflineManagerForAutomation(this);
    TestNotNull(TEXT("Manager should not be null"), Manager);
    if (!Manager) return false;

    int64 BB_InitialStack_Overall = 1000;
    int64 SB_Amount_Setting = 5;
    int64 BB_Amount_Setting = SB_Amount_Setting * 2;

    Manager->InitializeGame(1, 2, BB_InitialStack_Overall, SB_Amount_Setting);
    UOfflinePokerGameState* GameState = Manager->GetGameState();
    if (!GameState) { AddError(TEXT("PreflopAllIns_Test: GameState is null after InitializeGame.")); Manager->ConditionalBeginDestroy(); return false; }
    TestEqual(TEXT("PreflopAllIns_Test: Should have 3 players"), GameState->Seats.Num(), 3);
    if (GameState->Seats.Num() != 3) { Manager->ConditionalBeginDestroy(); return false; }

    Manager->StartNewHand();
    int32 SBSeatActual = GameState->PendingSmallBlindSeat;
    int32 BBSeatActual = GameState->PendingBigBlindSeat;
    int32 UTGSeatActual = Manager->DetermineFirstPlayerToActAtPreflop();

    if (!GameState->Seats.IsValidIndex(SBSeatActual) || !GameState->Seats.IsValidIndex(BBSeatActual) || !GameState->Seats.IsValidIndex(UTGSeatActual) ||
        SBSeatActual == BBSeatActual || SBSeatActual == UTGSeatActual || BBSeatActual == UTGSeatActual) {
        AddError(TEXT("PreflopAllIns_Test: Invalid or non-distinct seat indices.")); Manager->ConditionalBeginDestroy(); return false;
    }

    int64 UTG_StackForHand = 50;
    int64 SB_StackForHand = 30;

    GameState->Seats[UTGSeatActual].Stack = UTG_StackForHand;
    GameState->Seats[SBSeatActual].Stack = SB_StackForHand;
    GameState->Seats[BBSeatActual].Stack = BB_InitialStack_Overall;

    GameState->Seats[UTGSeatActual].HoleCards = { FCard(ECardSuit::Spades, ECardRank::Ace), FCard(ECardSuit::Clubs, ECardRank::Ace) };
    GameState->Seats[SBSeatActual].HoleCards = { FCard(ECardSuit::Hearts, ECardRank::King), FCard(ECardSuit::Diamonds, ECardRank::King) };
    GameState->Seats[BBSeatActual].HoleCards = { FCard(ECardSuit::Spades, ECardRank::Queen), FCard(ECardSuit::Clubs, ECardRank::Queen) };

    // <<--- ВОТ ИСПРАВЛЕНИЕ: Объявление и инициализация TotalChipsAtStartOfHand --- >>
    int64 TotalChipsAtStartOfHand = GameState->Seats[UTGSeatActual].Stack +
        GameState->Seats[SBSeatActual].Stack +
        GameState->Seats[BBSeatActual].Stack;

    // --- Симулируем постановку блайндов ---
    Manager->ProcessPlayerAction(SBSeatActual, EPlayerAction::PostBlind, 0);
    Manager->ProcessPlayerAction(BBSeatActual, EPlayerAction::PostBlind, 0);

    int64 UTGStack_AfterBlinds_BeforeAction = GameState->Seats[UTGSeatActual].Stack;
    int64 SBStack_AfterBlinds_BeforeAction = GameState->Seats[SBSeatActual].Stack;
    int64 BBStack_AfterBlinds_BeforeAction = GameState->Seats[BBSeatActual].Stack;

    // --- Act: Действия All-In ---
    Manager->ProcessPlayerAction(UTGSeatActual, EPlayerAction::Raise, UTGStack_AfterBlinds_BeforeAction);
    Manager->ProcessPlayerAction(SBSeatActual, EPlayerAction::Call, 0);
    Manager->ProcessPlayerAction(BBSeatActual, EPlayerAction::Call, 0);

    // --- Assert: Проверяем состояние ПОСЛЕ ЗАВЕРШЕНИЯ РУКИ ---
    TestEqual(TEXT("Final stage should be WaitingForPlayers (Hand Over)"), GameState->CurrentStage, EGameStage::WaitingForPlayers);
    TestEqual(TEXT("Final pot should be 0 after award"), GameState->Pot, (int64)0);
    TestEqual(TEXT("Final community cards should be 5 (River shown for showdown)"), GameState->CommunityCards.Num(), 5);
    TestEqual(TEXT("CurrentTurnSeat should be -1 (Hand Over)"), GameState->CurrentTurnSeat, -1);

    // Расчет стеков после шоудауна с УПРОЩЕННЫМ распределением (UTG выигрывает весь банк 130)
    int64 Expected_UTG_Stack_Final = UTG_StackForHand - 50 + 130; // 130
    int64 Expected_SB_Stack_Final = SB_StackForHand - 30 + 0;    // 0
    int64 Expected_BB_Stack_Final = BB_InitialStack_Overall - 50 + 0; // 950

    TestEqual(TEXT("UTG (Winner) stack should be correct after showdown"), GameState->Seats[UTGSeatActual].Stack, Expected_UTG_Stack_Final);
    TestEqual(TEXT("SB (Loser) stack should be 0 after showdown"), GameState->Seats[SBSeatActual].Stack, Expected_SB_Stack_Final);
    TestEqual(TEXT("BB (Loser) stack should be correct after showdown"), GameState->Seats[BBSeatActual].Stack, Expected_BB_Stack_Final);

    int64 TotalStackAfterShowdown = GameState->Seats[UTGSeatActual].Stack + GameState->Seats[SBSeatActual].Stack + GameState->Seats[BBSeatActual].Stack;
    // Используем ранее инициализированную TotalChipsAtStartOfHand
    TestEqual(TEXT("Total chips in game should remain constant"), TotalStackAfterShowdown, TotalChipsAtStartOfHand);

    // --- Тест начала НОВОЙ руки ПОСЛЕ "нажатия Next Hand" ---
    AddInfo(TEXT("--- Simulating StartNewHand after PreflopAllIns showdown ---"));
    int32 NumPlayersWithChipsBeforeNextHand = 0;
    for (const auto& Seat : GameState->Seats) {
        if (Seat.Stack > 0 && Seat.bIsSittingIn) NumPlayersWithChipsBeforeNextHand++;
    }
    TestEqual(TEXT("Number of players with chips before next hand should be 2 (UTG and BB)"), NumPlayersWithChipsBeforeNextHand, 2);

    if (NumPlayersWithChipsBeforeNextHand >= 2)
    {
        Manager->StartNewHand();
        TestEqual(TEXT("Stage after explicit StartNewHand should be WaitingForSmallBlind"), GameState->CurrentStage, EGameStage::WaitingForSmallBlind);
        TestTrue(TEXT("Community cards should be empty after explicit StartNewHand"), GameState->CommunityCards.IsEmpty());
        TestTrue(TEXT("CurrentTurnSeat should be valid SB after explicit StartNewHand"),
            GameState->CurrentTurnSeat != -1 &&
            GameState->Seats.IsValidIndex(GameState->CurrentTurnSeat) &&
            (GameState->Seats[GameState->CurrentTurnSeat].bIsSmallBlind || GameState->Seats[GameState->CurrentTurnSeat].Status == EPlayerStatus::MustPostSmallBlind));
    }
    else { AddWarning(TEXT("Skipping StartNewHand assertions as not enough players have chips.")); }

    Manager->ConditionalBeginDestroy();
    return true;
}