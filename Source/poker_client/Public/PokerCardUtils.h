#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PokerDataTypes.h" // Убедитесь, что FCard здесь
#include "PokerCardUtils.generated.h"

UCLASS()
class POKER_CLIENT_API UPokerCardUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Poker|Card Utils", meta = (DisplayName = "To String (Card)", CompactNodeTitle = "->S"))
    static FString Conv_CardToString(const FCard& Card);
};