#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PokerDataTypes.h"       
#include "PokerHandEvaluator.h"    
#include "PokerBotAI.generated.h"

class UOfflinePokerGameState;

UENUM(BlueprintType)
enum class EPlayerPokerPosition : uint8
{
    BTN         UMETA(DisplayName = "Button (BTN)"),
    SB          UMETA(DisplayName = "Small Blind (SB)"),
    BB          UMETA(DisplayName = "Big Blind (BB)"),
    UTG         UMETA(DisplayName = "Under The Gun (UTG)"), 
    UTG1        UMETA(DisplayName = "UTG+1"),
    MP1         UMETA(DisplayName = "Middle Position 1 (MP1)"), 
    MP2         UMETA(DisplayName = "Middle Position 2 (MP2)"),
    HJ          UMETA(DisplayName = "Hijack (HJ)"),           
    CO          UMETA(DisplayName = "Cutoff (CO)"),           
    Unknown     UMETA(DisplayName = "Unknown")               
};

UCLASS()
class POKER_CLIENT_API UPokerBotAI : public UObject 
{
    GENERATED_BODY()

public:


    UPokerBotAI();

    void SetPersonalityFactors(const FBotPersonalitySettings& Settings);

    // Флаг для переключения на детерминированное поведение в тестах
    bool bIsTesting = false;
    // Значение, которое будет возвращать FRand() в тестовом режиме
    float TestFixedRandValue = 0.5f;

    // --- ПАРАМЕТРЫ "ЛИЧНОСТИ" БОТА ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float AggressivenessFactor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float BluffFrequency;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot Personality", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float TightnessFactor;

    EPlayerAction GetBestAction(
        const UOfflinePokerGameState* GameState,
        const FPlayerSeatData& BotPlayerSeatData,
        const TArray<EPlayerAction>& AllowedActions,
        int64 CurrentBetToCallOnTable,
        int64 MinValidPureRaiseAmount,
        int64& OutDecisionAmount
    );

    virtual EPlayerPokerPosition GetPlayerPosition(const UOfflinePokerGameState* GameState, int32 BotSeatIndex, int32 NumActivePlayers) const;

    virtual float CalculatePreflopHandStrength(const FCard& Card1, const FCard& Card2, EPlayerPokerPosition BotPosition, int32 NumActivePlayers) const;

    virtual int64 CalculateBetSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, float CalculatedHandStrength, bool bIsBluff = false, float PotFractionOverride = 0.0f) const;

    virtual int64 CalculateRaiseSize(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, int64 CurrentBetToCallOnTable, int64 MinValidPureRaiseAmount, float CalculatedHandStrength, bool bIsBluff = false, float PotFractionOverride = 0.0f) const;

    virtual float GetScoreForMadeHand(EPokerHandRank HandRank) const;

protected:



    virtual FPokerHandResult EvaluateCurrentMadeHand(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards) const;

    virtual float CalculateDrawStrength(const TArray<FCard>& HoleCards, const TArray<FCard>& CommunityCards, int64 PotSize, int64 AmountToCallIfAny) const;

    virtual bool ShouldAttemptBluff(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData, EPlayerPokerPosition BotPosition, int32 NumOpponentsStillInHand) const;

    virtual int32 CountActiveOpponents(const UOfflinePokerGameState* GameState, int32 BotSeatIndex) const;

    virtual TArray<FPlayerSeatData> GetActiveOpponentData(const UOfflinePokerGameState* GameState, int32 BotSeatIndex) const;

    virtual bool bIsOpenRaiserSituation(const UOfflinePokerGameState* GameState, const FPlayerSeatData& BotPlayerSeatData) const;

};