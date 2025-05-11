#include "MenuScreenUIManager.h"
#include "LevelTransitionManager.h"
#include "MyGameInstance.h" // Для доступа к функциям GameInstance
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h" // Для CreateWidget

// Включите .h файлы ваших конкретных виджетов, если будете делать касты к ним
// #include "WBP_MainMenu_InGame.h" 
// #include "WBP_OfflineLobby_InGame.h"
// ...

UMenuScreenUIManager::UMenuScreenUIManager()
{
    OwningGameInstance = nullptr;
    MainMenuClass = nullptr;
    OfflineLobbyClass = nullptr;
    OnlineLobbyClass = nullptr;
    ProfileScreenClass = nullptr;
    SettingsClass = nullptr;

    MainMenuInstance = nullptr;
    OfflineLobbyInstance = nullptr;
    OnlineLobbyInstance = nullptr;
    ProfileScreenInstance = nullptr;
    SettingsInstance = nullptr;
    CurrentActiveMenuScreenWidget = nullptr;
}

void UMenuScreenUIManager::Initialize(
    UMyGameInstance* InGameInstance,
    TSubclassOf<UUserWidget> InMainMenuClass,
    TSubclassOf<UUserWidget> InOfflineLobbyClass,
    TSubclassOf<UUserWidget> InOnlineLobbyClass,
    TSubclassOf<UUserWidget> InProfileScreenClass,
    TSubclassOf<UUserWidget> InSettingsClass)
{
    OwningGameInstance = InGameInstance;
    MainMenuClass = InMainMenuClass;
    OfflineLobbyClass = InOfflineLobbyClass;
    OnlineLobbyClass = InOnlineLobbyClass;
    ProfileScreenClass = InProfileScreenClass;
    SettingsClass = InSettingsClass;

    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("UMenuScreenUIManager::Initialize - OwningGameInstance is null!"));
    }
}

void UMenuScreenUIManager::ChangeActiveMenuScreenWidget(TSubclassOf<UUserWidget> NewWidgetClassToShow, UUserWidget*& OutWidgetInstanceVariable, bool bIsFullscreenWidget /*= false*/)
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("ChangeActiveGameScreenWidget: OwningGameInstance is null.")); return; }
    if (!NewWidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ChangeActiveGameScreenWidget: NewWidgetClassToShow is null.")); return; }

    APlayerController* PC = OwningGameInstance->GetFirstLocalPlayerController();
    if (!PC) { UE_LOG(LogTemp, Error, TEXT("ChangeActiveGameScreenWidget: PlayerController is null.")); return; }

    if (CurrentActiveMenuScreenWidget && CurrentActiveMenuScreenWidget->IsInViewport())
    {
        CurrentActiveMenuScreenWidget->RemoveFromParent();
        UE_LOG(LogTemp, Log, TEXT("ChangeActiveGameScreenWidget: Removed previous widget: %s"), *CurrentActiveMenuScreenWidget->GetName());
    }
    CurrentActiveMenuScreenWidget = nullptr;

    // Создаем или получаем существующий экземпляр виджета
    // OutWidgetInstanceVariable здесь - это временный сырой указатель
    if (!OutWidgetInstanceVariable)
    {
        OutWidgetInstanceVariable = CreateWidget<UUserWidget>(PC, NewWidgetClassToShow);
    }

    if (OutWidgetInstanceVariable)
    {
        OutWidgetInstanceVariable->AddToViewport(0);
        CurrentActiveMenuScreenWidget = OutWidgetInstanceVariable; // CurrentActiveGameScreenWidget тоже сырой указатель
        UE_LOG(LogTemp, Log, TEXT("ChangeActiveGameScreenWidget: Displayed new widget: %s"), *NewWidgetClassToShow->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CurrentActiveMenuScreenWidget: Failed to create or find instance for %s"), *NewWidgetClassToShow->GetName());
    }
}

void UMenuScreenUIManager::ShowMainMenu()
{
    if (!MainMenuClass) { UE_LOG(LogTemp, Error, TEXT("ShowMainMenu: MainMenuClass is not set!")); return; }

    UUserWidget* RawPtrInstance = MainMenuInstance.Get();
    ChangeActiveMenuScreenWidget(MainMenuClass, RawPtrInstance);
    // Если ChangeActiveGameScreenWidget создал новый виджет, RawPtrInstance обновится.
    // Присваиваем его обратно нашему TObjectPtr.
    if (RawPtrInstance != MainMenuInstance.Get())
    {
        MainMenuInstance = RawPtrInstance;
    }
}

void UMenuScreenUIManager::ShowOfflineLobby()
{
    if (!OfflineLobbyClass) { UE_LOG(LogTemp, Error, TEXT("ShowOfflineLobby: OfflineLobbyClass is not set!")); return; }

    UUserWidget* RawPtrInstance = OfflineLobbyInstance.Get();
    ChangeActiveMenuScreenWidget(OfflineLobbyClass, RawPtrInstance);
    if (RawPtrInstance != OfflineLobbyInstance.Get())
    {
        OfflineLobbyInstance = RawPtrInstance;
    }
}

void UMenuScreenUIManager::ShowOnlineLobby()
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("ShowOnlineLobby: OwningGameInstance is null.")); return; }

    if (OwningGameInstance->bIsLoggedIn)
    {
        if (!OnlineLobbyClass) { UE_LOG(LogTemp, Error, TEXT("ShowOnlineLobby: OnlineLobbyClass is not set!")); return; }

        UUserWidget* RawPtrInstance = OnlineLobbyInstance.Get(); // Убедитесь, что имя переменной правильное
        ChangeActiveMenuScreenWidget(OnlineLobbyClass, RawPtrInstance);
        if (RawPtrInstance != OnlineLobbyInstance.Get())
        {
            OnlineLobbyInstance = RawPtrInstance;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowOnlineLobby: User not logged in. Cannot show Online Lobby."));
        ShowMainMenu(); // Возвращаемся в главное игровое меню
    }
}

void UMenuScreenUIManager::ShowProfileScreen()
{
    if (!ProfileScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowProfileScreen: ProfileScreenClass is not set!")); return; }

    UUserWidget* RawPtrInstance = ProfileScreenInstance.Get();
    ChangeActiveMenuScreenWidget(ProfileScreenClass, RawPtrInstance);
    if (RawPtrInstance != ProfileScreenInstance.Get())
    {
        ProfileScreenInstance = RawPtrInstance;
    }
}

void UMenuScreenUIManager::ShowSettings()
{
    if (!SettingsClass) { UE_LOG(LogTemp, Error, TEXT("ShowSettings: SettingsClass is not set!")); return; }

    UUserWidget* RawPtrInstance = SettingsInstance.Get();
    ChangeActiveMenuScreenWidget(SettingsClass, RawPtrInstance);
    if (RawPtrInstance != SettingsInstance.Get())
    {
        SettingsInstance = RawPtrInstance;
    }
}

void UMenuScreenUIManager::TriggerTransitionToGameLevel()
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("UMenuScreenUIManager::TriggerTransitionToGameLevel - OwningGameInstance is null!"));
        return;
    }
    ULevelTransitionManager* LTM = OwningGameInstance->GetLevelTransitionManager();
    if (!LTM)
    {
        UE_LOG(LogTemp, Error, TEXT("UMenuScreenUIManager::TriggerTransitionToGameLevel - LevelTransitionManager is null!"));
        return;
    }

    FString GameModeOptions = FString::Printf(TEXT("?Game=/Game/Blueprints/GameModes/BP_OfflineGameMode.BP_OfflineGameMode_C"));
    // ^^^ ЗАМЕНИТЕ ПУТЬ НА ВАШ ПРАВИЛЬНЫЙ ПУТЬ к BP_OfflineGameMode (или BP_OnlinePokerGameMode для онлайна)

    UE_LOG(LogTemp, Log, TEXT("UMenuScreenUIManager: Triggering transition to GameLevel using 'LoadingVideo' assets."));

    // Используем набор ассетов "LoadingVideo"
    TSubclassOf<UUserWidget> WidgetClass = OwningGameInstance->LoadingVideo_WidgetClass;
    UMediaPlayer* MediaPlayer = OwningGameInstance->LoadingVideo_MediaPlayer;
    UMediaSource* MediaSource = OwningGameInstance->LoadingVideo_MediaSource;

    if (!WidgetClass || !MediaPlayer || !MediaSource)
    {
        UE_LOG(LogTemp, Error, TEXT("UMenuScreenUIManager: Missing 'LoadingVideo' assets in GameInstance for Menu->Game transition. Using fallback or default if available."));
        // Опционально: можно попробовать использовать DefaultScreensaver...
        return; // Или просто выходим
    }

    LTM->StartLoadLevelWithVideo(
        FName("MainLevel"), // Или L_PokerTable, как называется ваш игровой уровень
        WidgetClass,
        MediaPlayer,
        MediaSource,
        GameModeOptions
    );
}