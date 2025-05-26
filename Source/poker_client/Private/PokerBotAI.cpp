#include "PokerBotAI.h"
#include "OfflinePokerGameState.h"
#include "PokerDataTypes.h"
#include "PokerHandEvaluator.h"
#include "Math/UnrealMathUtility.h"
#include "Logging/LogMacros.h"
#include "Algo/Sort.h" 



UPokerBotAI::UPokerBotAI()
{
    // Устанавливаем дефолтные значения, которые будут переопределены через SetPersonalityFactors
    AggressivenessFactor = 0.5f;
    BluffFrequency = 0.1f;
    TightnessFactor = 0.5f;

    bIsTesting = false; // Оставляем
    TestFixedRandValue = 0.5f; // Оставляем

    UE_LOG(LogTemp, Log, TEXT("UPokerBotAI instance created with DEFAULT personality."));
}

void UPokerBotAI::SetPersonalityFactors(const FBotPersonalitySettings& Settings)
{
    AggressivenessFactor = FMath::Clamp(Settings.Aggressiveness, 0.0f, 1.0f);
    BluffFrequency = FMath::Clamp(Settings.BluffFrequency, 0.0f, 1.0f); // Можно ограничить верхний предел, если 1.0 это слишком много
    TightnessFactor = FMath::Clamp(Settings.Tightness, 0.0f, 1.0f);

    UE_LOG(LogTemp, Log, TEXT("UPokerBotAI Personality SET for next action - Aggro: %.2f, BluffFreq: %.2f, Tightness: %.2f"),
        AggressivenessFactor, BluffFrequency, TightnessFactor);
}


EPlayerPokerPosition UPokerBotAI::GetPlayerPosition(const UOfflinePokerGameState* GameState, int32 BotSeatIndex, int32 NumActivePlayers) const
{
    if (!GameState || !GameState->Seats.IsValidIndex(BotSeatIndex) || NumActivePlayers < 2 || GameState->DealerSeat == -1)
    {
        return EPlayerPokerPosition::Unknown;
    }

    // Получаем актуальный порядок игроков, которые еще в игре и сидят за столом
    TArray<int32> ActiveSeatsInOrder;
    int32 CurrentSeat = GameState->DealerSeat;
    for (int32 i = 0; i < GameState->Seats.Num(); ++i) // Проходим не более чем всего мест
    {

        bool bFoundNextSitting = false;
        for (int32 attempts = 0; attempts < GameState->Seats.Num(); ++attempts) {
            CurrentSeat = (CurrentSeat + 1) % GameState->Seats.Num();
            if (GameState->Seats[CurrentSeat].bIsSittingIn) {
                bFoundNextSitting = true;
                break;
            }
        }
        if (!bFoundNextSitting) { // Не должно произойти, если NumActivePlayers > 0
            UE_LOG(LogTemp, Warning, TEXT("GetPlayerPosition: Could not find next sitting player from %d"), GameState->DealerSeat);
            return EPlayerPokerPosition::Unknown;
        }

        // Добавляем только если еще не добавили всех NumActivePlayers
        // (на случай, если в GameState->Seats больше записей, чем активных NumActivePlayers)
        if (ActiveSeatsInOrder.Num() < NumActivePlayers && !ActiveSeatsInOrder.Contains(CurrentSeat))
        {
            ActiveSeatsInOrder.Add(CurrentSeat);
        }
        if (ActiveSeatsInOrder.Num() == NumActivePlayers) break;
    }


    if (ActiveSeatsInOrder.Num() != NumActivePlayers || !ActiveSeatsInOrder.Contains(BotSeatIndex)) {
        UE_LOG(LogTemp, Warning, TEXT("GetPlayerPosition: Mismatch in active players or bot not found. NumActive: %d, OrderSize: %d"), NumActivePlayers, ActiveSeatsInOrder.Num());
        return EPlayerPokerPosition::Unknown;
    }

    int32 BotPosInOrder = ActiveSeatsInOrder.Find(BotSeatIndex);

    if (NumActivePlayers == 2) { // Heads-up

        if (BotSeatIndex == GameState->DealerSeat) 
        {
            // Логируем, что BotPosInOrder должно соответствовать последнему элементу ActiveSeatsInOrder
            UE_LOG(LogTemp, Verbose, TEXT("GetPlayerPosition HU: Bot %d IS Dealer %d. BotPosInOrder: %d. ActiveSeatsOrder.Last: %d. Assigning SB."), BotSeatIndex, GameState->DealerSeat, BotPosInOrder, ActiveSeatsInOrder.Last(0)); // Last(0) чтобы не было крэша если массив пуст
            return EPlayerPokerPosition::SB;
        }
        else
        {
            // Логируем, что BotPosInOrder должно соответствовать первому элементу ActiveSeatsInOrder
            UE_LOG(LogTemp, Verbose, TEXT("GetPlayerPosition HU: Bot %d IS NOT Dealer %d. BotPosInOrder: %d. ActiveSeatsOrder.First: %d. Assigning BB."), BotSeatIndex, GameState->DealerSeat, BotPosInOrder, ActiveSeatsInOrder.Num() > 0 ? ActiveSeatsInOrder[0] : -1);
            return EPlayerPokerPosition::BB;
        }
    }


    if (BotSeatIndex == ActiveSeatsInOrder[0]) return EPlayerPokerPosition::SB;
    if (BotSeatIndex == ActiveSeatsInOrder[1]) return EPlayerPokerPosition::BB;
    if (BotSeatIndex == ActiveSeatsInOrder.Last()) return EPlayerPokerPosition::BTN;

    int32 SeatsBeforeButton = ActiveSeatsInOrder.Num() - 1 - BotPosInOrder; // Сколько игроков будет ходить после бота до баттона

    if (NumActivePlayers <= 6) { // Столы до 6-max
        if (BotPosInOrder == 2) return EPlayerPokerPosition::UTG; // Первый после BB
        if (SeatsBeforeButton <= 1) return EPlayerPokerPosition::CO; // CO или HJ (если считать CO как HJ для <6)
        return EPlayerPokerPosition::MP1; // Средняя
    }
    else { // Столы > 6-max (7-9)
        if (BotPosInOrder == 2) return EPlayerPokerPosition::UTG;
        if (BotPosInOrder == 3) return EPlayerPokerPosition::UTG1;
        if (SeatsBeforeButton == 0) return EPlayerPokerPosition::BTN; // Уже обработано
        if (SeatsBeforeButton == 1) return EPlayerPokerPosition::CO;
        if (SeatsBeforeButton == 2) return EPlayerPokerPosition::HJ;
        // Остальные - средние позиции
        if (NumActivePlayers == 7 && BotPosInOrder == 4) return EPlayerPokerPosition::MP1; // MP для 7-max
        if (NumActivePlayers >= 8 && BotPosInOrder == 4) return EPlayerPokerPosition::MP1;
        if (NumActivePlayers >= 9 && BotPosInOrder == 5) return EPlayerPokerPosition::MP2;
        return EPlayerPokerPosition::MP1; // Общая средняя
    }
}


float UPokerBotAI::CalculatePreflopHandStrength(const FCard& Card1, const FCard& Card2, EPlayerPokerPosition BotPosition, int32 NumActivePlayers) const
{


    int32 RankVal1 = static_cast<int32>(Card1.Rank); // Two=0, ..., Ace=12
    int32 RankVal2 = static_cast<int32>(Card2.Rank);
    bool bSuited = (Card1.Suit == Card2.Suit);

    // Упорядочим: HigherRankVal, LowerRankVal
    int32 HigherRankVal = FMath::Max(RankVal1, RankVal2);
    int32 LowerRankVal = FMath::Min(RankVal1, RankVal2);

    float Score = 0.0f;

    // 1. Очки за старшую карту (по Чену, примерно)
    if (HigherRankVal == static_cast<int32>(ECardRank::Ace)) Score = 10.0f;
    else if (HigherRankVal == static_cast<int32>(ECardRank::King)) Score = 8.0f;
    else if (HigherRankVal == static_cast<int32>(ECardRank::Queen)) Score = 7.0f;
    else if (HigherRankVal == static_cast<int32>(ECardRank::Jack)) Score = 6.0f;
    else Score = (HigherRankVal + 2) / 2.0f; // T=5, 9=4.5, ... 2=2

    // 2. Пары (Умножаем на 2 по Чену, минимум 5 очков)
    if (RankVal1 == RankVal2) {
        Score *= 2.0f;
        if (Score < 5.0f) Score = 5.0f; // Пары 22-44 получают 5 очков
    }

    // 3. Одномастность (добавляет 2 очка по Чену)
    if (bSuited) {
        Score += 2.0f;
    }

    // 4. Связанность (коннекторы, по Чену)
    int32 Gap = HigherRankVal - LowerRankVal;
    if (RankVal1 != RankVal2) { // Не для пар
        if (Gap == 1) {} // Коннекторы - нет штрафа
        else if (Gap == 2) Score -= 1.0f;
        else if (Gap == 3) Score -= 2.0f;
        else if (Gap == 4) Score -= 4.0f;
        else Score -= 5.0f; // Большой гэп
    }

    // 5. Дополнительные очки за коннекторы с небольшим гэпом (0 или 1), если обе карты ниже дамы
    // и нет туза или короля (чтобы не переоценивать слабые тузы/короли)
    if (Gap <= 1 && HigherRankVal < static_cast<int32>(ECardRank::Queen) && RankVal1 != RankVal2) {
        Score += 1.0f;
    }

    // Нормализация результата (очень грубая, максимум по Чену около 20 для AA)
    // Мы хотим получить что-то в диапазоне ~0.1 - 1.0
    float NormalizedStrength = Score / 20.0f;


    // --- Коррекция на позицию и количество игроков (упрощенно) ---
    float PositionAdjustment = 0.0f;
    float PlayerCountAdjustment = 0.0f;

    // Позиция
    switch (BotPosition)
    {
    case EPlayerPokerPosition::UTG:
    case EPlayerPokerPosition::UTG1:
        PositionAdjustment = -0.15f * TightnessFactor; // Требуется более сильная рука
        break;
    case EPlayerPokerPosition::MP1:
    case EPlayerPokerPosition::MP2:
        PositionAdjustment = -0.05f * TightnessFactor;
        break;
    case EPlayerPokerPosition::CO:
    case EPlayerPokerPosition::HJ:
        PositionAdjustment = 0.05f * (1.0f - TightnessFactor); // Можно играть чуть шире
        break;
    case EPlayerPokerPosition::BTN:
        PositionAdjustment = 0.1f * (1.0f - TightnessFactor); // Лучшая позиция
        break;
    case EPlayerPokerPosition::SB:
        PositionAdjustment = -0.05f; // Вне позиции, но есть "скидка"
        break;
    case EPlayerPokerPosition::BB:
        PositionAdjustment = 0.0f; // Последнее слово, может играть шире, если не было рейзов
        break;
    default: break;
    }

    // Количество игроков
    if (NumActivePlayers >= 7) PlayerCountAdjustment = -0.1f;
    else if (NumActivePlayers <= 3) PlayerCountAdjustment = 0.1f; // В коротких столах руки сильнее

    NormalizedStrength += PositionAdjustment + PlayerCountAdjustment;

    return FMath::Clamp(NormalizedStrength, 0.0f, 1.0f);
}


EPlayerAction UPokerBotAI::GetBestAction(
    const UOfflinePokerGameState* GameState,
    const FPlayerSeatData& BotPlayerSeatData,
    const TArray<EPlayerAction>& AllowedActions,
    int64 CurrentBetToCallOnTable,
    int64 MinValidPureRaiseAmount,
    int64& OutDecisionAmount
)
{
    OutDecisionAmount = 0;
    FString AllowedActionsString = TEXT("None");
    if (AllowedActions.Num() > 0) {
        AllowedActionsString = TEXT("");
        for (EPlayerAction Action : AllowedActions) { AllowedActionsString += UEnum::GetDisplayValueAsText(Action).ToString() + TEXT(" "); }
        AllowedActionsString = AllowedActionsString.TrimEnd();
    }

    UE_LOG(LogTemp, Warning, TEXT("==================== BotAI %s (Seat %d) - GetBestAction START (Testing: %s) ===================="),
        *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, bIsTesting ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Warning, TEXT("  GameState Stage: %s, Bot Status: %s, Stack: %lld, CurrentBetInRound: %lld"),
        GameState ? *UEnum::GetDisplayValueAsText(GameState->CurrentStage).ToString() : TEXT("NULL_GS"),
        *UEnum::GetDisplayValueAsText(BotPlayerSeatData.Status).ToString(),
        BotPlayerSeatData.Stack, BotPlayerSeatData.CurrentBet);
    UE_LOG(LogTemp, Warning, TEXT("  AllowedActions: [%s] (Count: %d)"), *AllowedActionsString, AllowedActions.Num());
    UE_LOG(LogTemp, Warning, TEXT("  CurrentBetToCallOnTable: %lld, MinValidPureRaiseAmount: %lld"), CurrentBetToCallOnTable, MinValidPureRaiseAmount);
    if (GameState) {
        UE_LOG(LogTemp, Warning, TEXT("  GS Pot: %lld, LastAggressor: %d (Amt: %lld), Opener: %d, SB: %lld, BB: %lld"),
            GameState->Pot, GameState->LastAggressorSeatIndex, GameState->LastBetOrRaiseAmountInCurrentRound,
            GameState->PlayerWhoOpenedBettingThisRound, GameState->SmallBlindAmount, GameState->BigBlindAmount);
    }

    if (!GameState || AllowedActions.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("BotAI %s (S%d): FATAL - GameState is null or No AllowedActions. Returning Fold."), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
        return EPlayerAction::Fold;
    }

    if (GameState->CurrentStage == EGameStage::WaitingForSmallBlind && BotPlayerSeatData.Status == EPlayerStatus::MustPostSmallBlind) {
        if (AllowedActions.Contains(EPlayerAction::PostBlind)) {
            UE_LOG(LogTemp, Log, TEXT("BotAI %s (S%d) Path: BlindDecision. ChosenAction: PostBlind (SB)"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
            return EPlayerAction::PostBlind;
        }
        UE_LOG(LogTemp, Error, TEXT("BotAI %s (S%d) State ERROR: WaitingForSmallBlind but PostBlind NOT in AllowedActions! Allowed: [%s]. Defaulting to first allowed."),
            *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *AllowedActionsString);
        return AllowedActions.IsEmpty() ? EPlayerAction::Fold : AllowedActions[0];
    }
    else if (GameState->CurrentStage == EGameStage::WaitingForBigBlind && BotPlayerSeatData.Status == EPlayerStatus::MustPostBigBlind) {
        if (AllowedActions.Contains(EPlayerAction::PostBlind)) {
            UE_LOG(LogTemp, Log, TEXT("BotAI %s (S%d) Path: BlindDecision. ChosenAction: PostBlind (BB)"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
            return EPlayerAction::PostBlind;
        }
        UE_LOG(LogTemp, Error, TEXT("BotAI %s (S%d) State ERROR: WaitingForBigBlind but PostBlind NOT in AllowedActions! Allowed: [%s]. Defaulting to first allowed."),
            *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *AllowedActionsString);
        return AllowedActions.IsEmpty() ? EPlayerAction::Fold : AllowedActions[0];
    }

    EPlayerAction ChosenAction = EPlayerAction::Fold;
    int32 NumActivePlayersInGame = 0;
    for (const auto& Seat : GameState->Seats) {
        if (Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded && Seat.Status != EPlayerStatus::SittingOut) {
            NumActivePlayersInGame++;
        }
    }
    if (NumActivePlayersInGame == 0 && GameState->Seats.Num() > 0) NumActivePlayersInGame = GameState->Seats.Num();
    else if (NumActivePlayersInGame == 0 && GameState->Seats.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("BotAI %s (S%d): GameState->Seats is empty! Defaulting to Fold."), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
        return EPlayerAction::Fold;
    }

    EPlayerPokerPosition BotPosition = GetPlayerPosition(GameState, BotPlayerSeatData.SeatIndex, NumActivePlayersInGame);
    int32 NumOpponentsStillInHand = CountActiveOpponents(GameState, BotPlayerSeatData.SeatIndex);
    float EffectiveHandStrength = 0.0f;
    bool bIsFacingBet = CurrentBetToCallOnTable > BotPlayerSeatData.CurrentBet;
    int64 AmountToCallAbsolute = bIsFacingBet ? (CurrentBetToCallOnTable - BotPlayerSeatData.CurrentBet) : 0;
    if (AmountToCallAbsolute < 0) AmountToCallAbsolute = 0;

    UE_LOG(LogTemp, Verbose, TEXT("BotAI %s (S%d): Context - Position: %s, NumOpponents: %d, IsFacingBet: %s, AmountToCallAbs: %lld"),
        *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *UEnum::GetDisplayValueAsText(BotPosition).ToString(), NumOpponentsStillInHand,
        bIsFacingBet ? TEXT("true") : TEXT("false"), AmountToCallAbsolute);

    if (GameState->CurrentStage == EGameStage::Preflop) {
        UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Path Preflop Logic"), *BotPlayerSeatData.PlayerName);
        if (BotPlayerSeatData.HoleCards.Num() == 2) {
            EffectiveHandStrength = CalculatePreflopHandStrength(BotPlayerSeatData.HoleCards[0], BotPlayerSeatData.HoleCards[1], BotPosition, NumActivePlayersInGame);
            UE_LOG(LogTemp, Log, TEXT("BotAI %s (S%d, Pos:%s, Stack:%lld) PreflopHS: %.2f. ToCallOnTable:%lld, MyBet:%lld, MinPureRaiseVal:%lld"),
                *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *UEnum::GetDisplayValueAsText(BotPosition).ToString(), BotPlayerSeatData.Stack,
                EffectiveHandStrength, CurrentBetToCallOnTable, BotPlayerSeatData.CurrentBet, MinValidPureRaiseAmount);

            float RaiseThreshold = 0.70f - (TightnessFactor * 0.20f) - (AggressivenessFactor * 0.10f) - (bIsOpenRaiserSituation(GameState, BotPlayerSeatData) ? 0.05f : 0.0f);
            float CallThreshold = 0.40f - (TightnessFactor * 0.15f) - (AggressivenessFactor * 0.05f) - (bIsOpenRaiserSituation(GameState, BotPlayerSeatData) ? 0.05f : 0.0f);
            if ((BotPosition == EPlayerPokerPosition::SB && CurrentBetToCallOnTable == GameState->BigBlindAmount && BotPlayerSeatData.CurrentBet == GameState->SmallBlindAmount) ||
                (BotPosition == EPlayerPokerPosition::BB && CurrentBetToCallOnTable == GameState->BigBlindAmount && BotPlayerSeatData.CurrentBet == GameState->BigBlindAmount)) {
                CallThreshold -= 0.15f; RaiseThreshold -= 0.10f;
            }
            UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Preflop Thresholds - Raise: %.2f, Call: %.2f. EffHS: %.2f"),
                *BotPlayerSeatData.PlayerName, RaiseThreshold, CallThreshold, EffectiveHandStrength);

            if (EffectiveHandStrength >= RaiseThreshold) {
                UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Preflop Path - HS >= RaiseThreshold (%.2f >= %.2f)"), *BotPlayerSeatData.PlayerName, EffectiveHandStrength, RaiseThreshold);
                if (AllowedActions.Contains(EPlayerAction::Raise)) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Decision Branch: Considering Raise"));
                    ChosenAction = EPlayerAction::Raise; OutDecisionAmount = CalculateRaiseSize(GameState, BotPlayerSeatData, CurrentBetToCallOnTable, MinValidPureRaiseAmount, EffectiveHandStrength);
                }
                else if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Bet)) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Decision Branch: Considering Bet (Raise not allowed)"));
                    ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength);
                }
                else if (AllowedActions.Contains(EPlayerAction::Call)) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Decision Branch: Considering Call (Raise/Bet not allowed)"));
                    ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0;
                }
                else {
                    UE_LOG(LogTemp, Verbose, TEXT("    Decision Branch: Strong hand, but no Raise/Bet/Call allowed. Choosing Fold."));
                    ChosenAction = EPlayerAction::Fold;
                }
            }
            else if (EffectiveHandStrength >= CallThreshold) {
                UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Preflop Path - HS >= CallThreshold (%.2f >= %.2f)"), *BotPlayerSeatData.PlayerName, EffectiveHandStrength, CallThreshold);
                if (!bIsFacingBet) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Decision Branch: Not facing bet. Considering Bet/Check."));
                    float RandomFactorForBet = bIsTesting ? TestFixedRandValue : FMath::FRand();
                    if (AllowedActions.Contains(EPlayerAction::Bet) && RandomFactorForBet < (0.05f + AggressivenessFactor * 0.25f)) {
                        ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength, false, 0.5f);
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Check)) {
                        ChosenAction = EPlayerAction::Check; OutDecisionAmount = 0;
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Call)) {
                        ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0;
                    }
                    else { ChosenAction = EPlayerAction::Fold; }
                }
                else if (AllowedActions.Contains(EPlayerAction::Call)) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Decision Branch: Facing bet. Considering Call."));
                    if (AmountToCallAbsolute > 0 && GameState->Pot > 0) {
                        float PotOddsRatio = static_cast<float>(AmountToCallAbsolute) / static_cast<float>(GameState->Pot + AmountToCallAbsolute);
                        if (PotOddsRatio < (0.4f + TightnessFactor * 0.1f)) { ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0; }
                        else if (AllowedActions.Contains(EPlayerAction::Fold)) { ChosenAction = EPlayerAction::Fold; }
                        else { ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0; }
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Fold)) { ChosenAction = EPlayerAction::Fold; }
                    else { ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0; }
                }
                else if (AllowedActions.Contains(EPlayerAction::Fold)) {
                    ChosenAction = EPlayerAction::Fold;
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Preflop Path - Medium hand, facing bet, no Call/Fold. Unusual. Defaulting to Fold (or first available)."), *BotPlayerSeatData.PlayerName);
                    ChosenAction = AllowedActions.IsEmpty() ? EPlayerAction::Fold : AllowedActions[0];
                }
            }
            else { // Weak Hand
                UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Preflop Path - Weak Hand (HS: %.2f)"), *BotPlayerSeatData.PlayerName, EffectiveHandStrength);
                if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Check)) {
                    ChosenAction = EPlayerAction::Check; OutDecisionAmount = 0;
                }
                else if (AllowedActions.Contains(EPlayerAction::Fold)) {
                    float RandomFactorForBBDefense = bIsTesting ? TestFixedRandValue : FMath::FRand();
                    if (BotPosition == EPlayerPokerPosition::BB && AllowedActions.Contains(EPlayerAction::Call) &&
                        AmountToCallAbsolute <= GameState->BigBlindAmount && RandomFactorForBBDefense < (0.4f - TightnessFactor * 0.3f)) {
                        ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0;
                    }
                    else { ChosenAction = EPlayerAction::Fold; }
                }
                else if (AllowedActions.Contains(EPlayerAction::Call)) {
                    ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0;
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Preflop Path - Weak hand, no Check/Fold/Call. Unusual. Defaulting to Fold (or first available)."), *BotPlayerSeatData.PlayerName);
                    ChosenAction = AllowedActions.IsEmpty() ? EPlayerAction::Fold : AllowedActions[0];
                }
            }
        }
        else { // No hole cards
            UE_LOG(LogTemp, Warning, TEXT("BotAI %s (S%d): Preflop, but bot has %d hole cards! Choosing Fold."), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, BotPlayerSeatData.HoleCards.Num());
            ChosenAction = EPlayerAction::Fold;
        }
        UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Preflop decision logic resulted in: %s, AmountBeforeValidation: %lld"),
            *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), OutDecisionAmount);
    }


    else if (GameState->CurrentStage >= EGameStage::Flop && GameState->CurrentStage <= EGameStage::River) {
        UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Path Postflop Logic. Stage: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(GameState->CurrentStage).ToString());
        if (BotPlayerSeatData.HoleCards.Num() != 2) {
            UE_LOG(LogTemp, Warning, TEXT("BotAI %s (S%d): Postflop, but bot has %d hole cards! Choosing Fold."), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, BotPlayerSeatData.HoleCards.Num());
            ChosenAction = EPlayerAction::Fold;
        }
        else {
            FPokerHandResult MadeHand = EvaluateCurrentMadeHand(BotPlayerSeatData.HoleCards, GameState->CommunityCards);
            float MadeHandScore = GetScoreForMadeHand(MadeHand.HandRank);
            float DrawScoreValue = CalculateDrawStrength(BotPlayerSeatData.HoleCards, GameState->CommunityCards, GameState->Pot, AmountToCallAbsolute);

            if (DrawScoreValue > 0.65f && MadeHandScore < 0.4f) { EffectiveHandStrength = DrawScoreValue; }
            else if (MadeHandScore >= 0.05f) { EffectiveHandStrength = MadeHandScore + DrawScoreValue * FMath::Lerp(0.5f, 0.1f, MadeHandScore); }
            else { EffectiveHandStrength = DrawScoreValue; }
            EffectiveHandStrength = FMath::Clamp(EffectiveHandStrength, 0.0f, 1.0f);

            UE_LOG(LogTemp, Log, TEXT("BotAI %s (S%d, Pos:%s, Stage:%s, Stack:%lld) PostflopHS: %.2f (Made: %s [%.2f], Draw: %.2f). ToCall:%lld, MyBet:%lld, MinPureRaise:%lld, Pot: %lld"),
                *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *UEnum::GetDisplayValueAsText(BotPosition).ToString(),
                *UEnum::GetDisplayValueAsText(GameState->CurrentStage).ToString(), BotPlayerSeatData.Stack, EffectiveHandStrength,
                *UEnum::GetDisplayValueAsText(MadeHand.HandRank).ToString(), MadeHandScore, DrawScoreValue,
                CurrentBetToCallOnTable, BotPlayerSeatData.CurrentBet, MinValidPureRaiseAmount, GameState->Pot);

            bool bIsBluffing = false;
            if (EffectiveHandStrength < 0.30f && !bIsFacingBet && AllowedActions.Contains(EPlayerAction::Bet) &&
                ShouldAttemptBluff(GameState, BotPlayerSeatData, BotPosition, NumOpponentsStillInHand)) {
                bIsBluffing = true;
                UE_LOG(LogTemp, Log, TEXT("BotAI %s: Path Postflop - Attempting BLUFF!"), *BotPlayerSeatData.PlayerName);
            }

            float PostflopRaiseMonsterThreshold = 0.80f - (0.1f * (1.0f - AggressivenessFactor));
            float PostflopRaiseStrongThreshold = 0.65f - (0.1f * (1.0f - AggressivenessFactor));
            float PostflopBetValueThreshold = 0.50f - (0.1f * (1.0f - AggressivenessFactor));
            float PostflopCallThreshold = 0.30f; // Этот порог используется в логах, но не в явных if/else if выше
            UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Thresholds - MonsterR: %.2f, StrongR: %.2f, BetV: %.2f, CallThresh(logged): %.2f, Bluff: %s"),
                *BotPlayerSeatData.PlayerName, PostflopRaiseMonsterThreshold, PostflopRaiseStrongThreshold, PostflopBetValueThreshold, PostflopCallThreshold, bIsBluffing ? TEXT("true") : TEXT("false"));

            if (bIsBluffing) {
                UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Path - Bluffing Logic"), *BotPlayerSeatData.PlayerName);
                if (AllowedActions.Contains(EPlayerAction::Bet)) {
                    // Для теста блеф-бет будет, например, 0.5 * PotFractionOverride
                    float BluffPotFraction = bIsTesting ? 0.5f : FMath::FRandRange(0.4f, 0.6f);
                    ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, 0.55f, true, BluffPotFraction);
                }
                else {
                    UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Bluff - Cannot Bet. Trying Check/Fold."), *BotPlayerSeatData.PlayerName);
                    if (AllowedActions.Contains(EPlayerAction::Check) && !bIsFacingBet) ChosenAction = EPlayerAction::Check;
                    else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold;
                    else if (AllowedActions.Contains(EPlayerAction::Call) && bIsFacingBet) ChosenAction = EPlayerAction::Call;
                    else { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Postflop Bluff Fallback - No valid action. Defaulting to Fold."), *BotPlayerSeatData.PlayerName); ChosenAction = EPlayerAction::Fold; }
                }
            }
            else { // Не блефуем
                UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Path - Value/Draw Logic, EffHS: %.2f"), *BotPlayerSeatData.PlayerName, EffectiveHandStrength);
                if (EffectiveHandStrength >= PostflopRaiseMonsterThreshold) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Path Monster Hand (>=%.2f)"), PostflopRaiseMonsterThreshold);
                    if (AllowedActions.Contains(EPlayerAction::Raise)) {
                        float RaisePotFraction = bIsTesting ? 0.75f : FMath::FRandRange(0.75f, 1.2f); // Нижняя граница диапазона для теста
                        ChosenAction = EPlayerAction::Raise; OutDecisionAmount = CalculateRaiseSize(GameState, BotPlayerSeatData, CurrentBetToCallOnTable, MinValidPureRaiseAmount, EffectiveHandStrength, false, RaisePotFraction);
                    }
                    else if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Bet)) {
                        float BetPotFraction = bIsTesting ? 0.66f : FMath::FRandRange(0.66f, 1.0f);
                        ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength, false, BetPotFraction);
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Call)) { ChosenAction = EPlayerAction::Call; }
                    else if (AllowedActions.Contains(EPlayerAction::Check)) { ChosenAction = EPlayerAction::Check; } // Добавил Check как опцию
                    else { ChosenAction = EPlayerAction::Fold; } // Если ничего не доступно
                }
                else if (EffectiveHandStrength >= PostflopRaiseStrongThreshold) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Path Strong Hand (>=%.2f)"), PostflopRaiseStrongThreshold);
                    float RandomFactorForStrongRaise = bIsTesting ? TestFixedRandValue : FMath::FRand();
                    if (AllowedActions.Contains(EPlayerAction::Raise) && RandomFactorForStrongRaise < (0.3f + AggressivenessFactor * 0.5f)) {
                        float RaisePotFraction = bIsTesting ? 0.5f : FMath::FRandRange(0.5f, 0.75f);
                        ChosenAction = EPlayerAction::Raise; OutDecisionAmount = CalculateRaiseSize(GameState, BotPlayerSeatData, CurrentBetToCallOnTable, MinValidPureRaiseAmount, EffectiveHandStrength, false, RaisePotFraction);
                    }
                    else if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Bet)) {
                        float BetPotFraction = bIsTesting ? 0.5f : FMath::FRandRange(0.5f, 0.75f);
                        ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength, false, BetPotFraction);
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Call)) { ChosenAction = EPlayerAction::Call; }
                    else if (AllowedActions.Contains(EPlayerAction::Check)) { ChosenAction = EPlayerAction::Check; } // Добавил Check
                    else { ChosenAction = EPlayerAction::Fold; }
                }
                else if (EffectiveHandStrength >= PostflopBetValueThreshold) { // Этот порог должен быть выше CallThreshold
                    UE_LOG(LogTemp, Verbose, TEXT("    Path Value Bet Hand (>=%.2f)"), PostflopBetValueThreshold);
                    if (!bIsFacingBet) {
                        float RandomFactorForValueBet = bIsTesting ? TestFixedRandValue : FMath::FRand();
                        if (AllowedActions.Contains(EPlayerAction::Bet) && RandomFactorForValueBet < (0.5f + AggressivenessFactor * 0.4f)) {
                            float BetFraction;
                            if (bIsTesting) { BetFraction = (DrawScoreValue > 0.5f) ? 0.625f : 0.465f; } // Средние от диапазонов
                            else { BetFraction = (DrawScoreValue > 0.5f) ? FMath::FRandRange(0.5f, 0.75f) : FMath::FRandRange(0.33f, 0.6f); }
                            ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength, false, BetFraction);
                        }
                        else if (AllowedActions.Contains(EPlayerAction::Check)) { ChosenAction = EPlayerAction::Check; }
                        else if (AllowedActions.Contains(EPlayerAction::Call)) { ChosenAction = EPlayerAction::Call; } // Маловероятно, но для полноты
                        else { ChosenAction = EPlayerAction::Fold; }
                    }
                    else { // Facing a bet
                        if (AllowedActions.Contains(EPlayerAction::Call)) {
                            bool bGoodPotOdds = false;
                            if (AmountToCallAbsolute > 0 && GameState->Pot > 0) {
                                float PotOddsRatio = static_cast<float>(AmountToCallAbsolute) / static_cast<float>(GameState->Pot + AmountToCallAbsolute);
                                float RequiredEquity = PotOddsRatio;
                                if (EffectiveHandStrength * (1.5f - TightnessFactor) > RequiredEquity) { bGoodPotOdds = true; }
                            }
                            if (bGoodPotOdds) { ChosenAction = EPlayerAction::Call; }
                            else if (AllowedActions.Contains(EPlayerAction::Fold)) { ChosenAction = EPlayerAction::Fold; }
                            else { ChosenAction = EPlayerAction::Call; } // Forced call
                        }
                        else if (AllowedActions.Contains(EPlayerAction::Fold)) { ChosenAction = EPlayerAction::Fold; }
                        else { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Postflop ValueBet FacingBet - No Call/Fold. Unusual. Defaulting to Fold."), *BotPlayerSeatData.PlayerName); ChosenAction = EPlayerAction::Fold; }
                    }
                }
                // НОВАЯ ВЕТКА ДЛЯ КОЛЛА С ДРО ИЛИ СРЕДНЕЙ РУКОЙ
                else if (EffectiveHandStrength >= PostflopCallThreshold) { // Например, 0.30
                    UE_LOG(LogTemp, Verbose, TEXT("    Path Call/Check With Medium/Draw Hand (>=%.2f)"), PostflopCallThreshold);
                    if (!bIsFacingBet) {
                        if (AllowedActions.Contains(EPlayerAction::Check)) { ChosenAction = EPlayerAction::Check; }
                        else if (AllowedActions.Contains(EPlayerAction::Call)) { ChosenAction = EPlayerAction::Call; } // Например, SB должен дополнить
                        else { ChosenAction = EPlayerAction::Fold; } // Если даже чек невозможен
                    }
                    else { // Facing a bet
                        if (AllowedActions.Contains(EPlayerAction::Call)) {
                            bool bSufficientPotOddsForDraw = false;
                            if (DrawScoreValue > 0.1f && AmountToCallAbsolute > 0 && GameState->Pot > 0) { // Есть какое-то дро
                                float PotOddsRatio = static_cast<float>(AmountToCallAbsolute) / static_cast<float>(GameState->Pot + AmountToCallAbsolute);
                                float RequiredEquity = PotOddsRatio;
                                // Простая проверка, что сила дро (которая уже может включать пот-оддсы) достаточна
                                if (DrawScoreValue > RequiredEquity * 0.8f) { // Умножаем на 0.8 для небольшой "погрешности" или требуем чуть лучшие шансы
                                    bSufficientPotOddsForDraw = true;
                                }
                            }
                            if (bSufficientPotOddsForDraw) {
                                ChosenAction = EPlayerAction::Call;
                                UE_LOG(LogTemp, Verbose, TEXT("      Decision: Calling with draw due to pot odds."));
                            }
                            else if (MadeHandScore >= PostflopCallThreshold * 0.9f) { // Если готовая рука почти дотягивает до колл-трешхолда
                                ChosenAction = EPlayerAction::Call;
                                UE_LOG(LogTemp, Verbose, TEXT("      Decision: Calling with medium made hand."));
                            }
                            else if (AllowedActions.Contains(EPlayerAction::Fold)) {
                                ChosenAction = EPlayerAction::Fold;
                            }
                            else {
                                ChosenAction = EPlayerAction::Call; // Forced call
                            }
                        }
                        else if (AllowedActions.Contains(EPlayerAction::Fold)) {
                            ChosenAction = EPlayerAction::Fold;
                        }
                        else {
                            UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Postflop Medium/Draw Hand FacingBet - No Call/Fold. Unusual. Defaulting to Fold."), *BotPlayerSeatData.PlayerName);
                            ChosenAction = EPlayerAction::Fold;
                        }
                    }
                }
                else { // Weak Hand
                    UE_LOG(LogTemp, Verbose, TEXT("    Path Weak Hand (<%.2f)"), PostflopCallThreshold);
                    if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Check)) {
                        ChosenAction = EPlayerAction::Check;
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Fold)) {
                        ChosenAction = EPlayerAction::Fold;
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Call)) { // Forced call
                        // Добавим небольшую вероятность случайного "float" колла с мусором, если ставка очень мала
                        float RandomFactorForWeakCall = bIsTesting ? TestFixedRandValue : FMath::FRand();
                        if (AmountToCallAbsolute <= GameState->BigBlindAmount * 0.5f && RandomFactorForWeakCall < 0.1f * (1.0f + AggressivenessFactor)) { // Меньше шанс для тайтовых
                            ChosenAction = EPlayerAction::Call;
                            UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Weak Hand FacingBet - Decided to make a 'loose float' call."), *BotPlayerSeatData.PlayerName);
                        }
                        else {
                            // Если нет Fold, но есть Call, и это не float call, то это вынужденный колл (например, олл-ин)
                            if (!AllowedActions.Contains(EPlayerAction::Fold)) {
                                ChosenAction = EPlayerAction::Call;
                                UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Weak Hand FacingBet - No Fold, forced Call."), *BotPlayerSeatData.PlayerName);
                            }
                            else ChosenAction = EPlayerAction::Fold; // Стандартный фолд
                        }
                    }
                    else { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Postflop Weak Hand - No Check/Fold/Call. Unusual. Defaulting to Fold."), *BotPlayerSeatData.PlayerName); ChosenAction = EPlayerAction::Fold; }
                }
            }
        } 
        UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop decision logic resulted in: %s, AmountBeforeValidation: %lld"),
            *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), OutDecisionAmount);
    } // End Postflop Logic

    UE_LOG(LogTemp, Log, TEXT("BotAI %s (S%d): Before Final Validation - ChosenAction: %s, OutAmount: %lld"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), OutDecisionAmount); if (!AllowedActions.Contains(ChosenAction) && !AllowedActions.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: ChosenAction %s (EffHS: %.2f) NOT in AllowedActions! Activating Fallback. Allowed: [%s]"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), EffectiveHandStrength, *AllowedActionsString); if (AllowedActions.Contains(EPlayerAction::Check) && !bIsFacingBet) ChosenAction = EPlayerAction::Check; else if (AllowedActions.Contains(EPlayerAction::Call) && bIsFacingBet) ChosenAction = EPlayerAction::Call; else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold; else { ChosenAction = AllowedActions[0]; UE_LOG(LogTemp, Error, TEXT("BotAI %s: Fallback took first available: %s from [%s]"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), *AllowedActionsString); } OutDecisionAmount = 0; UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Fallback Chose: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString()); } if (ChosenAction == EPlayerAction::Bet || ChosenAction == EPlayerAction::Raise) { int64 MaxPossibleTotalBet = BotPlayerSeatData.CurrentBet + BotPlayerSeatData.Stack; if (OutDecisionAmount <= BotPlayerSeatData.CurrentBet && OutDecisionAmount < MaxPossibleTotalBet) { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Calculated Bet/Raise total amount %lld is <= current bet %lld and not All-In Max. Activating Fallback."), *BotPlayerSeatData.PlayerName, OutDecisionAmount, BotPlayerSeatData.CurrentBet); if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Check)) ChosenAction = EPlayerAction::Check; else if (bIsFacingBet && AllowedActions.Contains(EPlayerAction::Call)) ChosenAction = EPlayerAction::Call; else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold; else if (!AllowedActions.IsEmpty()) ChosenAction = AllowedActions[0]; OutDecisionAmount = 0; UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Bet/Raise Amount Fallback (not > CurrentBet) Chose: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString()); } else if (OutDecisionAmount > MaxPossibleTotalBet) { UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Correcting OutDecisionAmount %lld to All-In amount %lld"), *BotPlayerSeatData.PlayerName, OutDecisionAmount, MaxPossibleTotalBet); OutDecisionAmount = MaxPossibleTotalBet; } if (ChosenAction == EPlayerAction::Bet) { int64 ActualBetAmountAdded = OutDecisionAmount - BotPlayerSeatData.CurrentBet; if (((ActualBetAmountAdded < GameState->BigBlindAmount && ActualBetAmountAdded < BotPlayerSeatData.Stack) || ActualBetAmountAdded <= 0) && OutDecisionAmount < MaxPossibleTotalBet) { if (!(ChosenAction == EPlayerAction::Check || ChosenAction == EPlayerAction::Call || ChosenAction == EPlayerAction::Fold)) { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Bet total amount %lld (adds %lld) too small for non-all-in. Final Bet Fallback."), *BotPlayerSeatData.PlayerName, OutDecisionAmount, ActualBetAmountAdded); if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Check)) ChosenAction = EPlayerAction::Check; else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold; else if (bIsFacingBet && AllowedActions.Contains(EPlayerAction::Call)) ChosenAction = EPlayerAction::Call; else if (!AllowedActions.IsEmpty()) ChosenAction = AllowedActions[0]; OutDecisionAmount = 0; UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Bet Amount Final Fallback Chose: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString()); } } } else if (ChosenAction == EPlayerAction::Raise) { int64 PureRaise = OutDecisionAmount - CurrentBetToCallOnTable; if (((OutDecisionAmount <= CurrentBetToCallOnTable || PureRaise < MinValidPureRaiseAmount) && OutDecisionAmount < MaxPossibleTotalBet) && MaxPossibleTotalBet > CurrentBetToCallOnTable) { if (!(ChosenAction == EPlayerAction::Call || ChosenAction == EPlayerAction::Fold)) { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Raise total amount %lld invalid. PureRaise %lld vs MinPure %lld for non-all-in. Final Raise Fallback."), *BotPlayerSeatData.PlayerName, OutDecisionAmount, PureRaise, MinValidPureRaiseAmount); if (AllowedActions.Contains(EPlayerAction::Call)) ChosenAction = EPlayerAction::Call; else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold; else if (!AllowedActions.IsEmpty()) ChosenAction = AllowedActions[0]; OutDecisionAmount = 0; UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Raise Amount Final Fallback Chose: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString()); } } } }
    else { OutDecisionAmount = 0; } UE_LOG(LogTemp, Warning, TEXT("BotAI %s (S%d) FINAL DECISION: Action=%s, OutAmountForProcessAction=%lld"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), OutDecisionAmount); UE_LOG(LogTemp, Warning, TEXT("==================== BotAI %s (Seat %d) - GetBestAction END ===================="), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);

    return ChosenAction;
}


int64 UPokerBotAI::CalculateBetSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, float CalculatedHandStrength, bool bIsBluffArgument, float PotFractionOverride) const
{
    if (!GameState) return GameState->BigBlindAmount; // Защита, хотя GameState не должен быть null
    int64 MinBet = GameState->BigBlindAmount;
    int64 MaxBetPlayerCanAdd = BotPlayerSeatData.Stack;
    if (MaxBetPlayerCanAdd <= 0) return BotPlayerSeatData.CurrentBet;

    float PotFraction;
    if (PotFractionOverride > 0.0f && PotFractionOverride <= 2.0f) {
        PotFraction = PotFractionOverride;
    }
    else if (bIsBluffArgument) { // Переименовал параметр, чтобы не конфликтовать с членом класса
        PotFraction = bIsTesting ? ((0.4f + 0.66f) / 2.0f) : FMath::FRandRange(0.4f, 0.66f);
    }
    else {
        PotFraction = FMath::Lerp(0.33f, 0.75f, FMath::Clamp((CalculatedHandStrength - 0.2f) / 0.6f, 0.0f, 1.0f));
        PotFraction = FMath::Max(PotFraction, 0.33f);
    }
    int64 CalculatedBetAmountToAdd = FMath::RoundToInt64(static_cast<float>(GameState->Pot) * PotFraction);
    CalculatedBetAmountToAdd = FMath::Clamp(CalculatedBetAmountToAdd, MinBet, MaxBetPlayerCanAdd);
    if (CalculatedBetAmountToAdd == 0 && MaxBetPlayerCanAdd > 0) CalculatedBetAmountToAdd = FMath::Min(MinBet, MaxBetPlayerCanAdd);
    return BotPlayerSeatData.CurrentBet + CalculatedBetAmountToAdd;
}

int64 UPokerBotAI::CalculateRaiseSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, int64 CurrentBetToCallOnTable, int64 MinValidPureRaiseAmount, float CalculatedHandStrength, bool bIsBluffArgument, float PotFractionOverride) const
{
    if (!GameState) { return CurrentBetToCallOnTable + MinValidPureRaiseAmount; }
    int64 AmountToCall = CurrentBetToCallOnTable - BotPlayerSeatData.CurrentBet; if (AmountToCall < 0) AmountToCall = 0;
    if (BotPlayerSeatData.Stack <= AmountToCall) { return BotPlayerSeatData.CurrentBet + BotPlayerSeatData.Stack; }
    int64 PotSizeAfterOurTheoreticalCall = GameState->Pot + AmountToCall;
    float TargetPureRaisePotFraction;
    if (PotFractionOverride > 0.0f && PotFractionOverride <= 2.0f) {
        TargetPureRaisePotFraction = PotFractionOverride;
        UE_LOG(LogTemp, Verbose, TEXT("BotAI CalculateRaiseSize: Using PotFractionOverride: %.2f"), PotFractionOverride); // Логирование уже есть
    }
    else if (bIsBluffArgument) {
        TargetPureRaisePotFraction = bIsTesting ? ((0.5f + 0.75f) / 2.0f) : FMath::FRandRange(0.5f, 0.75f);
        UE_LOG(LogTemp, Verbose, TEXT("BotAI CalculateRaiseSize: Bluffing, TargetPureRaisePotFraction: %.2f"), TargetPureRaisePotFraction);
    }
    else {
        float AggroMultiplier = FMath::Lerp(0.8f, 1.2f, AggressivenessFactor);
        TargetPureRaisePotFraction = FMath::Lerp(0.4f, 1.0f, FMath::Clamp(CalculatedHandStrength, 0.0f, 1.0f)) * AggroMultiplier;
        TargetPureRaisePotFraction = FMath::Clamp(TargetPureRaisePotFraction, 0.33f, 1.5f);
        UE_LOG(LogTemp, Verbose, TEXT("BotAI CalculateRaiseSize: Value raising, ...")); // Логирование уже есть
    }
    int64 CalculatedPureRaise = FMath::RoundToInt64(static_cast<float>(PotSizeAfterOurTheoreticalCall) * TargetPureRaisePotFraction);
    CalculatedPureRaise = FMath::Max(CalculatedPureRaise, MinValidPureRaiseAmount);
    int64 TotalAmountPlayerAddsThisAction = AmountToCall + CalculatedPureRaise;
    if (TotalAmountPlayerAddsThisAction > BotPlayerSeatData.Stack) { TotalAmountPlayerAddsThisAction = BotPlayerSeatData.Stack; }
    return BotPlayerSeatData.CurrentBet + TotalAmountPlayerAddsThisAction;
}



FPokerHandResult UPokerBotAI::EvaluateCurrentMadeHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards) const
{
    if (HoleCards.Num() < 2) // Нужны карманные карты для оценки руки бота
    {
        UE_LOG(LogTemp, Warning, TEXT("EvaluateCurrentMadeHand: Bot has less than 2 hole cards. Returning empty result."));
        return FPokerHandResult(); // Возвращаем пустой результат (HighCard по умолчанию)
    }
    return UPokerHandEvaluator::EvaluatePokerHand(HoleCards, CommunityCards);
}

float UPokerBotAI::GetScoreForMadeHand(EPokerHandRank HandRank) const
{
    switch (HandRank)
    {
    case EPokerHandRank::RoyalFlush:    return 1.0f;
    case EPokerHandRank::StraightFlush: return 0.98f;
    case EPokerHandRank::FourOfAKind:   return 0.90f;
    case EPokerHandRank::FullHouse:     return 0.85f; 
    case EPokerHandRank::Flush:         return 0.78f;
    case EPokerHandRank::Straight:      return 0.72f; 
    case EPokerHandRank::ThreeOfAKind:  return 0.60f;
    case EPokerHandRank::TwoPair:       return 0.45f; 
    case EPokerHandRank::OnePair:       return 0.30f; 
    case EPokerHandRank::HighCard:      return 0.10f;
    default:                            return 0.0f;
    }
}

bool UPokerBotAI::ShouldAttemptBluff(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, EPlayerPokerPosition BotPosition, int32 NumOpponentsStillInHand) const
{
    UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff CALLED for Bot %s (Seat %d). Testing: %s. TestRandVal: %.2f"),
        *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, bIsTesting ? TEXT("TRUE") : TEXT("FALSE"), TestFixedRandValue);

    if (!GameState) {
        UE_LOG(LogTemp, Warning, TEXT("ShouldAttemptBluff: GameState is NULL. Returning false."));
        return false;
    }
    if (NumOpponentsStillInHand == 0 || NumOpponentsStillInHand > 2) { // Блефуем только против 1-2 оппонентов
        UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: Condition (NumOpponents %d not 1 or 2) met. Returning false."), NumOpponentsStillInHand);
        return false;
    }

    // 1. Позиция
    bool bGoodPositionForBluff = (BotPosition == EPlayerPokerPosition::BTN || BotPosition == EPlayerPokerPosition::CO);
    if (GameState->CurrentStage == EGameStage::River && BotPosition == EPlayerPokerPosition::HJ && NumOpponentsStillInHand == 1) {
        bGoodPositionForBluff = true;
    }
    UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: BotPosition: %s, NumOpponents: %d. bGoodPositionForBluff: %s"),
        *UEnum::GetDisplayValueAsText(BotPosition).ToString(), NumOpponentsStillInHand, bGoodPositionForBluff ? TEXT("true") : TEXT("false"));
    if (!bGoodPositionForBluff) {
        return false;
    }

    // 2. Текстура борда (очень упрощенно): "Сухой" борд
    bool bScaryBoard = false;
    if (GameState->CommunityCards.Num() >= 3) {
        TMap<ECardSuit, int32> SuitCountsOnBoard;
        TArray<int32> RanksOnBoardInt;
        FString BoardStrForLog = TEXT("Board: ");
        for (const FCard& Card : GameState->CommunityCards) {
            SuitCountsOnBoard.FindOrAdd(Card.Suit)++;
            RanksOnBoardInt.Add(static_cast<int32>(Card.Rank));
            BoardStrForLog += Card.ToString() + TEXT(" ");
        }
        UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: %s"), *BoardStrForLog.TrimEnd());

        for (const auto& SuitPair : SuitCountsOnBoard) {
            if (SuitPair.Value >= 3) {
                bScaryBoard = true;
                UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: ScaryBoard due to >=3 cards of suit %s."), *UEnum::GetDisplayValueAsText(SuitPair.Key).ToString());
                break;
            }
        }
        if (!bScaryBoard && RanksOnBoardInt.Num() >= 3) {
            RanksOnBoardInt.Sort();
            int32 ConnectedCount = 0;
            for (int32 i = 0; i < RanksOnBoardInt.Num() - 1; ++i) { // Изменено size_t на int32 для совместимости с TArray::Num()
                if (FMath::Abs(RanksOnBoardInt[i] - RanksOnBoardInt[i + 1]) <= 2) ConnectedCount++;
            }
            UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: Board ConnectedCount: %d (for %d board cards)"), ConnectedCount, RanksOnBoardInt.Num());
            if (ConnectedCount >= GameState->CommunityCards.Num() - 1 ||
                (GameState->CommunityCards.Num() == 3 && ConnectedCount >= 1) ||
                (GameState->CommunityCards.Num() == 4 && ConnectedCount >= 2))
            {
                bScaryBoard = true;
                UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: ScaryBoard due to high connectedness."));
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: bScaryBoard: %s"), bScaryBoard ? TEXT("true") : TEXT("false"));
    if (bScaryBoard) {
        return false;
    }

    // 3. Оппоненты показали слабость
    bool bOpponentShowedWeaknessPrimary = (GameState->CurrentBetToCall == BotPlayerSeatData.CurrentBet && GameState->LastAggressorSeatIndex != BotPlayerSeatData.SeatIndex);
    bool bOpponentShowedWeaknessSecondary = (GameState->PlayerWhoOpenedBettingThisRound == BotPlayerSeatData.SeatIndex && GameState->LastAggressorSeatIndex == -1 && GameState->CurrentBetToCall == 0);
    bool bOpponentShowedWeakness = bOpponentShowedWeaknessPrimary || bOpponentShowedWeaknessSecondary;

    UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: CurrentBetToCall: %lld, BotCurrentBet: %lld, LastAggressor: %d, BotSeat: %d, Opener: %d"),
        GameState->CurrentBetToCall, BotPlayerSeatData.CurrentBet, GameState->LastAggressorSeatIndex, BotPlayerSeatData.SeatIndex, GameState->PlayerWhoOpenedBettingThisRound);
    UE_LOG(LogTemp, Log, TEXT("ShouldAttemptBluff: bOpponentShowedWeaknessPrimary: %s, bOpponentShowedWeaknessSecondary: %s -> bOpponentShowedWeakness: %s"),
        bOpponentShowedWeaknessPrimary ? TEXT("true") : TEXT("false"),
        bOpponentShowedWeaknessSecondary ? TEXT("true") : TEXT("false"),
        bOpponentShowedWeakness ? TEXT("true") : TEXT("false"));

    if (!bOpponentShowedWeakness) {
        return false;
    }

    // 4. Учитываем предыдущую агрессию бота (TODO)

    // Финальное решение на основе случайности и "личности"
    float ChanceToBluffThreshold = BluffFrequency * (0.4f + AggressivenessFactor * 0.6f);
    float RandomFactorForBluff = bIsTesting ? TestFixedRandValue : FMath::FRand();

    UE_LOG(LogTemp, Warning, TEXT("ShouldAttemptBluff FINAL CHECK: RandomFactorForBluff: %.3f, CalculatedChanceThreshold: %.3f (BluffFreq: %.2f, AggroTerm: %.2f)"),
        RandomFactorForBluff,
        ChanceToBluffThreshold,
        BluffFrequency,
        (0.4f + AggressivenessFactor * 0.6f));

    if (RandomFactorForBluff < ChanceToBluffThreshold) {
        UE_LOG(LogTemp, Warning, TEXT("ShouldAttemptBluff: Returning TRUE (%.3f < %.3f) -> WILL ATTEMPT BLUFF"), RandomFactorForBluff, ChanceToBluffThreshold);
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("ShouldAttemptBluff: Returning FALSE (%.3f >= %.3f) -> WILL NOT ATTEMPT BLUFF"), RandomFactorForBluff, ChanceToBluffThreshold);
    return false;
}


// --- ОБЩИЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---
int32 UPokerBotAI::CountActiveOpponents(const UOfflinePokerGameState* GameState, int32 BotSeatIndex) const
{
    // ... (код без изменений из Части 2) ...
    if (!GameState) return 0;
    int32 Count = 0;
    for (const FPlayerSeatData& Seat : GameState->Seats)
    {
        if (Seat.SeatIndex != BotSeatIndex && Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded && Seat.Status != EPlayerStatus::SittingOut)
        {
            if (Seat.Status == EPlayerStatus::Playing || Seat.Status == EPlayerStatus::AllIn || Seat.Status == EPlayerStatus::MustPostSmallBlind || Seat.Status == EPlayerStatus::MustPostBigBlind)
            {
                Count++;
            }
        }
    }
    return Count;
}

TArray<FPlayerSeatData> UPokerBotAI::GetActiveOpponentData(const UOfflinePokerGameState* GameState, int32 BotSeatIndex) const
{
    TArray<FPlayerSeatData> Opponents;
    if (!GameState) return Opponents;
    for (const FPlayerSeatData& Seat : GameState->Seats)
    {
        if (Seat.SeatIndex != BotSeatIndex && Seat.bIsSittingIn && Seat.Status != EPlayerStatus::Folded && Seat.Status != EPlayerStatus::SittingOut)
        {
            if (Seat.Status == EPlayerStatus::Playing || Seat.Status == EPlayerStatus::AllIn || Seat.Status == EPlayerStatus::MustPostSmallBlind || Seat.Status == EPlayerStatus::MustPostBigBlind)
            {
                Opponents.Add(Seat);
            }
        }
    }
    return Opponents;
}

float UPokerBotAI::CalculateDrawStrength(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards, int64 PotSize, int64 AmountToCallIfAny) const
{
    if (HoleCards.Num() < 2 || CommunityCards.Num() < 3 || CommunityCards.Num() >= 5) { return 0.0f; }

    TArray<FCard> AllKnownCards = HoleCards;
    AllKnownCards.Append(CommunityCards);

    int32 FlushDrawOuts = 0;
    float DrawScore = 0.0f;

    // --- Подсчет аутов на Флеш-Дро ---
    TMap<ECardSuit, int32> SuitCounts;
    for (const FCard& Card : AllKnownCards) { SuitCounts.FindOrAdd(Card.Suit)++; }
    for (const auto& SuitPair : SuitCounts) {
        if (SuitPair.Value == 4) {
            FlushDrawOuts = (13 - 4); // 9 аутов на флеш
            DrawScore = FMath::Max(DrawScore, 0.45f);
            UE_LOG(LogTemp, Verbose, TEXT("BotAI Draw: Flush Draw detected (%d outs)"), FlushDrawOuts);
            break;
        }
    }

    // --- Подсчет аутов на Стрит-Дро ---
    TSet<int32> KnownRanksForStraight; // Используем int32: 1 (Туз) до 13 (Король), Туз также 14
    for (const FCard& Card : AllKnownCards) {
        int32 RankValue = static_cast<int32>(Card.Rank) + 2; // 2=2, T=10, J=11, Q=12, K=13, A=14
        KnownRanksForStraight.Add(RankValue);
        if (Card.Rank == ECardRank::Ace) { // Туз как единица для A-5
            KnownRanksForStraight.Add(1);
        }
    }

    int32 StraightDrawOuts = 0;
    for (int32 LowCardPotential = 1; LowCardPotential <= 10; ++LowCardPotential) {
        int32 CardsInSequence = 0;
        TArray<int32> MissingCardsInSequence;
        for (int32 i = 0; i < 5; ++i) {
            if (KnownRanksForStraight.Contains(LowCardPotential + i)) {
                CardsInSequence++;
            }
            else {
                MissingCardsInSequence.Add(LowCardPotential + i);
            }
        }

        if (CardsInSequence == 4 && MissingCardsInSequence.Num() == 1) { // Есть 4 из 5 карт
            int32 MissingRank = MissingCardsInSequence[0];

            if (MissingRank == LowCardPotential && LowCardPotential > 1) { // Дырка снизу, не туз
                StraightDrawOuts = FMath::Max(StraightDrawOuts, 4); // Гатшот, если T-9-8-7 (нужен J)
                if (KnownRanksForStraight.Contains(LowCardPotential + 5)) {} // это для проверки что нет второй дырки с другой стороны
                else StraightDrawOuts = FMath::Max(StraightDrawOuts, 8); // OESD
            }
            else if (MissingRank == LowCardPotential + 4 && LowCardPotential + 4 < 14) { // Дырка сверху, не туз
                StraightDrawOuts = FMath::Max(StraightDrawOuts, 4);
                if (KnownRanksForStraight.Contains(LowCardPotential - 1)) {}
                else StraightDrawOuts = FMath::Max(StraightDrawOuts, 8); // OESD
            }
            else { // Дырка внутри - гатшот
                StraightDrawOuts = FMath::Max(StraightDrawOuts, 4);
            }
        }
    }
    if (StraightDrawOuts > 0) {
        DrawScore = FMath::Max(DrawScore, (StraightDrawOuts >= 8) ? 0.40f : 0.25f);
        UE_LOG(LogTemp, Verbose, TEXT("BotAI Draw: Straight Draw detected (%d outs)"), StraightDrawOuts);
    }

    int32 TotalOuts = FlushDrawOuts + StraightDrawOuts;

    if (TotalOuts == 0) return 0.0f;

    float PotOdds = 0.0f;
    if (AmountToCallIfAny > 0 && (PotSize + AmountToCallIfAny) > 0) {
        PotOdds = static_cast<float>(AmountToCallIfAny) / static_cast<float>(PotSize + AmountToCallIfAny);
    }

    int32 UnknownCardsInDeck = 52 - AllKnownCards.Num();
    if (UnknownCardsInDeck <= 0) return 0.0f;

    float HandOddsForOneCard = static_cast<float>(TotalOuts) / static_cast<float>(UnknownCardsInDeck);
    float RelevantHandOdds = HandOddsForOneCard; // Для терна или ривера (когда выходит одна карта)

    if (CommunityCards.Num() == 3) { // На флопе, есть две карты до вскрытия
        if (UnknownCardsInDeck > 1) {

            float ProbNotHittingTurn = static_cast<float>(UnknownCardsInDeck - TotalOuts) / UnknownCardsInDeck;
            float ProbNotHittingRiver = static_cast<float>(UnknownCardsInDeck - 1 - TotalOuts) / (UnknownCardsInDeck - 1);
            RelevantHandOdds = 1.0f - (ProbNotHittingTurn * ProbNotHittingRiver);
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("BotAI Draw: Total Outs: %d, PotOdds: %.3f, HandOdds (relevant): %.3f. DrawScore base: %.2f"), TotalOuts, PotOdds, RelevantHandOdds, DrawScore);

    if (AmountToCallIfAny >= 0 && RelevantHandOdds > PotOdds) { // >=0 для случая бесплатной карты
        DrawScore += 0.20f;
        UE_LOG(LogTemp, Verbose, TEXT("BotAI Draw: Draw has correct Pot Odds. Final DrawScore: %.2f"), DrawScore);
    }

    return FMath::Clamp(DrawScore, 0.0f, 0.85f);
}


bool UPokerBotAI::bIsOpenRaiserSituation(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData) const
{
    if (GameState->CurrentStage != EGameStage::Preflop) return false;
    bool bNoRealAggressionYet = (GameState->LastAggressorSeatIndex == -1 ||
        GameState->LastAggressorSeatIndex == GameState->PendingSmallBlindSeat ||
        GameState->LastAggressorSeatIndex == GameState->PendingBigBlindSeat);
    return bNoRealAggressionYet && (BotPlayerSeatData.CurrentBet <= GameState->BigBlindAmount);
}