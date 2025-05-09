#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameScreenUIManager.generated.h"

// Прямые объявления
class UMyGameInstance;
class UUserWidget;
class UWBP_MainMenu_InGame; // Ваше главное меню ВНУТРИ игры
class UWBP_OfflineLobby_InGame; // Ваше оффлайн лобби ВНУТРИ игры (если отличается от стартового)
class UWBP_OnlineLobby_InGame;  // Ваше онлайн лобби ВНУТРИ игры
class UWBP_ProfileScreen_InGame; // Ваш профиль ВНУТРИ игры
class UWBP_Settings_InGame;    // Ваши настройки ВНУТРИ игры

UCLASS()
class POKER_CLIENT_API UGameScreenUIManager : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UGameScreenUIManager();

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
    UFUNCTION(BlueprintCallable, Category = "UI Navigation|GameScreens")
    void ShowGameMainMenu(); // Главное меню после входа/оффлайн выбора

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|GameScreens")
    void ShowGameOfflineLobby();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|GameScreens")
    void ShowGameOnlineLobby(); // Проверит логин перед показом

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|GameScreens")
    void ShowGameProfileScreen();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|GameScreens")
    void ShowGameSettings();

    /** Возвращает текущий активный виджет, если он есть и принадлежит этому менеджеру.
     *  Может быть полезно для проверки, какой экран сейчас открыт.
     */
    UFUNCTION(BlueprintPure, Category = "UI Navigation|GameScreens")
    UUserWidget* GetCurrentActiveGameWidget() const { return CurrentActiveGameScreenWidget; }

protected:
    /**
     * Вспомогательная функция для смены активного игрового виджета.
     * @param NewWidgetClassToShow Класс виджета для показа.
     * @param OutWidgetInstanceVariable Ссылка на переменную, где хранится (или будет храниться) экземпляр виджета.
     * @param bIsFullscreenWidget Флаг, должен ли этот виджет занимать весь экран и менять режим окна (обычно false для меню на стене).
     */
    void ChangeActiveGameScreenWidget(TSubclassOf<UUserWidget> NewWidgetClassToShow, UUserWidget*& OutWidgetInstanceVariable, bool bIsFullscreenWidget = false);


private:
    UPROPERTY()
    TObjectPtr<UMyGameInstance> OwningGameInstance;

    // --- Классы виджетов (передаются при инициализации) ---
    UPROPERTY()
    TSubclassOf<UUserWidget> MainMenu_InGame_Class; // Используем более общие имена переменных

    UPROPERTY()
    TSubclassOf<UUserWidget> OfflineLobby_InGame_Class;

    UPROPERTY()
    TSubclassOf<UUserWidget> OnlineLobby_InGame_Class;

    UPROPERTY()
    TSubclassOf<UUserWidget> ProfileScreen_InGame_Class;

    UPROPERTY()
    TSubclassOf<UUserWidget> Settings_InGame_Class;

    // --- Экземпляры созданных виджетов (чтобы не создавать каждый раз) ---
    UPROPERTY()
    TObjectPtr<UUserWidget> MainMenu_InGame_Instance;

    UPROPERTY()
    TObjectPtr<UUserWidget> OfflineLobby_InGame_Instance;

    UPROPERTY()
    TObjectPtr<UUserWidget> OnlineLobby_InGame_Instance;

    UPROPERTY()
    TObjectPtr<UUserWidget> ProfileScreen_InGame_Instance;

    UPROPERTY()
    TObjectPtr<UUserWidget> Settings_InGame_Instance;

    // --- Текущий активный виджет из этой группы ---
    UPROPERTY()
    TObjectPtr<UUserWidget> CurrentActiveGameScreenWidget;
};
