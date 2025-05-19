#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PokerDataTypes.h" // Для FCard и TArray
#include "CommunityCardDisplayInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UCommunityCardDisplayInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Интерфейс для акторов, отображающих общие карты на столе.
 */
class POKER_CLIENT_API ICommunityCardDisplayInterface // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    /**
     * Обновляет отображение общих карт на столе.
     * @param CommunityCards Массив общих карт (от 0 до 5).
     */
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Community Card Display")
    void UpdateCommunityCards(const TArray<FCard>& CommunityCards);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Community Card Display")
    void HideCommunityCards();
};