#include "StartScreenUIManager.h"
#include "MyGameInstance.h" // Для доступа к функциям GameInstance
#include "LevelTransitionManager.h"
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
    CurrentTopLevelWidget = nullptr;
}

void UStartScreenUIManager::Initialize(
    UMyGameInstance* InGameInstance,
    TSubclassOf<UUserWidget> InStartScreenClass,
    TSubclassOf<UUserWidget> InLoginScreenClass,
    TSubclassOf<UUserWidget> InRegisterScreenClass
)

{
    OwningGameInstance = InGameInstance;
    StartScreenClass = InStartScreenClass;
    LoginScreenClass = InLoginScreenClass;
    RegisterScreenClass = InRegisterScreenClass;

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


void UStartScreenUIManager::TriggerTransitionToMenuLevel() // Пример новой функции
{
    if (!OwningGameInstance) return;
    ULevelTransitionManager* LTM = OwningGameInstance->GetLevelTransitionManager();
    if (LTM)
    {
        // Используем дефолтные ассеты из GameInstance
        LTM->StartLoadLevelWithVideo(
            FName("MenuLevel"), // Имя вашего уровня меню
            OwningGameInstance->DefaultScreensaverWidgetClass,
            OwningGameInstance->DefaultScreensaverMediaPlayer,
            OwningGameInstance->DefaultScreensaverMediaSource,
            TEXT("") // GameMode для MenuLevel будет установлен в его World Settings
        );
    }
}

void UStartScreenUIManager::TransitionToMainLevel(FName LevelName)
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("UStartScreenUIManager::TransitionToMainLevel - OwningGameInstance is null!"));
        return;
    }

    ULevelTransitionManager* LTM = OwningGameInstance->GetLevelTransitionManager();
    if (LTM)
    {
        UE_LOG(LogTemp, Log, TEXT("UStartScreenUIManager: Initiating transition to level '%s' via LevelTransitionManager."), *LevelName.ToString());

        // Получаем ассеты заставки из GameInstance
        TSubclassOf<UUserWidget> ScreensaverWidget = OwningGameInstance->DefaultScreensaverWidgetClass;
        UMediaPlayer* MediaPlayer = OwningGameInstance->DefaultScreensaverMediaPlayer;
        UMediaSource* MediaSource = OwningGameInstance->DefaultScreensaverMediaSource;

        if (!ScreensaverWidget || !MediaPlayer || !MediaSource)
        {
            UE_LOG(LogTemp, Error, TEXT("UStartScreenUIManager::TransitionToMainLevel - One or more screensaver assets are not set in GameInstance! Fallback to simple OpenLevel."));
            UGameplayStatics::OpenLevel(OwningGameInstance, LevelName); // Простой переход без видео
            return;
        }

        // Для перехода на MainLevel, где будет меню на стене, мы используем BP_MenuGameMode.
        // Этот GameMode должен быть назначен по умолчанию для MainLevel в его World Settings.
        // Поэтому GameModeOverrideOptions оставляем пустым.
        LTM->StartLoadLevelWithVideo(
            LevelName,
            ScreensaverWidget,
            MediaPlayer,
            MediaSource,
            TEXT("") // Пустые опции, GameMode уровня MainLevel сработает по умолчанию
        );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("UStartScreenUIManager::TransitionToMainLevel - LevelTransitionManager is null! Cannot transition."));
    }
}