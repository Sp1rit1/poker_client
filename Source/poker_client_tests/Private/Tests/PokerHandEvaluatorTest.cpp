#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include <string>        // Для std::string в проверке __VA_ARGS__
#include <type_traits>   // Для std::is_empty (хотя UE_ARRAY_COUNT лучше для C-массивов)

// ЗАМЕНИТЕ "poker_client" НА ИМЯ ВАШЕГО ОСНОВНОГО ИГРОВОГО МОДУЛЯ, ЕСЛИ ОНО ДРУГОЕ
#include "poker_client/Public/PokerDataTypes.h"
#include "poker_client/Public/PokerHandEvaluator.h"

// --- Вспомогательная структура FTestCardParser (остается без изменений) ---
struct FTestCardParser
{
    static TOptional<ECardRank> CharToRank(TCHAR Char)
    {
        switch (TChar<TCHAR>::ToUpper(Char)) // Добавим ToUpper для нечувствительности к регистру
        {
        case TEXT('2'): return ECardRank::Two; case TEXT('3'): return ECardRank::Three;
        case TEXT('4'): return ECardRank::Four; case TEXT('5'): return ECardRank::Five;
        case TEXT('6'): return ECardRank::Six; case TEXT('7'): return ECardRank::Seven;
        case TEXT('8'): return ECardRank::Eight; case TEXT('9'): return ECardRank::Nine;
        case TEXT('T'): return ECardRank::Ten; case TEXT('J'): return ECardRank::Jack;
        case TEXT('Q'): return ECardRank::Queen; case TEXT('K'): return ECardRank::King;
        case TEXT('A'): return ECardRank::Ace; default: return {};
        }
    }
    static TOptional<ECardSuit> CharToSuit(TCHAR Char)
    {
        switch (TChar<TCHAR>::ToUpper(Char)) // Добавим ToUpper
        {
        case TEXT('C'): return ECardSuit::Clubs;
        case TEXT('D'): return ECardSuit::Diamonds;
        case TEXT('H'): return ECardSuit::Hearts;
        case TEXT('S'): return ECardSuit::Spades;
        default: return {};
        }
    }
    static FCard Card(const FString& CardStr)
    {
        if (CardStr.Len() == 2)
        {
            TOptional<ECardRank> Rank = CharToRank(CardStr[0]);
            TOptional<ECardSuit> Suit = CharToSuit(CardStr[1]);
            if (Rank.IsSet() && Suit.IsSet())
            {
                return FCard(Suit.GetValue(), Rank.GetValue());
            }
        }
        UE_LOG(LogTemp, Error, TEXT("FTestCardParser::Card - Invalid card string format: %s. Returning default card (2C)."), *CardStr);
        return FCard(ECardSuit::Clubs, ECardRank::Two); // Возвращаем конкретную карту для предсказуемости
    }
    static TArray<FCard> Cards(const FString& CardsStr)
    {
        TArray<FCard> OutCards;
        TArray<FString> CardStrArray;
        CardsStr.ParseIntoArray(CardStrArray, TEXT(" "), true); // true для удаления пустых строк
        for (const FString& Str : CardStrArray)
        {
            if (!Str.IsEmpty())
            {
                FCard ParsedCard = Card(Str);
                // Проверка, что вернулась не дефолтная карта из-за ошибки парсинга
                // (можно добавить более строгую проверку, если Card() будет возвращать TOptional<FCard>)
                if (!(ParsedCard.Rank == ECardRank::Two && ParsedCard.Suit == ECardSuit::Clubs && Str.ToUpper() != TEXT("2C")))
                {
                    OutCards.Add(ParsedCard);
                }
            }
        }
        return OutCards;
    }
};

// --- ОБНОВЛЕННЫЙ Макрос TEST_HAND_VARIADIC ---
#define TEST_HAND_VARIADIC(TestName, CardsString, ExpectedRank, ...) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandEvaluatorTest_##TestName, "PokerClient.UnitTests.HandEvaluator." #TestName, EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter) \
bool FHandEvaluatorTest_##TestName::RunTest(const FString& Parameters) \
{ \
    TArray<FCard> AllCards = FTestCardParser::Cards(TEXT(CardsString)); \
    FPokerHandResult Result = UPokerHandEvaluator::EvaluatePokerHand({}, AllCards); \
    TestEqual(FString::Printf(TEXT("Hand Rank for %s (Cards: %s)"), TEXT(#TestName), TEXT(CardsString)), Result.HandRank, ExpectedRank); \
    /* Создаем массив ожидаемых кикеров из вариативных аргументов */ \
    ECardRank TempExpectedKickers[] = { __VA_ARGS__ }; \
    TArray<ECardRank> ExpectedKickersList; \
    /* Проверка, что __VA_ARGS__ не был полностью пустым (например, TEST_HAND_VARIADIC(Name, Cards, Rank) без кикеров) */ \
    /* Эта проверка может быть не идеальна для всех компиляторов, но часто работает для GNU/Clang */ \
    const char* VaArgsStrContents = #__VA_ARGS__; \
    bool bHasActualVaArgs = false; \
    for (const char* Ptr = VaArgsStrContents; *Ptr; ++Ptr) { if (!isspace(static_cast<unsigned char>(*Ptr))) { bHasActualVaArgs = true; break; } } \
    if (bHasActualVaArgs || (sizeof(TempExpectedKickers)/sizeof(ECardRank) > 0) ) \
    { \
        ExpectedKickersList.Append(TempExpectedKickers, UE_ARRAY_COUNT(TempExpectedKickers)); \
    } \
    /* Сравнение кикеров */ \
    if (Result.Kickers.Num() == ExpectedKickersList.Num()) \
    { \
        for (int32 i = 0; i < ExpectedKickersList.Num(); ++i) \
        { \
            TestEqual(FString::Printf(TEXT("Kicker %d for %s"), i, TEXT(#TestName)), Result.Kickers[i], ExpectedKickersList[i]); \
        } \
    } \
    else \
    { \
        FString ResKickersStr, ExpKickersStr; \
        for(ECardRank R : Result.Kickers) ResKickersStr += UEnum::GetDisplayValueAsText(R).ToString() + TEXT(" "); \
        for(ECardRank R : ExpectedKickersList) ExpKickersStr += UEnum::GetDisplayValueAsText(R).ToString() + TEXT(" "); \
        AddError(FString::Printf(TEXT("Incorrect number of kickers for %s. Expected %d [%s], Got %d [%s]. Input VA_ARGS: %s"), \
            TEXT(#TestName), ExpectedKickersList.Num(), *ExpKickersStr.TrimEnd(), Result.Kickers.Num(), *ResKickersStr.TrimEnd(), TEXT(#__VA_ARGS__))); \
    } \
    return !HasAnyErrors(); /* Тест пройден, если нет ошибок */ \
}

// --- ОБНОВЛЕННЫЙ Макрос TEST_COMPARE ---
#define TEST_COMPARE(TestName, HandAString, HandBString, ExpectedComparisonResultSign) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandEvaluatorTest_Compare_##TestName, "PokerClient.UnitTests.HandEvaluator.Compare." #TestName, EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter) \
bool FHandEvaluatorTest_Compare_##TestName::RunTest(const FString& Parameters) \
{ \
    TArray<FCard> CardsA_Input = FTestCardParser::Cards(TEXT(HandAString)); \
    TArray<FCard> CardsB_Input = FTestCardParser::Cards(TEXT(HandBString)); \
    FPokerHandResult ResultA = UPokerHandEvaluator::EvaluatePokerHand({}, CardsA_Input); \
    FPokerHandResult ResultB = UPokerHandEvaluator::EvaluatePokerHand({}, CardsB_Input); \
    int32 ActualComparison = UPokerHandEvaluator::CompareHandResults(ResultA, ResultB); \
    bool bTestPassedLogic = false; \
    if (ExpectedComparisonResultSign > 0) { bTestPassedLogic = ActualComparison > 0; } \
    else if (ExpectedComparisonResultSign < 0) { bTestPassedLogic = ActualComparison < 0; } \
    else { bTestPassedLogic = ActualComparison == 0; } \
    TestTrue(FString::Printf(TEXT("Comparison result for %s. ExpectedSign: %d, Actual: %d. A:[%s] vs B:[%s]"), \
        TEXT(#TestName), ExpectedComparisonResultSign, ActualComparison, \
        *UEnum::GetDisplayValueAsText(ResultA.HandRank).ToString(), \
        *UEnum::GetDisplayValueAsText(ResultB.HandRank).ToString()), bTestPassedLogic); \
    if (!bTestPassedLogic) \
    { \
        /* Дополнительный лог для упавшего сравнения с кикерами */ \
        FString KickersAStr, KickersBStr; \
        for(ECardRank R : ResultA.Kickers) KickersAStr += UEnum::GetDisplayValueAsText(R).ToString() + TEXT(" "); \
        for(ECardRank R : ResultB.Kickers) KickersBStr += UEnum::GetDisplayValueAsText(R).ToString() + TEXT(" "); \
        AddError(FString::Printf(TEXT("Compare Detail %s: A Kickers [%s] | B Kickers [%s]"), \
            TEXT(#TestName), *KickersAStr.TrimEnd(), *KickersBStr.TrimEnd())); \
    } \
    return !HasAnyErrors(); \
}

// --- Тесты для Старшей Карты (High Card) ---
TEST_HAND_VARIADIC(HighCard_AceHigh, "AH KD QS JC 9C", EPokerHandRank::HighCard, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Nine)
TEST_HAND_VARIADIC(HighCard_KingHigh, "KH QS JD TC 8D", EPokerHandRank::HighCard, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Ten, ECardRank::Eight)
TEST_HAND_VARIADIC(HighCard_TenHighMixed, "2C 4D 6H 8S TH", EPokerHandRank::HighCard, ECardRank::Ten, ECardRank::Eight, ECardRank::Six, ECardRank::Four, ECardRank::Two)

// --- Тесты для Одной Пары (One Pair) ---
TEST_HAND_VARIADIC(OnePair_Aces, "AH AD KS QC JD", EPokerHandRank::OnePair, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack)
TEST_HAND_VARIADIC(OnePair_Kings, "KH KD AS QC JD", EPokerHandRank::OnePair, ECardRank::King, ECardRank::Ace, ECardRank::Queen, ECardRank::Jack)
TEST_HAND_VARIADIC(OnePair_Deuces, "2H 2D AS QC JD", EPokerHandRank::OnePair, ECardRank::Two, ECardRank::Ace, ECardRank::Queen, ECardRank::Jack)
TEST_HAND_VARIADIC(OnePair_Sevens_From7, "7C 7S AH KH QH 2D 3S", EPokerHandRank::OnePair, ECardRank::Seven, ECardRank::Ace, ECardRank::King, ECardRank::Queen)

// --- Тесты для Двух Пар (Two Pair) ---
TEST_HAND_VARIADIC(TwoPair_AcesAndKings, "AH AD KH KD QC", EPokerHandRank::TwoPair, ECardRank::Ace, ECardRank::King, ECardRank::Queen)
TEST_HAND_VARIADIC(TwoPair_KingsAndQueens, "KH KD QH QD AC", EPokerHandRank::TwoPair, ECardRank::King, ECardRank::Queen, ECardRank::Ace)
TEST_HAND_VARIADIC(TwoPair_TensAndNines, "TH TD 9H 9D AC", EPokerHandRank::TwoPair, ECardRank::Ten, ECardRank::Nine, ECardRank::Ace)
TEST_HAND_VARIADIC(TwoPair_FivesAndDeuces, "5H 5D 2H 2D AC", EPokerHandRank::TwoPair, ECardRank::Five, ECardRank::Two, ECardRank::Ace)
TEST_HAND_VARIADIC(TwoPair_AcesKings_From6, "AH AD KH KD QC JS", EPokerHandRank::TwoPair, ECardRank::Ace, ECardRank::King, ECardRank::Queen)
TEST_HAND_VARIADIC(Problem_ThreePairsSelectsTopTwo, "AH AD KH KD QH QC 2S", EPokerHandRank::TwoPair, ECardRank::Ace, ECardRank::King, ECardRank::Queen) // Изменен с 6 на 7 карт

// --- Тесты для Сета/Тройки (Three of a Kind) ---
TEST_HAND_VARIADIC(ThreeOfAKind_Aces, "AH AD AC KH QC", EPokerHandRank::ThreeOfAKind, ECardRank::Ace, ECardRank::King, ECardRank::Queen)
TEST_HAND_VARIADIC(ThreeOfAKind_Kings, "KH KD KC AH QC", EPokerHandRank::ThreeOfAKind, ECardRank::King, ECardRank::Ace, ECardRank::Queen)
TEST_HAND_VARIADIC(ThreeOfAKind_Deuces, "2H 2D 2C AH QC", EPokerHandRank::ThreeOfAKind, ECardRank::Two, ECardRank::Ace, ECardRank::Queen)
TEST_HAND_VARIADIC(ThreeOfAKind_Aces_From7, "AH AD AC KH QC JS 9H", EPokerHandRank::ThreeOfAKind, ECardRank::Ace, ECardRank::King, ECardRank::Queen)
TEST_HAND_VARIADIC(Problem_SetQueensAKKicker, "QD QS QC AH KH", EPokerHandRank::ThreeOfAKind, ECardRank::Queen, ECardRank::Ace, ECardRank::King)


// --- Тесты для Стрита (Straight) ---
TEST_HAND_VARIADIC(Straight_AceHigh, "AS KD QH JC TH", EPokerHandRank::Straight, ECardRank::Ace)
TEST_HAND_VARIADIC(Straight_Wheel, "5H 4D 3C 2S AH", EPokerHandRank::Straight, ECardRank::Five)
TEST_HAND_VARIADIC(Straight_TenHigh, "TH 9D 8C 7S 6H", EPokerHandRank::Straight, ECardRank::Ten)
TEST_HAND_VARIADIC(Straight_KingHigh_From7, "KC QD JH TS 9D 8S 2C", EPokerHandRank::Straight, ECardRank::King)

// --- Тесты для Флеша (Flush) ---
TEST_HAND_VARIADIC(Flush_AceHighHearts, "AH KH QH JH 2H", EPokerHandRank::Flush, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Two)
TEST_HAND_VARIADIC(Flush_KingHighSpades, "KS QS JS TS 8S", EPokerHandRank::Flush, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Ten, ECardRank::Eight)
TEST_HAND_VARIADIC(Flush_TenHighDiamonds, "2D 4D 6D 8D TD", EPokerHandRank::Flush, ECardRank::Ten, ECardRank::Eight, ECardRank::Six, ECardRank::Four, ECardRank::Two)
TEST_HAND_VARIADIC(Flush_AceHighClubs_From7, "AC KC QC JC TC 9C 8D", EPokerHandRank::Flush, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Ten)

// --- Тесты для Фулл-Хауса (Full House) ---
TEST_HAND_VARIADIC(FullHouse_AcesOverKings, "AH AD AC KH KD", EPokerHandRank::FullHouse, ECardRank::Ace, ECardRank::King)
TEST_HAND_VARIADIC(FullHouse_KingsOverAces, "KH KD KC AH AD", EPokerHandRank::FullHouse, ECardRank::King, ECardRank::Ace)
TEST_HAND_VARIADIC(FullHouse_DeucesOverThrees, "2H 2D 2C 3H 3D", EPokerHandRank::FullHouse, ECardRank::Two, ECardRank::Three)
TEST_HAND_VARIADIC(FullHouse_SevensOverAces_From7, "7C 7S 7H AH AS KD QS", EPokerHandRank::FullHouse, ECardRank::Seven, ECardRank::Ace)

// --- Тесты для Каре (Four of a Kind) ---
TEST_HAND_VARIADIC(FourOfAKind_Aces, "AH AD AC AS KH", EPokerHandRank::FourOfAKind, ECardRank::Ace, ECardRank::King)
TEST_HAND_VARIADIC(FourOfAKind_Kings, "KH KD KC KS AH", EPokerHandRank::FourOfAKind, ECardRank::King, ECardRank::Ace)
TEST_HAND_VARIADIC(FourOfAKind_Deuces, "2H 2D 2C 2S AH", EPokerHandRank::FourOfAKind, ECardRank::Two, ECardRank::Ace)
TEST_HAND_VARIADIC(FourOfAKind_Jacks_From7, "JC JS JD JH AC KD QS", EPokerHandRank::FourOfAKind, ECardRank::Jack, ECardRank::Ace)

// --- Тесты для Стрит-Флеша (Straight Flush) ---
TEST_HAND_VARIADIC(StraightFlush_KingHighHearts, "KH QH JH TH 9H", EPokerHandRank::StraightFlush, ECardRank::King)
TEST_HAND_VARIADIC(StraightFlush_SteelWheelSpades, "5S 4S 3S 2S AS", EPokerHandRank::StraightFlush, ECardRank::Five)
TEST_HAND_VARIADIC(StraightFlush_NineHighDiamonds, "9D 8D 7D 6D 5D", EPokerHandRank::StraightFlush, ECardRank::Nine)
TEST_HAND_VARIADIC(StraightFlush_EightHighClubs_From7, "8C 7C 6C 5C 4C 3C 2S", EPokerHandRank::StraightFlush, ECardRank::Eight)

// --- Тесты для Роял-Флеша (Royal Flush) ---
// Для Роял Флеша, как и для Стрит-Флеша, кикером обычно считается старшая карта (Туз)
TEST_HAND_VARIADIC(RoyalFlush_Hearts, "AH KH QH JH TH", EPokerHandRank::RoyalFlush, ECardRank::Ace)
TEST_HAND_VARIADIC(RoyalFlush_Spades, "AS KS QS JS TS", EPokerHandRank::RoyalFlush, ECardRank::Ace)
TEST_HAND_VARIADIC(RoyalFlush_Clubs_From7, "AC KC QC JC TC KD QD", EPokerHandRank::RoyalFlush, ECardRank::Ace)
// Если для рояля не ожидается кикеров (массив пуст), то так:
// TEST_HAND_VARIADIC(RoyalFlush_NoKickersTest, "AC KC QC JC TC", EPokerHandRank::RoyalFlush) 


// --- Тесты для Сравнения Рук (CompareHandResults) ---
TEST_COMPARE(StraightFlush_vs_FourAces, "KH QH JH TH 9H", "AS AD AC AH KH", 1)
TEST_COMPARE(FourAces_vs_FullHouseKQQ, "AS AD AC AH KH", "KC KD KS QH QD", 1)
TEST_COMPARE(FullHouseAAA_vs_FullHouseKKK, "AS AD AC KH KD", "KC KD KS AH AD", 1)
TEST_COMPARE(FullHouseAAK_vs_FullHouseAAQ, "AS AD AC KH KD", "AC AH AS QH QD", 1)
TEST_COMPARE(FlushA_vs_FlushK, "AH KH QH JH 2H", "KS QS JS TS 8S", 1)
TEST_COMPARE(FlushAKQJ9_vs_FlushAKQJ8, "AH KH QH JH 9H", "AS KS QS JS 8S", 1)
TEST_COMPARE(StraightA_vs_StraightK, "AS KD QH JC TH", "KC QD JH TS 9D", 1)
TEST_COMPARE(SetAces_vs_SetKings, "AS AD AC KH QD", "KC KD KS AH QH", 1)
TEST_COMPARE(SetAcesKQ_vs_SetAcesKJ, "AS AD AC KH QD", "AC AH AS KH JD", 1)
TEST_COMPARE(TwoPairAAKKQ_vs_TwoPairAAKKJ, "AS AD KC KD QH", "AC AH KS KH JD", 1)
TEST_COMPARE(TwoPairAAKK_vs_TwoPairAAQQ, "AS AD KC KD QH", "AC AH QS QD JH", 1)
TEST_COMPARE(TwoPairAAKK_vs_TwoPairKKQQ, "AS AD KC KD QH", "KC KH QS QD JH", 1)
TEST_COMPARE(PairAcesKQJ_vs_PairAcesKQT, "AS AD KH QC JD", "AC AH KS QH TD", 1)
TEST_COMPARE(PairAces_vs_PairKings, "AS AD KH QC JD", "KC KD AH QS JH", 1)
TEST_COMPARE(HighCardAKQJ9_vs_HighCardAKQJ8, "AS KH QC JD 9C", "AC KD QH JS 8D", 1)
TEST_COMPARE(SplitPot_FlushAceHigh, "AH KH QH JH 2H", "AS KS QS JS 2S", 0)
TEST_COMPARE(SplitPot_StraightKingHigh, "KH QC JD TS 9H", "KS QD JH TC 9S", 0)

// --- Тесты на Граничные Случаи и Проблемные Комбинации ---
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandEvaluatorTest_Complex_FlushOverStraightOn7Cards, "PokerClient.UnitTests.HandEvaluator.Complex.FlushOverStraight7", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FHandEvaluatorTest_Complex_FlushOverStraightOn7Cards::RunTest(const FString& Parameters)
{
    TArray<FCard> Hole1 = FTestCardParser::Cards(TEXT("2H 3H")); // Игрок 1 пытается собрать флеш
    TArray<FCard> Community = FTestCardParser::Cards(TEXT("AH KH QH JS TC")); // На борде Роял Флеш
    FPokerHandResult Result1 = UPokerHandEvaluator::EvaluatePokerHand(Hole1, Community);
    TestEqual(TEXT("Player 1 should have RoyalFlush (using board)"), Result1.HandRank, EPokerHandRank::RoyalFlush);
    TestTrue(TEXT("Player 1 Kickers for RF should contain Ace"), Result1.Kickers.Contains(ECardRank::Ace));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandEvaluatorTest_Complex_FullHouseOverFlushOn7Cards, "PokerClient.UnitTests.HandEvaluator.Complex.FullHouseOverFlush7", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FHandEvaluatorTest_Complex_FullHouseOverFlushOn7Cards::RunTest(const FString& Parameters)
{
    TArray<FCard> Hole1 = FTestCardParser::Cards(TEXT("AC AD"));
    TArray<FCard> Hole2 = FTestCardParser::Cards(TEXT("KC KD"));
    TArray<FCard> Community = FTestCardParser::Cards(TEXT("AH AS QH JS TC")); // На борде два туза
    FPokerHandResult Result1 = UPokerHandEvaluator::EvaluatePokerHand(Hole1, Community); // Каре тузов
    FPokerHandResult Result2 = UPokerHandEvaluator::EvaluatePokerHand(Hole2, Community); // Фулл хаус короли на тузах
    TestEqual(TEXT("Player 1 should have FourOfAKind (Aces)"), Result1.HandRank, EPokerHandRank::FourOfAKind);
    TestEqual(TEXT("Player 2 should have FullHouse (Kings over Aces)"), Result2.HandRank, EPokerHandRank::FullHouse);
    TestTrue(TEXT("Four of a Kind (Player 1) should beat Full House (Player 2)"), UPokerHandEvaluator::CompareHandResults(Result1, Result2) > 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandEvaluatorTest_SeparateStraightAndFlushNotStraightFlush, "PokerClient.UnitTests.HandEvaluator.Complex.SeparateStraightFlush", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FHandEvaluatorTest_SeparateStraightAndFlushNotStraightFlush::RunTest(const FString& Parameters)
{
    // Игрок: AH 2H (Пытается собрать Флеш черв)
    // Борд:  3H 4H 5D 6D 7S (На борде есть 3-7 стрит разных мастей, и три червы)
    // Лучшая рука игрока: Флеш черв (AH, 2H, 3H, 4H + старшая из 5D, 6D, 7S, если бы это было 5 карт для флеша)
    // Нет, для флеша нужны 5 карт ОДНОЙ масти. AH 2H 3H 4H + любая_пятая_черва.
    // Если на борде 3H 4H 8H KD QS -> у игрока флеш AH 8H 4H 3H 2H
    TArray<FCard> HoleCards = FTestCardParser::Cards(TEXT("AH 2H"));
    TArray<FCard> CommunityCards = FTestCardParser::Cards(TEXT("3H 4H 8H KD QS"));
    FPokerHandResult Result = UPokerHandEvaluator::EvaluatePokerHand(HoleCards, CommunityCards);

    TestEqual(TEXT("Should be Flush"), Result.HandRank, EPokerHandRank::Flush);
    // Кикеры для флеша - это 5 старших карт флеша
    TArray<ECardRank> ExpectedFlushKickers = { ECardRank::Ace, ECardRank::Eight, ECardRank::Four, ECardRank::Three, ECardRank::Two };
    if (Result.Kickers.Num() == ExpectedFlushKickers.Num())
    {
        for (int32 i = 0; i < ExpectedFlushKickers.Num(); ++i)
        {
            TestEqual(FString::Printf(TEXT("Flush Kicker %d"), i), Result.Kickers[i], ExpectedFlushKickers[i]);
        }
    }
    else
    {
        AddError(FString::Printf(TEXT("Incorrect number of kickers for Flush. Expected %d, Got %d."), ExpectedFlushKickers.Num(), Result.Kickers.Num()));
    }
    return !HasAnyErrors();
}

// Тест для случая < 5 карт
TEST_HAND_VARIADIC(LessThan5Cards_FourCards, "AH KH QH JH", EPokerHandRank::HighCard, ECardRank::Ace)
TEST_HAND_VARIADIC(LessThan5Cards_OneCard, "AS", EPokerHandRank::HighCard, ECardRank::Ace)
TEST_HAND_VARIADIC(LessThan5_FourCards_AceHigh, "AH KD QS JC", EPokerHandRank::HighCard, ECardRank::Ace)
// 4 карты, Король старший
TEST_HAND_VARIADIC(LessThan5_FourCards_KingHigh, "KH QD JS TC", EPokerHandRank::HighCard, ECardRank::King)
// 4 карты, Двойка старшая (маловероятно, но для теста)
TEST_HAND_VARIADIC(LessThan5_FourCards_TwoHigh, "2C 2D 2H 2S", EPokerHandRank::HighCard, ECardRank::Two) // Ожидаем ранг Two, так как это старшая карта из имеющихся.
// То, что это каре из 4х карт, не должно учитываться, так как всего < 5 карт.

// 3 карты, Дама старшая
TEST_HAND_VARIADIC(LessThan5_ThreeCards_QueenHigh, "QC JD 2H", EPokerHandRank::HighCard, ECardRank::Queen)
// 3 карты, Семерка старшая
TEST_HAND_VARIADIC(LessThan5_ThreeCards_SevenHigh, "7S 5D 2C", EPokerHandRank::HighCard, ECardRank::Seven)

// 2 карты, Валет старший
TEST_HAND_VARIADIC(LessThan5_TwoCards_JackHigh, "JH 2S", EPokerHandRank::HighCard, ECardRank::Jack)
// 2 карты, Пятерка старшая (пара, но все равно HighCard по вашей логике <5 карт)
TEST_HAND_VARIADIC(LessThan5_TwoCards_PairFives, "5H 5S", EPokerHandRank::HighCard, ECardRank::Five)


// --- Тесты для сравнения рук < 5 карт (если CompareHandResults их как-то обрабатывает) ---
// Обычно сравнение не имеет смысла для < 5 карт, так как это не полные покерные руки,
// но если CompareHandResults просто сравнит по рангу HighCard и одному кикеру, то:

// "AH KD" (Ace high) vs "KH QD" (King high) -> A > K
TEST_COMPARE(LessThan5_Compare_AceHigh_vs_KingHigh, "AH KD", "KH QD", 1)
// "AH" (Ace high) vs "AS" (Ace high) -> Ничья
TEST_COMPARE(LessThan5_Compare_Ace_vs_Ace, "AH", "AS", 0)
// "KH QS JD TC" (King high) vs "AS 2D 3C 4H" (Ace high) -> A > K
TEST_COMPARE(LessThan5_Compare_KingHigh4_vs_AceHigh4, "KH QS JD TC", "AS 2D 3C 4H", -1)

// Тест на случай, если EvaluatePokerHand передаются карманные и 0 общих, и сумма < 5
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandEvaluatorTest_LessThan5_HoleAndNoCommunity, "PokerClient.UnitTests.HandEvaluator.EdgeCases.HoleNoCommunity", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FHandEvaluatorTest_LessThan5_HoleAndNoCommunity::RunTest(const FString& Parameters)
{
    TArray<FCard> HoleCards = FTestCardParser::Cards(TEXT("AH KH"));
    TArray<FCard> CommunityCards; // Пустой массив
    FPokerHandResult Result = UPokerHandEvaluator::EvaluatePokerHand(HoleCards, CommunityCards);

    TestEqual(TEXT("Hand Rank should be HighCard"), Result.HandRank, EPokerHandRank::HighCard);
    TestTrue(TEXT("Should have 1 kicker"), Result.Kickers.Num() == 1);
    if (Result.Kickers.Num() > 0)
    {
        TestEqual(TEXT("Highest kicker should be Ace"), Result.Kickers[0], ECardRank::Ace);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandEvaluatorTest_LessThan5_HoleAndOneCommunity, "PokerClient.UnitTests.HandEvaluator.EdgeCases.HoleOneCommunity", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FHandEvaluatorTest_LessThan5_HoleAndOneCommunity::RunTest(const FString& Parameters)
{
    TArray<FCard> HoleCards = FTestCardParser::Cards(TEXT("AH KH"));
    TArray<FCard> CommunityCards = FTestCardParser::Cards(TEXT("QC")); // 1 общая карта
    FPokerHandResult Result = UPokerHandEvaluator::EvaluatePokerHand(HoleCards, CommunityCards); // Всего 3 карты

    TestEqual(TEXT("Hand Rank should be HighCard"), Result.HandRank, EPokerHandRank::HighCard);
    TestTrue(TEXT("Should have 1 kicker"), Result.Kickers.Num() == 1);
    if (Result.Kickers.Num() > 0)
    {
        TestEqual(TEXT("Highest kicker should be Ace"), Result.Kickers[0], ECardRank::Ace);
    }
    return !HasAnyErrors();
}

// Тест для 0 карт
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandEvaluatorTest_LessThan5Cards_NoCards, "PokerClient.UnitTests.HandEvaluator.EdgeCases.NoCards", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FHandEvaluatorTest_LessThan5Cards_NoCards::RunTest(const FString& Parameters)
{
    FPokerHandResult Result = UPokerHandEvaluator::EvaluatePokerHand({}, {});
    TestEqual(TEXT("Hand Rank should be HighCard for no cards"), Result.HandRank, EPokerHandRank::HighCard);
    TestTrue(TEXT("Should have 0 kickers for no cards"), Result.Kickers.Num() == 0);
    return !HasAnyErrors();
}

// Дополнительные тесты на специфичные ситуации
// Стрит, где часть карт на борде, часть в руке
TEST_HAND_VARIADIC(Straight_MixedHandBoard, "5S 6D 7C 8H 9S KH AH", EPokerHandRank::Straight, ECardRank::Nine)

// Флеш, где часть карт на борде, часть в руке
// Игрок: AS KS, Борд: QS JS 2S KD QD -> Всего 7 карт: AS KS QS JS 2S KD QD. Лучшая рука: Флеш пик от Туза (AS KS QS JS 2S).
TEST_HAND_VARIADIC(Flush_MixedHandBoard, "AS KS QS JS 2S KD QD", EPokerHandRank::Flush, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Two)

// Две пары, одна на борде, одна в руке, кикер в руке
// Игрок: AH TC, Борд: AS TD KH QC JS -> Всего 7 карт: AH TC AS TD KH QC JS. Лучшая рука: Две пары Тузы и Десятки, кикер Король.
TEST_HAND_VARIADIC(TwoPair_BoardPairHandPairKickerHand, "AH TC AS TD KH QC JS", EPokerHandRank::TwoPair, ECardRank::Ace, ECardRank::Ten, ECardRank::King)

// Две пары, одна на борде, одна в руке, кикер на борде
// Игрок: AD TD, Борд: AC TC KH QS JS -> Всего 7 карт: AD TD AC TC KH QS JS. Лучшая рука: Две пары Тузы и Десятки, кикер Король.
TEST_HAND_VARIADIC(TwoPair_BoardPairHandPairKickerBoard, "AD TD AC TC KH QS JS", EPokerHandRank::TwoPair, ECardRank::Ace, ECardRank::Ten, ECardRank::King)

// Сет с использованием одной карты в руке и двух на борде
// Игрок: AH 2C, Борд: AS AD KH QC JS -> Всего 7 карт: AH 2C AS AD KH QC JS. Лучшая рука: Сет Тузов, кикеры Король, Дама.
TEST_HAND_VARIADIC(Set_OneInHandTwoOnBoard, "AH 2C AS AD KH QC JS", EPokerHandRank::ThreeOfAKind, ECardRank::Ace, ECardRank::King, ECardRank::Queen)

// --- Дополнительные Тесты: Комбинации Рука + Борд (всего 7 карт) ---

// Игрок имеет пару в руке, борд не улучшает до сета, но дает старшие кикеры
// Рука: 2H 2S, Борд: AH KH QH JC 9C. Лучшая рука: Пара двоек (2H 2S), кикеры A K Q.
TEST_HAND_VARIADIC(MoreTests_PairInHand_HighKickersOnBoard, "2H 2S AH KH QH JC 9C", EPokerHandRank::OnePair, ECardRank::Two, ECardRank::Ace, ECardRank::King, ECardRank::Queen)

// Игрок имеет пару в руке, на борде есть пара старше. Используется пара с борда.
// Рука: 2H 2S, Борд: AH AD KH QC JC. Лучшая рука: Пара тузов (AH AD), кикеры K Q J.
TEST_HAND_VARIADIC(MoreTests_PairInHand_HigherPairOnBoard, "2H 2S AH AD KH QC JC", EPokerHandRank::OnePair, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack)

// Игрок имеет пару в руке, на борде две пары старше. Используются две пары с борда.
// Рука: 2H 2S, Борд: AH AD KH KD QC. Лучшая рука: Две пары Тузы и Короли (AH AD KH KD), кикер Дама.
TEST_HAND_VARIADIC(MoreTests_PairInHand_TwoHigherPairsOnBoard, "2H 2S AH AD KH KD QC", EPokerHandRank::TwoPair, ECardRank::Ace, ECardRank::King, ECardRank::Queen)

// Игрок имеет пару в руке, на борде сет. Используется сет с борда.
// Рука: 2H 2S, Борд: AH AD AC KH QC. Лучшая рука: Сет Тузов (AH AD AC), кикеры K Q.
TEST_HAND_VARIADIC(MoreTests_PairInHand_SetOnBoard, "2H 2S AH AD AC KH QC", EPokerHandRank::ThreeOfAKind, ECardRank::Ace, ECardRank::King, ECardRank::Queen)

// Игрок имеет две карты для стрита, на борде три карты для завершения стрита.
// Рука: 8H 9S, Борд: TH JC QD KH AH. Лучшая рука: Стрит T-A (TH JC QD KH AH).
TEST_HAND_VARIADIC(MoreTests_Straight_TwoInHand_ThreeOnBoard, "8H 9S TH JC QD KH AH", EPokerHandRank::Straight, ECardRank::Ace)

// Игрок имеет одну карту для стрита, на борде четыре карты для стрита (стрит на борде).
// Рука: 2H 3S, Борд: 4C 5D 6H 7S KC. Лучшая рука: Стрит 3-7 (3S 4C 5D 6H 7S).
TEST_HAND_VARIADIC(MoreTests_Straight_OneInHand_FourOnBoard, "2H 3S 4C 5D 6H 7S KC", EPokerHandRank::Straight, ECardRank::Seven)

// Игрок имеет две карты для флеша, на борде три карты той же масти.
// Рука: AH KH, Борд: QH JH 2H QC JC. Лучшая рука: Флеш черв от Туза (AH KH QH JH 2H).
TEST_HAND_VARIADIC(MoreTests_Flush_TwoInHand_ThreeOnBoard, "AH KH QH JH 2H QC JC", EPokerHandRank::Flush, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Two)

// Игрок имеет одну карту для флеша, на борDE четыре карты той же масти (флеш на борде, игрок улучшает его старшей картой).
// Рука: AH 2S, Борд: KH QH JH TH 3S. Лучшая рука: Флеш черв от Туза (AH KH QH JH TH).
TEST_HAND_VARIADIC(MoreTests_Flush_OneInHandImprovesBoardFlush, "AH 2S KH QH JH TH 3S", EPokerHandRank::Flush, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Ten)

// Игрок не улучшает флеш на борде.
// Рука: 2H 3S, Борд: AH KH QH JH TH. Лучшая рука: Флеш черв от Туза (с борда).
TEST_HAND_VARIADIC(MoreTests_Flush_BoardPlays, "2H 3S AH KH QH JH TH", EPokerHandRank::Flush, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Jack, ECardRank::Ten)

// Фулл-хаус: пара в руке, тройка на борде.
// Рука: AH AS, Борд: KH KD KC QC JC. Лучшая рука: Фулл-хаус Короли на Тузах (KH KD KC AH AS).
TEST_HAND_VARIADIC(MoreTests_FullHouse_PairInHand_SetOnBoard, "AH AS KH KD KC QC JC", EPokerHandRank::FullHouse, ECardRank::King, ECardRank::Ace)

// Фулл-хаус: тройка в руке, пара на борде.
// Рука: AH AD AC, Борд: KH KD QC JC 2S. Лучшая рука: Фулл-хаус Тузы на Королях (AH AD AC KH KD).
TEST_HAND_VARIADIC(MoreTests_FullHouse_SetInHand_PairOnBoard, "AH AD AC KH KD QC JC 2S", EPokerHandRank::FullHouse, ECardRank::Ace, ECardRank::King)

// Фулл-хаус: одна карта тройки в руке + две на борде, одна карта пары в руке + одна на борде.
// Рука: AH KH, Борд: AS AD KC QC JC. Лучшая рука: Фулл-хаус Тузы на Королях (AH AS AD KH KC).
TEST_HAND_VARIADIC(MoreTests_FullHouse_SplitAcrossHandAndBoard, "AH KH AS AD KC QC JC", EPokerHandRank::FullHouse, ECardRank::Ace, ECardRank::King)

// Каре: две карты в руке, две на борде.
// Рука: AH AD, Борд: AC AS KH QC JC. Лучшая рука: Каре Тузов, кикер Король.
TEST_HAND_VARIADIC(MoreTests_FourOfAKind_TwoInHand_TwoOnBoard, "AH AD AC AS KH QC JC", EPokerHandRank::FourOfAKind, ECardRank::Ace, ECardRank::King)

// Стрит-флеш: две карты в руке, три на борде.
// Рука: 8H 9H, Борд: TH JH QH KD AS. Лучшая рука: Стрит-флеш черв от Дамы (8H 9H TH JH QH).
TEST_HAND_VARIADIC(MoreTests_StraightFlush_TwoInHand_ThreeOnBoard, "8H 9H TH JH QH KD AS", EPokerHandRank::StraightFlush, ECardRank::Queen)

// Роял-флеш на борде, рука игрока не участвует.
// Рука: 2C 3D, Борд: AH KH QH JH TH. Лучшая рука: Роял-флеш черв.
TEST_HAND_VARIADIC(MoreTests_RoyalFlush_BoardPlays, "2C 3D AH KH QH JH TH", EPokerHandRank::RoyalFlush, ECardRank::Ace)

// Случай, когда лучшая 5-карточная рука полностью на борде, и рука игрока не улучшает ее.
// Рука: 2C 3D, Борд: AH AD AC AS KH (Каре тузов, король кикер на борде).
TEST_HAND_VARIADIC(MoreTests_FourOfAKind_BoardPlays, "2C 3D AH AD AC AS KH", EPokerHandRank::FourOfAKind, ECardRank::Ace, ECardRank::King)

// Стрит с использованием одной карты из руки.
// Рука: 6C 2D, Борд: 3H 4S 5D 7H AH. Лучшая рука: Стрит 3-7.
TEST_HAND_VARIADIC(MoreTests_Straight_OneFromHandPlays, "6C 2D 3H 4S 5D 7H AH", EPokerHandRank::Straight, ECardRank::Seven)

// Две пары: одна в руке, одна на борде, кикер с борда.
// Рука: AH AD, Борд: KC KD QH JS TC. Лучшая рука: Две пары Тузы и Короли, кикер Дама.
TEST_HAND_VARIADIC(MoreTests_TwoPair_HandPair_BoardPair_BoardKicker, "AH AD KC KD QH JS TC", EPokerHandRank::TwoPair, ECardRank::Ace, ECardRank::King, ECardRank::Queen)

// Три карты одной масти в руке, две той же масти на борде (рука не образует флеш)
// Рука: AH KH QH, Борд: 2H 3H JS TC 9D. Лучшая рука: Флеш черв от Туза (AH KH QH 3H 2H).
TEST_HAND_VARIADIC(MoreTests_Flush_ThreeInHandTwoOnBoard, "AH KH QH 2H 3H JS TC", EPokerHandRank::Flush, ECardRank::Ace, ECardRank::King, ECardRank::Queen, ECardRank::Three, ECardRank::Two)

// У игрока две маленькие пары, на борде пара старше. Лучшая рука - две старшие пары (одна с борда, одна из руки).
// Рука: 2H 2S 3D 3C, Борд: AH AD KC QS JS. Лучшая рука: Тузы и Тройки, Король кикер.
TEST_HAND_VARIADIC(MoreTests_TwoPair_SmallPocketPairs_HigherBoardPair, "2H 2S 3D 3C AH AD KC QS JS", EPokerHandRank::TwoPair, ECardRank::Ace, ECardRank::Three, ECardRank::King)