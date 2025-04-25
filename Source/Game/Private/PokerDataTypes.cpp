#include "Game/Public/PokerDataTypes.h" // Важно указать правильный путь к .h файлу
#include "UObject/Package.h" // Для StaticEnum

// Реализация метода ToString для структуры FCard
FString FCard::ToString() const
{
	// Получаем текстовое представление ранга и масти из Enums
	const UEnum* SuitEnum = StaticEnum<ECardSuit>();
	const UEnum* RankEnum = StaticEnum<ECardRank>();

	FString SuitStr = SuitEnum ? SuitEnum->GetNameStringByValue(static_cast<int64>(Suit)) : TEXT("?");
	FString RankStr = RankEnum ? RankEnum->GetNameStringByValue(static_cast<int64>(Rank)) : TEXT("?");

	// Опционально: укоротить строки для читаемости
	// Например, "ECardRank::Ace" -> "A", "ECardSuit::Hearts" -> "h"
	if (RankStr.StartsWith(TEXT("ECardRank::"))) RankStr = RankStr.RightChop(11); // Убираем префикс
	if (RankStr == TEXT("Ten")) RankStr = TEXT("T"); // Особый случай для 10
	else if (RankStr.Len() > 1) RankStr = RankStr.Left(1); // Берем первую букву (J, Q, K, A) или цифру

	if (SuitStr.StartsWith(TEXT("ECardSuit::"))) SuitStr = SuitStr.RightChop(11); // Убираем префикс
	if (SuitStr.Len() > 0) SuitStr = SuitStr.ToLower().Left(1); // Берем первую букву (c, d, h, s)

	return FString::Printf(TEXT("%s%s"), *RankStr, *SuitStr); // Формат типа "Ah", "Ks", "Td", "7c"
}

// Здесь можно добавить реализации других методов структур, если они понадобятся.