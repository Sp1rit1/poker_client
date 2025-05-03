#pragma once // гаранитирует единичное включение файла при компиляции, предотвращая ошибки повторного определения

#include "CoreMinimal.h" // основной заголовочный файл UE, включающий базовые типы и макросы
#include "Engine/GameInstance.h" // определение базового класса GameInstance, от которого мы наследуемся
#include "Blueprint/UserWidget.h" // для работы с виджетами UMG
#include "GameFramework/GameUserSettings.h" // для управления окнами и разрешением
#include "Engine/Engine.h"  //  определение глобального объекта GameEngine               
#include "Interfaces/IHttpRequest.h" // интерфейс для создания и управления HTTP-запросов        
#include "Interfaces/IHttpResponse.h" // интерфейс для обработки HTTP-ответов      
#include "TimerManager.h" // управление таймерами
#include "OfflineGameManager.h"  // управляющий класс в оффлайн режиме

#include "SlateBasics.h"                   // Для доступа к SWindow и FSlateApplication
#include "GenericPlatform/GenericApplication.h" // Для IGenericApplication и GetOSWindowHandle
#include "Delegates/DelegateCombinations.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h" // Нужен для HWND и WinAPI функций
// AllowWindowsPlatformTypes/HideWindowsPlatformTypes используются для предотвращения конфликтов имен
// между типами Windows (как INT, FLOAT) и типами UE. Их нужно "обернуть" вокруг включения Windows.h,
// но так как WindowsHWrapper делает это внутри себя, явное добавление может не понадобиться:
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h> // Или более специфичные заголовки, если нужны
#include "Windows/HideWindowsPlatformTypes.h"
#endif


#include "MyGameInstance.generated.h" // // Сгенерированный заголовочный файл, содержащий код, сгенерированный Unreal Header Tool для поддержки системы рефлексии. Должен быть последним включением

class UUserWidget; 
class UMediaSource;
class UMediaPlayer;
class UPackage; // Добавили UPackage

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLoginAttemptCompletedSignature, bool, bWasSuccessful, const FString&, ErrorMessage);

UCLASS() // макрос, помечающий этот класс для системы рефлексии Unreal Engine.

// POKER_CLIENT_API - макрос для экспорта класса, чтобы он стал доступен для других модулей. Сам класс является основным и служит для управления глобальным состоянием и данными
class POKER_CLIENT_API UMyGameInstance : public UGameInstance 
{
	GENERATED_BODY() // макрос, обязателен для классов, помеченных UCLASS(), вставляет код, сгенерированный UHT. Должен быть первой строкой в теле класса.

public: 



	// --- Переменные Состояния Игры ---


// UPROPERTY: макрос, делающий переменную видимой для системы рефлексии UE.
// VisibleAnywhere: Переменную можно видеть в редакторе на панели Details для любого экземпляра этого объекта (но нельзя редактировать).
// BlueprintReadOnly: Переменную можно читать из Blueprint, но нельзя изменять.
// Category = "name": Группирует эту переменную в категорию "name" в редакторе.

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsLoggedIn = false; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int64 LoggedInUserId = -1; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FString LoggedInUsername;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsInOfflineMode = false; 

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
	TSubclassOf<UUserWidget> SettingsScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> ProfileScreenClass;

	/** Ассет Media Player, используемый для экрана загрузки. Устанавливается в Defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading Screen")
	TObjectPtr<UMediaPlayer> LoadingMediaPlayerAsset;

	//Должна быть вызвана из виджета WBP_LoadingScreen, когда его видео завершится.

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void NotifyLoadingVideoFinished();

	/** Ассет Media Source (видеофайл) для экрана загрузки. Устанавливается в Defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading Screen")
	TObjectPtr<UMediaSource> LoadingMediaSourceAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Managers")
	UOfflineGameManager* OfflineGameManager; 


	// --- Функции Навигации (Вызываются из Blueprint или C++) ---
	
// UFUNCTION: Макрос, делающий функцию видимой для системы рефлексии UE.

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowStartScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowLoginScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowRegisterScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowSettingsScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowProfileScreen(); 

	/** Запускает показ виджета загрузки с видео и асинхронную загрузку уровня */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartLoadLevelWithVideoWidget(FName LevelName);

	UFUNCTION(BlueprintCallable, Category = "State")
	void SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername);

	UFUNCTION(BlueprintCallable, Category = "State")
	void SetOfflineMode(bool bNewIsOffline);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestLogin(const FString& Username, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestRegister(const FString& Username, const FString& Password, const FString& Email);

	FString ApiBaseUrl = TEXT("http://localhost:8080/api/auth");  // Базовый URL для API аутентификации

	UPROPERTY(BlueprintAssignable, Category = "Login") // делегат логина
	FLoginAttemptCompletedSignature OnLoginAttemptCompleted;

	virtual void Init() override; // виртуальная (переопределяемая в дочерних классах), переопределённая (override) из UGameInstance функция инициализации GameInstance
	virtual void Shutdown() override; // функция завершения работы GameInstance

protected: 

	// --- Текущие Экземпляры Виджетов ---
	// Transient: Указывает, что значение этой переменной не должно сохраняться при сериализации (преобразовании состояния объекта в формат для сохранения). Оно будет инициализировано при запуске.
	// TObjectPtr<UUserWidget>: Современный тип умного указателя UE для объектов UObject (включая виджеты). Автоматически обнуляется, если объект удален.

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentTopLevelWidget = nullptr;  // указатель на текущий виджет верхнего уровня (контейнер или полноэкранный)

	/** Коллбэк завершения асинхронной загрузки пакета */
	void OnLevelPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);
	/** Проверяет, можно ли завершить переход */
	void CheckAndFinalizeLevelTransition();

	bool bIsInitialWindowSetupComplete = false;

	/** Рассчитанное и желаемое разрешение для оконного режима */
	FIntPoint DesiredWindowedResolution = FIntPoint(0, 0);

	/** Флаг, указывающий, что DesiredWindowedResolution было успешно рассчитано */
	bool bDesiredResolutionCalculated = false;

	// --- Обработчики Ответов HTTP ---
	// FHttpRequestPtr, FHttpResponsePtr: Типы умных указателей для объектов HTTP запроса и ответа
	// Объявление функций обратного вызова для обработки ответа на запрос логина и регистрации.
	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);


	void DisplayLoginError(const FString& Message); 
	void DisplayRegisterError(const FString& Message);
	void DisplayLoginSuccessMessage(const FString& Message); 

	// --- Вспомогательные Функции для Управления Окном и Вводом ---

	//Устанавливает режим ввода и видимость курсора мыши.
	// bIsUIOnly True для режима UI Only (оконные виджеты), False для Game And UI (полноэкранные).
	// bShowMouse True чтобы показать курсор, False чтобы скрыть.

	void SetupInputMode(bool bIsUIOnly, bool bShowMouse);

	void ApplyWindowMode(bool bWantFullscreen);

	// Шаблонная вспомогательная функция для создания, показа виджета и управления состоянием.
	template <typename T = UUserWidget> 
	T* ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget);

	FTimerHandle ResizeTimerHandle; 
	void DelayedInitialResize();

private:
	// --- Новые переменные состояния загрузки (ДОБАВИТЬ) ---
	FName LevelToLoadAsync = NAME_None;
	bool bIsLevelLoadComplete = false;
	bool bIsLoadingVideoFinished = false;

};