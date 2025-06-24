// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NetworkDataTypes.generated.h"


USTRUCT(BlueprintType)
struct POKER_CLIENT_API FPlayerFriendInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Friend Info")
    int64 UserId;

    UPROPERTY(BlueprintReadOnly, Category = "Friend Info")
    FString Username;

    UPROPERTY(BlueprintReadOnly, Category = "Friend Info")
    FString FriendCode;

    FPlayerFriendInfo() : UserId(-1), Username(TEXT("")), FriendCode(TEXT("")) {}
};

USTRUCT(BlueprintType)
struct POKER_CLIENT_API FPlayerStatsInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Player Stats")
    int32 HandsPlayed;

    UPROPERTY(BlueprintReadOnly, Category = "Player Stats")
    int32 HandsWon;

    // Добавьте сюда другие поля статистики, соответствующие UserStatsDto
    // UPROPERTY(BlueprintReadOnly, Category = "Player Stats")
    // float TotalWinnings; // Если используете float или FString для BigDecimal

    FPlayerStatsInfo() : HandsPlayed(0), HandsWon(0) /*, TotalWinnings(0.0f)*/ {}
};