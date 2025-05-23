#include "PokerDataTypes.h" // Важно указать правильный путь к .h файлу
#include "UObject/Package.h" // Для StaticEnum

// Реализация метода ToString для структуры FCard
FString FCard::ToString() const
{
	// Получаем текстовое представление ранга и масти из Enums
	const UEnum* SuitEnum = StaticEnum<ECardSuit>();
	const UEnum* RankEnum = StaticEnum<ECardRank>();

	FString SuitStr = SuitEnum ? SuitEnum->GetNameStringByValue(static_cast<int64>(Suit)) : TEXT("?");
	FString RankStr = RankEnum ? RankEnum->GetNameStringByValue(static_cast<int64>(Rank)) : TEXT("?");

	if (RankStr.StartsWith(TEXT("ECardRank::"))) RankStr = RankStr.RightChop(11); // Убираем префикс
	if (RankStr == TEXT("Ten")) RankStr = TEXT("T"); // Особый случай для 10
	else if (RankStr.Len() > 1) RankStr = RankStr.Left(1); // Берем первую букву (J, Q, K, A) или цифру

	if (SuitStr.StartsWith(TEXT("ECardSuit::"))) SuitStr = SuitStr.RightChop(11); // Убираем префикс
	if (SuitStr.Len() > 0) SuitStr = SuitStr.ToLower().Left(1); // Берем первую букву (c, d, h, s)

	return FString::Printf(TEXT("%s%s"), *RankStr, *SuitStr); // Формат типа "Ah", "Ks", "Td", "7c"
}

FString FCard::ToRussianString() const
{
    FString RankStrPart = TEXT("?");
    FString SuitStrPart = TEXT("?");

    // Получаем ранг
    switch (Rank)
    {
    case ECardRank::Two:   RankStrPart = TEXT("2"); break;
    case ECardRank::Three: RankStrPart = TEXT("3"); break;
    case ECardRank::Four:  RankStrPart = TEXT("4"); break;
    case ECardRank::Five:  RankStrPart = TEXT("5"); break;
    case ECardRank::Six:   RankStrPart = TEXT("6"); break;
    case ECardRank::Seven: RankStrPart = TEXT("7"); break;
    case ECardRank::Eight: RankStrPart = TEXT("8"); break;
    case ECardRank::Nine:  RankStrPart = TEXT("9"); break;
    case ECardRank::Ten:   RankStrPart = TEXT("10"); break; // Или "Т" если хотите одну букву
    case ECardRank::Jack:  RankStrPart = TEXT("В"); break;  // Валет
    case ECardRank::Queen: RankStrPart = TEXT("Д"); break;  // Дама
    case ECardRank::King:  RankStrPart = TEXT("К"); break;  // Король
    case ECardRank::Ace:   RankStrPart = TEXT("Т"); break;  // Туз
    default: break;
    }

    // Получаем масть
    switch (Suit)
    {
    case ECardSuit::Clubs:    SuitStrPart = TEXT("т"); break; // Трефы
    case ECardSuit::Diamonds: SuitStrPart = TEXT("б"); break; // Бубны
    case ECardSuit::Hearts:   SuitStrPart = TEXT("ч"); break; // Червы
    case ECardSuit::Spades:   SuitStrPart = TEXT("п"); break; // Пики
    default: break;
    }

    return RankStrPart + SuitStrPart; // Формат "Вч", "3п", "10б", "Тт"
}