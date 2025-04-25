#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
// �������� ���� ���� � ������ ���������/������
#include "PokerDataTypes.h"
#include "OfflinePokerGameState.h"
#include "Deck.h"
#include "OfflineGameManager.generated.h" // ������ ���� ���������

/**
 * ����� UObject, ���������� �� ���������� ������� ������� ���� � �����.
 * ��������� � �������� � GameInstance ��� ������� �� ���������� ������.
 */
UCLASS(BlueprintType) // BlueprintType �� ������, ���� ������� �������� ���-�� �� BP
class POKER_CLIENT_API UOfflineGameManager : public UObject
{
	GENERATED_BODY()

public:
	// ��������� �� ������, �������� ������� ��������� ����
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game")
	UOfflinePokerGameState* GameStateData;

	// ��������� �� ������ ������ ����
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Offline Game")
	UDeck* Deck;

	// ����������� �� ���������
	UOfflineGameManager();

	/**
	 * �������������� ����� ������� ����.
	 * ������� GameState, Deck, ����������� �������/�����.
	 * @param NumPlayers ���������� �������� ������� (������ 1).
	 * @param NumBots ���������� �����.
	 * @param InitialStack ��������� ���� ����� ��� ����.
	 */
	UFUNCTION(BlueprintCallable, Category = "Offline Game")
	void InitializeGame(int32 NumPlayers, int32 NumBots, int64 InitialStack);

	/**
	 * ���������� ������� ��������� ���� (��� ������).
	 * @return ��������� �� UOfflinePokerGameState ��� nullptr, ���� ���� �� ����������������.
	 */
	UFUNCTION(BlueprintPure, Category = "Offline Game")
	UOfflinePokerGameState* GetGameState() const;

};