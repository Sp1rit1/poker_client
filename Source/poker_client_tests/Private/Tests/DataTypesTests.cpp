#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

// ВАЖНО: Укажите правильный путь к вашему PokerDataTypes.h из основного модуля!
// Если ваш основной модуль называется "PokerClient", то:
#include "poker_client/Public/PokerDataTypes.h"
// Если структура другая, исправьте путь.

// --- Тесты для FCard::ToString() ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToStringTest_AceSpades, "PokerClient.UnitTests.DataTypes.FCard.ToString.AceSpades", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToStringTest_AceSpades::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Spades, ECardRank::Ace);
    FString Expected = TEXT("As");
    FString Actual = Card.ToString();
    TestEqual(TEXT("Ace of Spades should be 'As'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToStringTest_KingHearts, "PokerClient.UnitTests.DataTypes.FCard.ToString.KingHearts", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToStringTest_KingHearts::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Hearts, ECardRank::King);
    FString Expected = TEXT("Kh");
    FString Actual = Card.ToString();
    TestEqual(TEXT("King of Hearts should be 'Kh'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToStringTest_TenDiamonds, "PokerClient.UnitTests.DataTypes.FCard.ToString.TenDiamonds", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToStringTest_TenDiamonds::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Diamonds, ECardRank::Ten);
    FString Expected = TEXT("Td");
    FString Actual = Card.ToString();
    TestEqual(TEXT("Ten of Diamonds should be 'Td'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToStringTest_TwoClubs, "PokerClient.UnitTests.DataTypes.FCard.ToString.TwoClubs", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToStringTest_TwoClubs::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Clubs, ECardRank::Two);
    FString Expected = TEXT("2c");
    FString Actual = Card.ToString();
    TestEqual(TEXT("Two of Clubs should be '2c'"), Actual, Expected);
    return true;
}

// --- Тесты для FCard::ToRussianString() ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToRussianStringTest_AceSpades, "PokerClient.UnitTests.DataTypes.FCard.ToRussianString.AceSpades", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToRussianStringTest_AceSpades::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Spades, ECardRank::Ace);
    FString Expected = TEXT("Тп"); // Туз пик
    FString Actual = Card.ToRussianString();
    TestEqual(TEXT("Ace of Spades (Russian) should be 'Тп'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToRussianStringTest_QueenHearts, "PokerClient.UnitTests.DataTypes.FCard.ToRussianString.QueenHearts", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToRussianStringTest_QueenHearts::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Hearts, ECardRank::Queen);
    FString Expected = TEXT("Дч"); // Дама червей
    FString Actual = Card.ToRussianString();
    TestEqual(TEXT("Queen of Hearts (Russian) should be 'Дч'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToRussianStringTest_SevenDiamonds, "PokerClient.UnitTests.DataTypes.FCard.ToRussianString.SevenDiamonds", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToRussianStringTest_SevenDiamonds::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Diamonds, ECardRank::Seven);
    FString Expected = TEXT("7б"); // Семерка бубен
    FString Actual = Card.ToRussianString();
    TestEqual(TEXT("Seven of Diamonds (Russian) should be '7б'"), Actual, Expected);
    return true;
}

// --- Тесты для PokerRankToRussianString() ---
// (Эта функция глобальная, поэтому мы можем тестировать ее напрямую)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_HighCard, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.HighCard", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_HighCard::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::HighCard;
    FString Expected = TEXT("Старшая Карта");
    FString Actual = PokerRankToRussianString(Rank); // Прямой вызов глобальной функции
    TestEqual(TEXT("HighCard (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_FullHouse, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.FullHouse", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_FullHouse::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::FullHouse;
    FString Expected = TEXT("Фулл-Хаус");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("FullHouse (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_RoyalFlush, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.RoyalFlush", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_RoyalFlush::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::RoyalFlush;
    FString Expected = TEXT("Роял-Флеш");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("RoyalFlush (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToStringTest_QueenClubs, "PokerClient.UnitTests.DataTypes.FCard.ToString.QueenClubs", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToStringTest_QueenClubs::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Clubs, ECardRank::Queen);
    FString Expected = TEXT("Qc");
    FString Actual = Card.ToString();
    TestEqual(TEXT("Queen of Clubs should be 'Qc'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToStringTest_JackSpades, "PokerClient.UnitTests.DataTypes.FCard.ToString.JackSpades", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToStringTest_JackSpades::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Spades, ECardRank::Jack);
    FString Expected = TEXT("Js");
    FString Actual = Card.ToString();
    TestEqual(TEXT("Jack of Spades should be 'Js'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToStringTest_NineHearts, "PokerClient.UnitTests.DataTypes.FCard.ToString.NineHearts", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToStringTest_NineHearts::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Hearts, ECardRank::Nine);
    FString Expected = TEXT("9h");
    FString Actual = Card.ToString();
    TestEqual(TEXT("Nine of Hearts should be '9h'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToStringTest_FiveDiamonds, "PokerClient.UnitTests.DataTypes.FCard.ToString.FiveDiamonds", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToStringTest_FiveDiamonds::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Diamonds, ECardRank::Five);
    FString Expected = TEXT("5d");
    FString Actual = Card.ToString();
    TestEqual(TEXT("Five of Diamonds should be '5d'"), Actual, Expected);
    return true;
}


// --- Дополнительные тесты для FCard::ToRussianString() ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToRussianStringTest_KingClubs, "PokerClient.UnitTests.DataTypes.FCard.ToRussianString.KingClubs", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToRussianStringTest_KingClubs::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Clubs, ECardRank::King);
    FString Expected = TEXT("Кт"); // Король треф
    FString Actual = Card.ToRussianString();
    TestEqual(TEXT("King of Clubs (Russian) should be 'Кт'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToRussianStringTest_JackDiamonds, "PokerClient.UnitTests.DataTypes.FCard.ToRussianString.JackDiamonds", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToRussianStringTest_JackDiamonds::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Diamonds, ECardRank::Jack);
    FString Expected = TEXT("Вб"); // Валет бубен
    FString Actual = Card.ToRussianString();
    TestEqual(TEXT("Jack of Diamonds (Russian) should be 'Вб'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToRussianStringTest_TenSpades, "PokerClient.UnitTests.DataTypes.FCard.ToRussianString.TenSpades", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToRussianStringTest_TenSpades::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Spades, ECardRank::Ten);
    FString Expected = TEXT("10п"); // Десятка пик
    FString Actual = Card.ToRussianString();
    TestEqual(TEXT("Ten of Spades (Russian) should be '10п'"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardToRussianStringTest_TwoHearts, "PokerClient.UnitTests.DataTypes.FCard.ToRussianString.TwoHearts", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCardToRussianStringTest_TwoHearts::RunTest(const FString& Parameters)
{
    FCard Card(ECardSuit::Hearts, ECardRank::Two);
    FString Expected = TEXT("2ч"); // Двойка червей
    FString Actual = Card.ToRussianString();
    TestEqual(TEXT("Two of Hearts (Russian) should be '2ч'"), Actual, Expected);
    return true;
}

// --- Дополнительные тесты для PokerRankToRussianString() ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_OnePair, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.OnePair", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_OnePair::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::OnePair;
    FString Expected = TEXT("Одна Пара");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("OnePair (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_TwoPair, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.TwoPair", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_TwoPair::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::TwoPair;
    FString Expected = TEXT("Две Пары");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("TwoPair (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_ThreeOfAKind, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.ThreeOfAKind", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_ThreeOfAKind::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::ThreeOfAKind;
    FString Expected = TEXT("Сет");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("ThreeOfAKind (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_Straight, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.Straight", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_Straight::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::Straight;
    FString Expected = TEXT("Стрит");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("Straight (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_Flush, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.Flush", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_Flush::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::Flush;
    FString Expected = TEXT("Флеш");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("Flush (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_FourOfAKind, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.FourOfAKind", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_FourOfAKind::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::FourOfAKind;
    FString Expected = TEXT("Каре");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("FourOfAKind (Russian)"), Actual, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPokerRankToRussianStringTest_StraightFlush, "PokerClient.UnitTests.DataTypes.PokerRankToRussianString.StraightFlush", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPokerRankToRussianStringTest_StraightFlush::RunTest(const FString& Parameters)
{
    EPokerHandRank Rank = EPokerHandRank::StraightFlush;
    FString Expected = TEXT("Стрит-Флеш");
    FString Actual = PokerRankToRussianString(Rank);
    TestEqual(TEXT("StraightFlush (Russian)"), Actual, Expected);
    return true;
}