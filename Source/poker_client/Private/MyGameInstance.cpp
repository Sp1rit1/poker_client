#include "MyGameInstance.h" // Замените на имя вашего .h файла, если отличается

// --- Основные инклуды UE ---
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h" // Может пригодиться
#include "TimerManager.h" // Для таймеров

// --- Инклуды для HTTP и JSON ---
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
// #include "JsonObjectConverter.h" // Раскомментируйте, если используете

// --- Инклуды для UMG (только если нужны специфичные виджеты в C++) ---
// #include "Components/TextBlock.h"
// #include "Components/EditableTextBox.h"


// =============================================================================
// Инициализация и Завершение
// =============================================================================

void UMyGameInstance::Init()
{
	Super::Init();
	// НЕ показываем здесь первый экран, ждем вызова из GameMode::BeginPlay
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Initialized."));
}

void UMyGameInstance::Shutdown()
{
	// Опционально: вернуть оконный режим при выходе из игры
	// ApplyWindowMode(false);
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown."));
	Super::Shutdown();
}

// =============================================================================
// Вспомогательные Функции для UI/Окна/Ввода
// =============================================================================

void UMyGameInstance::SetupInputMode(bool bIsUIOnly, bool bShowMouse)
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (PC)
	{
		PC->SetShowMouseCursor(bShowMouse);
		if (bIsUIOnly)
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); // Не блокировать
			// InputModeData.SetWidgetToFocus(CurrentContainerInstance ? CurrentContainerInstance->TakeWidget() : nullptr); // Можно задать фокус контейнеру
			PC->SetInputMode(InputModeData);
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to UI Only. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
		else // GameAndUI для полноэкранного режима
		{
			FInputModeGameAndUI InputModeData;
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); // Можно не блокировать, если нужно выходить из окна
			InputModeData.SetHideCursorDuringCapture(false); // Не скрывать автоматически при клике
			PC->SetInputMode(InputModeData);
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to Game And UI. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
	} else {
		UE_LOG(LogTemp, Warning, TEXT("SetupInputMode: PlayerController is null."));
	}
}

void UMyGameInstance::ApplyWindowMode(bool bWantFullscreen)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (Settings)
	{
		EWindowMode::Type CurrentMode = Settings->GetFullscreenMode();
		// Используем WindowedFullscreen как целевой "полноэкранный" режим
		EWindowMode::Type TargetMode = bWantFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;

		bool bModeChanged = false;
		if (CurrentMode != TargetMode)
		{
			Settings->SetFullscreenMode(TargetMode);
			bModeChanged = true; // Меняем режим окна
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting FullscreenMode to %s"), *UEnum::GetValueAsString(TargetMode));
		}

		// Устанавливаем разрешение ТОЛЬКО если меняем режим или если текущее не соответствует
		FIntPoint TargetResolution;
		if (bWantFullscreen)
		{
			TargetResolution = Settings->GetDesktopResolution();
		}
		else
		{
			// Используем разрешение, сохраненное как "LastConfirmed", или дефолтное, или запасное
			TargetResolution = Settings->GetLastConfirmedScreenResolution();
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(Settings->GetDefaultResolution().X, Settings->GetDefaultResolution().Y);
			}
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(720, 540); // Запасное, если все остальное невалидно
			}
		}

		if (Settings->GetScreenResolution() != TargetResolution)
		{
			Settings->SetScreenResolution(TargetResolution);
			bModeChanged = true; // Меняем разрешение
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting Resolution to %dx%d"), TargetResolution.X, TargetResolution.Y);
		}

		// Применяем настройки, только если что-то изменилось
		if (bModeChanged)
		{
			Settings->ApplySettings(false); // false - не требовать подтверждения
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Settings Applied."));
		} else {
			UE_LOG(LogTemp, Verbose, TEXT("ApplyWindowMode: Window Mode and Resolution already match target. No change needed."));
		}

	} else { UE_LOG(LogTemp, Warning, TEXT("ApplyWindowMode: Could not get GameUserSettings.")); }
}

template <typename T>
T* UMyGameInstance::ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget)
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: PlayerController is null.")); return nullptr; }
	if (!WidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: WidgetClassToShow is null.")); return nullptr; }

	// 1. Устанавливаем режим окна и ввода/мыши
	// Устанавливаем режим ДО смены виджета, чтобы избежать мерцания/неправильного размера
	ApplyWindowMode(bIsFullscreenWidget);
	SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget); // UIOnly+Курсор для оконного, GameUI+БезКурсора для fullscreen

	// 2. Удаляем старый виджет верхнего уровня
	if (CurrentTopLevelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous top level widget: %s"), *CurrentTopLevelWidget->GetName());
		CurrentTopLevelWidget->RemoveFromParent();
	}
	CurrentTopLevelWidget = nullptr;
	CurrentContainerInstance = nullptr; // Контейнер точно не активен, если мы показываем новый виджет верхнего уровня

	// 3. Создаем и добавляем новый виджет
	T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
	if (NewWidget)
	{
		NewWidget->AddToViewport();
		CurrentTopLevelWidget = NewWidget; // Запоминаем новый виджет
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Added new widget: %s"), *WidgetClassToShow->GetName());
		return NewWidget;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ShowWidget: Failed to create widget: %s"), *WidgetClassToShow->GetName());
		return nullptr;
	}
}

// =============================================================================
// Функции Навигации
// =============================================================================

void UMyGameInstance::ShowStartScreen()
{
	if (!WindowContainerClass || !StartScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: WindowContainerClass or StartScreenClass is not set!"));
		return;
	}
	// Показываем контейнер (он вернется как UUserWidget*)
	UUserWidget* Container = ShowWidget<UUserWidget>(WindowContainerClass, false); // false = оконный режим

	if (Container)
	{
		CurrentContainerInstance = Container; // Запоминаем ссылку на контейнер
		// Вызываем функцию 'SetContentWidget' внутри созданного контейнера
		FName FunctionName = FName(TEXT("SetContentWidget")); // Имя функции в WBP_WindowContainer
		UFunction* Function = CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; }; // Структура для параметра
			FSetContentParams Params;
			Params.WidgetClassToSet = StartScreenClass; // Передаем класс стартового экрана
			CurrentContainerInstance->ProcessEvent(Function, &Params); // Вызываем BP функцию
			UE_LOG(LogTemp, Log, TEXT("ShowStartScreen: Set content to StartScreenClass."));
		} else { UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: Function 'SetContentWidget' not found in %s!"), *WindowContainerClass->GetName()); }
	}
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); // Останавливаем таймер загрузки
}

void UMyGameInstance::ShowLoginScreen()
{
	if (!WindowContainerClass || !LoginScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: WindowContainerClass or LoginScreenClass is not set!"));
		return;
	}
	// Пытаемся переиспользовать существующий контейнер, если он активен
	UUserWidget* Container = CurrentContainerInstance;
	if (!Container || CurrentTopLevelWidget != CurrentContainerInstance)
	{
		// Контейнера нет или он неактивен, создаем заново
		Container = ShowWidget<UUserWidget>(WindowContainerClass, false); // false = оконный режим
		if (Container) CurrentContainerInstance = Container;
		else return; // Не удалось создать контейнер
	} else {
		// Контейнер уже есть, просто настроим ввод/мышь для UI
		SetupInputMode(true, true);
	}

	// Вызываем 'SetContentWidget' в контейнере
	FName FunctionName = FName(TEXT("SetContentWidget"));
	UFunction* Function = CurrentContainerInstance ? CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName) : nullptr;
	if (Function)
	{
		struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
		FSetContentParams Params;
		Params.WidgetClassToSet = LoginScreenClass;
		CurrentContainerInstance->ProcessEvent(Function, &Params);
		UE_LOG(LogTemp, Log, TEXT("ShowLoginScreen: Set content to LoginScreenClass."));
	} else { UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: Could not find SetContentWidget in container.")); }
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

void UMyGameInstance::ShowRegisterScreen()
{
	// Логика аналогична ShowLoginScreen, но с RegisterScreenClass
	if (!WindowContainerClass || !RegisterScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: WindowContainerClass or RegisterScreenClass is not set!"));
		return;
	}
	UUserWidget* Container = CurrentContainerInstance;
	if (!Container || CurrentTopLevelWidget != CurrentContainerInstance)
	{
		Container = ShowWidget<UUserWidget>(WindowContainerClass, false);
		if (Container) CurrentContainerInstance = Container;
		else return;
	} else {
		SetupInputMode(true, true);
	}

	FName FunctionName = FName(TEXT("SetContentWidget"));
	UFunction* Function = CurrentContainerInstance ? CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName) : nullptr;
	if (Function)
	{
		struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
		FSetContentParams Params;
		Params.WidgetClassToSet = RegisterScreenClass;
		CurrentContainerInstance->ProcessEvent(Function, &Params);
		UE_LOG(LogTemp, Log, TEXT("ShowRegisterScreen: Set content to RegisterScreenClass."));
	} else { UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: Could not find SetContentWidget in container.")); }
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

void UMyGameInstance::ShowLoadingScreen(float Duration)
{
	if (!LoadingScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowLoadingScreen: LoadingScreenClass is not set!")); return; }

	// Показываем виджет загрузки в полноэкранном режиме
	UUserWidget* LoadingWidget = ShowWidget<UUserWidget>(LoadingScreenClass, true); // true = fullscreen

	if (LoadingWidget)
	{
		// Запускаем таймер для перехода после "загрузки"
		GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); // Очищаем старый таймер
		GetWorld()->GetTimerManager().SetTimer(LoadingScreenTimerHandle, this, &UMyGameInstance::OnLoadingScreenTimerComplete, Duration, false);
		UE_LOG(LogTemp, Log, TEXT("ShowLoadingScreen: Timer Started for %.2f seconds."), Duration);
	}
}

void UMyGameInstance::OnLoadingScreenTimerComplete()
{
	UE_LOG(LogTemp, Log, TEXT("Loading Screen Timer Complete. Showing Main Menu."));
	ShowMainMenu(); // Переходим в главное меню
}

void UMyGameInstance::ShowMainMenu()
{
	if (!MainMenuClass) { UE_LOG(LogTemp, Error, TEXT("ShowMainMenu: MainMenuClass is not set!")); return; }
	// Показываем главное меню в полноэкранном режиме
	ShowWidget<UUserWidget>(MainMenuClass, true); // true = fullscreen
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); // Останавливаем таймер загрузки, если он еще шел
}

void UMyGameInstance::ShowSettingsScreen()
{
	if (!SettingsScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowSettingsScreen: SettingsScreenClass is not set!")); return; }
	// Решите, должен ли этот экран быть полноэкранным или в контейнере
	// Пример для полноэкранного:
	ShowWidget<UUserWidget>(SettingsScreenClass, true);
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

void UMyGameInstance::ShowProfileScreen()
{
	if (!ProfileScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowProfileScreen: ProfileScreenClass is not set!")); return; }
	// Решите, должен ли этот экран быть полноэкранным или в контейнере
	// Пример для полноэкранного:
	ShowWidget<UUserWidget>(ProfileScreenClass, true);
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

// =============================================================================
// Управление Состоянием Игры
// =============================================================================

void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
	bIsLoggedIn = bNewIsLoggedIn;
	LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1;
	LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT("");
	if (bIsLoggedIn) {
		bIsInOfflineMode = false; // Нельзя быть залогиненым и в оффлайне одновременно
	}
	UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username=%s"), bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername);
}

void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
	bIsInOfflineMode = bNewIsOffline;
	if (bIsInOfflineMode) {
		SetLoginStatus(false, -1, TEXT("")); // Выходим из логина при переходе в оффлайн
	}
	UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));

    if(bIsInOfflineMode)
    {
        // При переходе в оффлайн режим, обычно показываем главное меню (или лобби оффлайн игры)
        // Убедитесь, что оно переключает окно в нужный режим (вероятно, fullscreen)
        ShowMainMenu();
    }
}

// =============================================================================
// Логика HTTP Запросов
// =============================================================================

void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password)
{
	UE_LOG(LogTemp, Log, TEXT("RequestLogin: Attempting login for user: %s"), *Username);

	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to serialize JSON body."));
		DisplayLoginError(TEXT("Client error: Could not create request."));
		return;
	}

	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/login"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	// Привязываем обработчик ответа
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnLoginResponseReceived);

	// Отправляем запрос
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayLoginError(TEXT("Network error: Could not start request."));
	} else {
		UE_LOG(LogTemp, Log, TEXT("RequestLogin: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/login")));
		// Можно показать индикатор загрузки в UI, если нужно
	}
}

void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
	UE_LOG(LogTemp, Log, TEXT("RequestRegister: Attempting registration for user: %s, email: %s"), *Username, *Email);

	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);
	RequestJson->SetStringField(TEXT("email"), Email);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("RequestRegister: Failed to serialize JSON body."));
		DisplayRegisterError(TEXT("Client error: Could not create request."));
		return;
	}

	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/register"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnRegisterResponseReceived);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestRegister: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayRegisterError(TEXT("Network error: Could not start request."));
	} else {
		UE_LOG(LogTemp, Log, TEXT("RequestRegister: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/register")));
	}
}

// =============================================================================
// Обработка Ответов HTTP
// =============================================================================

void UMyGameInstance::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayLoginError(TEXT("Network error or server unavailable."));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	if (ResponseCode == 200) // OK
	{
		TSharedPtr<FJsonObject> ResponseJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
		{
			int64 ReceivedUserId = -1;
			FString ReceivedUsername;
			// Используем TryGet... для безопасного извлечения
			if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
				ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername))
			{
				UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Login successful for user: %s (ID: %lld)"), *ReceivedUsername, ReceivedUserId);
				SetLoginStatus(true, ReceivedUserId, ReceivedUsername);
				// Переходим на экран загрузки -> главное меню
				ShowLoadingScreen(); // Используем значение Duration по умолчанию
			} else {
				UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to parse 'userId' or 'username' from JSON response."));
				DisplayLoginError(TEXT("Server error: Invalid response format."));
			}
		} else {
			UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to deserialize JSON response. Body: %s"), *ResponseBody);
			DisplayLoginError(TEXT("Server error: Could not parse response."));
		}
	} else if (ResponseCode == 401) // Unauthorized
	{
		UE_LOG(LogTemp, Warning, TEXT("OnLoginResponseReceived: Login failed - Invalid credentials (401)."));
		DisplayLoginError(TEXT("Login failed: Incorrect username or password."));
	} else { // Другие ошибки
		FString ErrorMessage = FString::Printf(TEXT("Login failed: Server error (Code: %d)"), ResponseCode);
		// Попытка извлечь сообщение из JSON тела ошибки
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Login failed: %s"), *ServerErrorMsg); // Используем сообщение сервера
			}
		}
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: %s"), *ErrorMessage);
		DisplayLoginError(ErrorMessage);
	}
}

void UMyGameInstance::OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayRegisterError(TEXT("Network error or server unavailable."));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	if (ResponseCode == 201) // Created
	{
		UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Registration successful!"));
		// Переходим на экран логина, чтобы пользователь мог войти
		ShowLoginScreen();
		// Запускаем таймер для показа сообщения об успехе ПОСЛЕ того, как экран логина отобразится
		FTimerHandle TempTimerHandle;
		if(GetWorld()) // Проверяем валидность World перед использованием TimerManager
		{
			GetWorld()->GetTimerManager().SetTimer(TempTimerHandle, [this]() {
				DisplayLoginSuccessMessage(TEXT("Registration successful! Please log in."));
			}, 0.1f, false); // Небольшая задержка
		}
	} else if (ResponseCode == 409) // Conflict
	{
		FString ErrorMessage = TEXT("Registration failed: Username or Email already exists."); // Сообщение по умолчанию
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (409)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	} else if (ResponseCode == 400) // Bad Request (ошибка валидации на сервере)
	{
		FString ErrorMessage = TEXT("Registration failed: Invalid data provided.");
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			// Попробуем извлечь детальное сообщение, если сервер его шлет
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
			// Можно также парсить массив errors, если сервер его возвращает для детальных ошибок по полям
		}
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (400)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	} else { // Другие ошибки
		FString ErrorMessage = FString::Printf(TEXT("Registration failed: Server error (Code: %d)"), ResponseCode);
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: %s"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
}


// =============================================================================
// Вызов Функций Отображения Сообщений в Blueprint
// =============================================================================

// Вспомогательная функция для вызова функции в текущем виджете
void CallWidgetFunction(UUserWidget* Widget, FName FunctionName, const FString& Message)
{
    if (!Widget) { UE_LOG(LogTemp, Warning, TEXT("CallWidgetFunction: Widget is null for function %s"), *FunctionName.ToString()); return; }

    UFunction* Function = Widget->GetClass()->FindFunctionByName(FunctionName);
    if (Function)
    {
        // Структура для передачи параметра FString
        struct FDisplayParams { FString Message; };
        FDisplayParams Params;
        Params.Message = Message;
        Widget->ProcessEvent(Function, &Params);
         UE_LOG(LogTemp, Verbose, TEXT("CallWidgetFunction: Called function %s on widget %s"), *FunctionName.ToString(), *Widget->GetName());
    } else {
        UE_LOG(LogTemp, Warning, TEXT("CallWidgetFunction: Function '%s' not found in widget %s"), *FunctionName.ToString(), *Widget->GetName());
    }
}

void UMyGameInstance::DisplayLoginError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: %s"), *Message);
	// Добавляем лог для проверки имени виджета
	if (CurrentTopLevelWidget) {
		UE_LOG(LogTemp, Log, TEXT("DisplayLoginError: Attempting to call function on widget: %s"), *CurrentTopLevelWidget->GetName());
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("DisplayLoginError: CurrentTopLevelWidget is NULL!"));
	}
	CallWidgetFunction(CurrentTopLevelWidget, FName(TEXT("DisplayErrorMessage")), Message);
}

void UMyGameInstance::DisplayRegisterError(const FString& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: %s"), *Message);
    // Ошибку регистрации показываем так же
    CallWidgetFunction(CurrentTopLevelWidget, FName(TEXT("DisplayErrorMessage")), Message);
}

void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
    UE_LOG(LogTemp, Log, TEXT("DisplayLoginSuccessMessage: %s"), *Message);
    // Сообщение об успехе регистрации показываем на экране логина (который должен быть текущим)
    // Пытаемся вызвать "DisplaySuccessMessage", если нет - пробуем "DisplayErrorMessage"
    FName SuccessFuncName = FName(TEXT("DisplaySuccessMessage"));
    FName ErrorFuncName = FName(TEXT("DisplayErrorMessage"));
    if (CurrentTopLevelWidget && CurrentTopLevelWidget->GetClass()->FindFunctionByName(SuccessFuncName))
    {
        CallWidgetFunction(CurrentTopLevelWidget, SuccessFuncName, Message);
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Function '%s' not found, falling back to '%s'."), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
        CallWidgetFunction(CurrentTopLevelWidget, ErrorFuncName, Message); // Фоллбэк на функцию ошибки
    }
}