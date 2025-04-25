#include "GameClasses/Deck.h" // �������� .h ����
#include "Algo/RandomShuffle.h" // ��� �������������
#include "Containers/EnumAsByte.h" // ��� �������� �� Enum

void UDeck::Initialize()
{
	Cards.Empty(); // ������� �� ������ ������
	Cards.Reserve(52); // ����������� ������

	// �������� ������ � �������������
	const UEnum* SuitEnum = StaticEnum<ECardSuit>();
	const UEnum* RankEnum = StaticEnum<ECardRank>();

	if (!SuitEnum || !RankEnum)
	{
		UE_LOG(LogTemp, Error, TEXT("UDeck::Initialize - Failed to get Suit or Rank Enum!"));
		return;
	}

	// �������� �� ���� ������
	for (int32 s = 0; s < SuitEnum->NumEnums() - 1; ++s) // -1 ����� ���������� UMETA(Hidden) ��� MAX
	{
		ECardSuit CurrentSuit = static_cast<ECardSuit>(SuitEnum->GetValueByIndex(s));

		// �������� �� ���� ������
		for (int32 r = 0; r < RankEnum->NumEnums() - 1; ++r)
		{
			ECardRank CurrentRank = static_cast<ECardRank>(RankEnum->GetValueByIndex(r));

			// ������� � ��������� �����
			Cards.Add(FCard(CurrentSuit, CurrentRank));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UDeck::Initialize - Deck created with %d cards."), Cards.Num());
}

void UDeck::Shuffle()
{
	if (Cards.Num() > 0)
	{
		Algo::RandomShuffle(Cards);
		UE_LOG(LogTemp, Verbose, TEXT("UDeck::Shuffle - Deck shuffled."));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("UDeck::Shuffle - Cannot shuffle an empty deck."));
	}
}

FCard UDeck::DealCard()
{
	if (!IsEmpty())
	{
		// ������� ��������� ����� �� ������� (�����������, ��� ������)
		return Cards.Pop(); // Pop ������� � ���������� �������
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UDeck::DealCard - Deck is empty!"));
		// ���������� "����������" ����� ��� ����� ���� �� ������� FCard* � ������� nullptr
		return FCard(); // ���������� ����� �� ��������� (2 Clubs)
	}
}

bool UDeck::IsEmpty() const
{
	return Cards.IsEmpty();
}

int32 UDeck::NumCardsLeft() const
{
	return Cards.Num();
}