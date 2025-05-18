#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PokerDataTypes.h" // Для FCard и TArray
#include "HoleCardDisplayInterface.generated.h"

// Этот класс не нужно изменять
UINTERFACE(MinimalAPI, Blueprintable) // Blueprintable, чтобы интерфейс был виден и реализуем в BP
class UHoleCardDisplayInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Интерфейс для акторов, отображающих карманные карты игрока.
 */
class POKER_CLIENT_API IHoleCardDisplayInterface // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    /**
     * Обновляет отображение карманных карт.
     * @param HoleCards Массив из двух карманных карт. Если массив пуст или содержит не 2 карты, должны отображаться рубашки или ничего.
     * @param bShowFace Должны ли карты показываться лицевой стороной (true для локального игрока) или рубашкой (false для оппонентов).
     */
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Hole Card Display")
    void UpdateHoleCards(const TArray<FCard>& HoleCards, bool bShowFaceToPlayer);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Hole Card Display")
    void HideHoleCards();
};