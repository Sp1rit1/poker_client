#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PokerDataTypes.h" // Убедитесь, что FCard и FPokerHandResult здесь
#include "Containers/Map.h" // Для TMap
#include "PokerHandEvaluator.generated.h"

/**
 * Библиотека функций для полной оценки покерных комбинаций (лучшая 5-карточная из 7).
 */
UCLASS()
class POKER_CLIENT_API UPokerHandEvaluator : public UBlueprintFunctionLibrary // Замените YOURPROJECT_API
{
	GENERATED_BODY()

public:
	/**
	 * Оценивает лучшую 5-карточную покерную комбинацию из предоставленного набора карт (обычно до 7).
	 * @param HoleCards Карманные карты игрока (может быть пустой).
	 * @param CommunityCards Общие карты на столе (может быть от 0 до 5).
	 * @return Структура FPokerHandResult, содержащая ранг лучшей комбинации и её кикеры.
	 */
	UFUNCTION(BlueprintPure, Category = "Poker|Evaluation")
	static FPokerHandResult EvaluatePokerHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards);

	/**
	 * Сравнивает две покерные комбинации.
	 * @param HandA Результат первой комбинации.
	 * @param HandB Результат второй комбинации.
	 * @return > 0 если HandA лучше, < 0 если HandB лучше, 0 если равны.
	 */
	UFUNCTION(BlueprintPure, Category = "Poker|Evaluation")
	static int32 CompareHandResults(const FPokerHandResult& HandA, const FPokerHandResult& HandB);


private:
	// --- Вспомогательные функции ---

	// Оценивает КОНКРЕТНУЮ 5-карточную комбинацию
	static FPokerHandResult EvaluateSingleFiveCardHand(const TArray<FCard>& FiveCards);

	// Сортирует 5 карт по рангу (от старшей к младшей) для анализа
	static void SortFiveCardsDesc(TArray<FCard>& FiveCards); // Использует статичный массив для эффективности

	// Проверяет, являются ли 5 отсортированных карт стритом. Учитывает A-2-3-4-5.
	static bool IsStraight(const TArray<FCard>& SortedFiveCards, ECardRank& OutHighCard);

	// Проверяет, являются ли 5 карт флешем.
	static bool IsFlush(const TArray<FCard>& FiveCards, ECardSuit& OutSuit);

	// Подсчитывает количество карт каждого ранга в 5 картах.
	static void CountRanks(const TArray<FCard>& FiveCards, TMap<ECardRank, int32>& OutRankCounts);

	// Вспомогательные функции для получения кикеров для каждого типа руки
	static void GetKickers(const TArray<FCard>& SortedFiveCards, int32 NumKickersToGet, TArray<ECardRank>& OutKickers, const TSet<ECardRank>& RanksToExclude);

	// Индексы для генерации 5-карточных комбинаций из 7
	static const int32 Combinations7c5[21][5];
};