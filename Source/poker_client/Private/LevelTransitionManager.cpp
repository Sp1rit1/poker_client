#include "LevelTransitionManager.h"
#include "MyGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h" // Для FindField
#include "UObject/UnrealType.h"     // Для FProperty

ULevelTransitionManager::ULevelTransitionManager()
{
    OwningGameInstance = nullptr;
    CurrentScreensaverWidget = nullptr;
    CurrentScreensaverWidgetClass = nullptr;
    ActiveMediaPlayer = nullptr;
    ActiveMediaSource = nullptr;
    LevelToLoadAsync = NAME_None;
    GameModeOptionsForNextLevel = TEXT("");
    bIsLevelLoadComplete = false;
    bIsLoadingVideoFinished = false;
}

void ULevelTransitionManager::Initialize(UMyGameInstance* InOwningGameInstance)
{
    OwningGameInstance = InOwningGameInstance;
}

void ULevelTransitionManager::StartLoadLevelWithVideo(
    FName InLevelNameToLoad,
    TSubclassOf<UUserWidget> InScreensaverWidgetClass,
    UMediaPlayer* InMediaPlayerToUse,
    UMediaSource* InMediaSourceToUse,
    const FString& InGameModeOverrideOptions)
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: OwningGameInstance is null.")); return; }
    if (!InScreensaverWidgetClass) { UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: ScreensaverWidgetClass is not set!")); return; }
    if (InLevelNameToLoad.IsNone()) { UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: LevelNameToLoad is None!")); return; }
    if (!InMediaPlayerToUse) { UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: MediaPlayerToUse is not set!")); return; }
    if (!InMediaSourceToUse) { UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: MediaSourceToUse is not set!")); return; }

    UE_LOG(LogTemp, Log, TEXT("ULevelTransitionManager: Starting transition to level '%s' with video."), *InLevelNameToLoad.ToString());

    APlayerController* PC = OwningGameInstance->GetFirstLocalPlayerController();
    if (!PC) { UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: PlayerController is null.")); return; }

    // Удаляем предыдущий виджет заставки, если он был
    if (CurrentScreensaverWidget && CurrentScreensaverWidget->IsInViewport())
    {
        CurrentScreensaverWidget->RemoveFromParent();
    }

    CurrentScreensaverWidget = CreateWidget<UUserWidget>(PC, InScreensaverWidgetClass);
    CurrentScreensaverWidgetClass = InScreensaverWidgetClass; // Сохраняем класс для рефлексии
    ActiveMediaPlayer = InMediaPlayerToUse;
    ActiveMediaSource = InMediaSourceToUse;

    if (!CurrentScreensaverWidget) {
        UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: Failed to create ScreensaverWidget! Fallback OpenLevel."));
        UGameplayStatics::OpenLevel(OwningGameInstance, InLevelNameToLoad, true, InGameModeOverrideOptions);
        return;
    }

    OwningGameInstance->ApplyWindowMode(true); // Полноэкранный для заставки
    OwningGameInstance->SetupInputMode(true, false); // UIOnly, мышь не видна
    CurrentScreensaverWidget->AddToViewport(100); // Большой ZOrder, чтобы быть поверх всего

    LevelToLoadAsync = InLevelNameToLoad;
    GameModeOptionsForNextLevel = InGameModeOverrideOptions;
    bIsLevelLoadComplete = false;
    bIsLoadingVideoFinished = false;

    // Передача ассетов в виджет через рефлексию
    UClass* ActualWidgetClass = CurrentScreensaverWidget->GetClass();
    if (ActualWidgetClass && ActualWidgetClass->IsChildOf(CurrentScreensaverWidgetClass))
    {
        FProperty* MediaPlayerBaseProp = ActualWidgetClass->FindPropertyByName(FName("ScreensaverMediaPlayer"));
        FObjectProperty* MediaPlayerProp = CastField<FObjectProperty>(MediaPlayerBaseProp);
        if (MediaPlayerProp) { MediaPlayerProp->SetObjectPropertyValue_InContainer(CurrentScreensaverWidget.Get(), ActiveMediaPlayer.Get()); }
        else { UE_LOG(LogTemp, Warning, TEXT("ULevelTransitionManager: Could not find/cast 'ScreensaverMediaPlayer' property in widget.")); }

        FProperty* MediaSourceBaseProp = ActualWidgetClass->FindPropertyByName(FName("ScreensaverMediaSource"));
        FObjectProperty* MediaSourceProp = CastField<FObjectProperty>(MediaSourceBaseProp);
        if (MediaSourceProp) { MediaSourceProp->SetObjectPropertyValue_InContainer(CurrentScreensaverWidget.Get(), ActiveMediaSource.Get()); }
        else { UE_LOG(LogTemp, Warning, TEXT("ULevelTransitionManager: Could not find/cast 'ScreensaverMediaSource' property in widget.")); }

        // Вызываем функцию инициализации медиа в виджете (если она есть)
        FName InitFuncName = FName(TEXT("InitializeAndPlayMedia")); // Имя вашей функции в WBP_Screensaver
        UFunction* InitFunction = ActualWidgetClass->FindFunctionByName(InitFuncName);
        if (InitFunction)
        {
            CurrentScreensaverWidget->ProcessEvent(InitFunction, nullptr);
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("ULevelTransitionManager: Could not find InitializeAndPlayMedia function in widget %s"), *ActualWidgetClass->GetName());
        }
    }
    else { UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: Widget class mismatch or null!")); }

    FString PackagePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelToLoadAsync.ToString());
    FLoadPackageAsyncDelegate LoadCallback = FLoadPackageAsyncDelegate::CreateUObject(this, &ULevelTransitionManager::OnLevelPackageLoaded);
    LoadPackageAsync(PackagePath, LoadCallback, 0, PKG_ContainsMap);
}

void ULevelTransitionManager::NotifyLoadingVideoFinished()
{
    bIsLoadingVideoFinished = true;
    CheckAndFinalizeLevelTransition();
}

void ULevelTransitionManager::OnLevelPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
    UE_LOG(LogTemp, Log, TEXT("ULevelTransitionManager::OnLevelPackageLoaded: Package='%s', Result=%s"), *PackageName.ToString(), *AsyncLoadingResultToString(Result));
    if (Result != EAsyncLoadingResult::Succeeded) {
        UE_LOG(LogTemp, Error, TEXT("ULevelTransitionManager: Async package load FAILED for '%s'!"), *PackageName.ToString());
    }
    bIsLevelLoadComplete = true;
    CheckAndFinalizeLevelTransition();
}

void ULevelTransitionManager::CheckAndFinalizeLevelTransition()
{
    if (!OwningGameInstance) { return; }
    if (bIsLevelLoadComplete && bIsLoadingVideoFinished)
    {
        UE_LOG(LogTemp, Log, TEXT("ULevelTransitionManager: Conditions MET. Finalizing transition to %s!"), *LevelToLoadAsync.ToString());
        if (CurrentScreensaverWidget)
        {
            CurrentScreensaverWidget->RemoveFromParent();
            CurrentScreensaverWidget = nullptr;
        }
        ActiveMediaPlayer = nullptr; // Сбрасываем активные ассеты
        ActiveMediaSource = nullptr;
        CurrentScreensaverWidgetClass = nullptr;

        FName LevelToOpen = LevelToLoadAsync;
        FString Options = GameModeOptionsForNextLevel;

        LevelToLoadAsync = NAME_None;
        GameModeOptionsForNextLevel = TEXT("");
        bIsLevelLoadComplete = false;
        bIsLoadingVideoFinished = false;

        if (!LevelToOpen.IsNone())
        {
            UGameplayStatics::OpenLevel(OwningGameInstance, LevelToOpen, true, Options);
        }
        OnLevelTransitionFinishedDelegate.Broadcast(); // Уведомляем о завершении
    }
}

FString ULevelTransitionManager::AsyncLoadingResultToString(EAsyncLoadingResult::Type Result)
{
    switch (Result)
    {
    case EAsyncLoadingResult::Succeeded: return TEXT("Succeeded");
    case EAsyncLoadingResult::Failed:    return TEXT("Failed");
    case EAsyncLoadingResult::Canceled:  return TEXT("Canceled");
    default: return FString::Printf(TEXT("Unknown (%d)"), static_cast<int32>(Result));
    }
    // Строка return TEXT("Unknown"); здесь больше не нужна, так как default покрывает все
}