#include "StartScreenUIManager.h"
#include "MyGameInstance.h" // Для доступа к функциям GameInstance

// Инклуды из вашего MyGameInstance.cpp, необходимые для этой логики
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h" // Для LoadPackageAsync и UPackage
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "UObject/UObjectGlobals.h" // Для FindField (если используется)
#include "UObject/UnrealType.h"     // Для FProperty, FObjectProperty (если используется)


UStartScreenUIManager::UStartScreenUIManager()
{
    OwningGameInstance = nullptr;
    StartScreenClass = nullptr;
    LoginScreenClass = nullptr;
    RegisterScreenClass = nullptr;
    ScreensaverClass = nullptr;
    ScreensaverMediaPlayerAsset = nullptr;
    ScreensaverMediaSourceAsset = nullptr;
    CurrentTopLevelWidget = nullptr;
    LevelToLoadAsync = NAME_None;
    bIsLevelLoadComplete = false;
    bIsScreensaverVideoFinished = false;
}

void UStartScreenUIManager::Initialize(
    UMyGameInstance* InGameInstance,
    TSubclassOf<UUserWidget> InStartScreenClass,
    TSubclassOf<UUserWidget> InLoginScreenClass,
    TSubclassOf<UUserWidget> InRegisterScreenClass,
    TSubclassOf<UUserWidget> InScreensaverClass,
    UMediaPlayer* InScreensaverMediaPlayerAsset,
    UMediaSource* InScreensaverMediaSourceAsset)
{
    OwningGameInstance = InGameInstance;
    StartScreenClass = InStartScreenClass;
    LoginScreenClass = InLoginScreenClass;
    RegisterScreenClass = InRegisterScreenClass;
    ScreensaverClass = InScreensaverClass;
    ScreensaverMediaPlayerAsset = InScreensaverMediaPlayerAsset;
    ScreensaverMediaSourceAsset = InScreensaverMediaSourceAsset;

    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("UStartScreenUIManager::Initialize - OwningGameInstance is null!"));
    }
    // Добавьте проверки для остальных параметров, если нужно
}

template <typename T>
T* UStartScreenUIManager::ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget)
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("ShowWidget: OwningGameInstance is null. Cannot proceed."));
        return nullptr;
    }

    APlayerController* PC = OwningGameInstance->GetFirstLocalPlayerController(); // Используем метод из GameInstance
    if (!PC) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: PlayerController is null.")); return nullptr; }
    if (!WidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: WidgetClassToShow is null.")); return nullptr; }

    // --- Шаг 1: Удаляем старый виджет ---
    if (CurrentTopLevelWidget)
    {
        UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous widget: %s"), *CurrentTopLevelWidget->GetName());
        CurrentTopLevelWidget->RemoveFromParent();
    }
    CurrentTopLevelWidget = nullptr;

    // --- Шаг 2: Меняем режим окна и ввода ---
    // Используем флаг bIsInitialWindowSetupComplete из GameInstance
    if (!OwningGameInstance->bIsInitialWindowSetupComplete || bIsFullscreenWidget)
    {
        OwningGameInstance->ApplyWindowMode(bIsFullscreenWidget);
        UE_LOG(LogTemp, Log, TEXT("ShowWidget: Applied window mode (Fullscreen: %s OR Initial setup not complete)"), bIsFullscreenWidget ? TEXT("True") : TEXT("False"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("ShowWidget: Skipping ApplyWindowMode for initial windowed widget because initial setup is complete."));
    }
    OwningGameInstance->SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget);

    // --- Шаг 3: Создаем и показываем новый виджет ---
    // Кастуем PC к UUserWidget, чтобы передать его как Outer, но лучше передавать PC
    T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
    if (NewWidget)
    {
        NewWidget->AddToViewport();
        CurrentTopLevelWidget = NewWidget;
        UE_LOG(LogTemp, Log, TEXT("ShowWidget: Added new widget: %s"), *WidgetClassToShow->GetName());
        return NewWidget;
    }
    else { UE_LOG(LogTemp, Error, TEXT("ShowWidget: Failed to create widget: %s"), *WidgetClassToShow->GetName()); return nullptr; }
}

void UStartScreenUIManager::ShowStartScreen()
{
    if (!StartScreenClass) {
        UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: StartScreenClass is not set!"));
        return;
    }
    ShowWidget<UUserWidget>(StartScreenClass, false); // Стартовый экран всегда в оконном режиме
}

void UStartScreenUIManager::ShowLoginScreen()
{
    if (!LoginScreenClass) {
        UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: LoginScreenClass is not set!"));
        return;
    }
    ShowWidget<UUserWidget>(LoginScreenClass, false);
}

void UStartScreenUIManager::ShowRegisterScreen()
{
    if (!RegisterScreenClass) {
        UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: RegisterScreenClass is not set!"));
        return;
    }
    ShowWidget<UUserWidget>(RegisterScreenClass, false);
}

void UStartScreenUIManager::StartLoadLevelWithScreensaverVideoWidget(FName LevelName)
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithScreensaverVideoWidget: OwningGameInstance is null.")); return; }

    UE_LOG(LogTemp, Log, TEXT(">>> UStartScreenUIManager::StartLoadLevelWithScreensaverVideoWidget: ENTERING. LevelName='%s'"), *LevelName.ToString());

    if (!ScreensaverClass) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithScreensaverVideoWidget: ScreensaverClass is not set!")); return; }
    if (LevelName.IsNone()) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithScreensaverVideoWidget: LevelName is None!")); return; }
    if (!ScreensaverMediaPlayerAsset) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithScreensaverVideoWidget: ScreensaverMediaPlayerAsset is not set!")); return; }
    if (!ScreensaverMediaSourceAsset) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithScreensaverVideoWidget: ScreensaverMediaSourceAsset is not set!")); return; }

    UE_LOG(LogTemp, Log, TEXT("StartLoadLevelWithScreensaverVideoWidget: Showing Screensaver widget '%s'..."), *ScreensaverClass->GetName());
    UUserWidget* ScreensaverWidgetInstance = ShowWidget<UUserWidget>(ScreensaverClass, true); // Экран загрузки полноэкранный
    if (!ScreensaverWidgetInstance) {
        UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithScreensaverVideoWidget: Failed to create/show ScreensaverWidget! Fallback OpenLevel."));
        UGameplayStatics::OpenLevel(OwningGameInstance, LevelName); // Используем OwningGameInstance как WorldContext
        return;
    }

    LevelToLoadAsync = LevelName;
    bIsLevelLoadComplete = false;
    bIsScreensaverVideoFinished = false;
    UE_LOG(LogTemp, Log, TEXT("StartLoadLevelWithScreensaverVideoWidget: State reset. LevelToLoad='%s'"), *LevelToLoadAsync.ToString());

    // Настройка виджета (передача ссылок на плеер/источник через рефлексию, как было у вас)
    // Этот код взят из вашего MyGameInstance.cpp и предполагает, что имена свойств в виджете такие же.
    UClass* ActualWidgetClass = ScreensaverWidgetInstance->GetClass();
    if (ActualWidgetClass && ActualWidgetClass->IsChildOf(ScreensaverClass))
    {
        FProperty* MediaPlayerBaseProp = ActualWidgetClass->FindPropertyByName(FName("ScreensaverMediaPlayer"));
        FObjectProperty* MediaPlayerProp = CastField<FObjectProperty>(MediaPlayerBaseProp);
        if (MediaPlayerProp) { MediaPlayerProp->SetObjectPropertyValue_InContainer(ScreensaverWidgetInstance, ScreensaverMediaPlayerAsset); }
        else { UE_LOG(LogTemp, Warning, TEXT("StartLoadLevelWithScreensaverVideoWidget: Could not find/cast 'ScreensaverMediaPlayer' property in widget.")); }

        FProperty* MediaSourceBaseProp = ActualWidgetClass->FindPropertyByName(FName("ScreensaverMediaSource"));
        FObjectProperty* MediaSourceProp = CastField<FObjectProperty>(MediaSourceBaseProp);
        if (MediaSourceProp) { MediaSourceProp->SetObjectPropertyValue_InContainer(ScreensaverWidgetInstance, ScreensaverMediaSourceAsset); }
        else { UE_LOG(LogTemp, Warning, TEXT("StartLoadLevelWithScreensaverVideoWidget: Could not find/cast 'ScreensaverMediaSource' property in widget.")); }
    }
    else { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithScreensaverVideoWidget: Widget class mismatch or null!")); }


    FString PackagePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelName.ToString()); // Убедитесь, что путь к картам правильный
    UE_LOG(LogTemp, Log, TEXT("StartLoadLevelWithScreensaverVideoWidget: Requesting async load for package '%s'..."), *PackagePath);
    FLoadPackageAsyncDelegate LoadCallback = FLoadPackageAsyncDelegate::CreateUObject(this, &UStartScreenUIManager::OnLevelPackageLoaded);
    if (LoadCallback.IsBound()) {
        LoadPackageAsync(PackagePath, LoadCallback, 0, PKG_ContainsMap);
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithScreensaverVideoWidget: Failed to bind LoadPackageAsync delegate!"));
        CheckAndFinalizeLevelTransition(); // Попытка завершить, если биндинг не удался
    }

    UE_LOG(LogTemp, Log, TEXT("<<< UStartScreenUIManager::StartLoadLevelWithScreensaverVideoWidget: EXITING function."));
}

void UStartScreenUIManager::OnLevelPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
    UE_LOG(LogTemp, Log, TEXT(">>> UStartScreenUIManager::OnLevelPackageLoaded: ENTERING (Callback). Package='%s', Result=%s"), *PackageName.ToString(), *AsyncLoadingResultToString(Result));
    if (Result != EAsyncLoadingResult::Succeeded) {
        UE_LOG(LogTemp, Error, TEXT("OnLevelPackageLoaded: Async package load FAILED for '%s'!"), *PackageName.ToString());
    }
    else {
        UE_LOG(LogTemp, Log, TEXT("OnLevelPackageLoaded: Async package load SUCCEEDED for '%s'."), *PackageName.ToString());
    }
    bIsLevelLoadComplete = true;
    CheckAndFinalizeLevelTransition();
    UE_LOG(LogTemp, Log, TEXT("<<< UStartScreenUIManager::OnLevelPackageLoaded: EXITING function."));
}

void UStartScreenUIManager::NotifyScreensaverVideoFinished()
{
    UE_LOG(LogTemp, Log, TEXT(">>> UStartScreenUIManager::NotifyScreensaverVideoFinished: ENTERING (Called by Widget)."));
    bIsScreensaverVideoFinished = true;
    CheckAndFinalizeLevelTransition();
    UE_LOG(LogTemp, Log, TEXT("<<< UStartScreenUIManager::NotifyScreensaverVideoFinished: EXITING."));
}

void UStartScreenUIManager::CheckAndFinalizeLevelTransition()
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("CheckAndFinalizeLevelTransition: OwningGameInstance is null.")); return; }

    UE_LOG(LogTemp, Verbose, TEXT(">>> UStartScreenUIManager::CheckAndFinalizeLevelTransition: Checking: LevelLoadComplete=%s, VideoFinished=%s"),
        bIsLevelLoadComplete ? TEXT("True") : TEXT("False"), bIsScreensaverVideoFinished ? TEXT("True") : TEXT("False"));

    if (bIsLevelLoadComplete && bIsScreensaverVideoFinished)
    {
        UE_LOG(LogTemp, Log, TEXT("CheckAndFinalizeLevelTransition: ---> Conditions MET. Finalizing transition!"));

        FName LevelToOpen = LevelToLoadAsync; // Сохраняем перед сбросом

        if (CurrentTopLevelWidget != nullptr)
        {
            UE_LOG(LogTemp, Log, TEXT("CheckAndFinalizeLevelTransition: Removing Screensaver widget: %s"), *CurrentTopLevelWidget->GetName());
            CurrentTopLevelWidget->RemoveFromParent();
            CurrentTopLevelWidget = nullptr;
        }

        LevelToLoadAsync = NAME_None;
        bIsLevelLoadComplete = false;
        bIsScreensaverVideoFinished = false;
        UE_LOG(LogTemp, Log, TEXT("CheckAndFinalizeLevelTransition: State flags reset."));

        if (!LevelToOpen.IsNone())
        {
            UE_LOG(LogTemp, Log, TEXT("CheckAndFinalizeLevelTransition: Calling OpenLevel for '%s'..."), *LevelToOpen.ToString());
            UGameplayStatics::OpenLevel(OwningGameInstance, LevelToOpen); // Используем OwningGameInstance как WorldContext
        }
        else { UE_LOG(LogTemp, Error, TEXT("CheckAndFinalizeLevelTransition: LevelToOpen is None!")); }
    }
    else { UE_LOG(LogTemp, Verbose, TEXT("CheckAndFinalizeLevelTransition: ---> Conditions NOT MET. Waiting...")); }
    UE_LOG(LogTemp, Verbose, TEXT("<<< UStartScreenUIManager::CheckAndFinalizeLevelTransition: EXITING."));
}

FString UStartScreenUIManager::AsyncLoadingResultToString(EAsyncLoadingResult::Type Result)
{
    // Эта функция была в вашем MyGameInstance.cpp, я ее просто скопировал
    switch (Result)
    {
    case EAsyncLoadingResult::Succeeded: return TEXT("Succeeded");
    case EAsyncLoadingResult::Failed:    return TEXT("Failed");
    case EAsyncLoadingResult::Canceled:  return TEXT("Canceled");
    default: return FString::Printf(TEXT("Unknown (%d)"), static_cast<int32>(Result));
    }
}