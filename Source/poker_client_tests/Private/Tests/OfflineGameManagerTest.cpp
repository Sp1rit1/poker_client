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