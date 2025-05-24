#include "PokerBotAI.h"
#include "OfflinePokerGameState.h"
#include "PokerDataTypes.h"
#include "PokerHandEvaluator.h"
#include "Math/UnrealMathUtility.h"
#include "Logging/LogMacros.h"
#include "Algo/Sort.h" // Для сортировки карт

// Можете определить свою лог категорию
// DEFINE_LOG_CATEGORY_STATIC(LogPokerBotAI, Log, All);

UPokerBotAI::UPokerBotAI()
{
    AggressivenessFactor = FMath::FRandRange(0.2f, 0.6f); // Менее агрессивные по умолчанию
    BluffFrequency = FMath::FRandRange(0.05f, 0.15f);
    TightnessFactor = FMath::FRandRange(0.4f, 0.8f); // От средне-лузовых до тайтовых

    UE_LOG(LogTemp, Log, TEXT("UPokerBotAI instance created. Personality - Aggro: %.2f, BluffFreq: %.2f, Tightness: %.2f"),
        AggressivenessFactor, BluffFrequency, TightnessFactor);
}

// --- ИТЕРАЦИЯ 2: УЧЕТ ПОЗИЦИИ ---
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
        // Ищем следующего активного игрока (bIsSittingIn)
        // GetNextPlayerToAct из OfflineGameManager может быть полезен, если адаптировать его для поиска
        // просто следующего сидящего, а не следующего для хода.
        // Пока сделаем простой поиск по кругу.
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

    // ActiveSeatsInOrder теперь содержит индексы активных игроков в порядке хода ПОСЛЕ дилера
    // Т.е., ActiveSeatsInOrder[0] = SB, ActiveSeatsInOrder[1] = BB, ... ActiveSeatsInOrder.Last() = BTN (дилер)

    if (ActiveSeatsInOrder.Num() != NumActivePlayers || !ActiveSeatsInOrder.Contains(BotSeatIndex)) {
        UE_LOG(LogTemp, Warning, TEXT("GetPlayerPosition: Mismatch in active players or bot not found. NumActive: %d, OrderSize: %d"), NumActivePlayers, ActiveSeatsInOrder.Num());
        return EPlayerPokerPosition::Unknown;
    }

    int32 BotPosInOrder = ActiveSeatsInOrder.Find(BotSeatIndex);

    if (NumActivePlayers == 2) { // Heads-up
        return (BotPosInOrder == 0) ? EPlayerPokerPosition::SB : EPlayerPokerPosition::BB; // SB (он же дилер), BB
    }

    // Для >2 игроков:
    // SB = ActiveSeatsInOrder[0]
    // BB = ActiveSeatsInOrder[1]
    // BTN = ActiveSeatsInOrder.Last()

    if (BotSeatIndex == ActiveSeatsInOrder[0]) return EPlayerPokerPosition::SB;
    if (BotSeatIndex == ActiveSeatsInOrder[1]) return EPlayerPokerPosition::BB;
    if (BotSeatIndex == ActiveSeatsInOrder.Last()) return EPlayerPokerPosition::BTN;

    // Более точное определение для столов разного размера
    // (Это примерная логика, можно улучшить)
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


// --- ИТЕРАЦИЯ 1: СИЛА СТАРТОВОЙ РУКИ (ПРЕФЛОП) ---
float UPokerBotAI::CalculatePreflopHandStrength(const FCard& Card1, const FCard& Card2, EPlayerPokerPosition BotPosition, int32 NumActivePlayers) const
{
    // Упрощенная таблица силы рук, модифицированная из различных источников + Chen Formula элементы
    // Возвращает значение примерно от 0.0 (мусор) до 1.0 (AA)
    // Реальная сила также сильно зависит от контекста, который здесь не полностью учтен.

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


// ... (код из Части 1: конструктор, GetPlayerPosition, CalculatePreflopHandStrength) ...

// --- ИСПОЛЬЗОВАНИЕ НОВЫХ ФУНКЦИЙ ДЛЯ ПРИНЯТИЯ РЕШЕНИЙ в GetBestAction (ПРЕФЛОП ЧАСТЬ ОБНОВЛЕНА) ---
EPlayerAction UPokerBotAI::GetBestAction(
    const UOfflinePokerGameState* GameState,
    const FPlayerSeatData& BotPlayerSeatData,
    const TArray<EPlayerAction>& AllowedActions,
    int64 CurrentBetToCallOnTable,
    int64 MinValidPureRaiseAmount,
    int64& OutDecisionAmount
)
{
    // --- НАЧАЛО ЛОГИРОВАНИЯ ВХОДНЫХ ДАННЫХ ---
    OutDecisionAmount = 0;
    FString AllowedActionsString = TEXT("None");
    if (AllowedActions.Num() > 0) {
        AllowedActionsString = TEXT("");
        for (EPlayerAction Action : AllowedActions) { AllowedActionsString += UEnum::GetDisplayValueAsText(Action).ToString() + TEXT(" "); }
        AllowedActionsString = AllowedActionsString.TrimEnd();
    }

    UE_LOG(LogTemp, Warning, TEXT("==================== BotAI %s (Seat %d) - GetBestAction START ===================="),
        *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
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
    // --- КОНЕЦ ЛОГИРОВАНИЯ ВХОДНЫХ ДАННЫХ ---

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

    EPlayerAction ChosenAction = EPlayerAction::Fold; // Действие по умолчанию
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

    // --- НАЧАЛО ВАШЕЙ ЛОГИКИ ПРИНЯТИЯ РЕШЕНИЙ (ПРЕФЛОП И ПОСТФЛОП) ---
    // --- С ДОБАВЛЕННЫМ МНОЙ ЛОГИРОВАНИЕМ ПУТЕЙ ---
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
                    if (AllowedActions.Contains(EPlayerAction::Bet) && FMath::FRand() < (0.05f + AggressivenessFactor * 0.25f)) {
                        ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength, false, 0.5f);
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Check)) {
                        ChosenAction = EPlayerAction::Check; OutDecisionAmount = 0;
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Call)) { // Fallback if Check not allowed (e.g. must complete SB)
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
                        else { ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0; } // Forced Call if no Fold
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Fold)) { ChosenAction = EPlayerAction::Fold; } // No pot odds, no bet to call, but facing "something"
                    else { ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0; } // Forced Call
                }
                else if (AllowedActions.Contains(EPlayerAction::Fold)) {
                    ChosenAction = EPlayerAction::Fold;
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Preflop Path - Medium hand, facing bet, no Call/Fold. Unusual. Defaulting to Fold (or first available)."), *BotPlayerSeatData.PlayerName);
                    ChosenAction = AllowedActions.IsEmpty() ? EPlayerAction::Fold : AllowedActions[0];
                }
            }
            else {
                UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Preflop Path - Weak Hand (HS: %.2f)"), *BotPlayerSeatData.PlayerName, EffectiveHandStrength);
                if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Check)) {
                    ChosenAction = EPlayerAction::Check; OutDecisionAmount = 0;
                }
                else if (AllowedActions.Contains(EPlayerAction::Fold)) {
                    if (BotPosition == EPlayerPokerPosition::BB && AllowedActions.Contains(EPlayerAction::Call) &&
                        AmountToCallAbsolute <= GameState->BigBlindAmount && FMath::FRand() < (0.4f - TightnessFactor * 0.3f)) {
                        ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0;
                    }
                    else { ChosenAction = EPlayerAction::Fold; }
                }
                else if (AllowedActions.Contains(EPlayerAction::Call)) { // Forced call if fold not allowed
                    ChosenAction = EPlayerAction::Call; OutDecisionAmount = 0;
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Preflop Path - Weak hand, no Check/Fold/Call. Unusual. Defaulting to Fold (or first available)."), *BotPlayerSeatData.PlayerName);
                    ChosenAction = AllowedActions.IsEmpty() ? EPlayerAction::Fold : AllowedActions[0];
                }
            }
        }
        else {
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
            float PostflopCallThreshold = 0.30f;
            UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Thresholds - MonsterR: %.2f, StrongR: %.2f, BetV: %.2f, Call: %.2f, Bluff: %s"),
                *BotPlayerSeatData.PlayerName, PostflopRaiseMonsterThreshold, PostflopRaiseStrongThreshold, PostflopBetValueThreshold, PostflopCallThreshold, bIsBluffing ? TEXT("true") : TEXT("false"));

            if (bIsBluffing) {
                UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Path - Bluffing Logic"), *BotPlayerSeatData.PlayerName);
                if (AllowedActions.Contains(EPlayerAction::Bet)) {
                    ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, 0.55f, true, FMath::FRandRange(0.4f, 0.6f));
                }
                else {
                    UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Bluff - Cannot Bet. Trying Check/Fold."), *BotPlayerSeatData.PlayerName);
                    if (AllowedActions.Contains(EPlayerAction::Check) && !bIsFacingBet) ChosenAction = EPlayerAction::Check; // Блеф-чек, если бет невозможен
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
                        ChosenAction = EPlayerAction::Raise; OutDecisionAmount = CalculateRaiseSize(GameState, BotPlayerSeatData, CurrentBetToCallOnTable, MinValidPureRaiseAmount, EffectiveHandStrength, false, FMath::FRandRange(0.75f, 1.2f));
                    }
                    else if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Bet)) {
                        ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength, false, FMath::FRandRange(0.66f, 1.0f));
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Call)) { ChosenAction = EPlayerAction::Call; }
                    else { ChosenAction = EPlayerAction::Check; }
                }
                else if (EffectiveHandStrength >= PostflopRaiseStrongThreshold) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Path Strong Hand (>=%.2f)"), PostflopRaiseStrongThreshold);
                    if (AllowedActions.Contains(EPlayerAction::Raise) && FMath::FRand() < (0.3f + AggressivenessFactor * 0.5f)) {
                        ChosenAction = EPlayerAction::Raise; OutDecisionAmount = CalculateRaiseSize(GameState, BotPlayerSeatData, CurrentBetToCallOnTable, MinValidPureRaiseAmount, EffectiveHandStrength, false, FMath::FRandRange(0.5f, 0.75f));
                    }
                    else if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Bet)) {
                        ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength, false, FMath::FRandRange(0.5f, 0.75f));
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Call)) { ChosenAction = EPlayerAction::Call; }
                    else { ChosenAction = EPlayerAction::Check; }
                }
                else if (EffectiveHandStrength >= PostflopBetValueThreshold) {
                    UE_LOG(LogTemp, Verbose, TEXT("    Path Value Bet Hand (>=%.2f)"), PostflopBetValueThreshold);
                    if (!bIsFacingBet) {
                        if (AllowedActions.Contains(EPlayerAction::Bet) && FMath::FRand() < (0.5f + AggressivenessFactor * 0.4f)) {
                            float BetFraction = (DrawScoreValue > 0.5f) ? FMath::FRandRange(0.5f, 0.75f) : FMath::FRandRange(0.33f, 0.6f);
                            ChosenAction = EPlayerAction::Bet; OutDecisionAmount = CalculateBetSize(GameState, BotPlayerSeatData, EffectiveHandStrength, false, BetFraction);
                        }
                        else if (AllowedActions.Contains(EPlayerAction::Check)) { ChosenAction = EPlayerAction::Check; }
                        else if (AllowedActions.Contains(EPlayerAction::Call)) { ChosenAction = EPlayerAction::Call; }
                        else { ChosenAction = EPlayerAction::Fold; }
                    }
                    else {
                        if (AllowedActions.Contains(EPlayerAction::Call)) {
                            bool bGoodPotOdds = false;
                            if (AmountToCallAbsolute > 0 && GameState->Pot > 0) {
                                float PotOddsRatio = static_cast<float>(AmountToCallAbsolute) / static_cast<float>(GameState->Pot + AmountToCallAbsolute);
                                float RequiredEquity = PotOddsRatio;
                                if (EffectiveHandStrength * (1.5f - TightnessFactor) > RequiredEquity) { bGoodPotOdds = true; }
                            }
                            if (bGoodPotOdds) { ChosenAction = EPlayerAction::Call; }
                            else if (AllowedActions.Contains(EPlayerAction::Fold)) { ChosenAction = EPlayerAction::Fold; }
                            else { ChosenAction = EPlayerAction::Call; }
                        }
                        else if (AllowedActions.Contains(EPlayerAction::Fold)) { ChosenAction = EPlayerAction::Fold; }
                        else { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Postflop ValueBet FacingBet - No Call/Fold. Unusual. Defaulting to Fold."), *BotPlayerSeatData.PlayerName); ChosenAction = EPlayerAction::Fold; }
                    }
                }
                else {
                    UE_LOG(LogTemp, Verbose, TEXT("    Path Weak Hand (<%.2f)"), PostflopCallThreshold);
                    if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Check)) {
                        ChosenAction = EPlayerAction::Check;
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Fold)) {
                        ChosenAction = EPlayerAction::Fold;
                    }
                    else if (AllowedActions.Contains(EPlayerAction::Call)) {
                        if (AmountToCallAbsolute <= GameState->BigBlindAmount * 0.5f && FMath::FRand() < 0.2f) {
                            ChosenAction = EPlayerAction::Call;
                        }
                        else {
                            if (!AllowedActions.Contains(EPlayerAction::Fold)) { ChosenAction = EPlayerAction::Call; UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop Weak Hand FacingBet - No Fold, forced Call."), *BotPlayerSeatData.PlayerName); }
                            else ChosenAction = EPlayerAction::Fold;
                        }
                    }
                    else { UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Postflop Weak Hand - No Check/Fold/Call. Unusual. Defaulting to Fold."), *BotPlayerSeatData.PlayerName); ChosenAction = EPlayerAction::Fold; }
                }
            }
        }
        UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Postflop decision logic resulted in: %s, AmountBeforeValidation: %lld"),
            *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), OutDecisionAmount);
    }
    // --- КОНЕЦ ВАШЕЙ СУЩЕСТВУЮЩЕЙ ЛОГИКИ ---

    // --- Финальная Проверка и Коррекция OutDecisionAmount ---
    UE_LOG(LogTemp, Log, TEXT("BotAI %s (S%d): Before Final Validation - ChosenAction: %s, OutAmount: %lld"),
        *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), OutDecisionAmount);

    if (!AllowedActions.Contains(ChosenAction) && !AllowedActions.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("BotAI %s: ChosenAction %s (EffHS: %.2f) NOT in AllowedActions! Activating Fallback. Allowed: [%s]"),
            *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), EffectiveHandStrength, *AllowedActionsString);
        if (AllowedActions.Contains(EPlayerAction::Check) && !bIsFacingBet) ChosenAction = EPlayerAction::Check;
        else if (AllowedActions.Contains(EPlayerAction::Call) && bIsFacingBet) ChosenAction = EPlayerAction::Call;
        else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold;
        else { ChosenAction = AllowedActions[0]; UE_LOG(LogTemp, Error, TEXT("BotAI %s: Fallback took first available: %s from [%s]"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), *AllowedActionsString); }
        OutDecisionAmount = 0;
        UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Fallback Chose: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString());
    }

    if (ChosenAction == EPlayerAction::Bet || ChosenAction == EPlayerAction::Raise) {
        int64 MaxPossibleTotalBet = BotPlayerSeatData.CurrentBet + BotPlayerSeatData.Stack;
        if (OutDecisionAmount <= BotPlayerSeatData.CurrentBet && OutDecisionAmount < MaxPossibleTotalBet) {
            UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Calculated Bet/Raise total amount %lld is <= current bet %lld and not All-In Max. Activating Fallback."),
                *BotPlayerSeatData.PlayerName, OutDecisionAmount, BotPlayerSeatData.CurrentBet);
            if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Check)) ChosenAction = EPlayerAction::Check;
            else if (bIsFacingBet && AllowedActions.Contains(EPlayerAction::Call)) ChosenAction = EPlayerAction::Call;
            else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold;
            else if (!AllowedActions.IsEmpty()) ChosenAction = AllowedActions[0];
            OutDecisionAmount = 0;
            UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Bet/Raise Amount Fallback (not > CurrentBet) Chose: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString());
        }
        else if (OutDecisionAmount > MaxPossibleTotalBet) {
            UE_LOG(LogTemp, Verbose, TEXT("BotAI %s: Correcting OutDecisionAmount %lld to All-In amount %lld"), *BotPlayerSeatData.PlayerName, OutDecisionAmount, MaxPossibleTotalBet);
            OutDecisionAmount = MaxPossibleTotalBet;
        }

        if (ChosenAction == EPlayerAction::Bet) {
            int64 ActualBetAmountAdded = OutDecisionAmount - BotPlayerSeatData.CurrentBet;
            // Проверяем, что добавляемая сумма положительна и не меньше минимального бета (если это не олл-ин)
            if (((ActualBetAmountAdded < GameState->BigBlindAmount && ActualBetAmountAdded < BotPlayerSeatData.Stack) || ActualBetAmountAdded <= 0) && OutDecisionAmount < MaxPossibleTotalBet) {
                if (!(ChosenAction == EPlayerAction::Check || ChosenAction == EPlayerAction::Call || ChosenAction == EPlayerAction::Fold)) {
                    UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Bet total amount %lld (adds %lld) too small for non-all-in. Final Bet Fallback."), *BotPlayerSeatData.PlayerName, OutDecisionAmount, ActualBetAmountAdded);
                    if (!bIsFacingBet && AllowedActions.Contains(EPlayerAction::Check)) ChosenAction = EPlayerAction::Check;
                    else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold;
                    else if (bIsFacingBet && AllowedActions.Contains(EPlayerAction::Call)) ChosenAction = EPlayerAction::Call;
                    else if (!AllowedActions.IsEmpty()) ChosenAction = AllowedActions[0];
                    OutDecisionAmount = 0;
                    UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Bet Amount Final Fallback Chose: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString());
                }
            }
        }
        else if (ChosenAction == EPlayerAction::Raise) {
            int64 PureRaise = OutDecisionAmount - CurrentBetToCallOnTable;
            if (((OutDecisionAmount <= CurrentBetToCallOnTable || PureRaise < MinValidPureRaiseAmount) && OutDecisionAmount < MaxPossibleTotalBet) && MaxPossibleTotalBet > CurrentBetToCallOnTable) {
                if (!(ChosenAction == EPlayerAction::Call || ChosenAction == EPlayerAction::Fold)) {
                    UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Raise total amount %lld invalid. PureRaise %lld vs MinPure %lld for non-all-in. Final Raise Fallback."),
                        *BotPlayerSeatData.PlayerName, OutDecisionAmount, PureRaise, MinValidPureRaiseAmount);
                    if (AllowedActions.Contains(EPlayerAction::Call)) ChosenAction = EPlayerAction::Call;
                    else if (AllowedActions.Contains(EPlayerAction::Fold)) ChosenAction = EPlayerAction::Fold;
                    else if (!AllowedActions.IsEmpty()) ChosenAction = AllowedActions[0];
                    OutDecisionAmount = 0;
                    UE_LOG(LogTemp, Warning, TEXT("BotAI %s: Raise Amount Final Fallback Chose: %s"), *BotPlayerSeatData.PlayerName, *UEnum::GetDisplayValueAsText(ChosenAction).ToString());
                }
            }
        }
    }
    else {
        OutDecisionAmount = 0;
    }

    UE_LOG(LogTemp, Warning, TEXT("BotAI %s (S%d) FINAL DECISION: Action=%s, OutAmountForProcessAction=%lld"),
        *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, *UEnum::GetDisplayValueAsText(ChosenAction).ToString(), OutDecisionAmount);
    UE_LOG(LogTemp, Warning, TEXT("==================== BotAI %s (Seat %d) - GetBestAction END ===================="),
        *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
    return ChosenAction;
}

int64 UPokerBotAI::CalculateBetSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, float CalculatedHandStrength, bool bIsBluff, float PotFractionOverride) const
{
    // ... (код без изменений из Части 2) ...
    if (!GameState) return GameState->BigBlindAmount;
    int64 MinBet = GameState->BigBlindAmount;
    int64 MaxBetPlayerCanAdd = BotPlayerSeatData.Stack;
    if (MaxBetPlayerCanAdd <= 0) return BotPlayerSeatData.CurrentBet; // Не может добавить, возвращаем текущую ставку (0 если это первый бет)

    float PotFraction;
    if (PotFractionOverride > 0.0f && PotFractionOverride <= 2.0f) {
        PotFraction = PotFractionOverride;
    }
    else if (bIsBluff) {
        PotFraction = FMath::FRandRange(0.4f, 0.66f);
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

// --- CalculateRaiseSize (остается как в Части 2) ---
int64 UPokerBotAI::CalculateRaiseSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, int64 CurrentBetToCallOnTable, int64 MinValidPureRaiseAmount, float CalculatedHandStrength, bool bIsBluff, float PotFractionOverride) const
{
    if (!GameState) {
        // Возвращаем минимально возможный валидный рейз, если нет GameState
        return CurrentBetToCallOnTable + MinValidPureRaiseAmount;
    }

    int64 AmountToCall = CurrentBetToCallOnTable - BotPlayerSeatData.CurrentBet;
    if (AmountToCall < 0)
    {
        AmountToCall = 0; // Уже поставил достаточно или больше
    }

    // Если бот не может даже заколлировать, то любой его "рейз" будет олл-ином на текущий стек
    if (BotPlayerSeatData.Stack <= AmountToCall) { // Используем AmountToCall
        return BotPlayerSeatData.CurrentBet + BotPlayerSeatData.Stack; // All-in (общая сумма ставки)
    }

    // Банк ПОСЛЕ нашего предполагаемого колла
    int64 PotSizeAfterOurTheoreticalCall = GameState->Pot + AmountToCall; // Используем AmountToCall

    float TargetPureRaisePotFraction; // Доля от PotSizeAfterOurTheoreticalCall для чистого рейза

    if (PotFractionOverride > 0.0f && PotFractionOverride <= 2.0f) { // Позволяем овербет-рейзы до 2x пота
        TargetPureRaisePotFraction = PotFractionOverride;
        UE_LOG(LogTemp, Verbose, TEXT("BotAI CalculateRaiseSize: Using PotFractionOverride: %.2f"), PotFractionOverride);
    }
    else if (bIsBluff) {
        // Для блеф-рейза можно использовать чуть меньший сайзинг, но все же убедительный
        TargetPureRaisePotFraction = FMath::FRandRange(0.5f, 0.75f); // Блеф-рейз 50-75% пота (после колла)
        UE_LOG(LogTemp, Verbose, TEXT("BotAI CalculateRaiseSize: Bluffing, TargetPureRaisePotFraction: %.2f"), TargetPureRaisePotFraction);
    }
    else {
        // Сильнее рука / агрессивнее бот -> больше % от банка для чистого рейза
        // Пример: 0.5 (средняя сила) -> 50% пота, 0.8 (сильная) -> 75% пота, 1.0 (монстр) -> 100% пота
        // Множитель для агрессивности (0.5 до 1.5, если AggressivenessFactor от 0 до 1)
        float AggroMultiplier = FMath::Lerp(0.8f, 1.2f, AggressivenessFactor);
        TargetPureRaisePotFraction = FMath::Lerp(0.4f, 1.0f, FMath::Clamp(CalculatedHandStrength, 0.0f, 1.0f)) * AggroMultiplier;
        // Ограничиваем разумными пределами, например, от 1/3 пота до 1.5x пота
        TargetPureRaisePotFraction = FMath::Clamp(TargetPureRaisePotFraction, 0.33f, 1.5f);
        UE_LOG(LogTemp, Verbose, TEXT("BotAI CalculateRaiseSize: Value raising, CalculatedHandStrength: %.2f, AggroMultiplier: %.2f, TargetPureRaisePotFraction: %.2f"),
            CalculatedHandStrength, AggroMultiplier, TargetPureRaisePotFraction);
    }

    int64 CalculatedPureRaise = FMath::RoundToInt64(static_cast<float>(PotSizeAfterOurTheoreticalCall) * TargetPureRaisePotFraction);

    // Чистый рейз должен быть не меньше MinValidPureRaiseAmount
    CalculatedPureRaise = FMath::Max(CalculatedPureRaise, MinValidPureRaiseAmount);

    // Общая сумма, которую игрок добавит в банк в этом действии = колл + чистый рейз
    int64 TotalAmountPlayerAddsThisAction = AmountToCall + CalculatedPureRaise;

    // Если это больше стека, то это олл-ин (добавляем весь оставшийся стек)
    if (TotalAmountPlayerAddsThisAction > BotPlayerSeatData.Stack) {
        TotalAmountPlayerAddsThisAction = BotPlayerSeatData.Stack;
    }

    // Возвращаем ОБЩУЮ сумму ставки бота в этом раунде
    int64 FinalTotalBetAmount = BotPlayerSeatData.CurrentBet + TotalAmountPlayerAddsThisAction;

    UE_LOG(LogTemp, Verbose, TEXT("BotAI CalculateRaiseSize: PotAfterCall: %lld, PureRaise: %lld, TotalAdds: %lld, FinalTotalBet: %lld"),
        PotSizeAfterOurTheoreticalCall, CalculatedPureRaise, TotalAmountPlayerAddsThisAction, FinalTotalBetAmount);

    return FinalTotalBetAmount;
}



// --- ИТЕРАЦИЯ 3: БАЗОВАЯ ЛОГИКА НА ПОСТФЛОПЕ ---
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
    case EPokerHandRank::FullHouse:     return 0.85f; // Немного повысил
    case EPokerHandRank::Flush:         return 0.78f; // Немного повысил
    case EPokerHandRank::Straight:      return 0.72f; // Немного повысил
    case EPokerHandRank::ThreeOfAKind:  return 0.60f;
    case EPokerHandRank::TwoPair:       return 0.45f; // Немного понизил, т.к. две пары часто уязвимы
    case EPokerHandRank::OnePair:       return 0.30f; // Особенно уязвима, зависит от кикера и силы пары
    case EPokerHandRank::HighCard:      return 0.10f;
    default:                            return 0.0f;
    }
}

// --- ИТЕРАЦИЯ 5: БАЗОВЫЙ БЛЕФ (ЗАГЛУШКА) ---
bool UPokerBotAI::ShouldAttemptBluff(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, EPlayerPokerPosition BotPosition, int32 NumOpponentsStillInHand) const
{
    if (!GameState || NumOpponentsStillInHand == 0 || NumOpponentsStillInHand > 2) { // Блефуем только против 1-2 оппонентов
        return false;
    }

    // 1. Позиция
    bool bGoodPositionForBluff = (BotPosition == EPlayerPokerPosition::BTN || BotPosition == EPlayerPokerPosition::CO);
    if (GameState->CurrentStage == EGameStage::River && BotPosition == EPlayerPokerPosition::HJ && NumOpponentsStillInHand == 1) bGoodPositionForBluff = true; // На ривере можно шире

    if (!bGoodPositionForBluff) return false;

    // 2. Текстура борда (очень упрощенно): "Сухой" борд (нет очевидных флеш-дро или стрит-дро, мало связанных карт)
    bool bScaryBoard = false;
    if (GameState->CommunityCards.Num() >= 3) {
        TMap<ECardSuit, int32> SuitCountsOnBoard;
        TArray<int32> RanksOnBoardInt;
        for (const FCard& Card : GameState->CommunityCards) {
            SuitCountsOnBoard.FindOrAdd(Card.Suit)++;
            RanksOnBoardInt.Add(static_cast<int32>(Card.Rank));
        }
        for (const auto& SuitPair : SuitCountsOnBoard) {
            if (SuitPair.Value >= 3) { bScaryBoard = true; break; } // Флеш уже на борде или очень вероятен
        }
        if (!bScaryBoard && RanksOnBoardInt.Num() >= 3) {
            RanksOnBoardInt.Sort();
            int32 ConnectedCount = 0;
            for (size_t i = 0; i < RanksOnBoardInt.Num() - 1; ++i) {
                if (FMath::Abs(RanksOnBoardInt[i] - RanksOnBoardInt[i + 1]) <= 2) ConnectedCount++; // 2-gap или меньше
            }
            if (ConnectedCount >= GameState->CommunityCards.Num() - 1 || // Почти все карты борда связаны
                (GameState->CommunityCards.Num() == 3 && ConnectedCount >= 1) || // Связанный флоп
                (GameState->CommunityCards.Num() == 4 && ConnectedCount >= 2))   // Связанный терн
            {
                bScaryBoard = true; // Много стрит-возможностей
            }
        }
    }
    if (bScaryBoard) return false; // Не блефуем на "страшном" или "мокром" борде

    // 3. Оппоненты показали слабость (чекали до нас на этой улице)
    bool bOpponentShowedWeakness = (GameState->CurrentBetToCall == BotPlayerSeatData.CurrentBet && GameState->LastAggressorSeatIndex != BotPlayerSeatData.SeatIndex);
    // Если мы PlayerWhoOpenedBettingThisRound, и до нас все чекнули, это тоже слабость.
    if (GameState->PlayerWhoOpenedBettingThisRound == BotPlayerSeatData.SeatIndex && GameState->LastAggressorSeatIndex == -1 && GameState->CurrentBetToCall == 0) {
        bOpponentShowedWeakness = true;
    }


    if (!bOpponentShowedWeakness) return false;

    // 4. Учитываем предыдущую агрессию бота (TODO: это нужно отслеживать)
    // bool bBotWasAggressorPreviously = ... ;
    // if (bBotWasAggressorPreviously) { /* Повышаем шанс блефа */ }

    if (FMath::FRand() < (BluffFrequency * (0.4f + AggressivenessFactor * 0.6f))) {
        return true;
    }
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
    // ... (код без изменений из Части 2) ...
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
    // Проверяем на стрит, начиная с самой высокой возможной карты (Туз-хай стрит: 10,J,Q,K,A -> LowCard = 10)
    // до самой низкой (А-5 стрит: A,2,3,4,5 -> LowCard = 1 (Туз))
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
            // OESD: дырка с краю (но не тупиковая для самой низкой/высокой карты)
            // Пример: у нас 6,7,8,9. Не хватает 5 или T.
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
    // Убираем пересекающиеся ауты (для стрит-флеш дро)
    // TODO: Более точный подсчет для стрит-флеш дро, если есть и флеш-дро и стрит-дро,
    // то ауты на стрит-флеш (обычно 1 или 2) не должны считаться дважды.
    // Пока для MVP это можно опустить.

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
            // P(не улучшиться на терне) = (UC-O)/UC
            // P(не улучшиться на ривере, если не улучшился на терне) = (UC-1-O)/(UC-1)
            // P(не улучшиться ни на терне, ни на ривере) = P1 * P2
            // P(улучшиться хотя бы раз) = 1 - P(не улучшиться)
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
    // ... (код без изменений из Части 2) ...
    if (GameState->CurrentStage != EGameStage::Preflop) return false;
    bool bNoRealAggressionYet = (GameState->LastAggressorSeatIndex == -1 ||
        GameState->LastAggressorSeatIndex == GameState->PendingSmallBlindSeat ||
        GameState->LastAggressorSeatIndex == GameState->PendingBigBlindSeat);
    return bNoRealAggressionYet && (BotPlayerSeatData.CurrentBet <= GameState->BigBlindAmount);
}