#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h" // Включаем для FPlayerSeatData, EPlayerAction, FCard и т.д.
#include "PokerBotAI.generated.h"

// Прямое объявление, чтобы избежать циклической зависимости или полного инклюда GameState в .h
// Полный инклюд UOfflinePokerGameState будет в .cpp файле.
class UOfflinePokerGameState;

/**
 * Базовый класс для логики принятия решений ИИ бота в покере.
 */
UCLASS(Blueprintable) // Blueprintable, если вдруг захотите создавать подклассы в BP или вызывать из BP для тестов
class POKER_CLIENT_API UPokerBotAI : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UPokerBotAI();

    /**
     * Основная функция для принятия решения ботом.
     * Эту функцию можно будет переопределять в дочерних классах для создания разных "личностей" ботов.
     *
     * @param GameState Текущее состояние всего стола.
     * @param BotPlayerSeatData Данные конкретного бота, для которого принимается решение.
     * @param AllowedActions Массив действий, которые валидны для бота в текущей ситуации.
     * @param CurrentBetToCallOnTable Сумма, которую нужно доставить на стол, чтобы заколлировать.
     * @param MinValidPureRaiseAmount Минимальная чистая сумма для рейза (сверх CurrentBetToCallOnTable).
     *                                 Если это первый бет на улице, то это минимальная сумма самого бета.
     * @param OutDecisionAmount Выходной параметр: сумма, которую бот решает поставить/доставить (для Bet, Raise, Call).
     *                          Для Fold и Check это значение обычно игнорируется или 0.
     * @return Выбранное ботом действие (EPlayerAction).
     */
    UFUNCTION(BlueprintCallable, Category = "Bot AI|Decision Making")
    virtual EPlayerAction GetBestAction(
        const UOfflinePokerGameState* GameState,
        const FPlayerSeatData& BotPlayerSeatData,
        const TArray<EPlayerAction>& AllowedActions,
        int64 CurrentBetToCallOnTable,
        int64 MinValidPureRaiseAmount,
        int64& OutDecisionAmount
    );

    // Можно добавить методы для настройки "личности" или стратегии бота, если потребуется в будущем
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot AI|Personality")
    // float AggressivenessFactor;

    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot AI|Personality")
    // float BluffFrequency;

protected:
    // --- Вспомогательные функции для ИИ (пока могут быть пустыми или с простой логикой) ---

    /**
     * (Пример) Оценивает относительную силу карманных карт бота.
     * @param HoleCards Карманные карты бота.
     * @return Значение от 0.0 (очень слабые) до 1.0 (очень сильные).
     */
     // virtual float CalculatePreFlopHandStrength(const TArray<FCard>& HoleCards) const;

     /**
      * (Пример) Оценивает силу руки бота на текущей улице с учетом общих карт.
      * @param HoleCards Карманные карты бота.
      * @param CommunityCards Общие карты на столе.
      * @return Результат оценки FPokerHandResult.
      */
      // virtual FPokerHandResult EvaluateCurrentHandStrength(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards) const;

      /**
       * (Пример) Принимает решение о размере ставки/рейза.
       * @param BotPlayerSeatData Данные бота.
       * @param GameState Состояние игры.
       * @param ActionToTake Предполагаемое действие (Bet или Raise).
       * @param MinValidBetOrRaiseAmount Минимально допустимая сумма для этого действия.
       * @return Рассчитанная сумма ставки/рейза.
       */
       // virtual int64 CalculateBetOrRaiseAmount(
       //     const FPlayerSeatData& BotPlayerSeatData,
       //     const UOfflinePokerGameState* GameState,
       //     EPlayerAction ActionToTake,
       //     int64 MinValidBetOrRaiseAmount
       // ) const;
};