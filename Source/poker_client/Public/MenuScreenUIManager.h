#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MenuScreenUIManager.generated.h"

// Прямые объявления
class UMyGameInstance;
class UUserWidget;
class UWBP_MainMenu_InGame; // Ваше главное меню ВНУТРИ игры
class UWBP_OfflineLobby_InGame; // Ваше оффлайн лобби ВНУТРИ игры (если отличается от стартового)
class UWBP_OnlineLobby_InGame;  // Ваше онлайн лобби ВНУТРИ игры
class UWBP_ProfileScreen_InGame; // Ваш профиль ВНУТРИ игры
class UWBP_Settings_InGame;    // Ваши настройки ВНУТРИ игры

UCLASS()
class POKER_CLIENT_API UMenuScreenUIManager : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UMenuScreenUIManager();

    /**
     * Инициализирует менеджер UI игровых экранов.
     * @param InGameInstance Указатель на владеющий GameInstance.
     * @param InMainMenuClass Класс виджета главного игрового меню.
     * @param InOfflineLobbyClass Класс виджета игрового оффлайн лобби.
     * @param InOnlineLobbyClass Класс виджета игрового онлайн лобби.
     * @param InProfileScreenClass Класс виджета игрового профиля.
     * @param InSettingsClass Класс виджета игровых настроек.
     */
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

    /** Возвращает текущий активный виджет, если он есть и принадлежит этому менеджеру.
     *  Может быть полезно для проверки, какой экран сейчас открыт.
     */
    UFUNCTION(BlueprintPure, Category = "UI Navigation|MenuScreens")
    UUserWidget* GetCurrentActiveMenuWidget() const { return CurrentActiveMenuScreenWidget; }

protected:
    /**
     * Вспомогательная функция для смены активного игрового виджета.
     * @param NewWidgetClassToShow Класс виджета для показа.
     * @param OutWidgetInstanceVariable Ссылка на переменную, где хранится (или будет храниться) экземпляр виджета.
     * @param bIsFullscreenWidget Флаг, должен ли этот виджет занимать весь экран и менять режим окна (обычно false для меню на стене).
     */
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
