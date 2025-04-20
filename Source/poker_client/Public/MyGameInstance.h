// MyGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h" // <--- ДОБАВЛЕНО: Необходимо для FTimerHandle и GetTimerManager()
#include "Blueprint/UserWidget.h" // Для UUserWidget и TSubclassOf
// #include "Interfaces/IHttpRequest.h" // Добавим позже для шага 2.7
#include "MyGameInstance.generated.h" // Должен быть последним

UCLASS()
class POKER_CLIENT_API UMyGameInstance : public UGameInstance // Замените POKERCLIENT_API!
{
    GENERATED_BODY()

public:
    // --- Состояние Аутентификации и Режима ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsLoggedIn = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    int64 LoggedInUserId = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    FString LoggedInUsername = TEXT("");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsInOfflineMode = false;

    // --- UI Навигация ---

    // Класс виджета для стартового экрана (назначается в Blueprint)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> StartScreenClass;

    // Класс виджета для экрана логина (назначается в Blueprint)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> LoginScreenClass;

    // Класс виджета для ЭКРАНА ЗАГРУЗКИ (назначается в Blueprint) - НОВОЕ
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> LoadingScreenClass;

    // Класс виджета для главного меню (назначается в Blueprint)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> MainMenuClass;

    // Указатель на текущий отображаемый виджет (чтобы его удалять)
    UPROPERTY() // Не нужен доступ извне, просто храним ссылку
        TObjectPtr<UUserWidget> CurrentScreenWidget = nullptr;

    // Сделаем таймер приватным, т.к. им управляет сам GameInstance
private:
    // --- Таймер для экрана загрузки ---
    FTimerHandle LoadingScreenTimerHandle; // Хранит идентификатор таймера

    /** Функция, которая будет вызвана по завершению таймера загрузки */
    void OnLoadingScreenTimerComplete();

public:
    // --- Методы Управления UI ---

    /** Инициализация GameInstance (вызывается при старте игры) */
    virtual void Init() override;

    /** Показать стартовый экран */
    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowStartScreen();

    /** Показать экран логина */
    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowLoginScreen();

    /** Показать ЭКРАН ЗАГРУЗКИ и запустить таймер */
    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowLoadingScreen(float Duration = 5.0f); // <-- Установите здесь желаемую длительность видео/загрузки

    /** Показать главное меню */
    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowMainMenu();

    // --- Методы Управления Состоянием ---

    /** Установить статус логина (вызывается после успешного ответа от сервера) */
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername);

    /** Установить оффлайн режим (вызывается по кнопке "Оффлайн") */
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetOfflineMode(bool bNewIsOffline);

    // --- Методы для HTTP запросов (объявления для Шага 2.7) ---
    FString ApiBaseUrl = TEXT("http://localhost:8080/api/auth");

    UFUNCTION(BlueprintCallable, Category = "Network")
    void RequestLogin(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Network")
    void RequestRegister(const FString& Username, const FString& Password, const FString& Email);

private:
    // --- Приватные Методы ---

    /** Вспомогательная функция для смены экрана */
    void SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass);

    // Объявления обработчиков HTTP (реализация будет в Шаге 2.7)
    // void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    // void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

};