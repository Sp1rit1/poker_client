#include "PokerCardUtils.h" 
#include "PokerDataTypes.h" 

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

FString UPokerCardUtils::Conv_CardToRussianString(const FCard& Card)
{
    return Card.ToRussianString(); 
}

FString UPokerCardUtils::Conv_CardToString(const FCard& Card)
{
    return Card.ToString();
}

EPokerHandRank UPokerCardUtils::GetHandRankFromResult(const FPokerHandResult& HandResult)
{
    return HandResult.HandRank;
}

TArray<ECardRank> UPokerCardUtils::GetKickersFromResult(const FPokerHandResult& HandResult)
{
    return HandResult.Kickers;
}

FString UPokerCardUtils::Conv_PokerHandRankToRussianString(EPokerHandRank HandRankEnum)
{
    // Вызываем свободную функцию, определенную в PokerDataTypes
    return PokerRankToRussianString(HandRankEnum);
}
