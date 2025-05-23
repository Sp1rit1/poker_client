#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PokerDataTypes.h" // Включаем для EPlayerAction и FPlayerSeatData (если будете использовать для инициализации)
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
class POKER_CLIENT_API IGameHUDInterface // Замените YOURPROJECT_API. Имя интерфейса обычно начинается с 'I'
{
    GENERATED_BODY()

public:
    /**
     * Инициализирует/обновляет основную информацию о состоянии игры и текущем ходе в HUD.
     * Вызывается как при начальной настройке HUD, так и после каждого действия, чтобы показать, чей ход.
     * @param ForPlayerSeatIndex Индекс места игрока, чей сейчас ход (или -1, если ход не определен/ожидание).
     * @param CurrentPot Текущий размер общего банка.
     * @param CurrentBetToCall Сумма, которую нужно доставить, чтобы остаться в игре (сделать колл).
     * @param MinimumRaise Минимальная сумма для бета или рейза.
     * @param PlayerStack Стек игрока, чей сейчас ход (или стек локального игрока, если ForPlayerSeatIndex = -1).
     */
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Update")
    void UpdateGameInfo(
        const FString& MovingPlayerName,        // Имя ходящего
        int64 CurrentPotGlobal,                 // Общий банк на столе
        int64 ActualAmountPlayerNeedsToCall,    // Сколько ХОДЯЩИЙ игрок должен ДОБАВИТЬ для колла
        int64 MinPureRaiseValue,                // ЧИСТАЯ сумма минимального рейза (или мин. бета)
        int64 MovingPlayerActualStack,          // Текущий стек ХОДЯЩЕГО игрока
        int64 MovingPlayerCurrentBetInRound     // Текущая ставка ХОДЯЩЕГО игрока в этом раунде
        // Возможно, вам понадобится еще и GameStateData->CurrentBetToCall (общая сумма для колла на столе)
        // int64 OverallBetToCallOnTable 
    );

    /**
     * Обновляет состояние кнопок действий в HUD на основе доступных действий для текущего игрока.
     * Вызывается, когда наступает ход локального игрока.
     * @param AllowedActions Массив действий, доступных текущему игроку.
     */
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Update")
    void UpdateActionButtons(const TArray<EPlayerAction>& AllowedActions);

    /**
     * Деактивирует все кнопки действий игрока в HUD.
     * Вызывается, когда не ход локального игрока или когда никакие действия невозможны.
     */
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Controls")
    void DisableButtons();

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface")
    void AddGameHistoryMessage(const FString& Message);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Notifications")
    void ShowNotificationMessage(const FString& Message, float Duration = 3.0f);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Game Events")
    void UpdateCommunityCardsDisplay(const TArray<FCard>& CommunityCards);

};