#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PokerDataTypes.h" // Если будете передавать FPlayerSeatData
#include "PlayerInfoWidgetInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable) // Blueprintable, чтобы можно было реализовать в BP
class UIPlayerInfoWidgetInterface : public UInterface
{
    GENERATED_BODY()
};

class POKER_CLIENT_API IIPlayerInfoWidgetInterface // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    // Функция для инициализации/обновления виджета данными игрока
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Player Info Interface")
    void UpdatePlayerInfo(const FString& PlayerName, int64 PlayerStack);
    // Или: void InitializePlayerInfo(const FPlayerSeatData& SeatData);
    // BlueprintImplementableEvent означает, что реализация будет в Blueprint
};