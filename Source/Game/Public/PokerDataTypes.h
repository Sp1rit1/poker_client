#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h" // Needed for UENUM, USTRUCT
#include "PokerDataTypes.generated.h" // MUST be the last include

// ����� �����
UENUM(BlueprintType)
enum class ECardSuit : uint8
{
	Clubs		UMETA(DisplayName = "Clubs"),	 // �����
	Diamonds	UMETA(DisplayName = "Diamonds"), // �����
	Hearts		UMETA(DisplayName = "Hearts"),	 // �����
	Spades		UMETA(DisplayName = "Spades")	 // ����
};

// ���� �����
UENUM(BlueprintType)
enum class ECardRank : uint8
{
	Two			UMETA(DisplayName = "2"),
	Three		UMETA(DisplayName = "3"),
	Four		UMETA(DisplayName = "4"),
	Five		UMETA(DisplayName = "5"),
	Six			UMETA(DisplayName = "6"),
	Seven		UMETA(DisplayName = "7"),
	Eight		UMETA(DisplayName = "8"),
	Nine		UMETA(DisplayName = "9"),
	Ten			UMETA(DisplayName = "10"),
	Jack		UMETA(DisplayName = "Jack"),	 // �����
	Queen		UMETA(DisplayName = "Queen"),	 // ����
	King		UMETA(DisplayName = "King"),	 // ������
	Ace			UMETA(DisplayName = "Ace")		 // ���
};

// ���� �������� ���������� (�� ������� � �������)
UENUM(BlueprintType)
enum class EPokerHandRank : uint8
{
	HighCard		UMETA(DisplayName = "High Card"),		// ������� �����
	OnePair			UMETA(DisplayName = "One Pair"),		// ���� ����
	TwoPair			UMETA(DisplayName = "Two Pair"),		// ��� ����
	ThreeOfAKind	UMETA(DisplayName = "Three of a Kind"), // ������ (���/�����)
	Straight		UMETA(DisplayName = "Straight"),		// �����
	Flush			UMETA(DisplayName = "Flush"),			// ����
	FullHouse		UMETA(DisplayName = "Full House"),		// ���� ����
	FourOfAKind		UMETA(DisplayName = "Four of a Kind"),	// ����
	StraightFlush	UMETA(DisplayName = "Straight Flush"),	// ����� ����
	RoyalFlush		UMETA(DisplayName = "Royal Flush")		// ���� ����
};

// ������ ������ � �������/�� ������
UENUM(BlueprintType)
enum class EPlayerStatus : uint8
{
	Waiting			UMETA(DisplayName = "Waiting"),			// ������� ������ �������
	SittingOut		UMETA(DisplayName = "Sitting Out"),		// ����� �� ������, �� ���������� �������
	Playing			UMETA(DisplayName = "Playing"),			// ��������� � ������� ������� (�����) - ����� ��������������
	Folded			UMETA(DisplayName = "Folded"),			// ������� ����� � ������� �������
	Checked			UMETA(DisplayName = "Checked"),			// ������ ���
	Called			UMETA(DisplayName = "Called"),			// ������ ����
	Bet				UMETA(DisplayName = "Bet"),				// ������ ���
	Raised			UMETA(DisplayName = "Raised"),			// ������ ����
	AllIn			UMETA(DisplayName = "All-In")			// ����� ��-����
};

// ������ ������� ����/�������
UENUM(BlueprintType)
enum class EGameStage : uint8
{
	WaitingForPlayers	UMETA(DisplayName = "Waiting For Players"), // �������� �������
	Preflop				UMETA(DisplayName = "Preflop"),			// ������� (������ ����� ������� ��������� ����)
	Flop				UMETA(DisplayName = "Flop"),				// ���� (�������� 3 ����� �����)
	Turn				UMETA(DisplayName = "Turn"),				// ���� (�������� 4-� ����� �����)
	River				UMETA(DisplayName = "River"),				// ����� (�������� 5-� ����� �����)
	Showdown			UMETA(DisplayName = "Showdown")			// �������� ����
};



// ���������, �������������� ���� ��������� �����
USTRUCT(BlueprintType)
struct FCard
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	ECardSuit Suit = ECardSuit::Clubs; // ����� �� ���������

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	ECardRank Rank = ECardRank::Two; // ���� �� ���������

	// ������������
	FCard() = default;
	FCard(ECardSuit InSuit, ECardRank InRank) : Suit(InSuit), Rank(InRank) {}

	// ������� ��� ���������� ������������� (��� �������)
	FString ToString() const;

	// �������� ���������
	bool operator==(const FCard& Other) const
	{
		return Suit == Other.Suit && Rank == Other.Rank;
	}
	// ������� �������� ����������� ��� ��������
	bool operator!=(const FCard& Other) const
	{
		return !(*this == Other);
	}
	// (�����������) �������� < ��� ����������, ���� �����������
	bool operator<(const FCard& Other) const
	{
		return Rank < Other.Rank || (Rank == Other.Rank && Suit < Other.Suit);
	}
};

// ���������, �������������� ��������� ������ �������� ����������
USTRUCT(BlueprintType)
struct FPokerHandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Poker")
	EPokerHandRank HandRank = EPokerHandRank::HighCard; // ���� ����������

	// ������ - �����, ������������ ���� ���� ��� ������ ������
	// ������ ������ �����, �.�. ����� � ������� �� ����� ��� ������������ ������
	UPROPERTY(BlueprintReadOnly, Category = "Poker")
	TArray<ECardRank> Kickers;

	// (�����������) ����� �������� ����� �������� ���� ����������
	// UPROPERTY(BlueprintReadOnly, Category = "Poker")
	// ECardRank PrimaryRank = ECardRank::Two; // ���� ����, ������, ����, ������� ����� ������/�����
	// UPROPERTY(BlueprintReadOnly, Category = "Poker")
	// ECardRank SecondaryRank = ECardRank::Two; // ���� ������ ����, ���� � ����-�����

	FPokerHandResult() = default;
};

// ���������, �������� ������ �� ����� ������ (��� �����) �� ������
USTRUCT(BlueprintType)
struct FPlayerSeatData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Player Seat")
	int32 SeatIndex = -1; // ������ ����� (0-8)

	UPROPERTY(BlueprintReadOnly, Category = "Player Seat")
	FString PlayerName = TEXT("Empty"); // ��� ������

	UPROPERTY(BlueprintReadOnly, Category = "Player Seat")
	int64 PlayerId = -1; // ID ������������ (���� ���������)

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ����� ����� ���� ������ �����
		int64 Stack = 0; // ���������� �����

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite - ���� ����� ������ �����
		TArray<FCard> HoleCards; // ��������� ����� (������ 2)

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ��� ���������� �������
		EPlayerStatus Status = EPlayerStatus::Waiting; // ������� ������ ������

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ��� ���������, ��� ���
		bool bIsBot = false; // �������� �� ��� ����� �����

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ��� ��������� ����
		bool bIsTurn = false; // ������ ��� ����� ������?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ��� ����������� �������
		bool bIsDealer = false; // ��������� �� ������ ������ �� ���� �����?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ��� ����������� SB
		bool bIsSmallBlind = false; // �������� �� ����� ������?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ��� ����������� BB
		bool bIsBigBlind = false; // �������� �� ������� ������?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ��� ����������� ������
		int64 CurrentBet = 0; // �����, ������������ � ������� ������ ��������

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite ��� ����������� ������ ��� ����
		bool bIsSittingIn = true; // ��������� �� ����� ������ (�� � Sit Out)?

	FPlayerSeatData() = default;

	// ����������� ��� ������� �������������
	FPlayerSeatData(int32 InSeatIndex, const FString& InPlayerName, int64 InPlayerId, int64 InStack, bool InIsBot)
		: SeatIndex(InSeatIndex), PlayerName(InPlayerName), PlayerId(InPlayerId), Stack(InStack), bIsBot(InIsBot)
	{
		Status = EPlayerStatus::Waiting; // ��������� ������
		HoleCards.Reserve(2); // ����������� ����� ��� 2 �����
	}
};