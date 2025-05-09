#include "GameScreenUIManager.h"
#include "MyGameInstance.h" // Для доступа к функциям GameInstance
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h" // Для CreateWidget

// Включите .h файлы ваших конкретных виджетов, если будете делать касты к ним
// #include "WBP_MainMenu_InGame.h" 
// #include "WBP_OfflineLobby_InGame.h"
// ...

UGameScreenUIManager::UGameScreenUIManager()
{
    OwningGameInstance = nullptr;
    MainMenu_InGame_Class = nullptr;
    OfflineLobby_InGame_Class = nullptr;
    OnlineLobby_InGame_Class = nullptr;
    ProfileScreen_InGame_Class = nullptr;
    Settings_InGame_Class = nullptr;

    MainMenu_InGame_Instance = nullptr;
    OfflineLobby_InGame_Instance = nullptr;
    OnlineLobby_InGame_Instance = nullptr;
    ProfileScreen_InGame_Instance = nullptr;
    Settings_InGame_Instance = nullptr;
    CurrentActiveGameScreenWidget = nullptr;
}

void UGameScreenUIManager::Initialize(
    UMyGameInstance* InGameInstance,
    TSubclassOf<UUserWidget> InMainMenuClass,
    TSubclassOf<UUserWidget> InOfflineLobbyClass,
    TSubclassOf<UUserWidget> InOnlineLobbyClass,
    TSubclassOf<UUserWidget> InProfileScreenClass,
    TSubclassOf<UUserWidget> InSettingsClass)
{
    OwningGameInstance = InGameInstance;
    MainMenu_InGame_Class = InMainMenuClass;
    OfflineLobby_InGame_Class = InOfflineLobbyClass;
    OnlineLobby_InGame_Class = InOnlineLobbyClass;
    ProfileScreen_InGame_Class = InProfileScreenClass;
    Settings_InGame_Class = InSettingsClass;

    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("UGameScreenUIManager::Initialize - OwningGameInstance is null!"));
    }
}

void UGameScreenUIManager::ChangeActiveGameScreenWidget(TSubclassOf<UUserWidget> NewWidgetClassToShow, UUserWidget*& OutWidgetInstanceVariable, bool bIsFullscreenWidget /*= false*/)
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("ChangeActiveGameScreenWidget: OwningGameInstance is null.")); return; }
    if (!NewWidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ChangeActiveGameScreenWidget: NewWidgetClassToShow is null.")); return; }

    APlayerController* PC = OwningGameInstance->GetFirstLocalPlayerController();
    if (!PC) { UE_LOG(LogTemp, Error, TEXT("ChangeActiveGameScreenWidget: PlayerController is null.")); return; }

    if (CurrentActiveGameScreenWidget && CurrentActiveGameScreenWidget->IsInViewport())
    {
        CurrentActiveGameScreenWidget->RemoveFromParent();
        UE_LOG(LogTemp, Log, TEXT("ChangeActiveGameScreenWidget: Removed previous widget: %s"), *CurrentActiveGameScreenWidget->GetName());
    }
    CurrentActiveGameScreenWidget = nullptr;

    OwningGameInstance->SetupInputMode(true, true);

    // Создаем или получаем существующий экземпляр виджета
    // OutWidgetInstanceVariable здесь - это временный сырой указатель
    if (!OutWidgetInstanceVariable)
    {
        OutWidgetInstanceVariable = CreateWidget<UUserWidget>(PC, NewWidgetClassToShow);
    }

    if (OutWidgetInstanceVariable)
    {
        OutWidgetInstanceVariable->AddToViewport(0);
        CurrentActiveGameScreenWidget = OutWidgetInstanceVariable; // CurrentActiveGameScreenWidget тоже сырой указатель
        UE_LOG(LogTemp, Log, TEXT("ChangeActiveGameScreenWidget: Displayed new widget: %s"), *NewWidgetClassToShow->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ChangeActiveGameScreenWidget: Failed to create or find instance for %s"), *NewWidgetClassToShow->GetName());
    }
}

void UGameScreenUIManager::ShowGameMainMenu()
{
    if (!MainMenu_InGame_Class) { UE_LOG(LogTemp, Error, TEXT("ShowGameMainMenu: MainMenu_InGame_Class is not set!")); return; }

    UUserWidget* RawPtrInstance = MainMenu_InGame_Instance.Get();
    ChangeActiveGameScreenWidget(MainMenu_InGame_Class, RawPtrInstance);
    // Если ChangeActiveGameScreenWidget создал новый виджет, RawPtrInstance обновится.
    // Присваиваем его обратно нашему TObjectPtr.
    if (RawPtrInstance != MainMenu_InGame_Instance.Get())
    {
        MainMenu_InGame_Instance = RawPtrInstance;
    }
}

void UGameScreenUIManager::ShowGameOfflineLobby()
{
    if (!OfflineLobby_InGame_Class) { UE_LOG(LogTemp, Error, TEXT("ShowGameOfflineLobby: OfflineLobby_InGame_Class is not set!")); return; }

    UUserWidget* RawPtrInstance = OfflineLobby_InGame_Instance.Get();
    ChangeActiveGameScreenWidget(OfflineLobby_InGame_Class, RawPtrInstance);
    if (RawPtrInstance != OfflineLobby_InGame_Instance.Get())
    {
        OfflineLobby_InGame_Instance = RawPtrInstance;
    }
}

void UGameScreenUIManager::ShowGameOnlineLobby()
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("ShowGameOnlineLobby: OwningGameInstance is null.")); return; }

    if (OwningGameInstance->bIsLoggedIn)
    {
        if (!OnlineLobby_InGame_Class) { UE_LOG(LogTemp, Error, TEXT("ShowGameOnlineLobby: OnlineLobby_InGame_Class is not set!")); return; }

        UUserWidget* RawPtrInstance = OnlineLobby_InGame_Instance.Get(); // Убедитесь, что имя переменной правильное
        ChangeActiveGameScreenWidget(OnlineLobby_InGame_Class, RawPtrInstance);
        if (RawPtrInstance != OnlineLobby_InGame_Instance.Get())
        {
            OnlineLobby_InGame_Instance = RawPtrInstance;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowGameOnlineLobby: User not logged in. Cannot show Online Lobby."));
        ShowGameMainMenu(); // Возвращаемся в главное игровое меню
    }
}

void UGameScreenUIManager::ShowGameProfileScreen()
{
    if (!ProfileScreen_InGame_Class) { UE_LOG(LogTemp, Error, TEXT("ShowGameProfileScreen: ProfileScreen_InGame_Class is not set!")); return; }

    UUserWidget* RawPtrInstance = ProfileScreen_InGame_Instance.Get();
    ChangeActiveGameScreenWidget(ProfileScreen_InGame_Class, RawPtrInstance);
    if (RawPtrInstance != ProfileScreen_InGame_Instance.Get())
    {
        ProfileScreen_InGame_Instance = RawPtrInstance;
    }
}

void UGameScreenUIManager::ShowGameSettings()
{
    if (!Settings_InGame_Class) { UE_LOG(LogTemp, Error, TEXT("ShowGameSettings: Settings_InGame_Class is not set!")); return; }

    UUserWidget* RawPtrInstance = Settings_InGame_Instance.Get();
    ChangeActiveGameScreenWidget(Settings_InGame_Class, RawPtrInstance);
    if (RawPtrInstance != Settings_InGame_Instance.Get())
    {
        Settings_InGame_Instance = RawPtrInstance;
    }
}