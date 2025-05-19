#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h" // Needed for UENUM, USTRUCT
#include "PokerDataTypes.generated.h" // MUST be the last include

// Масть карты
UENUM(BlueprintType)
enum class ECardSuit : uint8
{
	Clubs		UMETA(DisplayName = "Clubs"),	 // Трефы
	Diamonds	UMETA(DisplayName = "Diamonds"), // Бубны
	Hearts		UMETA(DisplayName = "Hearts"),	 // Червы
	Spades		UMETA(DisplayName = "Spades")	 // Пики
};

// Ранг карты
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
	Jack		UMETA(DisplayName = "Jack"),	 // Валет
	Queen		UMETA(DisplayName = "Queen"),	 // Дама
	King		UMETA(DisplayName = "King"),	 // Король
	Ace			UMETA(DisplayName = "Ace")		 // Туз
};

// Ранг покерной комбинации (от младшей к старшей)
UENUM(BlueprintType)
enum class EPokerHandRank : uint8
{
	HighCard		UMETA(DisplayName = "High Card"),		// Старшая карта
	OnePair			UMETA(DisplayName = "One Pair"),		// Одна пара
	TwoPair			UMETA(DisplayName = "Two Pair"),		// Две пары
	ThreeOfAKind	UMETA(DisplayName = "Three of a Kind"), // Тройка (Сет/Трипс)
	Straight		UMETA(DisplayName = "Straight"),		// Стрит
	Flush			UMETA(DisplayName = "Flush"),			// Флеш
	FullHouse		UMETA(DisplayName = "Full House"),		// Фулл Хаус
	FourOfAKind		UMETA(DisplayName = "Four of a Kind"),	// Каре
	StraightFlush	UMETA(DisplayName = "Straight Flush"),	// Стрит Флеш
	RoyalFlush		UMETA(DisplayName = "Royal Flush")		// Роял Флеш
};

UENUM(BlueprintType)
enum class EPlayerStatus : uint8
{
	Waiting			UMETA(DisplayName = "Waiting"),
	SittingOut		UMETA(DisplayName = "Sitting Out"),
	Playing			UMETA(DisplayName = "Playing"), // Игрок в текущей раздаче (после блайндов/раздачи карт)
	MustPostSmallBlind UMETA(DisplayName = "Must Post Small Blind"), // Ожидание постановки малого блайнда
	MustPostBigBlind UMETA(DisplayName = "Must Post Big Blind"),   // Ожидание постановки большого блайнда
	Folded			UMETA(DisplayName = "Folded"),
	Checked			UMETA(DisplayName = "Checked"),
	Called			UMETA(DisplayName = "Called"),
	Bet				UMETA(DisplayName = "Bet"),
	Raised			UMETA(DisplayName = "Raised"),
	AllIn			UMETA(DisplayName = "All-In"),
	MAX_None        UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EGameStage : uint8
{
	WaitingForPlayers	UMETA(DisplayName = "Waiting For Players"),
	Dealing				UMETA(DisplayName = "Dealing"),             // Общая стадия подготовки руки
	WaitingForSmallBlind UMETA(DisplayName = "Waiting For Small Blind"),
	WaitingForBigBlind	UMETA(DisplayName = "Waiting For Big Blind"),
	Preflop				UMETA(DisplayName = "Preflop"),
	Flop				UMETA(DisplayName = "Flop"),
	Turn				UMETA(DisplayName = "Turn"),
	River				UMETA(DisplayName = "River"),
	Showdown			UMETA(DisplayName = "Showdown")
};

UENUM(BlueprintType)
enum class EPlayerAction : uint8
{
	None		UMETA(DisplayName = "None"),
	Fold		UMETA(DisplayName = "Fold"),
	Check		UMETA(DisplayName = "Check"),
	Call		UMETA(DisplayName = "Call"),
	Bet			UMETA(DisplayName = "Bet"),
	Raise		UMETA(DisplayName = "Raise"),
	AllIn       UMETA(DisplayName = "All-In"),
	PostBlind   UMETA(DisplayName = "Post Blind") // Новое действие
};



// Структура, представляющая одну игральную карту
USTRUCT(BlueprintType)
struct FCard
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	ECardSuit Suit = ECardSuit::Clubs; // Масть по умолчанию

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	ECardRank Rank = ECardRank::Two; // Ранг по умолчанию

	// Конструкторы
	FCard() = default;
	FCard(ECardSuit InSuit, ECardRank InRank) : Suit(InSuit), Rank(InRank) {}

	// Функция для текстового представления (для отладки)
	FString ToString() const;

	// Оператор сравнения
	bool operator==(const FCard& Other) const
	{
		return Suit == Other.Suit && Rank == Other.Rank;
	}
	// Добавим оператор неравенства для удобства
	bool operator!=(const FCard& Other) const
	{
		return !(*this == Other);
	}
	// (Опционально) Оператор < для сортировки, если понадобится
	bool operator<(const FCard& Other) const
	{
		return Rank < Other.Rank || (Rank == Other.Rank && Suit < Other.Suit);
	}
};

// Структура, представляющая результат оценки покерной комбинации
USTRUCT(BlueprintType)
struct FPokerHandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Poker")
	EPokerHandRank HandRank = EPokerHandRank::HighCard; // Ранг комбинации

	// Кикеры - карты, определяющие силу руки при равных рангах
	// Храним только ранги, т.к. масть в кикерах не важна для стандартного покера
	UPROPERTY(BlueprintReadOnly, Category = "Poker")
	TArray<ECardRank> Kickers;

	// (Опционально) Можно добавить ранги ключевых карт комбинации
	// UPROPERTY(BlueprintReadOnly, Category = "Poker")
	// ECardRank PrimaryRank = ECardRank::Two; // Ранг пары, тройки, каре, старшая карта стрита/флеша
	// UPROPERTY(BlueprintReadOnly, Category = "Poker")
	// ECardRank SecondaryRank = ECardRank::Two; // Ранг второй пары, пары в фулл-хаусе

	FPokerHandResult() = default;
};

// Структура, хранящая данные об одном игроке (или месте) за столом
USTRUCT(BlueprintType)
struct FPlayerSeatData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Player Seat")
	int32 SeatIndex = -1; // Индекс места (0-8)

	UPROPERTY(BlueprintReadOnly, Category = "Player Seat")
	FString PlayerName = TEXT("Empty"); // Имя игрока

	UPROPERTY(BlueprintReadOnly, Category = "Player Seat")
	int64 PlayerId = -1; // ID пользователя (если залогинен)

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite чтобы можно было менять извне
	int64 Stack = 0; // Количество фишек

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite - сюда будем класть карты
	TArray<FCard> HoleCards; // Карманные карты (обычно 2)

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite для обновления статуса
	EPlayerStatus Status = EPlayerStatus::Waiting; // Текущий статус игрока

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite для установки, кто бот
	bool bIsBot = false; // Является ли это место ботом

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite для подсветки хода
	bool bIsTurn = false; // Сейчас ход этого игрока?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite для отображения баттона
	bool bIsDealer = false; // Находится ли баттон дилера на этом месте?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite для отображения SB
	bool bIsSmallBlind = false; // Поставил ли малый блайнд?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite для отображения BB
	bool bIsBigBlind = false; // Поставил ли большой блайнд?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite для отображения ставки
	int64 CurrentBet = 0; // Сумма, поставленная в текущем раунде торговли

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat") // ReadWrite для возможности сидеть вне игры
	bool bIsSittingIn = true; // Участвует ли игрок сейчас (не в Sit Out)?

	UPROPERTY(BlueprintReadWrite, Category = "Player Seat")
	bool bHasActedThisSubRound = false; // Сделал ли игрок ход в текущем "под-раунде" ставок

	FPlayerSeatData() = default;

	// Конструктор для удобной инициализации
	FPlayerSeatData(int32 InSeatIndex, const FString& InPlayerName, int64 InPlayerId, int64 InStack, bool InIsBot)
		: SeatIndex(InSeatIndex), PlayerName(InPlayerName), PlayerId(InPlayerId), Stack(InStack), bIsBot(InIsBot)
	{
		Status = EPlayerStatus::Waiting; // Начальный статус
		HoleCards.Reserve(2); // Резервируем место под 2 карты
	}
};