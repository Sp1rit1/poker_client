#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h" // �������� ��� ���� � ������
#include "Deck.generated.h"

/**
 * ����� UObject, �������������� ��������� ������ ����.
 */
UCLASS() // �� Blueprintable, �.�. ������������ ������ ������ C++ ������
class POKER_CLIENT_API UDeck : public UObject // ������ POKER_CLIENT_API
{
	GENERATED_BODY()

private:
	// ������ ���� � ������
	TArray<FCard> Cards;

public:
	// �������������� ������ ������������ 52 �������
	void Initialize();

	// ������������ ����� � ������
	void Shuffle();

	// ������� (���������� � �������) ���� ����� � ����� ������
	// ���������� ���������� ����� (����� ��������� ����� ���. �����), ���� ������ �����
	FCard DealCard();

	// ���������, ����� �� ������
	bool IsEmpty() const;

	// ���������� ���������� ���������� ����
	int32 NumCardsLeft() const;
};