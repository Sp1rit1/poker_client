#include "PokerDataTypes.h" 
#include "UObject/Package.h" 

// Реализация метода ToString для структуры FCard
FString FCard::ToString() const
{
    FString SuitString = TEXT("UnknownSuit"); // Значение по умолчанию на случай ошибки
    FString RankString = TEXT("UnknownRank"); // Значение по умолчанию на случай ошибки

    // --- Формирование Строки для Масти ---
    switch (Suit)
    {
    case ECardSuit::Clubs:    SuitString = TEXT("Clubs");    break;
    case ECardSuit::Diamonds: SuitString = TEXT("Diamonds"); break;
    case ECardSuit::Hearts:   SuitString = TEXT("Hearts");   break;
    case ECardSuit::Spades:   SuitString = TEXT("Spades");   break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("FCard::ToString() - Unknown ECardSuit value: %d"), static_cast<int32>(Suit));
        // SuitString останется "UnknownSuit"
        break;
    }

    // --- Формирование Строки для Ранга (чтобы соответствовать вашим Row Names "Hearts_2", "Spades_Ace") ---
    switch (Rank)
    {
    case ECardRank::Two:   RankString = TEXT("2");     break;
    case ECardRank::Three: RankString = TEXT("3");     break;
    case ECardRank::Four:  RankString = TEXT("4");     break;
    case ECardRank::Five:  RankString = TEXT("5");     break;
    case ECardRank::Six:   RankString = TEXT("6");     break;
    case ECardRank::Seven: RankString = TEXT("7");     break;
    case ECardRank::Eight: RankString = TEXT("8");     break;
    case ECardRank::Nine:  RankString = TEXT("9");     break;
    case ECardRank::Ten:   RankString = TEXT("10");    break; // Используем "10", как в вашем примере "Hearts_10"
    case ECardRank::Jack:  RankString = TEXT("Jack");  break;
    case ECardRank::Queen: RankString = TEXT("Queen"); break;
    case ECardRank::King:  RankString = TEXT("King");  break;
    case ECardRank::Ace:   RankString = TEXT("Ace");   break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("FCard::ToString() - Unknown ECardRank value: %d"), static_cast<int32>(Rank));
        // RankString останется "UnknownRank"
        break;
    }

    // Собираем итоговую строку в формате "Масть_Ранг"
    return FString::Printf(TEXT("%s_%s"), *SuitString, *RankString);
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