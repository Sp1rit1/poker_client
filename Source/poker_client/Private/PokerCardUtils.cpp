#include "PokerCardUtils.h" // или PokerBlueprintUtils.h
#include "PokerDataTypes.h" // Для FShowdownPlayerInfo, FCard, FPokerHandResult

// --- Функции для FShowdownPlayerInfo ---

int32 UPokerCardUtils::GetShowdownPlayerSeatIndex(const FShowdownPlayerInfo& ShowdownInfo)
{
    return ShowdownInfo.SeatIndex;
}

FString UPokerCardUtils::GetShowdownPlayerName(const FShowdownPlayerInfo& ShowdownInfo)
{
    return ShowdownInfo.PlayerName;
}

TArray<FCard> UPokerCardUtils::GetShowdownPlayerHoleCards(const FShowdownPlayerInfo& ShowdownInfo)
{
    return ShowdownInfo.HoleCards;
}

FPokerHandResult UPokerCardUtils::GetShowdownPlayerHandResult(const FShowdownPlayerInfo& ShowdownInfo)
{
    return ShowdownInfo.HandResult;
}

bool UPokerCardUtils::IsShowdownPlayerWinner(const FShowdownPlayerInfo& ShowdownInfo)
{
    return ShowdownInfo.bIsWinner;
}

int64 UPokerCardUtils::GetShowdownPlayerAmountWon(const FShowdownPlayerInfo& ShowdownInfo)
{
    return ShowdownInfo.AmountWon;
}

int64 UPokerCardUtils::GetShowdownPlayerNetResult(const FShowdownPlayerInfo& ShowdownInfo)
{
    return ShowdownInfo.NetResult;
}

EPlayerStatus UPokerCardUtils::GetShowdownPlayerStatus(const FShowdownPlayerInfo& ShowdownInfo)
{
    return ShowdownInfo.PlayerStatusAtShowdown;
}

FString UPokerCardUtils::Conv_CardToString(const FCard& Card)
{
    return Card.ToRussianString(); // Просто вызываем существующий метод структуры
}

EPokerHandRank UPokerCardUtils::GetHandRankFromResult(const FPokerHandResult& HandResult)
{
    return HandResult.HandRank;
}

TArray<ECardRank> UPokerCardUtils::GetKickersFromResult(const FPokerHandResult& HandResult)
{
    return HandResult.Kickers;
}

FString UPokerCardUtils::Conv_PokerHandRankToString(EPokerHandRank HandRankEnum)
{
    // UEnum::GetDisplayValueAsText() более предпочтителен, если у вас есть UMETA(DisplayName) для всех значений Enum.
    // Если нет, то простой switch:
    const UEnum* EnumPtr = StaticEnum<EPokerHandRank>();
    if (EnumPtr)
    {
        return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(HandRankEnum)).ToString();
    }

    // Fallback, если GetDisplayNameTextByValue не сработал или для простоты
    switch (HandRankEnum)
    {
    case EPokerHandRank::HighCard: return TEXT("High Card");
    case EPokerHandRank::OnePair: return TEXT("One Pair");
    case EPokerHandRank::TwoPair: return TEXT("Two Pair");
    case EPokerHandRank::ThreeOfAKind: return TEXT("Three of a Kind");
    case EPokerHandRank::Straight: return TEXT("Straight");
    case EPokerHandRank::Flush: return TEXT("Flush");
    case EPokerHandRank::FullHouse: return TEXT("Full House");
    case EPokerHandRank::FourOfAKind: return TEXT("Four of a Kind");
    case EPokerHandRank::StraightFlush: return TEXT("Straight Flush");
    case EPokerHandRank::RoyalFlush: return TEXT("Royal Flush");
    default: return TEXT("Unknown HandRank");
    }
}