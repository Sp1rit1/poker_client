#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"           // Для TSubclassOf и TObjectPtr
#include "GameFramework/GameUserSettings.h" // Для управления настройками окна
#include "Engine/Engine.h"                  // Для GEngine
#include "Interfaces/IHttpRequest.h"        // Для HTTP запросов
#include "Interfaces/IHttpResponse.h"       // Для HTTP ответов
#include "TimerManager.h"                   // Для FTimerHandle и управления таймерами
// --- Опциональные инклуды, если вы их использовали ранее для парсинга JSON напрямую ---
// #include "Json.h"
// #include "JsonUtilities.h"
// #include "Serialization/JsonSerializer.h"
// #include "JsonObjectConverter.h"
// --- Конец опциональных инклудов ---

#include "MyGameInstance.generated.h" // Должен быть последним инклудом

// Forward declaration, чтобы не включать весь .h виджета здесь
class UUserWidget;

UCLASS()
class POKER_CLIENT_API UMyGameInstance : public UGameInstance // ЗАМЕНИТЕ YOURPROJECTNAME_API на ваше (например, POKER_CLIENT_API)
{
	GENERATED_BODY()

public: // Доступно извне и из Blueprint (если UFUNCTION/UPROPERTY)

	// --- Переменные Состояния Игры ---
	// Доступны для чтения из Blueprint
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsLoggedIn = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int64 LoggedInUserId = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FString LoggedInUsername;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsInOfflineMode = false;

	// --- Классы Виджетов (Назначаются в BP_MyGameInstance) ---
	// Доступны для чтения из Blueprint, редактируются только в defaults
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> WindowContainerClass; // НОВЫЙ: Класс виджета-контейнера

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> StartScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> LoginScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> RegisterScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> LoadingScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> MainMenuClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> SettingsScreenClass; // Если используется

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> ProfileScreenClass; // Если используется

	// --- Функции Навигации (Вызываются из Blueprint или C++) ---
	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowStartScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowLoginScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowRegisterScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowLoadingScreen(float Duration = 2.0f); // Длительность по умолчанию

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowSettingsScreen(); // Если используется

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowProfileScreen(); // Если используется

	// --- Функции Управления Состоянием ---
	UFUNCTION(BlueprintCallable, Category = "State")
	void SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername);

	UFUNCTION(BlueprintCallable, Category = "State")
	void SetOfflineMode(bool bNewIsOffline);

	// --- Функции для Сетевых Запросов (Вызываются из Blueprint) ---
	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestLogin(const FString& Username, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestRegister(const FString& Username, const FString& Password, const FString& Email);

	// Базовый URL для API аутентификации (можно сделать UPROPERTY для настройки в Editor)
	// UPROPERTY(EditDefaultsOnly, Category="Network")
	FString ApiBaseUrl = TEXT("http://localhost:8080/api/auth");

	// --- Переопределенные Функции GameInstance ---
	virtual void Init() override;
	virtual void Shutdown() override; // Добавлено для возможного сброса настроек окна

protected: // Доступно только из этого класса и его наследников

	// --- Текущие Экземпляры Виджетов ---
	// Используем Transient, чтобы они не сохранялись и создавались заново при запуске
	// Используем TObjectPtr для современного управления указателями UE
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentTopLevelWidget = nullptr; // ПЕРЕИМЕНОВАНО: Был CurrentScreenWidget

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentContainerInstance = nullptr; // НОВЫЙ: Указатель на экземпляр контейнера

	// --- Таймер для Экрана Загрузки ---
	FTimerHandle LoadingScreenTimerHandle;
	void OnLoadingScreenTimerComplete(); // Callback для таймера

	// --- Обработчики Ответов HTTP ---
	// Эти функции вызываются по завершению HTTP запросов
	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// --- Функции для Вызова Blueprint из C++ (Отображение Сообщений) ---
	// Вызывают функции с заданными именами в CurrentTopLevelWidget или CurrentContainerInstance
	void DisplayLoginError(const FString& Message);
	void DisplayRegisterError(const FString& Message);
	void DisplayLoginSuccessMessage(const FString& Message); // Для сообщения после успешной регистрации

	// --- Вспомогательные Функции для Управления Окном и Вводом ---
	/**
	 * Устанавливает режим ввода и видимость курсора мыши.
	 * @param bIsUIOnly True для режима UI Only (оконные виджеты), False для Game And UI (полноэкранные).
	 * @param bShowMouse True чтобы показать курсор, False чтобы скрыть.
	 */
	void SetupInputMode(bool bIsUIOnly, bool bShowMouse);

	/**
	 * Применяет оконный или полноэкранный режим к основному окну игры.
	 * @param bWantFullscreen True для установки WindowedFullscreen, False для Windowed.
	 */
	void ApplyWindowMode(bool bWantFullscreen);

	/**
	 * Шаблонная вспомогательная функция для создания, показа виджета и управления состоянием.
	 * Устанавливает режим окна, ввод/мышь, удаляет старый виджет, создает и добавляет новый.
	 * @param WidgetClassToShow Класс виджета для отображения.
	 * @param bIsFullscreenWidget True, если виджет должен быть в полноэкранном режиме, False для оконного.
	 * @return Указатель на созданный виджет или nullptr в случае ошибки.
	 */
	template <typename T = UUserWidget> // По умолчанию UUserWidget, но можно указать конкретный тип
	T* ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget);

	// Приватная секция больше не нужна, все внутренние хелперы в protected
	// private:
	// void SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass); // СТАРЫЙ МЕТОД - УДАЛЕН
};