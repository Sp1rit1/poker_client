#include "PokerDataTypes.h" 
#include "UObject/Package.h" 

// Реализация метода ToString для структуры FCard
FString FCard::ToString() const
{
    FString RankStr = TEXT("?");
    FString SuitStr = TEXT("?");

    // --- Обработка Ранга с помощью switch-case ---
    switch (Rank)
    {
    case ECardRank::Two:   RankStr = TEXT("2"); break;
    case ECardRank::Three: RankStr = TEXT("3"); break;
    case ECardRank::Four:  RankStr = TEXT("4"); break;
    case ECardRank::Five:  RankStr = TEXT("5"); break;
    case ECardRank::Six:   RankStr = TEXT("6"); break;
    case ECardRank::Seven: RankStr = TEXT("7"); break;
    case ECardRank::Eight: RankStr = TEXT("8"); break;
    case ECardRank::Nine:  RankStr = TEXT("9"); break;
    case ECardRank::Ten:   RankStr = TEXT("T"); break;
    case ECardRank::Jack:  RankStr = TEXT("J"); break;
    case ECardRank::Queen: RankStr = TEXT("Q"); break;
    case ECardRank::King:  RankStr = TEXT("K"); break;
    case ECardRank::Ace:   RankStr = TEXT("A"); break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("FCard::ToString() - Unknown ECardRank value: %d"), static_cast<int32>(Rank));
        break;
    }

    // --- Обработка Масти с помощью switch-case ---
    switch (Suit)
    {
    case ECardSuit::Clubs:    SuitStr = TEXT("c"); break;
    case ECardSuit::Diamonds: SuitStr = TEXT("d"); break;
    case ECardSuit::Hearts:   SuitStr = TEXT("h"); break;
    case ECardSuit::Spades:   SuitStr = TEXT("s"); break;
    default:
        // Можно добавить логирование или оставить "?" если Suit невалиден
        UE_LOG(LogTemp, Warning, TEXT("FCard::ToString() - Unknown ECardSuit value: %d"), static_cast<int32>(Suit));
        break;
    }

    return FString::Printf(TEXT("%s%s"), *RankStr, *SuitStr);
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

FString PokerRankToRussianString(EPokerHandRank HandRank)
{
    switch (HandRank)
    {
    case EPokerHandRank::HighCard:     return TEXT("Старшая Карта");
    case EPokerHandRank::OnePair:      return TEXT("Одна Пара");
    case EPokerHandRank::TwoPair:      return TEXT("Две Пары");
    case EPokerHandRank::ThreeOfAKind: return TEXT("Сет"); // или "Тройка"
    case EPokerHandRank::Straight:     return TEXT("Стрит");
    case EPokerHandRank::Flush:        return TEXT("Флеш");
    case EPokerHandRank::FullHouse:    return TEXT("Фулл-Хаус");
    case EPokerHandRank::FourOfAKind:  return TEXT("Каре");
    case EPokerHandRank::StraightFlush:return TEXT("Стрит-Флеш");
    case EPokerHandRank::RoyalFlush:   return TEXT("Роял-Флеш");
    default:                           return TEXT("Неизвестная Комбинация");
    }
}

uint32 GetTypeHash(const FCard& Card)
{
    uint32 SuitHash = GetTypeHash(Card.Suit);
    uint32 RankHash = GetTypeHash(Card.Rank);
    return HashCombine(SuitHash, RankHash);
}