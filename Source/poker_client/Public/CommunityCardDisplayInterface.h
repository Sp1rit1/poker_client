#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PokerDataTypes.h" 
#include "CommunityCardDisplayInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UCommunityCardDisplayInterface : public UInterface
{
    GENERATED_BODY()
};


class POKER_CLIENT_API ICommunityCardDisplayInterface 
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Community Card Display")
    void UpdateCommunityCards(const TArray<FCard>& CommunityCards);

    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Community Card Display")
    void HideCommunityCards();
};