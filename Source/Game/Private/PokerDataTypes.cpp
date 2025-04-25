#include "Game/Public/PokerDataTypes.h" // ����� ������� ���������� ���� � .h �����
#include "UObject/Package.h" // ��� StaticEnum

// ���������� ������ ToString ��� ��������� FCard
FString FCard::ToString() const
{
	// �������� ��������� ������������� ����� � ����� �� Enums
	const UEnum* SuitEnum = StaticEnum<ECardSuit>();
	const UEnum* RankEnum = StaticEnum<ECardRank>();

	FString SuitStr = SuitEnum ? SuitEnum->GetNameStringByValue(static_cast<int64>(Suit)) : TEXT("?");
	FString RankStr = RankEnum ? RankEnum->GetNameStringByValue(static_cast<int64>(Rank)) : TEXT("?");

	// �����������: ��������� ������ ��� ����������
	// ��������, "ECardRank::Ace" -> "A", "ECardSuit::Hearts" -> "h"
	if (RankStr.StartsWith(TEXT("ECardRank::"))) RankStr = RankStr.RightChop(11); // ������� �������
	if (RankStr == TEXT("Ten")) RankStr = TEXT("T"); // ������ ������ ��� 10
	else if (RankStr.Len() > 1) RankStr = RankStr.Left(1); // ����� ������ ����� (J, Q, K, A) ��� �����

	if (SuitStr.StartsWith(TEXT("ECardSuit::"))) SuitStr = SuitStr.RightChop(11); // ������� �������
	if (SuitStr.Len() > 0) SuitStr = SuitStr.ToLower().Left(1); // ����� ������ ����� (c, d, h, s)

	return FString::Printf(TEXT("%s%s"), *RankStr, *SuitStr); // ������ ���� "Ah", "Ks", "Td", "7c"
}

// ����� ����� �������� ���������� ������ ������� ��������, ���� ��� �����������.