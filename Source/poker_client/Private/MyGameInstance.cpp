#include "MyGameInstance.h" // Включаем заголовочный файл нашего класса GameInstance.

// --- Основные инклуды UE ---
#include "GameFramework/PlayerController.h"   // Для получения PlayerController (управление вводом, мышью, создание виджетов).
#include "GameFramework/GameUserSettings.h" // Для доступа к настройкам игры (разрешение, режим окна).
#include "Engine/Engine.h"                  // Для доступа к глобальному объекту GEngine (через него получаем GameUserSettings).
#include "Kismet/GameplayStatics.h"         // Содержит статические вспомогательные функции (например, GetPlayerController, хотя GetFirstLocalPlayerController предпочтительнее в GameInstance).
#include "TimerManager.h"                   // Для работы с таймерами (FTimerHandle, SetTimer, ClearTimer).
#include "Blueprint/UserWidget.h"           // Для работы с классом UUserWidget и его создания.
#include "Components/PanelWidget.h"         // Для приведения (Cast) виджетов-панелей (как UBorder) к базовому типу, чтобы получить дочерние элементы в FindWidgetInContainer.
#include "UObject/UnrealType.h"             // Для работы с системой рефлексии (FObjectProperty) в FindWidgetInContainer.

// --- Инклуды для HTTP и JSON ---
#include "HttpModule.h"                 // Основной модуль для работы с HTTP.
#include "Interfaces/IHttpRequest.h"    // Интерфейс для создания и настройки HTTP запроса.
#include "Interfaces/IHttpResponse.h"   // Интерфейс для работы с HTTP ответом.
#include "Json.h"                       // Основные классы для работы с JSON (FJsonObject, FJsonValue).
#include "JsonUtilities.h"              // Вспомогательные функции для работы с JSON (хотя мы используем FJsonSerializer).
#include "Serialization/JsonSerializer.h" // Класс для сериализации (C++ -> JSON) и десериализации (JSON -> C++) объектов JSON.


// =============================================================================
// Инициализация и Завершение
// =============================================================================

// Функция Init() вызывается один раз при создании экземпляра GameInstance (в начале игры).
void UMyGameInstance::Init()
{
	// Вызываем реализацию Init() из базового класса UGameInstance. Важно для корректной работы движка.
	Super::Init();
	// НЕ вызываем здесь ShowStartScreen, так как PlayerController может быть еще не полностью инициализирован.
	// Показ первого экрана лучше делать из GameMode::BeginPlay.
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Initialized.")); // Выводим сообщение в лог о завершении инициализации.
}

// Функция Shutdown() вызывается при завершении работы GameInstance (при закрытии игры).
void UMyGameInstance::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown.")); // Выводим сообщение в лог о начале завершения работы.
	// Опционально: можно вернуть окно в оконный режим при выходе.
	// ApplyWindowMode(false);
	// Вызываем реализацию Shutdown() из базового класса UGameInstance.
	Super::Shutdown();
}


// =============================================================================
// Вспомогательные Функции для UI/Окна/Ввода
// =============================================================================

// Устанавливает режим ввода и видимость курсора мыши.
void UMyGameInstance::SetupInputMode(bool bIsUIOnly, bool bShowMouse)
{
	// Получаем первого локального игрока-контроллера. В GameInstance обычно работаем с ним.
	APlayerController* PC = GetFirstLocalPlayerController();
	// Проверяем, что PlayerController успешно получен.
	if (PC)
	{
		// Устанавливаем видимость системного курсора мыши.
		PC->SetShowMouseCursor(bShowMouse);
		// Если нужен режим "Только UI" (для оконных виджетов).
		if (bIsUIOnly)
		{
			// Создаем структуру настроек для режима FInputModeUIOnly.
			FInputModeUIOnly InputModeData;
			// Устанавливаем поведение блокировки мыши: DoNotLock - не блокировать курсор в пределах окна.
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// Применяем установленный режим ввода к PlayerController.
			PC->SetInputMode(InputModeData);
			// Логируем установленный режим и видимость мыши.
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to UI Only. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
		// Иначе используем режим "Игра и UI" (для полноэкранных меню или игры).
		else
		{
			// Создаем структуру настроек для режима FInputModeGameAndUI.
			FInputModeGameAndUI InputModeData;
			// Устанавливаем поведение блокировки мыши: можно не блокировать, чтобы можно было легко переключаться между окнами.
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// Указываем не скрывать курсор автоматически при клике в окне (т.к. мы управляем видимостью сами).
			InputModeData.SetHideCursorDuringCapture(false);
			// Применяем установленный режим ввода.
			PC->SetInputMode(InputModeData);
			// Логируем установленный режим и видимость мыши.
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to Game And UI. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
	}
	else {
		// Выводим предупреждение, если не удалось получить PlayerController.
		UE_LOG(LogTemp, Warning, TEXT("SetupInputMode: PlayerController is null."));
	}
}

// Применяет оконный или полноэкранный режим к основному окну игры.
void UMyGameInstance::ApplyWindowMode(bool bWantFullscreen)
{
	// Получаем доступ к объекту настроек игры через глобальный объект GEngine.
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	// Проверяем, что объект настроек успешно получен.
	if (Settings)
	{
		// Получаем текущий режим окна.
		EWindowMode::Type CurrentMode = Settings->GetFullscreenMode();
		// Определяем целевой режим: WindowedFullscreen для полноэкранного, Windowed для оконного.
		EWindowMode::Type TargetMode = bWantFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;

		// Флаг, указывающий, нужно ли применять изменения.
		bool bSettingsChanged = false;
		// Если текущий режим не совпадает с целевым, устанавливаем новый режим.
		if (CurrentMode != TargetMode)
		{
			Settings->SetFullscreenMode(TargetMode);
			bSettingsChanged = true; // Отмечаем, что настройки изменились.
			// Логируем изменение режима окна. UEnum::GetValueAsString используется для получения строкового представления enum.
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting FullscreenMode to %s"), *UEnum::GetValueAsString(TargetMode));
		}

		// Определяем целевое разрешение экрана.
		FIntPoint TargetResolution;
		// Если нужен полноэкранный режим, берем текущее разрешение рабочего стола.
		if (bWantFullscreen)
		{
			TargetResolution = Settings->GetDesktopResolution();
		}
		// Если нужен оконный режим.
		else
		{
			// Пытаемся взять последнее подтвержденное пользователем разрешение.
			TargetResolution = Settings->GetLastConfirmedScreenResolution();
			// Если оно невалидно (0 или меньше), пытаемся взять разрешение по умолчанию из настроек проекта.
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(Settings->GetDefaultResolution().X, Settings->GetDefaultResolution().Y);
			}
			// Если и оно невалидно, используем запасное значение (например, 720x540).
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(720, 540); // Запасное значение
			}
		}

		// Если текущее разрешение отличается от целевого, устанавливаем новое разрешение.
		if (Settings->GetScreenResolution() != TargetResolution)
		{
			Settings->SetScreenResolution(TargetResolution);
			bSettingsChanged = true; // Отмечаем, что настройки изменились.
			// Логируем изменение разрешения.
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting Resolution to %dx%d"), TargetResolution.X, TargetResolution.Y);
		}

		// Применяем настройки к окну игры, только если что-то действительно изменилось.
		if (bSettingsChanged)
		{
			// false в ApplySettings означает, что не нужно ждать подтверждения пользователя (настройки применяются сразу).
			Settings->ApplySettings(false);
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Settings Applied."));
		}
		else {
			// Логируем, что изменения не потребовались.
			UE_LOG(LogTemp, Verbose, TEXT("ApplyWindowMode: Window Mode and Resolution already match target. No change needed."));
		}

	}
	else {
		// Выводим предупреждение, если не удалось получить доступ к настройкам игры.
		UE_LOG(LogTemp, Warning, TEXT("ApplyWindowMode: Could not get GameUserSettings."));
	}
}

// Шаблонная вспомогательная функция для создания и показа виджета (оконного или полноэкранного).
// <typename T = UUserWidget>: Определяет шаблонный параметр T, по умолчанию UUserWidget. Позволяет функции возвращать указатель нужного типа.
template <typename T>
T* UMyGameInstance::ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget)
{
	// Получаем PlayerController.
	APlayerController* PC = GetFirstLocalPlayerController();
	// Проверяем PlayerController.
	if (!PC) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: PlayerController is null.")); return nullptr; }
	// Проверяем, что класс виджета передан и валиден.
	if (!WidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: WidgetClassToShow is null.")); return nullptr; }

	// 1. Применяем нужный режим окна (полноэкранный или оконный).
	ApplyWindowMode(bIsFullscreenWidget);
	// 2. Устанавливаем соответствующий режим ввода и видимость курсора.
	//    !bIsFullscreenWidget передается в bIsUIOnly и bShowMouse:
	//    - Если НЕ полноэкранный (оконный): bIsUIOnly=true, bShowMouse=true.
	//    - Если полноэкранный: bIsUIOnly=false, bShowMouse=false.
	SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget);

	// 3. Удаляем предыдущий виджет верхнего уровня, если он существует.
	if (CurrentTopLevelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous top level widget: %s"), *CurrentTopLevelWidget->GetName());
		// Удаляем виджет из Viewport и уничтожаем его.
		CurrentTopLevelWidget->RemoveFromParent();
	}
	// Обнуляем указатели на старые виджеты.
	CurrentTopLevelWidget = nullptr;
	CurrentContainerInstance = nullptr; // Сбрасываем контейнер, т.к. показываем новый виджет верхнего уровня.

	// 4. Создаем новый экземпляр виджета указанного класса.
	//    CreateWidget<T> использует шаблонный тип T для возвращаемого значения.
	T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
	// Проверяем, успешно ли создан виджет.
	if (NewWidget)
	{
		// Добавляем созданный виджет в Viewport (делаем видимым).
		NewWidget->AddToViewport();
		// Запоминаем указатель на новый виджет как текущий виджет верхнего уровня.
		CurrentTopLevelWidget = NewWidget;
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Added new widget: %s"), *WidgetClassToShow->GetName());
		// Возвращаем указатель на созданный виджет.
		return NewWidget;
	}
	else
	{
		// Логируем ошибку, если не удалось создать виджет.
		UE_LOG(LogTemp, Error, TEXT("ShowWidget: Failed to create widget: %s"), *WidgetClassToShow->GetName());
		// Возвращаем nullptr в случае ошибки.
		return nullptr;
	}
}


// =============================================================================
// Функции Навигации
// =============================================================================

// Показывает стартовый экран (в оконном режиме внутри контейнера).
void UMyGameInstance::ShowStartScreen()
{
	// Проверяем, назначены ли классы контейнера и стартового экрана в Blueprint.
	if (!WindowContainerClass || !StartScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: WindowContainerClass or StartScreenClass is not set!"));
		return; // Выходим, если классы не заданы.
	}
	// Вызываем вспомогательную функцию ShowWidget для показа контейнера.
	// false указывает, что нужен оконный режим. Возвращаем как UUserWidget*.
	UUserWidget* Container = ShowWidget<UUserWidget>(WindowContainerClass, false);

	// Если контейнер успешно создан и показан.
	if (Container)
	{
		// Запоминаем указатель на текущий экземпляр контейнера.
		CurrentContainerInstance = Container;

		// Вызываем Blueprint-функцию "SetContentWidget" внутри контейнера.
		FName FunctionName = FName(TEXT("SetContentWidget")); // Точное имя функции в Blueprint.
		// Ищем функцию по имени в классе контейнера.
		UFunction* Function = CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName);
		// Если функция найдена.
		if (Function)
		{
			// Создаем структуру для передачи параметра (класса виджета).
			struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
			FSetContentParams Params;
			// Устанавливаем параметр - класс стартового экрана.
			Params.WidgetClassToSet = StartScreenClass;
			// Вызываем Blueprint-функцию через ProcessEvent, передавая параметры.
			CurrentContainerInstance->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Log, TEXT("ShowStartScreen: Set content to StartScreenClass."));
		}
		else {
			// Логируем ошибку, если функция не найдена в контейнере.
			UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: Function 'SetContentWidget' not found in %s!"), *WindowContainerClass->GetName());
		}
	}
	// Останавливаем таймер загрузки, если он был запущен.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

// Показывает экран логина (в оконном режиме внутри контейнера).
void UMyGameInstance::ShowLoginScreen()
{
	// Проверяем, назначены ли классы контейнера и экрана логина.
	if (!WindowContainerClass || !LoginScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: WindowContainerClass or LoginScreenClass is not set!"));
		return;
	}
	// Пытаемся получить текущий активный контейнер.
	UUserWidget* Container = CurrentContainerInstance;
	// Если контейнера нет или текущий виджет верхнего уровня - не этот контейнер.
	if (!Container || CurrentTopLevelWidget != CurrentContainerInstance)
	{
		// Создаем и показываем контейнер заново.
		Container = ShowWidget<UUserWidget>(WindowContainerClass, false);
		// Если создание успешно, запоминаем новый контейнер.
		if (Container) CurrentContainerInstance = Container;
		// Если создать не удалось, выходим.
		else return;
	}
	else {
		// Контейнер уже существует и активен, просто настраиваем режим ввода/мыши для UI.
		SetupInputMode(true, true);
	}

	// Вызываем Blueprint-функцию "SetContentWidget" в контейнере, чтобы установить LoginScreenClass.
	FName FunctionName = FName(TEXT("SetContentWidget"));
	// Ищем функцию в классе текущего контейнера.
	UFunction* Function = CurrentContainerInstance ? CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName) : nullptr;
	// Если функция найдена.
	if (Function)
	{
		// Готовим параметры.
		struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
		FSetContentParams Params;
		Params.WidgetClassToSet = LoginScreenClass; // Указываем класс экрана логина.
		// Вызываем функцию.
		CurrentContainerInstance->ProcessEvent(Function, &Params);
		UE_LOG(LogTemp, Log, TEXT("ShowLoginScreen: Set content to LoginScreenClass."));
	}
	else { UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: Could not find SetContentWidget in container.")); }
	// Останавливаем таймер загрузки.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

// Показывает экран регистрации (в оконном режиме внутри контейнера).
void UMyGameInstance::ShowRegisterScreen()
{
	// Проверяем классы.
	if (!WindowContainerClass || !RegisterScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: WindowContainerClass or RegisterScreenClass is not set!"));
		return;
	}
	// Логика получения/создания контейнера аналогична ShowLoginScreen.
	UUserWidget* Container = CurrentContainerInstance;
	if (!Container || CurrentTopLevelWidget != CurrentContainerInstance)
	{
		Container = ShowWidget<UUserWidget>(WindowContainerClass, false);
		if (Container) CurrentContainerInstance = Container;
		else return;
	}
	else {
		SetupInputMode(true, true);
	}

	// Вызываем "SetContentWidget" в контейнере для RegisterScreenClass.
	FName FunctionName = FName(TEXT("SetContentWidget"));
	UFunction* Function = CurrentContainerInstance ? CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName) : nullptr;
	if (Function)
	{
		struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
		FSetContentParams Params;
		Params.WidgetClassToSet = RegisterScreenClass; // Указываем класс экрана регистрации.
		CurrentContainerInstance->ProcessEvent(Function, &Params);
		UE_LOG(LogTemp, Log, TEXT("ShowRegisterScreen: Set content to RegisterScreenClass."));
	}
	else { UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: Could not find SetContentWidget in container.")); }
	// Останавливаем таймер загрузки.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

// Показывает экран загрузки (полноэкранный режим).
void UMyGameInstance::ShowLoadingScreen(float Duration)
{
	// Проверяем класс экрана загрузки.
	if (!LoadingScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowLoadingScreen: LoadingScreenClass is not set!")); return; }

	// Вызываем ShowWidget для показа LoadingScreenClass. true означает полноэкранный режим.
	UUserWidget* LoadingWidget = ShowWidget<UUserWidget>(LoadingScreenClass, true);

	// Если виджет успешно создан.
	if (LoadingWidget)
	{
		// Очищаем предыдущий таймер (на всякий случай).
		GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
		// Запускаем новый таймер, который по завершении вызовет OnLoadingScreenTimerComplete.
		// Duration - время задержки, false - таймер не повторяющийся.
		GetWorld()->GetTimerManager().SetTimer(LoadingScreenTimerHandle, this, &UMyGameInstance::OnLoadingScreenTimerComplete, Duration, false);
		UE_LOG(LogTemp, Log, TEXT("ShowLoadingScreen: Timer Started for %.2f seconds."), Duration);
	}
}

// Функция обратного вызова для таймера экрана загрузки.
void UMyGameInstance::OnLoadingScreenTimerComplete()
{
	UE_LOG(LogTemp, Log, TEXT("Loading Screen Timer Complete. Showing Main Menu."));
	// Просто вызываем функцию показа главного меню.
	ShowMainMenu();
}

// Показывает главное меню (полноэкранный режим).
void UMyGameInstance::ShowMainMenu()
{
	// Проверяем класс главного меню.
	if (!MainMenuClass) { UE_LOG(LogTemp, Error, TEXT("ShowMainMenu: MainMenuClass is not set!")); return; }
	// Вызываем ShowWidget для показа MainMenuClass. true означает полноэкранный режим.
	ShowWidget<UUserWidget>(MainMenuClass, true);
	// Останавливаем таймер загрузки, если он еще не завершился.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

// Показывает экран настроек (предполагаем полноэкранный режим).
void UMyGameInstance::ShowSettingsScreen()
{
	// Проверяем класс.
	if (!SettingsScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowSettingsScreen: SettingsScreenClass is not set!")); return; }
	// Показываем как полноэкранный.
	ShowWidget<UUserWidget>(SettingsScreenClass, true);
	// Останавливаем таймер.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

// Показывает экран профиля (предполагаем полноэкранный режим).
void UMyGameInstance::ShowProfileScreen()
{
	// Проверяем класс.
	if (!ProfileScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowProfileScreen: ProfileScreenClass is not set!")); return; }
	// Показываем как полноэкранный.
	ShowWidget<UUserWidget>(ProfileScreenClass, true);
	// Останавливаем таймер.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}


// =============================================================================
// Управление Состоянием Игры
// =============================================================================

// Устанавливает статус логина пользователя.
void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
	// Устанавливаем флаг логина.
	bIsLoggedIn = bNewIsLoggedIn;
	// Если залогинен, устанавливаем ID и имя, иначе сбрасываем в значения по умолчанию.
	LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1;
	LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT("");
	// Если пользователь залогинился, он не может быть в оффлайн режиме.
	if (bIsLoggedIn) {
		bIsInOfflineMode = false;
	}
	// Логируем изменение статуса.
	UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username=%s"), bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername);
}

// Устанавливает оффлайн режим.
void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
	// Устанавливаем флаг оффлайн режима.
	bIsInOfflineMode = bNewIsOffline;
	// Если перешли в оффлайн режим, сбрасываем статус логина.
	if (bIsInOfflineMode) {
		SetLoginStatus(false, -1, TEXT(""));
	}
	// Логируем изменение статуса.
	UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));

	// Если включен оффлайн режим.
	if (bIsInOfflineMode)
	{
		// Обычно после выбора оффлайн режима показываем главное меню (или специальное оффлайн лобби).
		// ShowMainMenu() уже настроен на показ в полноэкранном режиме.
		ShowMainMenu();
	}
}


// =============================================================================
// Логика HTTP Запросов
// =============================================================================

// Отправляет запрос на логин пользователя.
void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password)
{
	UE_LOG(LogTemp, Log, TEXT("RequestLogin: Attempting login for user: %s"), *Username);

	// 1. Создаем JSON объект для тела запроса. TSharedPtr - умный указатель для управления памятью.
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	// Добавляем поля в JSON.
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);

	// 2. Сериализуем JSON объект в строку FString.
	FString RequestBody;
	// Создаем JsonWriter, который будет писать в строку RequestBody. TSharedRef - умная ссылка.
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	// Выполняем сериализацию.
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		// Логируем ошибку, если сериализация не удалась.
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to serialize JSON body."));
		// Показываем ошибку пользователю.
		DisplayLoginError(TEXT("Client error: Could not create request."));
		return; // Прерываем выполнение.
	}

	// 3. Получаем доступ к HTTP модулю.
	FHttpModule& HttpModule = FHttpModule::Get();
	// Создаем объект HTTP запроса. ESPMode::ThreadSafe важен для асинхронных операций.
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	// 4. Настраиваем запрос.
	HttpRequest->SetVerb(TEXT("POST")); // Метод запроса.
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/login")); // Полный URL эндпоинта.
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json")); // Указываем тип контента.
	HttpRequest->SetContentAsString(RequestBody); // Устанавливаем тело запроса (сериализованный JSON).

	// 5. Привязываем функцию обратного вызова (callback), которая будет вызвана по завершении запроса.
	// BindUObject привязывает метод OnLoginResponseReceived текущего объекта (*this).
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnLoginResponseReceived);

	// 6. Отправляем запрос в сеть.
	if (!HttpRequest->ProcessRequest())
	{
		// Логируем ошибку, если не удалось даже начать отправку запроса.
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayLoginError(TEXT("Network error: Could not start request."));
	}
	else {
		// Логируем успешную отправку.
		UE_LOG(LogTemp, Log, TEXT("RequestLogin: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/login")));
		// Здесь можно добавить логику показа индикатора загрузки в UI.
	}
}

// Отправляет запрос на регистрацию пользователя.
void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
	UE_LOG(LogTemp, Log, TEXT("RequestRegister: Attempting registration for user: %s, email: %s"), *Username, *Email);

	// 1. Создаем JSON объект.
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);
	RequestJson->SetStringField(TEXT("email"), Email); // Добавляем поле email.

	// 2. Сериализуем JSON в строку.
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("RequestRegister: Failed to serialize JSON body."));
		DisplayRegisterError(TEXT("Client error: Could not create request."));
		return;
	}

	// 3. Получаем HTTP модуль и создаем запрос.
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	// 4. Настраиваем запрос.
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/register")); // URL эндпоинта регистрации.
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	// 5. Привязываем callback для ответа на регистрацию.
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnRegisterResponseReceived);

	// 6. Отправляем запрос.
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestRegister: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayRegisterError(TEXT("Network error: Could not start request."));
	}
	else {
		UE_LOG(LogTemp, Log, TEXT("RequestRegister: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/register")));
	}
}


// =============================================================================
// Обработка Ответов HTTP
// =============================================================================

// Функция обратного вызова, вызываемая по завершении HTTP запроса на логин.
void UMyGameInstance::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// Проверяем базовый успех запроса на сетевом уровне и валидность ответа.
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayLoginError(TEXT("Network error or server unavailable."));
		return; // Выходим, если связи нет.
	}

	// Получаем код ответа HTTP (например, 200, 401, 500).
	int32 ResponseCode = Response->GetResponseCode();
	// Получаем тело ответа как строку (может содержать JSON или текст ошибки).
	FString ResponseBody = Response->GetContentAsString();
	// Логируем код и тело ответа.
	UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	// Обрабатываем успешный ответ (200 OK).
	if (ResponseCode == 200)
	{
		// Пытаемся десериализовать тело ответа как JSON объект.
		TSharedPtr<FJsonObject> ResponseJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
		{
			// Если JSON успешно распарсен, пытаемся извлечь поля userId и username.
			int64 ReceivedUserId = -1;
			FString ReceivedUsername;
			// TryGet... методы безопаснее, чем Get... так как не вызовут крэш, если поля нет.
			if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
				ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername))
			{
				// Если оба поля успешно извлечены.
				UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Login successful for user: %s (ID: %lld)"), *ReceivedUsername, ReceivedUserId);
				// Обновляем статус логина в GameInstance.
				SetLoginStatus(true, ReceivedUserId, ReceivedUsername);
				// Показываем экран загрузки (который затем покажет главное меню).
				ShowLoadingScreen(); // Используем длительность по умолчанию.
			}
			else {
				// Ошибка: не удалось найти нужные поля в JSON.
				UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to parse 'userId' or 'username' from JSON response."));
				DisplayLoginError(TEXT("Server error: Invalid response format."));
			}
		}
		else {
			// Ошибка: не удалось распарсить JSON из тела ответа.
			UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to deserialize JSON response. Body: %s"), *ResponseBody);
			DisplayLoginError(TEXT("Server error: Could not parse response."));
		}
	}
	// Обрабатываем ошибку "Не авторизован" (неверный логин/пароль).
	else if (ResponseCode == 401)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnLoginResponseReceived: Login failed - Invalid credentials (401)."));
		// Показываем пользователю сообщение о неверных данных.
		DisplayLoginError(TEXT("Login failed: Incorrect username or password."));
	}
	// Обрабатываем все остальные коды ошибок.
	else {
		// Формируем сообщение об ошибке по умолчанию.
		FString ErrorMessage = FString::Printf(TEXT("Login failed: Server error (Code: %d)"), ResponseCode);
		// Пытаемся извлечь более детальное сообщение из тела ответа (если сервер шлет JSON с полем "message").
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				// Используем сообщение от сервера, если оно есть.
				ErrorMessage = FString::Printf(TEXT("Login failed: %s"), *ServerErrorMsg);
			}
		}
		// Логируем и показываем ошибку пользователю.
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: %s"), *ErrorMessage);
		DisplayLoginError(ErrorMessage);
	}
}

// Функция обратного вызова, вызываемая по завершении HTTP запроса на регистрацию.
void UMyGameInstance::OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// Проверяем базовый успех запроса.
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayRegisterError(TEXT("Network error or server unavailable."));
		return;
	}

	// Получаем код и тело ответа.
	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	// Обрабатываем успешный ответ (201 Created).
	if (ResponseCode == 201)
	{
		UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Registration successful!"));
		// Переходим на экран логина, чтобы пользователь мог войти с новыми данными.
		ShowLoginScreen();
		// Используем таймер с небольшой задержкой, чтобы сообщение об успехе появилось уже ПОСЛЕ
		// того, как экран логина будет показан.
		FTimerHandle TempTimerHandle;
		// Проверяем, валиден ли World перед использованием TimerManager.
		if (GetWorld())
		{
			// Запускаем таймер, который через 0.1 секунды вызовет лямбда-функцию.
			GetWorld()->GetTimerManager().SetTimer(TempTimerHandle, [this]() {
				// Лямбда-функция вызывает DisplayLoginSuccessMessage. [this] захватывает текущий объект.
				DisplayLoginSuccessMessage(TEXT("Registration successful! Please log in."));
				}, 0.1f, false); // 0.1f - задержка, false - не повторять.
		}
	}
	// Обрабатываем ошибку "Конфликт" (пользователь/email уже существует).
	else if (ResponseCode == 409)
	{
		// Сообщение по умолчанию.
		FString ErrorMessage = TEXT("Registration failed: Username or Email already exists.");
		// Пытаемся получить более детальное сообщение от сервера.
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
		}
		// Логируем и показываем ошибку.
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (409)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	// Обрабатываем ошибку "Неверный запрос" (ошибка валидации на сервере).
	else if (ResponseCode == 400)
	{
		// Сообщение по умолчанию.
		FString ErrorMessage = TEXT("Registration failed: Invalid data provided.");
		// Пытаемся получить более детальное сообщение от сервера.
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
			// Можно добавить парсинг массива 'errors', если сервер его шлет для ошибок по полям.
		}
		// Логируем и показываем ошибку.
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (400)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	// Обрабатываем все остальные коды ошибок.
	else {
		FString ErrorMessage = FString::Printf(TEXT("Registration failed: Server error (Code: %d)"), ResponseCode);
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: %s"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
}


// =============================================================================
// Поиск Виджета Внутри Контейнера
// =============================================================================

// Вспомогательная функция для поиска виджета заданного класса внутри контейнера.
UUserWidget* UMyGameInstance::FindWidgetInContainer(TSubclassOf<UUserWidget> WidgetClassToFind) const // const - функция не меняет объект
{
	// Проверяем, что контейнер и искомый класс валидны.
	if (!CurrentContainerInstance || !WidgetClassToFind)
	{
		return nullptr; // Нечего искать или негде искать.
	}

	// Имя переменной UBorder в WBP_WindowContainer, где лежит контент.
	FName BorderVariableName = FName(TEXT("ContentSlotBorder"));
	// Используем рефлексию UE для поиска свойства (переменной) по имени в классе контейнера.
	// FindFProperty ищет FProperty (базовый класс для всех свойств). Нам нужен FObjectProperty.
	FObjectProperty* BorderProp = FindFProperty<FObjectProperty>(CurrentContainerInstance->GetClass(), BorderVariableName);

	// Если свойство найдено.
	if (BorderProp)
	{
		// Получаем значение этого свойства (указатель на UObject) из экземпляра контейнера.
		UObject* BorderObject = BorderProp->GetObjectPropertyValue_InContainer(CurrentContainerInstance);
		// Пытаемся привести (cast) полученный UObject к UPanelWidget.
		// UBorder наследуется от UPanelWidget, что позволяет получить дочерние элементы.
		UPanelWidget* ContentPanel = Cast<UPanelWidget>(BorderObject);

		// Если приведение успешно и у панели есть дочерние элементы.
		if (ContentPanel && ContentPanel->GetChildrenCount() > 0)
		{
			// Получаем первый (и предполагаем, что единственный) дочерний элемент.
			UWidget* ChildWidget = ContentPanel->GetChildAt(0);
			// Проверяем, что дочерний элемент существует и его класс совпадает с искомым классом.
			if (ChildWidget && ChildWidget->IsA(WidgetClassToFind))
			{
				// Если все совпало, приводим к UUserWidget и возвращаем.
				return Cast<UUserWidget>(ChildWidget);
			}
			// Логируем предупреждение, если дочерний виджет не того класса.
			else if (ChildWidget) {
				UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: Child widget %s is not of expected class %s"),
					*ChildWidget->GetName(), *WidgetClassToFind->GetName());
			}
		}
		// Логируем, если у панели нет дочерних элементов.
		else if (ContentPanel) {
			UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: ContentSlotBorder has no children."));
		}
		// Логируем, если не удалось привести к UPanelWidget.
		else {
			UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: Failed to cast ContentSlotBorder object to UPanelWidget."));
		}
	}
	// Логируем ошибку, если не удалось найти свойство 'ContentSlotBorder'.
	// Убедитесь, что переменная UBorder в WBP_WindowContainer называется 'ContentSlotBorder' и помечена UPROPERTY() (например, через 'Is Variable').
	else {
		UE_LOG(LogTemp, Error, TEXT("FindWidgetInContainer: Could not find FObjectProperty 'ContentSlotBorder' in %s. Make sure the variable exists and is public/UPROPERTY()."),
			*CurrentContainerInstance->GetClass()->GetName());
	}

	// Возвращаем nullptr, если виджет не найден.
	return nullptr;
}


// =============================================================================
// Вызов Функций Отображения Сообщений в Конкретных Виджетах Контента
// =============================================================================

// Показывает сообщение об ошибке логина.
void UMyGameInstance::DisplayLoginError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: %s"), *Message);

	// Ищем активный экземпляр WBP_LoginScreen внутри контейнера.
	UUserWidget* LoginWidget = FindWidgetInContainer(LoginScreenClass);

	// Если виджет найден.
	if (LoginWidget)
	{
		// Имя Blueprint-функции, которую нужно вызвать в WBP_LoginScreen.
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		// Ищем эту функцию в классе найденного виджета.
		UFunction* Function = LoginWidget->GetClass()->FindFunctionByName(FunctionName);
		// Если функция найдена.
		if (Function)
		{
			// Готовим параметры.
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			// Вызываем Blueprint-функцию на найденном виджете логина.
			LoginWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginError: Called DisplayErrorMessage on WBP_LoginScreen."));
		}
		else {
			// Логируем ошибку, если функция не найдена в WBP_LoginScreen.
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginError: Function 'DisplayErrorMessage' not found in WBP_LoginScreen!"));
		}
	}
	else {
		// Логируем предупреждение, если не удалось найти активный WBP_LoginScreen.
		UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: Could not find active WBP_LoginScreen inside container to display message."));
	}
}

// Показывает сообщение об ошибке регистрации.
void UMyGameInstance::DisplayRegisterError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: %s"), *Message);

	// Ищем активный экземпляр WBP_RegisterScreen внутри контейнера.
	UUserWidget* RegisterWidget = FindWidgetInContainer(RegisterScreenClass);

	// Если виджет найден.
	if (RegisterWidget)
	{
		// Имя функции ошибки.
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		// Ищем функцию.
		UFunction* Function = RegisterWidget->GetClass()->FindFunctionByName(FunctionName);
		// Если найдена.
		if (Function)
		{
			// Готовим параметры и вызываем.
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			RegisterWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayRegisterError: Called DisplayErrorMessage on WBP_RegisterScreen."));
		}
		else {
			// Логируем ошибку.
			UE_LOG(LogTemp, Error, TEXT("DisplayRegisterError: Function 'DisplayErrorMessage' not found in WBP_RegisterScreen!"));
		}
	}
	else {
		// Логируем предупреждение.
		UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: Could not find active WBP_RegisterScreen inside container to display message."));
	}
}

// Показывает сообщение об успехе регистрации (обычно на экране логина).
void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("DisplayLoginSuccessMessage: %s"), *Message);

	// Ищем активный экземпляр WBP_LoginScreen (т.к. мы перешли на него после успешной регистрации).
	UUserWidget* LoginWidget = FindWidgetInContainer(LoginScreenClass);

	// Если виджет найден.
	if (LoginWidget)
	{
		// Имена функций: предпочтительная для успеха и запасная (функция ошибки).
		FName SuccessFuncName = FName(TEXT("DisplaySuccessMessage"));
		FName ErrorFuncName = FName(TEXT("DisplayErrorMessage"));

		// Пытаемся найти функцию для успеха.
		UFunction* FunctionToCall = LoginWidget->GetClass()->FindFunctionByName(SuccessFuncName);
		// Если функция успеха не найдена.
		if (!FunctionToCall)
		{
			// Логируем, что используем запасной вариант.
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Function '%s' not found, falling back to '%s'."), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
			// Пытаемся найти функцию ошибки.
			FunctionToCall = LoginWidget->GetClass()->FindFunctionByName(ErrorFuncName);
		}

		// Если удалось найти хотя бы одну из функций.
		if (FunctionToCall)
		{
			// Готовим параметры и вызываем найденную функцию.
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			LoginWidget->ProcessEvent(FunctionToCall, &Params);
			// Логируем, какая именно функция была вызвана.
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Called %s on WBP_LoginScreen."), *FunctionToCall->GetName());
		}
		else {
			// Логируем ошибку, если не найдена ни функция успеха, ни функция ошибки.
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginSuccessMessage: Neither '%s' nor '%s' found in WBP_LoginScreen!"), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
		}
	}
	else {
		// Логируем предупреждение, если не найден активный WBP_LoginScreen.
		UE_LOG(LogTemp, Warning, TEXT("DisplayLoginSuccessMessage: Could not find active WBP_LoginScreen inside container to display message."));
	}
}