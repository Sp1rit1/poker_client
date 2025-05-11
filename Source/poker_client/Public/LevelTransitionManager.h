#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/LatentActionManager.h" // Для EAsyncLoadingResult
#include "LevelTransitionManager.generated.h"

class UMyGameInstance; // Прямое объявление
class UUserWidget;
class UMediaPlayer;
class UMediaSource;

// Делегат для уведомления о завершении перехода (опционально)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelTransitionFinished);

UCLASS(BlueprintType) // BlueprintType, если захотите его как-то использовать из BP напрямую
class POKER_CLIENT_API ULevelTransitionManager : public UObject
{
    GENERATED_BODY()

public:
    ULevelTransitionManager();

    /** Инициализирует менеджер */
    void Initialize(UMyGameInstance* InOwningGameInstance);

    /**
     * Запускает процесс загрузки нового уровня с показом виджета с видео.
     * @param LevelNameToLoad Имя уровня для загрузки (без префиксов пути).
     * @param ScreensaverWidgetClass Класс виджета экрана загрузки с видео.
     * @param MediaPlayerToUse Ассет MediaPlayer для экрана загрузки.
     * @param MediaSourceToUse Ассет MediaSource для экрана загрузки.
     * @param GameModeOverrideOptions Опции для OpenLevel, например, для смены GameMode ("?Game=/Path/To/BP_MyGameMode.BP_MyGameMode_C"). Пусто, если GameMode не меняется.
     */
    UFUNCTION(BlueprintCallable, Category = "Level Transition")
    void StartLoadLevelWithVideo(
        FName LevelNameToLoad,
        TSubclassOf<UUserWidget> ScreensaverWidgetClass,
        UMediaPlayer* MediaPlayerToUse,
        UMediaSource* MediaSourceToUse,
        const FString& GameModeOverrideOptions = TEXT("")
    );

    /** Вызывается из Blueprint виджета экрана загрузки, когда видео завершило проигрывание. */
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
    TSubclassOf<UUserWidget> CurrentScreensaverWidgetClass; // Храним класс для рефлексии

    // Ассеты, используемые для ТЕКУЩЕГО перехода
    UPROPERTY()
    TObjectPtr<UMediaPlayer> ActiveMediaPlayer;
    UPROPERTY()
    TObjectPtr<UMediaSource> ActiveMediaSource;

    FName LevelToLoadAsync;
    FString GameModeOptionsForNextLevel;
    bool bIsLevelLoadComplete;
    bool bIsLoadingVideoFinished;
};