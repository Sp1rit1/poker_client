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
// Реализация Методов UMyGameInstance
// =============================================================================

/**
 * @brief Метод инициализации GameInstance.
 * Запускает таймер для отложенной установки настроек окна.
 * Также здесь можно инициализировать другие глобальные менеджеры.
 */
void UMyGameInstance::Init()
{
	// 1. Вызов реализации базового класса - ОБЯЗАТЕЛЬНО первым делом.
	Super::Init();
	UE_LOG(LogTemp, Log, TEXT("UMyGameInstance::Init() Started."));

	// 2. Логирование начального состояния окна (для отладки)
	// Это покажет, какое разрешение и режим установлены *до* срабатывания нашего таймера.
	UGameUserSettings* InitialSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (InitialSettings) {
		UE_LOG(LogTemp, Warning, TEXT("Init Start Check: CurrentRes=%dx%d | CurrentMode=%d"),
			InitialSettings->GetScreenResolution().X, InitialSettings->GetScreenResolution().Y, (int32)InitialSettings->GetFullscreenMode());
	}
	else { UE_LOG(LogTemp, Warning, TEXT("Init Start Check: Settings=NULL")); }

	// 3. Запуск таймера для вызова функции DelayedInitialResize
	// Используем GetTimerManager() - глобальный менеджер таймеров, доступный из UObject/UGameInstance.
	// Timer будет вызван один раз (false в последнем параметре) через 0.5 секунды.
	// Задержка нужна, чтобы дать движку время завершить свою стандартную инициализацию окна и загрузку .ini.
	const float ResizeDelay = 0.1f; // Задержка в секундах (можно настроить, 0.2-0.5 обычно достаточно)
	GetTimerManager().SetTimer(ResizeTimerHandle, this, &UMyGameInstance::DelayedInitialResize, ResizeDelay, false);
	UE_LOG(LogTemp, Log, TEXT("Init: Timer scheduled for DelayedInitialResize in %.2f seconds."), ResizeDelay);

	// 4. Инициализация других ваших глобальных систем (пример)
	OfflineGameManager = NewObject<UOfflineGameManager>(this);
	if (OfflineGameManager) { UE_LOG(LogTemp, Log, TEXT("OfflineGameManager created successfully.")); }
	else { UE_LOG(LogTemp, Error, TEXT("Failed to create OfflineGameManager!")); }

	UE_LOG(LogTemp, Log, TEXT("UMyGameInstance::Init() Finished."));
}

/**
 * @brief Функция, вызываемая таймером для установки и сохранения настроек окна.
 * Выполняется после небольшой задержки, чтобы обойти проблемы ранней инициализации.
 */
void UMyGameInstance::DelayedInitialResize()
{
	// Логируем начало выполнения отложенной функции (используем Error уровень для легкого поиска в логах при отладке).
	UE_LOG(LogTemp, Warning, TEXT("--- DelayedInitialResize() Called ---"));

	// 1. Получаем указатель на объект настроек пользователя.
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	// Проверяем, удалось ли его получить.
	if (Settings)
	{
		// 2. Логируем состояние окна ПЕРЕД нашими изменениями.
		UE_LOG(LogTemp, Warning, TEXT("DelayedResize Start: CurrentRes=%dx%d | CurrentMode=%d"),
			Settings->GetScreenResolution().X, Settings->GetScreenResolution().Y, (int32)Settings->GetFullscreenMode());

		// 3. Получаем информацию о дисплее.
		FDisplayMetrics DisplayMetrics;
		// Проверяем, инициализирована ли система Slate, прежде чем получать метрики.
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetCachedDisplayMetrics(DisplayMetrics);

			// 4. Задаем желаемые пропорции окна в долях от размера экрана.
			const float DesiredWidthFraction = 0.22f;  // 15% ширины
			const float DesiredHeightFraction = 0.45f; // 30% высоты

			// 5. Рассчитываем целевое разрешение в пикселях.
			int32 CalculatedWidth = FMath::RoundToInt(DisplayMetrics.PrimaryDisplayWidth * DesiredWidthFraction);
			int32 CalculatedHeight = FMath::RoundToInt(DisplayMetrics.PrimaryDisplayHeight * DesiredHeightFraction);

			// 6. Устанавливаем минимальные размеры окна, чтобы избежать слишком маленького окна.
			const int32 MinWidth = 422;  // Минимальная ширина
			const int32 MinHeight = 486; // Минимальная высота
			CalculatedWidth = FMath::Max(CalculatedWidth, MinWidth);
			CalculatedHeight = FMath::Max(CalculatedHeight, MinHeight);

			// 7. Формируем целевые параметры: разрешение и оконный режим.
			FIntPoint TargetResolution(CalculatedWidth, CalculatedHeight);
			EWindowMode::Type TargetMode = EWindowMode::Windowed; // Обязательно Windowed

			// 8. Принудительно устанавливаем и сохраняем настройки.
			// Мы не делаем проверку if(Current != Target), чтобы гарантированно применить и сохранить
			// нужные значения, даже если они случайно совпали с промежуточными.
			UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Forcing resolution to %dx%d and mode to Windowed."), TargetResolution.X, TargetResolution.Y);

			// Устанавливаем значения в объекте настроек.
			Settings->SetScreenResolution(TargetResolution);
			Settings->SetFullscreenMode(TargetMode);

			// Применяем настройки к реальному окну игры. false - не ждать подтверждения.
			Settings->ApplySettings(false);

			// Сохраняем примененные настройки в файл GameUserSettings.ini пользователя.
			// Это КЛЮЧЕВОЙ шаг, чтобы настройки сохранились и использовались при следующих запусках.
			Settings->SaveSettings();

			UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Settings applied and saved."));

			// 9. (Опционально, для отладки) Проверяем настройки СРАЗУ ПОСЛЕ сохранения.
			FPlatformProcess::Sleep(0.1f); // Даем системе небольшую паузу на всякий случай.
			FIntPoint SettingsAfterApply = Settings->GetScreenResolution();
			EWindowMode::Type ModeAfterApply = Settings->GetFullscreenMode();
			UE_LOG(LogTemp, Warning, TEXT("DelayedResize End Check: CurrentRes=%dx%d | CurrentMode=%d"),
				SettingsAfterApply.X, SettingsAfterApply.Y, (int32)ModeAfterApply);

		}
		else {
			// Ошибка: Не удалось получить метрики дисплея.
			UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: FSlateApplication not initialized yet, cannot get display metrics. Skipping resize."));
		}
	}
	else {
		// Ошибка: Не удалось получить GameUserSettings.
		UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get GameUserSettings!"));
	}
	// Логируем завершение функции.
	UE_LOG(LogTemp, Warning, TEXT("--- DelayedInitialResize() Finished ---"));
}


/**
 * @brief Метод завершения работы GameInstance.
 */
void UMyGameInstance::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown started."));
	// Можно добавить здесь код для очистки, если нужно.

	// Обязательно вызываем Shutdown базового класса.
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
 * @brief Шаблонная функция для показа виджетов (ИЗМЕНЕННАЯ)
 * Удаляет предыдущий виджет и показывает новый.
 * @param WidgetClassToShow Класс виджета для показа.
 * @param bIsFullscreenWidget Флаг полноэкранного режима (влияет на окно и ввод).
 * @return Указатель на созданный виджет или nullptr.
 */
template <typename T>
T* UMyGameInstance::ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget)
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: PlayerController is null.")); return nullptr; }
	if (!WidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: WidgetClassToShow is null.")); return nullptr; }

	// --- Шаг 1: Удаляем старый виджет ---
	if (CurrentTopLevelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous widget: %s"), *CurrentTopLevelWidget->GetName());
		CurrentTopLevelWidget->RemoveFromParent();
	}
	CurrentTopLevelWidget = nullptr;
	// CurrentContainerInstance = nullptr; // Строка удалена, т.к. переменной больше нет

	// --- Шаг 2: Меняем режим окна и ввода ---
	ApplyWindowMode(bIsFullscreenWidget);
	SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget); // Ввод UI для оконного, Game+UI для полноэкранного

	// --- Шаг 3: Создаем и показываем новый виджет ---
	T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
	if (NewWidget)
	{
		NewWidget->AddToViewport();
		CurrentTopLevelWidget = NewWidget; // Сохраняем указатель на новый виджет
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Added new widget: %s"), *WidgetClassToShow->GetName());
		return NewWidget;
	}
	else { UE_LOG(LogTemp, Error, TEXT("ShowWidget: Failed to create widget: %s"), *WidgetClassToShow->GetName()); return nullptr; }
}


// =============================================================================
// Функции Навигации Между Экранами
// =============================================================================

// =============================================================================
// Функции Навигации Между Экранами (ИЗМЕНЕННЫЕ)
// =============================================================================

/**
 * @brief Показывает стартовый экран приложения напрямую.
 */
void UMyGameInstance::ShowStartScreen()
{
	// Проверяем, что класс стартового экрана задан в настройках GameInstance (в Blueprint).
	if (!StartScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: StartScreenClass is not set!"));
		return; // Прерываем, если класс не задан.
	}

	// Вызываем ShowWidget для отображения стартового экрана напрямую в оконном режиме (false).
	// ShowWidget сама удалит предыдущий CurrentTopLevelWidget.
	UUserWidget* StartWidget = ShowWidget<UUserWidget>(StartScreenClass, false);

	// Проверяем, успешно ли создан и показан виджет.
	if (!StartWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: Failed to create/show StartScreen!"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ShowStartScreen: StartScreen displayed successfully."));
	}

	// Останавливаем таймер экрана загрузки, если он был активен.
	GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief Показывает экран входа пользователя напрямую.
 */
void UMyGameInstance::ShowLoginScreen()
{
	// Проверка наличия класса экрана логина.
	if (!LoginScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: LoginScreenClass is not set!"));
		return;
	}

	// Показываем экран логина напрямую в оконном режиме (false).
	UUserWidget* LoginWidget = ShowWidget<UUserWidget>(LoginScreenClass, false);

	if (!LoginWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: Failed to create/show LoginScreen!"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ShowLoginScreen: LoginScreen displayed successfully."));
	}

	// Останавливаем таймер загрузки.
	GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief Показывает экран регистрации нового пользователя напрямую.
 */
void UMyGameInstance::ShowRegisterScreen()
{
	// Проверка наличия класса.
	if (!RegisterScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: RegisterScreenClass is not set!"));
		return;
	}

	// Показываем экран регистрации напрямую в оконном режиме (false).
	UUserWidget* RegisterWidget = ShowWidget<UUserWidget>(RegisterScreenClass, false);

	if (!RegisterWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: Failed to create/show RegisterScreen!"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ShowRegisterScreen: RegisterScreen displayed successfully."));
	}

	// Останавливаем таймер загрузки.
	GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
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
// Вспомогательные Функции для Взаимодействия с Виджетами 
// =============================================================================


/**
 * @brief Находит активный виджет логина (проверяя CurrentTopLevelWidget)
 * и вызывает на нем Blueprint-функцию DisplayErrorMessage.
 * @param Message Сообщение об ошибке для отображения.
 */
void UMyGameInstance::DisplayLoginError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: %s"), *Message);

	// Проверяем, что текущий виджет верхнего уровня существует,
	// и что он имеет правильный класс (LoginScreenClass).
	if (CurrentTopLevelWidget && LoginScreenClass && CurrentTopLevelWidget->IsA(LoginScreenClass))
	{
		// Указатель на текущий виджет (уже проверен).
		UUserWidget* LoginWidget = CurrentTopLevelWidget;

		// Имя Blueprint-функции для вызова.
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		// Ищем функцию в классе виджета.
		UFunction* Function = LoginWidget->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			// Готовим параметры и вызываем функцию через ProcessEvent.
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			LoginWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginError: Called DisplayErrorMessage on %s."), *LoginWidget->GetName());
		}
		else {
			// Ошибка: функция не найдена в Blueprint виджета.
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginError: Function 'DisplayErrorMessage' not found in %s!"), *LoginScreenClass->GetName());
		}
	}
	else {
		// Логируем причину, по которой не удалось показать ошибку.
		if (!CurrentTopLevelWidget) {
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: CurrentTopLevelWidget is null when trying to display error."));
		}
		else if (!LoginScreenClass) {
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: LoginScreenClass is null variable in GameInstance."));
		}
		else {
			// Текущий виджет не является виджетом логина.
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: CurrentTopLevelWidget (%s) is not the expected LoginScreenClass (%s). Cannot display error."),
				*CurrentTopLevelWidget->GetName(), *LoginScreenClass->GetName());
		}
	}
}

/**
 * @brief Находит активный виджет регистрации (проверяя CurrentTopLevelWidget)
 * и вызывает на нем Blueprint-функцию DisplayErrorMessage.
 * @param Message Сообщение об ошибке для отображения.
 */
void UMyGameInstance::DisplayRegisterError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: %s"), *Message);

	// Проверяем текущий виджет верхнего уровня на тип RegisterScreenClass.
	if (CurrentTopLevelWidget && RegisterScreenClass && CurrentTopLevelWidget->IsA(RegisterScreenClass))
	{
		UUserWidget* RegisterWidget = CurrentTopLevelWidget;

		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		UFunction* Function = RegisterWidget->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			RegisterWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayRegisterError: Called DisplayErrorMessage on %s."), *RegisterWidget->GetName());
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("DisplayRegisterError: Function 'DisplayErrorMessage' not found in %s!"), *RegisterScreenClass->GetName());
		}
	}
	else {
		if (!CurrentTopLevelWidget) {
			UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: CurrentTopLevelWidget is null when trying to display error."));
		}
		else if (!RegisterScreenClass) {
			UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: RegisterScreenClass is null variable in GameInstance."));
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: CurrentTopLevelWidget (%s) is not the expected RegisterScreenClass (%s). Cannot display error."),
				*CurrentTopLevelWidget->GetName(), *RegisterScreenClass->GetName());
		}
	}
}

/**
 * @brief Находит активный виджет логина (проверяя CurrentTopLevelWidget)
 * и вызывает на нем Blueprint-функцию DisplaySuccessMessage (или DisplayErrorMessage).
 * @param Message Сообщение об успехе для отображения.
 */
void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("DisplayLoginSuccessMessage: %s"), *Message);

	// Проверяем текущий виджет верхнего уровня на тип LoginScreenClass.
	if (CurrentTopLevelWidget && LoginScreenClass && CurrentTopLevelWidget->IsA(LoginScreenClass))
	{
		UUserWidget* LoginWidget = CurrentTopLevelWidget;

		FName SuccessFuncName = FName(TEXT("DisplaySuccessMessage"));
		FName ErrorFuncName = FName(TEXT("DisplayErrorMessage"));

		UFunction* FunctionToCall = LoginWidget->GetClass()->FindFunctionByName(SuccessFuncName);
		if (!FunctionToCall)
		{
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Function '%s' not found, falling back to '%s'."), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
			FunctionToCall = LoginWidget->GetClass()->FindFunctionByName(ErrorFuncName);
		}

		if (FunctionToCall)
		{
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			LoginWidget->ProcessEvent(FunctionToCall, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Called %s on %s."), *FunctionToCall->GetName(), *LoginWidget->GetName());
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginSuccessMessage: Neither '%s' nor '%s' found in %s!"), *SuccessFuncName.ToString(), *ErrorFuncName.ToString(), *LoginScreenClass->GetName());
		}
	}
	else {
		if (!CurrentTopLevelWidget) {
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginSuccessMessage: CurrentTopLevelWidget is null when trying to display message."));
		}
		else if (!LoginScreenClass) {
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginSuccessMessage: LoginScreenClass is null variable in GameInstance."));
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginSuccessMessage: CurrentTopLevelWidget (%s) is not the expected LoginScreenClass (%s). Cannot display message."),
				*CurrentTopLevelWidget->GetName(), *LoginScreenClass->GetName());
		}
	}
}

