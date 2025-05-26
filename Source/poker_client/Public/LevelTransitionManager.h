#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/LatentActionManager.h" 
#include "LevelTransitionManager.generated.h"

class UMyGameInstance; 
class UUserWidget;
class UMediaPlayer;
class UMediaSource;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelTransitionFinished);

UCLASS(BlueprintType) 
class POKER_CLIENT_API ULevelTransitionManager : public UObject
{
    GENERATED_BODY()

public:
    ULevelTransitionManager();

    void Initialize(UMyGameInstance* InOwningGameInstance);

    UFUNCTION(BlueprintCallable, Category = "Level Transition")
    void StartLoadLevelWithVideo(
        FName LevelNameToLoad,
        TSubclassOf<UUserWidget> ScreensaverWidgetClass,
        UMediaPlayer* MediaPlayerToUse,
        UMediaSource* MediaSourceToUse,
        const FString& GameModeOverrideOptions = TEXT("")
    );

    UFUNCTION(BlueprintCallable, Category = "Level Transition")
    void NotifyLoadingVideoFinished();

    UPROPERTY(BlueprintAssignable, Category = "Level Transition")
    FOnLevelTransitionFinished OnLevelTransitionFinishedDelegate;

private:
    void OnLevelPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);
    void CheckAndFinalizeLevelTransition();
    FString AsyncLoadingResultToString(EAsyncLoadingResult::Type Result);

    UPROPERTY()
    TObjectPtr<UMyGameInstance> OwningGameInstance;

    UPROPERTY()
    TObjectPtr<UUserWidget> CurrentScreensaverWidget;

    UPROPERTY()
    TSubclassOf<UUserWidget> CurrentScreensaverWidgetClass; 

    UPROPERTY()
    TObjectPtr<UMediaPlayer> ActiveMediaPlayer;
    UPROPERTY()
    TObjectPtr<UMediaSource> ActiveMediaSource;

    FName LevelToLoadAsync;
    FString GameModeOptionsForNextLevel;
    bool bIsLevelLoadComplete;
    bool bIsLoadingVideoFinished;
};