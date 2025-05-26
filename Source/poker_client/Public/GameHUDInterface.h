#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PokerDataTypes.h" 
#include "GameHUDInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable) 
class UGameHUDInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Интерфейс для взаимодействия с игровым HUD.
 */
class POKER_CLIENT_API IGameHUDInterface 
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Update")
    void UpdateGameInfo(
        const FString& MovingPlayerName,        
        int64 CurrentPot,                       
        int64 LocalPlayerActualStack,           
        int64 LocalPlayerCurrentBetInRound,     
        int64 TotalBetToCallOnTableForLocal,    
        int64 MinPureRaiseAmountOnTableForLocal 
    );

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Update")
    void UpdateActionButtons(const TArray<EPlayerAction>& AllowedActions);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Controls")
    void DisableButtons();

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface")
    void AddGameHistoryMessage(const FString& Message);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Notifications")
    void ShowNotificationMessage(const FString& Message, float Duration = 3.0f);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Game Events")
    void UpdateCommunityCardsDisplay(const TArray<FCard>& CommunityCards);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HUD Interface|Game Events")
    void DisplayShowdownResults(const TArray<FShowdownPlayerInfo>& ShowdownPlayerResults, const FString& WinnerAnnouncement);

};