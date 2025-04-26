// Включаем заголовочный файл нашего кастомного GameInstance.
#include "MyGameInstance.h"

// --- Основные инклуды Unreal Engine ---
// Класс для управления локальным игроком: ввод, отображение мыши, создание виджетов.
#include "GameFramework/PlayerController.h"
// Класс для доступа и изменения настроек игры пользователя (разрешение, режим окна).
#include "GameFramework/GameUserSettings.h"
// Глобальный объект движка, через который можно получить доступ ко многим подсистемам, включая GameUserSettings.
#include "Engine/Engine.h"
// Набор статических утилитарных функций для общих игровых задач (получение контроллера, управление уровнями и т.д.).
#include "Kismet/GameplayStatics.h"
// Система для управления таймерами (одноразовыми или повторяющимися).
#include "TimerManager.h"
// Базовый класс для всех виджетов, создаваемых в UMG (Unreal Motion Graphics).
#include "Blueprint/UserWidget.h"
// Базовый класс для виджетов-контейнеров (как Border, VerticalBox и т.д.), позволяет работать с дочерними элементами.
#include "Components/PanelWidget.h"
// Необходим для работы с системой рефлексии Unreal (интроспекция типов), используется для поиска свойств по имени.
#include "UObject/UnrealType.h"
// Включаем наш класс для управления логикой оффлайн игры.
#include "OfflineGameManager.h"

// --- Инклуды для работы с HTTP и JSON ---
// Модуль Unreal Engine, предоставляющий функциональность для HTTP запросов.
#include "HttpModule.h"
// Интерфейс для создания, настройки и отправки HTTP запроса.
#include "Interfaces/IHttpRequest.h"
// Интерфейс для получения и анализа ответа на HTTP запрос.
#include "Interfaces/IHttpResponse.h"
// Базовые классы для представления JSON данных (FJsonObject, FJsonValue).
#include "Json.h"
// Вспомогательные утилиты для работы с JSON (не используются напрямую в этом коде, но могут быть полезны).
#include "JsonUtilities.h"
// Класс для преобразования между структурами данных C++ (через FJsonObject) и строковым представлением JSON.
#include "Serialization/JsonSerializer.h"


// =============================================================================
// Инициализация и Завершение Работы GameInstance
// =============================================================================


/*@brief <краткое описание>
Назначение: Предоставляет краткое, однострочное описание сущности(класса, функции, переменной, enum и т.д.), к которой относится комментарий.
@param <имя_параметра> <описание>
Назначение: Описывает входной параметр функции или метода.
@return <описание>
Назначение: Описывает значение, возвращаемое функцией или методом.
@warning <описание предупреждения>
Назначение: Выделяет важное предупреждение или информацию о потенциальных проблемах, побочных эффектах или особых условиях использования.
@note <описание заметки>
Назначение: Добавляет дополнительную заметку или примечание, которое не является ни основным описанием, ни предупреждением, но может быть полезно для понимания.
*/


/**
 * @brief Вызывается один раз при создании экземпляра GameInstance (в начале игры).
 * Идеальное место для инициализации глобальных менеджеров и систем,
 * которые должны существовать на протяжении всей сессии игры.
 */
void UMyGameInstance::Init()
{
	// Обязательно вызываем Init() базового класса UGameInstance.
	Super::Init();

	// --- Создание Менеджера Оффлайн Игры ---
	// Создаем экземпляр UOfflineGameManager. 'this' (текущий UMyGameInstance)
	// передается как Outer (владелец), связывая жизненные циклы.
	OfflineGameManager = NewObject<UOfflineGameManager>(this);
	// Проверяем, успешно ли был создан объект.
	if (OfflineGameManager)
	{
		// Логируем успех.
		UE_LOG(LogTemp, Log, TEXT("OfflineGameManager created successfully."));
	}
	else {
		// Логируем критическую ошибку, если создать не удалось.
		UE_LOG(LogTemp, Error, TEXT("Failed to create OfflineGameManager!"));
	}

	// Логируем завершение основной инициализации GameInstance.
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Initialized."));
}

/**
 * @brief Вызывается при уничтожении экземпляра GameInstance (при закрытии игры).
 * Место для освобождения ресурсов, сохранения прогресса и т.п.
 */
void UMyGameInstance::Shutdown()
{
	// Логируем начало процесса завершения работы.
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown."));
	// Обязательно вызываем Shutdown() базового класса.
	Super::Shutdown();
}


// =============================================================================
// Вспомогательные Функции для Управления UI, Окном и Вводом
// =============================================================================

/**
 * @brief Настраивает режим ввода для PlayerController и видимость курсора мыши.
 * @param bIsUIOnly Если true, устанавливается режим "только UI" (ввод обрабатывается только виджетами). Если false, "игра и UI".
 * @param bShowMouse Управляет видимостью системного курсора мыши.
 */
void UMyGameInstance::SetupInputMode(bool bIsUIOnly, bool bShowMouse)
{
	// Получаем указатель на первого локального игрока-контроллера.
	APlayerController* PC = GetFirstLocalPlayerController();
	// Всегда проверяем указатели перед использованием.
	if (PC)
	{
		// Устанавливаем видимость курсора.
		PC->SetShowMouseCursor(bShowMouse);
		// Выбираем режим ввода.
		if (bIsUIOnly)
		{
			// Режим "Только UI": ввод идет только на виджеты, игра "на паузе".
			FInputModeUIOnly InputModeData;
			// Не блокировать курсор в пределах окна, чтобы можно было легко кликнуть вне его.
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// Применяем режим.
			PC->SetInputMode(InputModeData);
			// Логируем (Verbose - детальный лог).
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to UI Only. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
		else
		{
			// Режим "Игра и UI": ввод идет и на виджеты, и в игру (если не перехвачен виджетами).
			FInputModeGameAndUI InputModeData;
			// Также не блокируем курсор.
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// Не скрывать курсор автоматически при клике в игровом окне (мы управляем им вручную).
			InputModeData.SetHideCursorDuringCapture(false);
			// Применяем режим.
			PC->SetInputMode(InputModeData);
			// Логируем.
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to Game And UI. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
	}
	else {
		// Предупреждение, если PlayerController не найден.
		UE_LOG(LogTemp, Warning, TEXT("SetupInputMode: PlayerController is null."));
	}
}

/**
 * @brief Устанавливает режим окна (оконный или полноэкранный) и соответствующее разрешение.
 * @param bWantFullscreen Если true, устанавливается полноэкранный режим (без рамок), иначе оконный.
 */
void UMyGameInstance::ApplyWindowMode(bool bWantFullscreen)
{
	// Получаем доступ к настройкам пользователя через GEngine.
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	// Проверяем, что настройки доступны.
	if (Settings)
	{
		// Определяем текущий и целевой режимы окна.
		EWindowMode::Type CurrentMode = Settings->GetFullscreenMode();
		// Используем WindowedFullscreen для полноэкранного режима (обычно лучший вариант).
		EWindowMode::Type TargetMode = bWantFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;

		// Флаг для отслеживания изменений.
		bool bModeChanged = false;
		// Если режим нужно изменить.
		if (CurrentMode != TargetMode)
		{
			Settings->SetFullscreenMode(TargetMode); // Устанавливаем новый режим.
			bModeChanged = true;                    // Помечаем, что были изменения.
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting FullscreenMode to %s"), *UEnum::GetValueAsString(TargetMode)); // Логируем.
		}

		// Определяем целевое разрешение.
		FIntPoint TargetResolution;
		if (bWantFullscreen)
		{
			// Для полноэкранного режима используем нативное разрешение рабочего стола.
			TargetResolution = Settings->GetDesktopResolution();
		}
		else
		{
			// Для оконного режима сначала пытаемся взять последнее подтвержденное разрешение.
			TargetResolution = Settings->GetLastConfirmedScreenResolution();
			// Если оно невалидно, берем разрешение по умолчанию из настроек проекта.
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(Settings->GetDefaultResolution().X, Settings->GetDefaultResolution().Y);
			}
			// Если и оно невалидно, используем жестко заданное значение (наше "оконное" разрешение).
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(720, 540); // Запасное значение.
			}
		}

		// Если текущее разрешение отличается от целевого.
		if (Settings->GetScreenResolution() != TargetResolution)
		{
			Settings->SetScreenResolution(TargetResolution); // Устанавливаем новое разрешение.
			bModeChanged = true;                             // Помечаем, что были изменения.
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting Resolution to %dx%d"), TargetResolution.X, TargetResolution.Y); // Логируем.
		}

		// Если были какие-либо изменения (режима или разрешения).
		if (bModeChanged)
		{
			// Применяем все измененные настройки. false - не требовать подтверждения.
			Settings->ApplySettings(false);
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Settings Applied."));
		}
		else {
			// Логируем, если изменять ничего не пришлось.
			UE_LOG(LogTemp, Verbose, TEXT("ApplyWindowMode: Window Mode and Resolution already match target. No change needed."));
		}

	}
	else {
		// Предупреждение, если не удалось получить GameUserSettings.
		UE_LOG(LogTemp, Warning, TEXT("ApplyWindowMode: Could not get GameUserSettings."));
	}
}

/**
 * @brief Шаблонная функция для унифицированного создания, показа и управления виджетами верхнего уровня.
 * Удаляет предыдущий виджет верхнего уровня перед показом нового.
 * Настраивает режим окна и ввода в зависимости от флага bIsFullscreenWidget.
 * @tparam T Тип виджета, который нужно создать (по умолчанию UUserWidget). Позволяет вернуть корректный тип указателя.
 * @param WidgetClassToShow Класс виджета (TSubclassOf<UUserWidget>), экземпляр которого нужно создать.
 * @param bIsFullscreenWidget Если true, виджет считается полноэкранным (применяется полноэкранный режим окна, ввод GameAndUI, мышь скрыта).
 *                            Если false, виджет считается "оконным" (применяется оконный режим, ввод UIOnly, мышь показана).
 * @return T* Указатель на созданный экземпляр виджета типа T, или nullptr в случае ошибки.
 */
template <typename T>
T* UMyGameInstance::ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget)
{
	// Получаем PlayerController.
	APlayerController* PC = GetFirstLocalPlayerController();
	// Проверяем PlayerController и переданный класс виджета.
	if (!PC) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: PlayerController is null.")); return nullptr; }
	if (!WidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: WidgetClassToShow is null.")); return nullptr; }

	// --- Шаг 1: Настройка режима окна и ввода ---
	ApplyWindowMode(bIsFullscreenWidget);                 // Устанавливаем оконный/полноэкранный режим.
	SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget); // Устанавливаем режим ввода и видимость мыши (инвертировано от bIsFullscreenWidget).

	// --- Шаг 2: Удаление предыдущего виджета верхнего уровня ---
	// Если есть указатель на предыдущий виджет.
	if (CurrentTopLevelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous top level widget: %s"), *CurrentTopLevelWidget->GetName());
		// Удаляем виджет из вьюпорта и памяти.
		CurrentTopLevelWidget->RemoveFromParent();
	}
	// Обнуляем указатели на старые виджеты.
	CurrentTopLevelWidget = nullptr;
	CurrentContainerInstance = nullptr; // Также сбрасываем указатель на контейнер, если он был активен.

	// --- Шаг 3: Создание и показ нового виджета ---
	// Создаем экземпляр виджета нужного класса. CreateWidget возвращает тип UUserWidget*, но так как функция шаблонная, мы можем привести к T*.
	T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
	// Проверяем, успешно ли создан виджет.
	if (NewWidget)
	{
		// Добавляем виджет в основной вьюпорт игры.
		NewWidget->AddToViewport();
		// Сохраняем указатель на новый виджет как текущий виджет верхнего уровня.
		CurrentTopLevelWidget = NewWidget;
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Added new widget: %s"), *WidgetClassToShow->GetName());
		// Возвращаем указатель на созданный виджет.
		return NewWidget;
	}
	else
	{
		// Логируем ошибку создания виджета.
		UE_LOG(LogTemp, Error, TEXT("ShowWidget: Failed to create widget: %s"), *WidgetClassToShow->GetName());
		// Возвращаем nullptr.
		return nullptr;
	}
}


// =============================================================================
// Функции Навигации Между Экранами
// =============================================================================

/**
 * @brief Показывает стартовый экран приложения (например, с кнопками "Войти", "Оффлайн", "Выход").
 * Использует виджет-контейнер для имитации оконного режима.
 */
void UMyGameInstance::ShowStartScreen()
{
	// Проверяем, что классы виджетов контейнера и стартового экрана заданы в настройках GameInstance (в Blueprint).
	if (!WindowContainerClass || !StartScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: WindowContainerClass or StartScreenClass is not set!"));
		return; // Прерываем, если классы не заданы.
	}

	// Вызываем ShowWidget для отображения виджета-контейнера в оконном режиме (false).
	UUserWidget* Container = ShowWidget<UUserWidget>(WindowContainerClass, false);

	// Если контейнер был успешно создан и показан.
	if (Container)
	{
		// Сохраняем указатель на текущий экземпляр контейнера.
		CurrentContainerInstance = Container;

		// --- Вызов Blueprint-функции внутри контейнера для установки контента ---
		FName FunctionName = FName(TEXT("SetContentWidget")); // Имя функции в Blueprint (WBP_WindowContainer).
		// Ищем функцию по имени в классе виджета-контейнера.
		UFunction* Function = CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName);
		// Если функция найдена.
		if (Function)
		{
			// Подготавливаем структуру для передачи параметров в Blueprint-функцию.
			// Структура должна совпадать с параметрами функции в BP.
			struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
			FSetContentParams Params;
			Params.WidgetClassToSet = StartScreenClass; // Указываем класс стартового экрана как параметр.
			// Вызываем Blueprint-функцию через систему рефлексии ProcessEvent.
			CurrentContainerInstance->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Log, TEXT("ShowStartScreen: Set content to StartScreenClass."));
		}
		else {
			// Ошибка, если обязательная функция не найдена в виджете контейнера.
			UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: Function 'SetContentWidget' not found in %s!"), *WindowContainerClass->GetName());
		}
	}
	// Останавливаем таймер экрана загрузки, если он был активен (например, при возврате из игры).
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief Показывает экран входа пользователя (логин/пароль).
 * Использует виджет-контейнер.
 */
void UMyGameInstance::ShowLoginScreen()
{
	// Проверка наличия классов контейнера и экрана логина.
	if (!WindowContainerClass || !LoginScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: WindowContainerClass or LoginScreenClass is not set!"));
		return;
	}

	// --- Логика переиспользования/создания контейнера ---
	// Пытаемся получить текущий экземпляр контейнера.
	UUserWidget* Container = CurrentContainerInstance;
	// Если контейнера нет ИЛИ текущий виджет верхнего уровня - это НЕ наш контейнер
	// (значит, был показан какой-то другой виджет, например, полноэкранный).
	if (!Container || CurrentTopLevelWidget != CurrentContainerInstance)
	{
		// Нужно создать и показать контейнер заново.
		Container = ShowWidget<UUserWidget>(WindowContainerClass, false);
		// Если успешно создан, сохраняем указатель.
		if (Container) CurrentContainerInstance = Container;
		else return; // Если не удалось создать контейнер, выходим.
	}
	else {
		// Контейнер уже активен, просто убедимся, что режим ввода и мышь настроены для UI.
		SetupInputMode(true, true);
	}

	// --- Установка контента внутри контейнера ---
	FName FunctionName = FName(TEXT("SetContentWidget")); // Имя функции в BP.
	// Ищем функцию в классе ТЕКУЩЕГО контейнера.
	UFunction* Function = CurrentContainerInstance ? CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName) : nullptr;
	if (Function)
	{
		// Готовим параметры и вызываем функцию, передавая LoginScreenClass.
		struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
		FSetContentParams Params;
		Params.WidgetClassToSet = LoginScreenClass;
		CurrentContainerInstance->ProcessEvent(Function, &Params);
		UE_LOG(LogTemp, Log, TEXT("ShowLoginScreen: Set content to LoginScreenClass."));
	}
	else { UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: Could not find SetContentWidget in container.")); }
	// Останавливаем таймер загрузки.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief Показывает экран регистрации нового пользователя.
 * Использует виджет-контейнер.
 */
void UMyGameInstance::ShowRegisterScreen()
{
	// Проверка наличия классов.
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

	// Установка RegisterScreenClass как контента контейнера.
	FName FunctionName = FName(TEXT("SetContentWidget"));
	UFunction* Function = CurrentContainerInstance ? CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName) : nullptr;
	if (Function)
	{
		struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
		FSetContentParams Params;
		Params.WidgetClassToSet = RegisterScreenClass;
		CurrentContainerInstance->ProcessEvent(Function, &Params);
		UE_LOG(LogTemp, Log, TEXT("ShowRegisterScreen: Set content to RegisterScreenClass."));
	}
	else { UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: Could not find SetContentWidget in container.")); }
	// Останавливаем таймер загрузки.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief Показывает полноэкранный виджет "загрузки" на заданное время.
 * По истечении времени автоматически вызывает OnLoadingScreenTimerComplete (которая покажет главное меню).
 * @param Duration Время в секундах, на которое показывается экран загрузки.
 */
void UMyGameInstance::ShowLoadingScreen(float Duration)
{
	// Проверка класса виджета загрузки.
	if (!LoadingScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowLoadingScreen: LoadingScreenClass is not set!")); return; }

	// Показываем виджет загрузки в полноэкранном режиме (true).
	UUserWidget* LoadingWidget = ShowWidget<UUserWidget>(LoadingScreenClass, true);

	// Если виджет успешно создан.
	if (LoadingWidget)
	{
		// Останавливаем предыдущий таймер (если был).
		GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
		// Запускаем новый таймер.
		GetWorld()->GetTimerManager().SetTimer(
			LoadingScreenTimerHandle, // Хэндл таймера для возможности его остановки.
			this,                     // Объект, на котором будет вызван метод.
			&UMyGameInstance::OnLoadingScreenTimerComplete, // Указатель на метод-колбэк.
			Duration,                 // Задержка перед вызовом.
			false);                   // false - таймер не повторяющийся.
		UE_LOG(LogTemp, Log, TEXT("ShowLoadingScreen: Timer Started for %.2f seconds."), Duration);
	}
}

/**
 * @brief Метод, вызываемый по завершении таймера экрана загрузки.
 * Переводит игру на главный экран.
 */
void UMyGameInstance::OnLoadingScreenTimerComplete()
{
	UE_LOG(LogTemp, Log, TEXT("Loading Screen Timer Complete. Showing Main Menu."));
	// Просто вызываем функцию показа главного меню.
	ShowMainMenu();
}

/**
 * @brief Показывает главное меню игры (полноэкранный режим).
 */
void UMyGameInstance::ShowMainMenu()
{
	// Проверка класса.
	if (!MainMenuClass) { UE_LOG(LogTemp, Error, TEXT("ShowMainMenu: MainMenuClass is not set!")); return; }
	// Показываем виджет главного меню в полноэкранном режиме (true).
	ShowWidget<UUserWidget>(MainMenuClass, true);
	// Останавливаем таймер загрузки, если он еще работал.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief Показывает экран настроек (полноэкранный режим).
 */
void UMyGameInstance::ShowSettingsScreen()
{
	// Проверка класса.
	if (!SettingsScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowSettingsScreen: SettingsScreenClass is not set!")); return; }
	// Показ виджета.
	ShowWidget<UUserWidget>(SettingsScreenClass, true);
	// Остановка таймера.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief Показывает экран профиля пользователя (полноэкранный режим).
 */
void UMyGameInstance::ShowProfileScreen()
{
	// Проверка класса.
	if (!ProfileScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowProfileScreen: ProfileScreenClass is not set!")); return; }
	// Показ виджета.
	ShowWidget<UUserWidget>(ProfileScreenClass, true);
	// Остановка таймера.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}


// =============================================================================
// Управление Глобальным Состоянием Игры (Логин, Оффлайн Режим)
// =============================================================================

/**
 * @brief Обновляет статус логина пользователя и связанные данные (ID, имя).
 * @param bNewIsLoggedIn Новый статус логина (true - залогинен, false - нет).
 * @param NewUserId ID пользователя (если залогинен), иначе -1.
 * @param NewUsername Имя пользователя (если залогинен), иначе пустая строка.
 */
void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
	// Устанавливаем основные переменные состояния.
	bIsLoggedIn = bNewIsLoggedIn;
	LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1; // Используем тернарный оператор для установки ID или значения по умолчанию.
	LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT(""); // Устанавливаем имя или пустую строку.
	// Логическое следствие: если пользователь залогинен, он не находится в оффлайн режиме.
	if (bIsLoggedIn) {
		bIsInOfflineMode = false;
	}
	// Логируем измененное состояние.
	UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username=%s"), bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername);
}

/**
 * @brief Устанавливает или снимает флаг оффлайн режима.
 * При переходе в оффлайн режим сбрасывает статус логина.
 * @param bNewIsOffline Новый статус оффлайн режима (true - включен, false - выключен).
 */
void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
	// Устанавливаем флаг оффлайн режима.
	bIsInOfflineMode = bNewIsOffline;
	// Если пользователь выбрал оффлайн режим, он не может быть одновременно залогинен.
	if (bIsInOfflineMode) {
		// Вызываем SetLoginStatus, чтобы сбросить данные логина.
		SetLoginStatus(false, -1, TEXT(""));
	}
	// Логируем изменение статуса.
	UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));

	// Если перешли в оффлайн режим.
	if (bIsInOfflineMode)
	{
		// Показываем главное меню (из которого потом можно будет выбрать оффлайн игру).
		ShowMainMenu();
	}
}


// =============================================================================
// Отправка HTTP Запросов на Сервер (Аутентификация)
// =============================================================================

/**
 * @brief Инициирует асинхронный HTTP POST запрос на сервер для аутентификации пользователя.
 * @param Username Введенное пользователем имя.
 * @param Password Введенный пользователем пароль.
 */
void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password)
{
	// Логируем начало операции.
	UE_LOG(LogTemp, Log, TEXT("RequestLogin: Attempting login for user: %s"), *Username);

	// --- Шаг 1: Подготовка JSON тела запроса ---
	// Создаем умный указатель на объект JSON. MakeShareable удобен для этого.
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	// Заполняем JSON объект полями "username" и "password".
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);

	// --- Шаг 2: Сериализация JSON в строку ---
	FString RequestBody; // Строка для хранения результата сериализации.
	// Создаем JsonWriter, который будет писать в строку RequestBody.
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	// Выполняем сериализацию. Проверяем результат.
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		// Ошибка сериализации - прерываем операцию.
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to serialize JSON body."));
		// Сообщаем об ошибке пользователю через UI.
		DisplayLoginError(TEXT("Client error: Could not create request."));
		return;
	}

	// --- Шаг 3: Создание и настройка HTTP запроса ---
	// Получаем ссылку на синглтон HTTP модуля.
	FHttpModule& HttpModule = FHttpModule::Get();
	// Создаем объект запроса. Указание ESPMode::ThreadSafe важно для асинхронных операций.
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	// Настраиваем параметры запроса:
	HttpRequest->SetVerb(TEXT("POST"));                            // HTTP метод.
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/login"));             // URL эндпоинта логина.
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json")); // Тип передаваемого контента.
	HttpRequest->SetContentAsString(RequestBody);                   // Тело запроса (наш JSON).

	// --- Шаг 4: Привязка обработчика ответа ---
	// Устанавливаем функцию, которая будет вызвана, когда сервер ответит (или произойдет ошибка сети).
	// BindUObject(this, ...) привязывает метод OnLoginResponseReceived текущего объекта UMyGameInstance.
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnLoginResponseReceived);

	// --- Шаг 5: Отправка запроса ---
	// Инициируем отправку запроса. Это асинхронная операция.
	if (!HttpRequest->ProcessRequest())
	{
		// Если запрос даже не удалось начать отправлять (например, проблемы с HttpModule).
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayLoginError(TEXT("Network error: Could not start request."));
	}
	else {
		// Запрос успешно отправлен (но ответ еще не получен).
		UE_LOG(LogTemp, Log, TEXT("RequestLogin: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/login")));
		// В этом месте можно было бы показать пользователю индикатор загрузки.
	}
}

/**
 * @brief Инициирует асинхронный HTTP POST запрос на сервер для регистрации нового пользователя.
 * @param Username Желаемое имя пользователя.
 * @param Password Желаемый пароль.
 * @param Email Адрес электронной почты пользователя.
 */
void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
	// Логируем начало операции.
	UE_LOG(LogTemp, Log, TEXT("RequestRegister: Attempting registration for user: %s, email: %s"), *Username, *Email);

	// --- Шаг 1: Подготовка JSON тела запроса ---
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);
	RequestJson->SetStringField(TEXT("email"), Email); // Добавляем email в запрос.

	// --- Шаг 2: Сериализация JSON в строку ---
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("RequestRegister: Failed to serialize JSON body."));
		DisplayRegisterError(TEXT("Client error: Could not create request."));
		return;
	}

	// --- Шаг 3: Создание и настройка HTTP запроса ---
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/register")); // URL эндпоинта регистрации.
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	// --- Шаг 4: Привязка обработчика ответа ---
	// Привязываем другой метод-колбэк, специфичный для ответа на регистрацию.
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnRegisterResponseReceived);

	// --- Шаг 5: Отправка запроса ---
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
// Обработка Ответов от Сервера (Колбэки HTTP Запросов)
// =============================================================================

/**
 * @brief Метод обратного вызова (callback), который выполняется по завершении HTTP запроса на логин.
 * Анализирует ответ сервера и обновляет состояние игры или показывает ошибку.
 * @param Request Указатель на исходный запрос (можно получить доп. инфо, если нужно).
 * @param Response Указатель на ответ сервера.
 * @param bWasSuccessful Флаг, указывающий на успех выполнения запроса на сетевом уровне (НЕ означает успешный логин!).
 */
void UMyGameInstance::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// --- Шаг 1: Проверка базовой успешности запроса ---
	// Если запрос не удался на уровне сети ИЛИ объект ответа невалиден.
	if (!bWasSuccessful || !Response.IsValid())
	{
		// Вероятно, проблема с соединением или сервер недоступен.
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayLoginError(TEXT("Network error or server unavailable."));
		// Здесь можно было бы скрыть индикатор загрузки, если он показывался.
		return; // Выходим.
	}

	// --- Шаг 2: Анализ ответа сервера ---
	// Получаем HTTP код ответа (e.g., 200, 401).
	int32 ResponseCode = Response->GetResponseCode();
	// Получаем тело ответа как строку.
	FString ResponseBody = Response->GetContentAsString();
	// Логируем полученные данные.
	UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	// --- Шаг 3: Обработка в зависимости от кода ответа ---
	// Успешный логин (200 OK).
	if (ResponseCode == 200)
	{
		// Пытаемся распарсить тело ответа как JSON.
		TSharedPtr<FJsonObject> ResponseJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
		{
			// JSON успешно распарсен. Пытаемся извлечь необходимые данные (userId, username).
			int64 ReceivedUserId = -1;
			FString ReceivedUsername;
			// Используем TryGet... для безопасного извлечения.
			if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
				ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername))
			{
				// Данные успешно извлечены!
				UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Login successful for user: %s (ID: %lld)"), *ReceivedUsername, ReceivedUserId);
				// Обновляем глобальное состояние GameInstance.
				SetLoginStatus(true, ReceivedUserId, ReceivedUsername);
				// Инициируем переход к следующему экрану (через загрузку).
				ShowLoadingScreen();
			}
			else {
				// Ошибка: JSON корректный, но не содержит нужных полей.
				UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to parse 'userId' or 'username' from JSON response."));
				DisplayLoginError(TEXT("Server error: Invalid response format."));
			}
		}
		else {
			// Ошибка: не удалось распарсить тело ответа как JSON.
			UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to deserialize JSON response. Body: %s"), *ResponseBody);
			DisplayLoginError(TEXT("Server error: Could not parse response."));
		}
	}
	// Ошибка аутентификации (401 Unauthorized).
	else if (ResponseCode == 401)
	{
		// Неверный логин или пароль.
		UE_LOG(LogTemp, Warning, TEXT("OnLoginResponseReceived: Login failed - Invalid credentials (401)."));
		// Сообщаем пользователю.
		DisplayLoginError(TEXT("Login failed: Incorrect username or password."));
	}
	// Другие ошибки сервера (e.g., 500 Internal Server Error).
	else {
		// Формируем сообщение об ошибке по умолчанию.
		FString ErrorMessage = FString::Printf(TEXT("Login failed: Server error (Code: %d)"), ResponseCode);
		// Пытаемся получить более конкретное сообщение из тела ответа (если сервер шлет JSON с ошибкой).
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				// Используем сообщение от сервера.
				ErrorMessage = FString::Printf(TEXT("Login failed: %s"), *ServerErrorMsg);
			}
		}
		// Логируем и отображаем ошибку.
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: %s"), *ErrorMessage);
		DisplayLoginError(ErrorMessage);
	}
	// Здесь можно было бы скрыть индикатор загрузки.
}

/**
 * @brief Метод обратного вызова (callback), который выполняется по завершении HTTP запроса на регистрацию.
 * Анализирует ответ сервера и либо переводит на экран логина, либо показывает ошибку.
 * @param Request Указатель на исходный запрос.
 * @param Response Указатель на ответ сервера.
 * @param bWasSuccessful Флаг успеха выполнения запроса на сетевом уровне.
 */
void UMyGameInstance::OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// --- Шаг 1: Проверка базовой успешности запроса ---
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayRegisterError(TEXT("Network error or server unavailable."));
		return;
	}

	// --- Шаг 2: Анализ ответа сервера ---
	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	// --- Шаг 3: Обработка в зависимости от кода ответа ---
	// Успешная регистрация (201 Created).
	if (ResponseCode == 201)
	{
		UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Registration successful!"));
		// Перенаправляем пользователя на экран логина.
		ShowLoginScreen();
		// --- Показ сообщения об успехе на экране логина с небольшой задержкой ---
		FTimerHandle TempTimerHandle; // Временный хэндл для таймера.
		// Проверяем валидность World перед использованием таймера.
		if (GetWorld())
		{
			// Запускаем таймер, который выполнит лямбда-функцию через 0.1 секунды.
			GetWorld()->GetTimerManager().SetTimer(TempTimerHandle, [this]() {
				// Лямбда-функция вызывает показ сообщения об успехе.
				// [this] захватывает указатель на текущий объект UMyGameInstance.
				DisplayLoginSuccessMessage(TEXT("Registration successful! Please log in."));
				}, 0.1f, false); // false - таймер не повторяющийся.
		}
		// --- Конец показа сообщения ---
	}
	// Ошибка: Конфликт (имя пользователя или email уже заняты) (409 Conflict).
	else if (ResponseCode == 409)
	{
		// Сообщение по умолчанию.
		FString ErrorMessage = TEXT("Registration failed: Username or Email already exists.");
		// Пытаемся извлечь более детальное сообщение из ответа сервера.
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
	// Ошибка: Неверный запрос (ошибка валидации данных на сервере) (400 Bad Request).
	else if (ResponseCode == 400)
	{
		// Сообщение по умолчанию.
		FString ErrorMessage = TEXT("Registration failed: Invalid data provided.");
		// Пытаемся извлечь более детальное сообщение.
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
			// Здесь можно было бы добавить парсинг специфичных ошибок валидации по полям, если сервер их предоставляет.
		}
		// Логируем и показываем ошибку.
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (400)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	// Другие ошибки сервера.
	else {
		FString ErrorMessage = FString::Printf(TEXT("Registration failed: Server error (Code: %d)"), ResponseCode);
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: %s"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	// Здесь можно было бы скрыть индикатор загрузки.
}



// =============================================================================
// Вспомогательные Функции для Взаимодействия с Виджетами в Контейнере
// =============================================================================

/**
 * @brief Находит активный виджет указанного класса внутри виджета-контейнера.
 * Использует рефлексию для доступа к дочернему виджету, хранящемуся в слоте 'ContentSlotBorder'.
 * @param WidgetClassToFind Класс виджета, который мы ищем.
 * @return UUserWidget* Указатель на найденный виджет или nullptr, если не найден или произошла ошибка.
 * @note Функция помечена `const`, так как она не изменяет состояние GameInstance.
 */
UUserWidget* UMyGameInstance::FindWidgetInContainer(TSubclassOf<UUserWidget> WidgetClassToFind) const
{
	// Проверяем наличие активного контейнера и искомого класса.
	if (!CurrentContainerInstance || !WidgetClassToFind)
	{
		return nullptr; // Нечего искать или негде.
	}

	// Имя переменной (UPROPERTY) в классе WBP_WindowContainer, которая ссылается на UBorder, служащий слотом.
	FName BorderVariableName = FName(TEXT("ContentSlotBorder"));
	// Используем систему рефлексии UE для поиска свойства (переменной) типа FObjectProperty по имени.
	FObjectProperty* BorderProp = FindFProperty<FObjectProperty>(CurrentContainerInstance->GetClass(), BorderVariableName);

	// Если свойство (переменная UBorder) найдено в классе контейнера.
	if (BorderProp)
	{
		// Получаем значение этого свойства (указатель на UObject) из КОНКРЕТНОГО ЭКЗЕМПЛЯРА контейнера.
		UObject* BorderObject = BorderProp->GetObjectPropertyValue_InContainer(CurrentContainerInstance);
		// Пытаемся привести (cast) этот UObject к UPanelWidget (базовый класс для виджетов, имеющих дочерние элементы, включая UBorder).
		UPanelWidget* ContentPanel = Cast<UPanelWidget>(BorderObject);

		// Если приведение успешно и у панели есть дочерние элементы.
		if (ContentPanel && ContentPanel->GetChildrenCount() > 0)
		{
			// Получаем первый (и предполагаем, что единственный) дочерний виджет внутри UBorder.
			UWidget* ChildWidget = ContentPanel->GetChildAt(0);
			// Проверяем, что дочерний виджет существует и его класс является или наследуется от WidgetClassToFind.
			if (ChildWidget && ChildWidget->IsA(WidgetClassToFind))
			{
				// Если всё совпадает, приводим указатель к UUserWidget и возвращаем его.
				return Cast<UUserWidget>(ChildWidget);
			}
			// Логируем предупреждение, если дочерний виджет есть, но он не того класса, который мы искали.
			else if (ChildWidget) {
				UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: Child widget %s is not of expected class %s"),
					*ChildWidget->GetName(), *WidgetClassToFind->GetName());
			}
		}
		// Логируем предупреждение, если у панели (Border) нет дочерних виджетов.
		else if (ContentPanel) {
			UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: ContentSlotBorder has no children."));
		}
		// Логируем предупреждение, если объект, на который указывает ContentSlotBorder, не является UPanelWidget (маловероятно, но возможно).
		else {
			UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: Failed to cast ContentSlotBorder object to UPanelWidget."));
		}
	}
	// Логируем ошибку, если переменная с именем 'ContentSlotBorder' не найдена в классе контейнера.
	// Убедитесь, что в WBP_WindowContainer есть UBorder, он назван 'ContentSlotBorder', и у него включен флаг 'Is Variable'.
	else {
		UE_LOG(LogTemp, Error, TEXT("FindWidgetInContainer: Could not find FObjectProperty 'ContentSlotBorder' in %s. Make sure the variable exists and is public/UPROPERTY()."),
			*CurrentContainerInstance->GetClass()->GetName());
	}

	// Возвращаем nullptr во всех случаях, когда виджет не был успешно найден.
	return nullptr;
}


/**
 * @brief Находит активный виджет логина и вызывает на нем Blueprint-функцию DisplayErrorMessage.
 * @param Message Сообщение об ошибке для отображения.
 */
void UMyGameInstance::DisplayLoginError(const FString& Message)
{
	// Логируем сообщение об ошибке.
	UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: %s"), *Message);

	// Ищем активный экземпляр WBP_LoginScreen внутри нашего виджета-контейнера.
	UUserWidget* LoginWidget = FindWidgetInContainer(LoginScreenClass);

	// Если виджет логина найден (т.е., он сейчас активен внутри контейнера).
	if (LoginWidget)
	{
		// Имя Blueprint-функции, отвечающей за отображение ошибки в WBP_LoginScreen.
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		// Ищем эту функцию по имени в классе виджета логина.
		UFunction* Function = LoginWidget->GetClass()->FindFunctionByName(FunctionName);
		// Если функция найдена.
		if (Function)
		{
			// Подготавливаем параметры для вызова функции (ожидается один параметр типа FString).
			struct FDisplayParams { FString Message; }; // Структура должна соответствовать сигнатуре функции в BP.
			FDisplayParams Params;
			Params.Message = Message; // Передаем сообщение.
			// Вызываем Blueprint-функцию на конкретном экземпляре виджета логина.
			LoginWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginError: Called DisplayErrorMessage on WBP_LoginScreen."));
		}
		else {
			// Ошибка: в WBP_LoginScreen нет функции с таким именем.
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginError: Function 'DisplayErrorMessage' not found in WBP_LoginScreen!"));
		}
	}
	else {
		// Предупреждение: не удалось найти активный виджет логина для отображения ошибки
		// (возможно, пользователь уже перешел на другой экран).
		UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: Could not find active WBP_LoginScreen inside container to display message."));
	}
}

/**
 * @brief Находит активный виджет регистрации и вызывает на нем Blueprint-функцию DisplayErrorMessage.
 * @param Message Сообщение об ошибке для отображения.
 */
void UMyGameInstance::DisplayRegisterError(const FString& Message)
{
	// Логируем ошибку.
	UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: %s"), *Message);

	// Ищем активный виджет регистрации внутри контейнера.
	UUserWidget* RegisterWidget = FindWidgetInContainer(RegisterScreenClass);

	// Если виджет найден.
	if (RegisterWidget)
	{
		// Ищем функцию DisplayErrorMessage в WBP_RegisterScreen.
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		UFunction* Function = RegisterWidget->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			// Готовим параметры и вызываем функцию.
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			RegisterWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayRegisterError: Called DisplayErrorMessage on WBP_RegisterScreen."));
		}
		else {
			// Ошибка: функция не найдена.
			UE_LOG(LogTemp, Error, TEXT("DisplayRegisterError: Function 'DisplayErrorMessage' not found in WBP_RegisterScreen!"));
		}
	}
	else {
		// Предупреждение: виджет не найден.
		UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: Could not find active WBP_RegisterScreen inside container to display message."));
	}
}

/**
 * @brief Находит активный виджет логина и вызывает на нем Blueprint-функцию DisplaySuccessMessage (или DisplayErrorMessage как запасной вариант).
 * Используется для показа сообщения об успешной регистрации на экране логина.
 * @param Message Сообщение об успехе для отображения.
 */
void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
	// Логируем сообщение об успехе.
	UE_LOG(LogTemp, Log, TEXT("DisplayLoginSuccessMessage: %s"), *Message);

	// Ищем активный виджет логина (т.к. показываем сообщение именно там).
	UUserWidget* LoginWidget = FindWidgetInContainer(LoginScreenClass);

	// Если виджет найден.
	if (LoginWidget)
	{
		// Имена функций: предпочтительная для успеха и запасная (обычная функция ошибки).
		FName SuccessFuncName = FName(TEXT("DisplaySuccessMessage"));
		FName ErrorFuncName = FName(TEXT("DisplayErrorMessage"));

		// Пытаемся найти специальную функцию для сообщений об успехе.
		UFunction* FunctionToCall = LoginWidget->GetClass()->FindFunctionByName(SuccessFuncName);
		// Если она не найдена...
		if (!FunctionToCall)
		{
			// ...логируем это и пытаемся найти стандартную функцию для сообщений об ошибках.
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Function '%s' not found, falling back to '%s'."), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
			FunctionToCall = LoginWidget->GetClass()->FindFunctionByName(ErrorFuncName);
		}

		// Если удалось найти хотя бы одну из функций (успеха или ошибки).
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
			// Ошибка: в WBP_LoginScreen нет ни функции успеха, ни функции ошибки. Не можем показать сообщение.
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginSuccessMessage: Neither '%s' nor '%s' found in WBP_LoginScreen!"), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
		}
	}
	else {
		// Предупреждение: не найден активный виджет логина.
		UE_LOG(LogTemp, Warning, TEXT("DisplayLoginSuccessMessage: Could not find active WBP_LoginScreen inside container to display message."));
	}
}