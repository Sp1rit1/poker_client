#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MenuScreenUIManager.generated.h"


class UMyGameInstance;
class UUserWidget;
class UWBP_MainMenu_InGame; 
class UWBP_OfflineLobby_InGame; 
class UWBP_OnlineLobby_InGame;  
class UWBP_ProfileScreen_InGame; 
class UWBP_Settings_InGame;    

UCLASS()
class POKER_CLIENT_API UMenuScreenUIManager : public UObject 
{
    GENERATED_BODY()

public:
    UMenuScreenUIManager();

    void Initialize(
        UMyGameInstance* InGameInstance,
        TSubclassOf<UUserWidget> InMainMenuClass, // Используем базовый UUserWidget для гибкости
        TSubclassOf<UUserWidget> InOfflineLobbyClass,
        TSubclassOf<UUserWidget> InOnlineLobbyClass,
        TSubclassOf<UUserWidget> InProfileScreenClass,
        TSubclassOf<UUserWidget> InSettingsClass
    );

    // --- Функции Навигации Игровых Экранов ---
    UFUNCTION(BlueprintCallable, Category = "UI Navigation|MenuScreens")
    void ShowMainMenu(); // Главное меню после входа/оффлайн выбора

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|MenuScreens")
    void ShowOfflineLobby();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|MenuScreens")
    void ShowOnlineLobby(); // Проверит логин перед показом

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|MenuScreens")
    void ShowProfileScreen();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|MenuScreens")
    void ShowSettings();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|MenuScreens")
    void TriggerTransitionToGameLevel();

    UFUNCTION(BlueprintPure, Category = "UI Navigation|MenuScreens")
    UUserWidget* GetCurrentActiveMenuWidget() const { return CurrentActiveMenuScreenWidget; }

protected:

    void ChangeActiveMenuScreenWidget(TSubclassOf<UUserWidget> NewWidgetClassToShow, UUserWidget*& OutWidgetInstanceVariable, bool bIsFullscreenWidget = false);


private:
    UPROPERTY()
    TObjectPtr<UMyGameInstance> OwningGameInstance;

    // --- Классы виджетов (передаются при инициализации) ---
    UPROPERTY()
    TSubclassOf<UUserWidget> MainMenuClass; // Используем более общие имена переменных

    UPROPERTY()
    TSubclassOf<UUserWidget> OfflineLobbyClass;

    UPROPERTY()
    TSubclassOf<UUserWidget> OnlineLobbyClass;

    UPROPERTY()
    TSubclassOf<UUserWidget> ProfileScreenClass;

    UPROPERTY()
    TSubclassOf<UUserWidget> SettingsClass;

    // --- Экземпляры созданных виджетов (чтобы не создавать каждый раз) ---
    UPROPERTY()
    TObjectPtr<UUserWidget> MainMenuInstance;

    UPROPERTY()
    TObjectPtr<UUserWidget> OfflineLobbyInstance;

    UPROPERTY()
    TObjectPtr<UUserWidget> OnlineLobbyInstance;

    UPROPERTY()
    TObjectPtr<UUserWidget> ProfileScreenInstance;

    UPROPERTY()
    TObjectPtr<UUserWidget> SettingsInstance;

    // --- Текущий активный виджет из этой группы ---
    UPROPERTY()
    TObjectPtr<UUserWidget> CurrentActiveMenuScreenWidget;
};
