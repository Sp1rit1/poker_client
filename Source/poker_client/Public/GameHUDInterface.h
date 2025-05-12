// GameHUDInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PokerDataTypes.h" // Включаем для EPlayerAction
#include "GameHUDInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable) // Blueprintable чтобы интерфейс был виден в BP
class UGameHUDInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Интерфейс для взаимодействия с игровым HUD.
 */
class POKER_CLIENT_API IGameHUDInterface // Имя интерфейса обычно начинается с 'I'
{
    GENERATED_BODY()

    // Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
    /**
     * Обновляет кнопки действий и информацию о текущем ходе в HUD.
     * Эту функцию нужно будет реализовать в Blueprint виджете HUD.
     */
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface")
    void UpdateActionButtonsAndPlayerTurn(int32 ForPlayerSeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 CurrentBetToCall, int64 MinimumRaise, int64 PlayerStack);

    /**
     * Инициализирует HUD начальными данными состояния игры.
     * Эту функцию нужно будет реализовать в Blueprint виджете HUD.
     */
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface")
    void InitializeHUDFromState(int64 Pot, const TArray<FPlayerSeatData>& Seats);

    // Можете добавить другие функции интерфейса по мере необходимости
    // Например, для показа карманных карт локального игрока, обновления банка и т.д.
    // UFUNCTION(BlueprintImplementableEvent, Category = "HUD Interface")
    // void ShowLocalPlayerHoleCards(const TArray<FCard>& HoleCards);
};