#include "PokerBotAI.h"
#include "OfflinePokerGameState.h" // Полный инклюд для доступа к членам GameState
#include "PokerDataTypes.h"       // Для FPlayerSeatData, EPlayerAction и т.д.
#include "Math/UnrealMathUtility.h" // Для FMath::FRandRange, FMath::Min

// (Опционально) Можете добавить свою лог категорию для ИИ
// DECLARE_LOG_CATEGORY_EXTERN(LogPokerBotAI, Log, All);
// DEFINE_LOG_CATEGORY(LogPokerBotAI);

UPokerBotAI::UPokerBotAI()
{
    // Здесь можно инициализировать какие-либо свойства бота по умолчанию, если они есть
    // Например:
    // AggressivenessFactor = 0.5f;
    // BluffFrequency = 0.1f;
    UE_LOG(LogTemp, Log, TEXT("UPokerBotAI instance created."));
}

EPlayerAction UPokerBotAI::GetBestAction(
    const UOfflinePokerGameState* GameState,
    const FPlayerSeatData& BotPlayerSeatData,
    const TArray<EPlayerAction>& AllowedActions,
    int64 CurrentBetToCallOnTable,
    int64 MinValidPureRaiseAmount, // Это чистый рейз или мин. бет
    int64& OutDecisionAmount // Сумма, которую бот ставит/коллирует/рейзит (общая сумма его ставки в этом рауnde для Bet/Raise)
)
{
    OutDecisionAmount = 0; // Инициализируем выходной параметр

    if (!GameState)
    {
        UE_LOG(LogTemp, Error, TEXT("PokerBotAI::GetBestAction - GameState is null. Defaulting to Fold."));
        return EPlayerAction::Fold; // Безопасное действие по умолчанию
    }

    if (AllowedActions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PokerBotAI::GetBestAction for %s (Seat %d) - No allowed actions provided. Defaulting to Fold (or Check if possible, though unlikely)."),
            *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
        // Это не должно происходить, если RequestPlayerAction корректно работает и игрок может ходить
        return EPlayerAction::Fold;
    }

    // --- Логика для этапов постановки блайндов ---
    if (GameState->CurrentStage == EGameStage::WaitingForSmallBlind && BotPlayerSeatData.Status == EPlayerStatus::MustPostSmallBlind)
    {
        if (AllowedActions.Contains(EPlayerAction::PostBlind))
        {
            UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) is SB and MUST post blind."), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
            OutDecisionAmount = FMath::Min(GameState->SmallBlindAmount, BotPlayerSeatData.Stack); // Сумма блайнда (или олл-ин)
            return EPlayerAction::PostBlind;
        }
    }
    else if (GameState->CurrentStage == EGameStage::WaitingForBigBlind && BotPlayerSeatData.Status == EPlayerStatus::MustPostBigBlind)
    {
        if (AllowedActions.Contains(EPlayerAction::PostBlind))
        {
            UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) is BB and MUST post blind."), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
            OutDecisionAmount = FMath::Min(GameState->BigBlindAmount, BotPlayerSeatData.Stack); // Сумма блайнда (или олл-ин)
            return EPlayerAction::PostBlind;
        }
    }

    // --- Очень Простая Логика Принятия Решений для Префлопа и Постфлопа ---
    // Мы будем использовать случайные числа для простоты.
    // 1. Возможность Check
    if (AllowedActions.Contains(EPlayerAction::Check))
    {
        if (FMath::FRand() < 0.8f) // 80% шанс чека
        {
            UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) Decides: Check"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
            OutDecisionAmount = 0; // Для Check сумма не важна
            return EPlayerAction::Check;
        }
        else if (AllowedActions.Contains(EPlayerAction::Bet))
        {
            // Делаем минимальный бет
            // MinValidPureRaiseAmount здесь выступает как MinBetAmount
            int64 BetTotalAmount = FMath::Min(MinValidPureRaiseAmount, BotPlayerSeatData.Stack);
            OutDecisionAmount = BetTotalAmount; // ОБЩАЯ СУММА БЕТА
            UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) Decides: Bet (Min) %lld"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, OutDecisionAmount);
            return EPlayerAction::Bet;
        }
        UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) Decides: Check (fallback after failed bet attempt)"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
        OutDecisionAmount = 0;
        return EPlayerAction::Check;
    }

    // 2. Если Check невозможен, значит есть ставка для колла
    if (AllowedActions.Contains(EPlayerAction::Call))
    {
        int64 AmountPlayerNeedsToAddForCall = CurrentBetToCallOnTable - BotPlayerSeatData.CurrentBet;
        if (AmountPlayerNeedsToAddForCall < 0) AmountPlayerNeedsToAddForCall = 0;

        if (AmountPlayerNeedsToAddForCall < BotPlayerSeatData.Stack / 2 && FMath::FRand() < 0.7f)
        {
            // OutDecisionAmount для Call - это сколько ДОБАВЛЯЕМ. ProcessPlayerAction это обработает.
            // НЕТ, ProcessPlayerAction для Call ожидает Amount = 0, а сам вычисляет сумму колла.
            // Поэтому для Call OutDecisionAmount должен быть 0.
            OutDecisionAmount = 0; // Сумма для Call вычисляется в ProcessPlayerAction
            UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) Decides: Call (AmountToCall: %lld)"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, AmountPlayerNeedsToAddForCall);
            return EPlayerAction::Call;
        }
        else if (AllowedActions.Contains(EPlayerAction::Raise))
        {
            // Делаем минимальный рейз
            // TotalBetAfterRaise = CurrentBetToCallOnTable (до чего коллим) + MinValidPureRaiseAmount (чистый рейз)
            int64 TotalBetAfterRaise = CurrentBetToCallOnTable + MinValidPureRaiseAmount;
            // Убедимся, что бот не ставит больше, чем у него есть + его текущая ставка
            TotalBetAfterRaise = FMath::Min(TotalBetAfterRaise, BotPlayerSeatData.CurrentBet + BotPlayerSeatData.Stack);

            // Проверка, что это действительно рейз (общая ставка бота будет больше CurrentBetToCallOnTable)
            if (TotalBetAfterRaise > CurrentBetToCallOnTable)
            {
                OutDecisionAmount = TotalBetAfterRaise; // ОБЩАЯ СУММА РЕЙЗА
                UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) Decides: Raise to %lld (MinRaise)"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, OutDecisionAmount);
                return EPlayerAction::Raise;
            }
            // Если минимальный рейз не получается, просто коллируем
            OutDecisionAmount = 0; // Сумма для Call
            UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) Decides: Call (AmountToCall: %lld) (fallback after failed raise attempt)"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, AmountPlayerNeedsToAddForCall);
            return EPlayerAction::Call;
        }
        // Если рейз невозможен, но мы не выбрали колл, все равно коллируем
        OutDecisionAmount = 0; // Сумма для Call
        UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) Decides: Call (AmountToCall: %lld) (fallback)"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex, AmountPlayerNeedsToAddForCall);
        return EPlayerAction::Call;
    }

    // 3. Если ничего из вышеперечисленного не подошло (например, остался только Fold)
    if (AllowedActions.Contains(EPlayerAction::Fold))
    {
        UE_LOG(LogTemp, Log, TEXT("Bot %s (Seat %d) Decides: Fold (last resort)"), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
        OutDecisionAmount = 0;
        return EPlayerAction::Fold;
    }

    // 4. Крайний случай (не должно происходить, если AllowedActions всегда содержит Fold для активного игрока)
    UE_LOG(LogTemp, Error, TEXT("Bot %s (Seat %d) - NO VALID ACTION FOUND from allowed list. Defaulting to Fold."), *BotPlayerSeatData.PlayerName, BotPlayerSeatData.SeatIndex);
    OutDecisionAmount = 0;
    return EPlayerAction::Fold;
}

// --- Реализации вспомогательных функций ИИ (пока не нужны для простейшего варианта) ---

// float UPokerBotAI::CalculatePreFlopHandStrength(const TArray<FCard>& HoleCards) const
// {
//     // TODO: Реализовать простую оценку силы стартовой руки (например, по таблице Ченя)
//     return 0.5f; // Placeholder
// }

// FPokerHandResult UPokerBotAI::EvaluateCurrentHandStrength(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards) const
// {
//     // Можно использовать UPokerHandEvaluator, если бот "знает" общие карты
//     // return UPokerHandEvaluator::EvaluatePokerHand(HoleCards, CommunityCards);
//     return FPokerHandResult(); // Placeholder
// }

// int64 UPokerBotAI::CalculateBetOrRaiseAmount(...) const
// {
//     // TODO: Логика расчета суммы ставки/рейза (например, % от банка, % от стека, на основе силы руки)
//     return MinValidBetOrRaiseAmount; // Placeholder - всегда минимальный
// }