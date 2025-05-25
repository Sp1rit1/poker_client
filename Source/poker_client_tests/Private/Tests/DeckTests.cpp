#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Containers/Set.h" // Для проверки уникальности карт

// ВАЖНО: Укажите правильные пути к вашим заголовочным файлам из основного модуля!
#include "poker_client/Public/Deck.h"
#include "poker_client/Public/PokerDataTypes.h"

// --- Тесты для UDeck ---

// Тест инициализации колоды
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckInitializationTest, "PokerClient.UnitTests.Deck.Initialization", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDeckInitializationTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDeck* TestDeck = NewObject<UDeck>(); // Создаем экземпляр UDeck

    // Act
    TestDeck->Initialize();

    // Assert
    TestEqual(TEXT("Deck should contain 52 cards after initialization"), TestDeck->NumCardsLeft(), 52);
    TestFalse(TEXT("Deck should not be empty after initialization"), TestDeck->IsEmpty());

    // Проверка на уникальность всех карт (более сложная, но полезная)
    if (TestDeck->NumCardsLeft() == 52)
    {
        TSet<FCard> UniqueCards; // Используем TSet для автоматической проверки уникальности (FCard должен иметь GetTypeHash и operator==)
        // Если у FCard нет GetTypeHash, нужно будет реализовать его или использовать другой способ проверки.
        // Предположим, FCard::operator== уже реализован.
        // Для TSet нужен GetTypeHash. Если его нет, можно создать TArray и сортировать/сравнивать.

// Для простоты пока проверим первые и последние несколько карт или сделаем выборочную проверку,
// если реализация GetTypeHash для FCard сейчас нежелательна.
// Полная проверка уникальности:
        TArray<FCard> AllCardsInDeck;
        AllCardsInDeck.Reserve(52);
        UDeck* TempDeckForExtraction = NewObject<UDeck>(); // Временная колода, чтобы не портить TestDeck
        TempDeckForExtraction->Initialize();
        while (!TempDeckForExtraction->IsEmpty())
        {
            TOptional<FCard> CardOpt = TempDeckForExtraction->DealCard();
            if (CardOpt.IsSet())
            {
                AllCardsInDeck.Add(CardOpt.GetValue());
                UniqueCards.Add(CardOpt.GetValue());
            }
        }
        TestEqual(TEXT("Number of unique cards should be 52"), UniqueCards.Num(), 52);
    }

    // Очистка
    TestDeck->ConditionalBeginDestroy(); // Правильное удаление UObject

    return true;
}

// Тест состояния пустой колоды
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckEmptyStateTest, "PokerClient.UnitTests.Deck.EmptyState", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDeckEmptyStateTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDeck* TestDeck = NewObject<UDeck>();
    // Не вызываем Initialize, чтобы проверить начальное состояние или состояние после очистки

    // Assert (для свежесозданного объекта, если он не инициализируется в конструкторе)
    TestTrue(TEXT("Newly created deck should be empty"), TestDeck->IsEmpty());
    TestEqual(TEXT("Newly created deck should have 0 cards"), TestDeck->NumCardsLeft(), 0);

    // Arrange 2: инициализируем и очищаем
    TestDeck->Initialize();
    while (!TestDeck->IsEmpty())
    {
        TestDeck->DealCard();
    }

    // Assert 2
    TestTrue(TEXT("Deck should be empty after dealing all cards"), TestDeck->IsEmpty());
    TestEqual(TEXT("Deck should have 0 cards after dealing all cards"), TestDeck->NumCardsLeft(), 0);

    // Очистка
    TestDeck->ConditionalBeginDestroy();

    return true;
}

// Тест раздачи карт
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckDealCardTest, "PokerClient.UnitTests.Deck.DealCard", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDeckDealCardTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDeck* TestDeck = NewObject<UDeck>();
    TestDeck->Initialize();
    int32 InitialCardCount = TestDeck->NumCardsLeft();

    // Act: Раздаем одну карту
    TOptional<FCard> CardOpt1 = TestDeck->DealCard();

    // Assert 1
    TestTrue(TEXT("Dealing a card from a full deck should return a valid card"), CardOpt1.IsSet());
    if (CardOpt1.IsSet())
    {
        // Здесь можно было бы проверить, что это какая-то из известных карт, но без знания порядка это сложно.
        // Просто проверяем, что карта существует.
        UE_LOG(LogTemp, Log, TEXT("Dealt card 1: %s"), *CardOpt1.GetValue().ToString());
    }
    TestEqual(TEXT("Card count should decrease by 1 after dealing one card"), TestDeck->NumCardsLeft(), InitialCardCount - 1);

    // Act: Раздаем все карты
    TArray<FCard> DealtCards;
    DealtCards.Reserve(InitialCardCount);
    if (CardOpt1.IsSet()) DealtCards.Add(CardOpt1.GetValue()); // Добавляем уже розданную

    while (!TestDeck->IsEmpty())
    {
        TOptional<FCard> CardOpt = TestDeck->DealCard();
        if (CardOpt.IsSet())
        {
            DealtCards.Add(CardOpt.GetValue());
        }
        else
        {
            AddError(TEXT("DealCard returned an unset optional while deck was not reported empty."));
            TestDeck->ConditionalBeginDestroy();
            return false;
        }
    }

    // Assert 2
    TestEqual(TEXT("Total dealt cards should match initial card count"), DealtCards.Num(), InitialCardCount);
    TestTrue(TEXT("Deck should be empty after dealing all cards"), TestDeck->IsEmpty());
    TestEqual(TEXT("Card count should be 0 after dealing all cards"), TestDeck->NumCardsLeft(), 0);

    // Act: Попытка раздать из пустой колоды
    TOptional<FCard> CardOptEmpty = TestDeck->DealCard();

    // Assert 3
    TestFalse(TEXT("Dealing from an empty deck should return an unset optional"), CardOptEmpty.IsSet());

    // Очистка
    TestDeck->ConditionalBeginDestroy();

    return true;
}

// Тест перемешивания (базовая проверка)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckShuffleTest, "PokerClient.UnitTests.Deck.Shuffle", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDeckShuffleTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDeck* Deck1 = NewObject<UDeck>();
    Deck1->Initialize();
    TArray<FCard> InitialOrderDeck1;
    for (int i = 0; i < 52; ++i) { // Копируем карты, не раздавая, чтобы сохранить порядок
        TOptional<FCard> card = Deck1->DealCard(); // Раздаем, чтобы получить их в обратном порядке (как они в Cards)
        if (card.IsSet()) InitialOrderDeck1.Insert(card.GetValue(), 0); // Вставляем в начало, чтобы сохранить порядок инициализации
    }
    Deck1->Initialize(); // Восстанавливаем Deck1 для перемешивания

    UDeck* Deck2 = NewObject<UDeck>(); // Второй экземпляр для сравнения
    Deck2->Initialize();
    TArray<FCard> InitialOrderDeck2;
    for (int i = 0; i < 52; ++i) {
        TOptional<FCard> card = Deck2->DealCard();
        if (card.IsSet()) InitialOrderDeck2.Insert(card.GetValue(), 0);
    }
    Deck2->Initialize();


    // Assert: начальные порядки должны совпадать (если Initialize всегда создает в одном порядке)
    bool bInitialOrdersMatch = true;
    if (InitialOrderDeck1.Num() == 52 && InitialOrderDeck2.Num() == 52) {
        for (int32 i = 0; i < 52; ++i) {
            if (InitialOrderDeck1[i] != InitialOrderDeck2[i]) {
                bInitialOrdersMatch = false;
                break;
            }
        }
    }
    else {
        bInitialOrdersMatch = false;
    }
    TestTrue(TEXT("Initially, two freshly initialized decks should have cards in the same order"), bInitialOrdersMatch);


    // Act
    Deck1->Shuffle();
    Deck1->Shuffle(); // Перемешиваем несколько раз для большей случайности

    // Assert: после перемешивания, порядок карт в Deck1 очень маловероятно будет таким же, как начальный
    // И он очень маловероятно будет таким же, как в Deck2 (который не перемешивался)
    // Это не 100% гарантия, но для юнит-теста достаточно.
    if (Deck1->NumCardsLeft() == 52 && InitialOrderDeck2.Num() == 52) // Убедимся, что карты не пропали
    {
        bool bOrderChanged = false;
        for (int32 i = 0; i < 52; ++i)
        {
            TOptional<FCard> ShuffledCardOpt = Deck1->DealCard(); // Раздаем из перемешанной колоды
            if (!ShuffledCardOpt.IsSet()) {
                AddError(FString::Printf(TEXT("Shuffle test: Shuffled deck ran out of cards prematurely at index %d"), i));
                Deck1->ConditionalBeginDestroy(); Deck2->ConditionalBeginDestroy();
                return false;
            }
            FCard ShuffledCard = ShuffledCardOpt.GetValue();
            // Сравниваем с НЕперемешанной колодой (InitialOrderDeck2)
            if (ShuffledCard != InitialOrderDeck2[51 - i]) // Сравниваем с обратным порядком, так как DealCard берет с конца
            {
                bOrderChanged = true;
                // Можно выйти из цикла раньше, но для полноты можно проверить все
                // break; 
            }
        }
        TestTrue(TEXT("After shuffling, the order of cards should likely be different from an un-shuffled deck"), bOrderChanged);
        if (!bOrderChanged)
        {
            AddWarning(TEXT("Shuffle test: Order did not change after shuffle. This is statistically very unlikely but possible. Run test again."));
        }
    }
    else
    {
        AddError(TEXT("Shuffle test: Card count changed after shuffle or initial deck was not 52."));
    }

    // Тест на перемешивание пустой колоды (не должно вызывать крэш)
    UDeck* EmptyDeck = NewObject<UDeck>();
    EmptyDeck->Shuffle(); // Вызываем на пустой
    TestTrue(TEXT("Shuffling an empty deck should not crash and deck should remain empty"), EmptyDeck->IsEmpty());


    // Очистка
    Deck1->ConditionalBeginDestroy();
    Deck2->ConditionalBeginDestroy();
    EmptyDeck->ConditionalBeginDestroy();

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckTest_IntegrityAfterShuffleAndPartialDeal, "PokerClient.UnitTests.Deck.IntegrityAfterShuffleAndPartialDeal", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDeckTest_IntegrityAfterShuffleAndPartialDeal::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("--- Test: Deck Integrity After Shuffle and Partial Deal ---"));

    // 1. ARRANGE
    UDeck* TestDeck = NewObject<UDeck>(GetTransientPackage());
    if (!TestDeck)
    {
        AddError(TEXT("Failed to create UDeck instance."));
        return false;
    }

    TestDeck->Initialize();
    TestEqual(TEXT("Initial card count should be 52"), TestDeck->NumCardsLeft(), 52);

    // 2. ACT: Несколько перемешиваний
    TestDeck->Shuffle();
    TestDeck->Shuffle();
    TestDeck->Shuffle();
    AddInfo(TEXT("Deck shuffled multiple times."));
    TestEqual(TEXT("Card count should still be 52 after shuffles"), TestDeck->NumCardsLeft(), 52);


    // 3. ACT: Частичная раздача (например, 10 карт)
    int32 NumCardsToDealPartially = 10;
    TArray<FCard> PartiallyDealtCards;
    PartiallyDealtCards.Reserve(NumCardsToDealPartially);

    for (int32 i = 0; i < NumCardsToDealPartially; ++i)
    {
        TOptional<FCard> CardOpt = TestDeck->DealCard();
        if (!CardOpt.IsSet())
        {
            AddError(FString::Printf(TEXT("DealCard returned empty optional prematurely at card %d during partial deal."), i + 1));
            TestDeck->ConditionalBeginDestroy();
            return false;
        }
        PartiallyDealtCards.Add(CardOpt.GetValue());
    }
    AddInfo(FString::Printf(TEXT("Dealt %d cards partially."), NumCardsToDealPartially));
    TestEqual(TEXT("Card count should be (52 - partial deal count) after partial deal"), TestDeck->NumCardsLeft(), 52 - NumCardsToDealPartially);

    // 4. ASSERT: Проверка оставшихся карт + ранее розданных на общую уникальность и полноту
    TSet<FCard> AllDealtCardsSet;
    // Добавляем частично розданные карты в сет
    for (const FCard& Card : PartiallyDealtCards)
    {
        AllDealtCardsSet.Add(Card);
    }

    // Раздаем оставшиеся карты и добавляем их в тот же сет
    int32 RemainingCardsToDeal = TestDeck->NumCardsLeft();
    AddInfo(FString::Printf(TEXT("Dealing remaining %d cards."), RemainingCardsToDeal));

    for (int32 i = 0; i < RemainingCardsToDeal; ++i)
    {
        TOptional<FCard> CardOpt = TestDeck->DealCard();
        if (!CardOpt.IsSet())
        {
            AddError(FString::Printf(TEXT("DealCard returned empty optional prematurely at remaining card %d."), i + 1));
            TestDeck->ConditionalBeginDestroy();
            return false;
        }
        AllDealtCardsSet.Add(CardOpt.GetValue());
    }

    TestTrue(TEXT("Deck should be empty after dealing all remaining cards"), TestDeck->IsEmpty());
    TestEqual(TEXT("Number of unique cards from all dealt cards (partial + remaining) should be 52"), AllDealtCardsSet.Num(), 52);

    // (Опционально, но полезно) Проверить, что все 52 стандартные карты присутствуют в AllDealtCardsSet
    // Это дублирует часть логики из FDeckInitializationTest, но здесь проверяет после шаффлов и частичной раздачи.
    if (AllDealtCardsSet.Num() == 52)
    {
        const UEnum* SuitEnum = StaticEnum<ECardSuit>();
        const UEnum* RankEnum = StaticEnum<ECardRank>();
        bool bAllPossibleCardsPresent = true;
        if (SuitEnum && RankEnum)
        {
            for (int32 s = 0; s < SuitEnum->NumEnums() - 1; ++s)
            {
                ECardSuit ExpectedSuit = static_cast<ECardSuit>(SuitEnum->GetValueByIndex(s));
                for (int32 r = 0; r < RankEnum->NumEnums() - 1; ++r)
                {
                    ECardRank ExpectedRank = static_cast<ECardRank>(RankEnum->GetValueByIndex(r));
                    if (!AllDealtCardsSet.Contains(FCard(ExpectedSuit, ExpectedRank)))
                    {
                        AddError(FString::Printf(TEXT("Card %s%s not found in the set of all dealt cards after shuffle and partial deal."),
                            *UEnum::GetDisplayValueAsText(ExpectedRank).ToString(), *UEnum::GetDisplayValueAsText(ExpectedSuit).ToString()));
                        bAllPossibleCardsPresent = false;
                    }
                }
            }
        }
        else { AddError(TEXT("Could not get Suit/Rank enums for full verification.")); bAllPossibleCardsPresent = false; }
        TestTrue(TEXT("All 52 standard cards should be present among all dealt cards"), bAllPossibleCardsPresent);
    }

    // Очистка
    TestDeck->ConditionalBeginDestroy();
    return !HasAnyErrors();
}