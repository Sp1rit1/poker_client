// PlayerSeatVisualizerInterface.h
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PokerDataTypes.h" // Для FCard, TArray, FPlayerSeatData
#include "PlayerSeatVisualizerInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPlayerSeatVisualizerInterface : public UInterface { GENERATED_BODY() };

class POKER_CLIENT_API IPlayerSeatVisualizerInterface 
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Player Seat Visualizer")
    void UpdateHoleCards(const TArray<FCard>& HoleCards, bool bShowFaceToPlayer);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Player Seat Visualizer")
    void HideHoleCards();

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Player Seat Visualizer")
    void UpdatePlayerInfo(const FString& PlayerName, int64 PlayerStack);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Player Seat Visualizer")
    void SetSeatVisibility(bool bIsVisible);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Player Seat Visualizer")
    int32 GetSeatIndexRepresentation() const;
};