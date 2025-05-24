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

protected:
    // --- ИТЕРАЦИЯ 1: СИЛА СТАРТОВОЙ РУКИ (ПРЕФЛОП) ---
    /**
     * Оценивает относительную силу стартовой руки (2 карманные карты).
     * @param Card1 Первая карманная карта.
     * @param Card2 Вторая карманная карта.
     * @param BotPosition Позиция бота за столом.
     * @param NumActivePlayers Общее количество активных игроков в текущей раздаче.
     * @return Значение от 0.0 (очень слабая) до 1.0 (очень сильная).
     */
    virtual float CalculatePreflopHandStrength(const FCard& Card1, const FCard& Card2, EPlayerPokerPosition BotPosition, int32 NumActivePlayers) const;

    // --- ИТЕРАЦИЯ 2: УЧЕТ ПОЗИЦИИ ---
    /**
     * Определяет позицию игрока за столом относительно дилера.
     * @param GameState Текущее состояние игры (для доступа к DealerSeat и Seats).
     * @param BotSeatIndex Индекс места бота.
     * @param NumActivePlayers Общее количество активных игроков в текущей раздаче.
     * @return EPlayerPokerPosition позиция бота.
     */
    virtual EPlayerPokerPosition GetPlayerPosition(const UOfflinePokerGameState* GameState, int32 BotSeatIndex, int32 NumActivePlayers) const;

    // --- ИТЕРАЦИЯ 3: БАЗОВАЯ ЛОГИКА НА ПОСТФЛОПЕ ---
    /**
     * Оценивает силу текущей ГОТОВОЙ комбинации бота на постфлопе.
     * @param HoleCards Карманные карты бота.
     * @param CommunityCards Общие карты на столе.
     * @return FPokerHandResult с рангом комбинации и кикерами.
     */
    virtual FPokerHandResult EvaluateCurrentMadeHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards) const;

    /**
     * Преобразует ранг готовой руки в числовую оценку.
     * @param HandRank Ранг комбинации.
     * @return Значение от 0.0 до 1.0.
     */
    virtual float GetScoreForMadeHand(EPokerHandRank HandRank) const;

    /**
     * Оценивает потенциал дро-руки.
     * @param HoleCards Карманные карты бота.
     * @param CommunityCards Общие карты на столе.
     * @param PotSize Текущий размер банка.
     * @param AmountToCallIfAny Сумма для колла текущей ставки (0, если нет ставки).
     * @return Значение от 0.0 до 1.0, представляющее "играбельность" дро с учетом шансов банка.
     */
    virtual float CalculateDrawStrength(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards, int64 PotSize, int64 AmountToCallIfAny) const;

    // --- ИТЕРАЦИЯ 4: РАЗМЕРЫ СТАВОК ---
    /**
     * Рассчитывает сумму для бета (общую сумму ставки).
     * @param GameState Текущее состояние игры.
     * @param BotPlayerSeatData Данные бота.
     * @param CalculatedHandStrength Оценка силы текущей руки/ситуации (0.0-1.0).
     * @param bIsBluff Является ли этот бет блефом.
     * @param PotFractionOverride (Опционально) Позволяет задать точную долю банка для ставки, игнорируя CalculatedHandStrength.
     * @return Общая сумма ставки (BotPlayerSeatData.CurrentBet + добавляемая сумма).
     */
    virtual int64 CalculateBetSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, float CalculatedHandStrength, bool bIsBluff = false, float PotFractionOverride = 0.0f) const;

    /**
     * Рассчитывает сумму для рейза (общую сумму, до которой рейзит бот).
     * @param GameState Текущее состояние игры.
     * @param BotPlayerSeatData Данные бота.
     * @param CurrentBetToCallOnTable Текущая сумма для колла на столе.
     * @param MinValidPureRaiseAmount Минимальный чистый рейз, который можно сделать.
     * @param CalculatedHandStrength Оценка силы текущей руки/ситуации (0.0-1.0).
     * @param bIsBluff Является ли этот рейз блефом/полублефом.
     * @param PotFractionOverride (Опционально) Можно использовать для задания конкретного множителя для рейза от банка.
     * @return Общая сумма ставки после рейза.
     */
    virtual int64 CalculateRaiseSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, int64 CurrentBetToCallOnTable, int64 MinValidPureRaiseAmount, float CalculatedHandStrength, bool bIsBluff = false, float PotFractionOverride = 0.0f) const;

    // --- ИТЕРАЦИЯ 5: БАЗОВЫЙ БЛЕФ ---
    /**
     * Определяет, стоит ли боту пытаться блефовать в текущей ситуации.
     * @param GameState Текущее состояние игры.
     * @param BotPlayerSeatData Данные бота.
     * @param BotPosition Позиция бота.
     * @param NumOpponentsStillInHand Количество активных оппонентов.
     * @return true, если бот должен попытаться блефовать.
     */
    virtual bool ShouldAttemptBluff(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, EPlayerPokerPosition BotPosition, int32 NumOpponentsStillInHand) const;

    // --- ОБЩИЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---
    /**
     * Подсчитывает количество активных оппонентов, оставшихся в раздаче (не сфолдили и могут продолжать игру).
     * @param GameState Текущее состояние игры.
     * @param BotSeatIndex Индекс места бота (чтобы не считать его самого).
     * @return Количество активных оппонентов.
     */
    virtual int32 CountActiveOpponents(const UOfflinePokerGameState* GameState, int32 BotSeatIndex) const;

    /**
     * Вспомогательная функция для получения данных активных оппонентов.
     */
    virtual TArray<FPlayerSeatData> GetActiveOpponentData(const UOfflinePokerGameState* GameState, int32 BotSeatIndex) const;

    /**
     * Проверяет, является ли текущая ситуация на префлопе "открывающим рейзом" для бота
     * (т.е. до него не было рейзов, только лимпы или фолды).
     */
    virtual bool bIsOpenRaiserSituation(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData) const;


    // --- ПАРАМЕТРЫ "ЛИЧНОСТИ" БОТА ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float AggressivenessFactor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float BluffFrequency;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float TightnessFactor;
};