#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"       // Для FPlayerSeatData, EPlayerAction, FCard и т.д.
#include "PokerHandEvaluator.h"    // Для FPokerHandResult (используется в EvaluateCurrentMadeHand)
#include "PokerBotAI.generated.h"

// Прямые объявления для уменьшения зависимостей в .h
class UOfflinePokerGameState;

// Перечисление для позиции за столом (можно вынести в PokerDataTypes.h, если используется где-то еще)
UENUM(BlueprintType)
enum class EPlayerPokerPosition : uint8
{
    BTN         UMETA(DisplayName = "Button (BTN)"),
    SB          UMETA(DisplayName = "Small Blind (SB)"),
    BB          UMETA(DisplayName = "Big Blind (BB)"),
    UTG         UMETA(DisplayName = "Under The Gun (UTG)"), // Первая позиция после BB
    UTG1        UMETA(DisplayName = "UTG+1"),
    MP1         UMETA(DisplayName = "Middle Position 1 (MP1)"), // Средние позиции
    MP2         UMETA(DisplayName = "Middle Position 2 (MP2)"),
    HJ          UMETA(DisplayName = "Hijack (HJ)"),           // Поздняя позиция перед CO
    CO          UMETA(DisplayName = "Cutoff (CO)"),           // Поздняя позиция перед BTN
    Unknown     UMETA(DisplayName = "Unknown")                // Для случаев <3 игроков или ошибок
};

UCLASS()
class POKER_CLIENT_API UPokerBotAI : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:


    UPokerBotAI();

    void SetPersonalityFactors(const FBotPersonalitySettings& Settings);

    // Флаг для переключения на детерминированное поведение в тестах
    bool bIsTesting = false;
    // Значение, которое будет возвращать FRand() в тестовом режиме
    float TestFixedRandValue = 0.5f;

    // --- ПАРАМЕТРЫ "ЛИЧНОСТИ" БОТА ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float AggressivenessFactor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float BluffFrequency;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float TightnessFactor;

    /**
     * Основная функция для принятия решения ботом.
     * @param GameState Текущее состояние игры.
     * @param BotPlayerSeatData Данные о месте бота.
     * @param AllowedActions Список доступных действий.
     * @param CurrentBetToCallOnTable Сумма, которую нужно доставить, чтобы уравнять текущую максимальную ставку на столе.
     * @param MinValidPureRaiseAmount Минимальная чистая сумма для рейза (или минимальная сумма для бета, если ставок еще не было).
     * @param OutDecisionAmount (Выходной параметр) Если действие Bet или Raise, это будет ОБЩАЯ сумма, до которой бот ставит/рейзит. Для Call/Check/Fold/PostBlind это значение игнорируется или 0.
     * @return Выбранное действие.
     */
    EPlayerAction GetBestAction(
        const UOfflinePokerGameState* GameState,
        const FPlayerSeatData& BotPlayerSeatData,
        const TArray<EPlayerAction>& AllowedActions,
        int64 CurrentBetToCallOnTable,
        int64 MinValidPureRaiseAmount,
        int64& OutDecisionAmount
    );

    virtual EPlayerPokerPosition GetPlayerPosition(const UOfflinePokerGameState* GameState, int32 BotSeatIndex, int32 NumActivePlayers) const;

    virtual float CalculatePreflopHandStrength(const FCard& Card1, const FCard& Card2, EPlayerPokerPosition BotPosition, int32 NumActivePlayers) const;

    virtual int64 CalculateBetSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, float CalculatedHandStrength, bool bIsBluff = false, float PotFractionOverride = 0.0f) const;

    virtual int64 CalculateRaiseSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, int64 CurrentBetToCallOnTable, int64 MinValidPureRaiseAmount, float CalculatedHandStrength, bool bIsBluff = false, float PotFractionOverride = 0.0f) const;

    virtual float GetScoreForMadeHand(EPokerHandRank HandRank) const;

protected:



    virtual FPokerHandResult EvaluateCurrentMadeHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards) const;

    virtual float CalculateDrawStrength(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards, int64 PotSize, int64 AmountToCallIfAny) const;

    virtual bool ShouldAttemptBluff(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, EPlayerPokerPosition BotPosition, int32 NumOpponentsStillInHand) const;

    virtual int32 CountActiveOpponents(const UOfflinePokerGameState* GameState, int32 BotSeatIndex) const;

    virtual TArray<FPlayerSeatData> GetActiveOpponentData(const UOfflinePokerGameState* GameState, int32 BotSeatIndex) const;

    virtual bool bIsOpenRaiserSituation(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData) const;

};