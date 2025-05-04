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


#include "MediaPlayer.h"

#include "MediaSource.h"

#include "UObject/UObjectGlobals.h" // Для FindField, LoadClass (если нужны)

#include "UObject/Package.h"


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


// --- Для работы с окнами
#include "SlateBasics.h" // Для FSlateApplication и SWindow
#include "GenericPlatform/GenericApplication.h" // Для IGenericApplication и GetOSWindowHandle

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h" // Нужен для HWND и WinAPI функций
#include "Windows/AllowWindowsPlatformTypes.h" // Разрешает использовать типы Windows
#endif



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

// --- Вспомогательная функция ---
FString AsyncLoadingResultToString(EAsyncLoadingResult::Type Result)
{
	switch (Result)
	{
	case EAsyncLoadingResult::Succeeded: return TEXT("Succeeded");
	case EAsyncLoadingResult::Failed:    return TEXT("Failed");
	case EAsyncLoadingResult::Canceled:  return TEXT("Canceled");
	default: return FString::Printf(TEXT("Unknown (%d)"), static_cast<int32>(Result));
	}
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
			const float DesiredWidthFraction = 0.16875f;  // 15% ширины
			const float DesiredHeightFraction = 0.45f; // 30% высоты

			// 5. Рассчитываем целевое разрешение в пикселях.
			int32 CalculatedWidth = FMath::RoundToInt(DisplayMetrics.PrimaryDisplayWidth * DesiredWidthFraction);
			int32 CalculatedHeight = FMath::RoundToInt(DisplayMetrics.PrimaryDisplayHeight * DesiredHeightFraction);

			// 6. Устанавливаем минимальные размеры окна, чтобы избежать слишком маленького окна.
			const int32 MinWidth = 324;  // Минимальная ширина
			const int32 MinHeight = 486; // Минимальная высота
			CalculatedWidth = FMath::Max(CalculatedWidth, MinWidth);
			CalculatedHeight = FMath::Max(CalculatedHeight, MinHeight);

			// 7. Формируем целевые параметры: разрешение и оконный режим.
			FIntPoint TargetResolution(CalculatedWidth, CalculatedHeight);
			EWindowMode::Type TargetMode = EWindowMode::Windowed; // Обязательно Windowed

			DesiredWindowedResolution = TargetResolution; // Запоминаем рассчитанное разрешение
			bDesiredResolutionCalculated = true;       // Ставим флаг, что расчет успешен
			UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Stored desired windowed resolution %dx%d"), DesiredWindowedResolution.X, DesiredWindowedResolution.Y);

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


			// Пытаемся получить главное окно приложения через GEngine->GameViewport
			TSharedPtr<SWindow> GameWindow = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
			// Проверяем, что окно получено
			if (GameWindow.IsValid())
			{
				// Получаем нативный хэндл окна ОС (для Windows это HWND)
				void* NativeWindowHandle = GameWindow->GetNativeWindow() ? GameWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
				// Проверяем, что хэндл получен
				if (NativeWindowHandle != nullptr)
				{
					UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Attempting to modify window style to prevent resizing..."));

#if PLATFORM_WINDOWS // Код выполнится только при компиляции под Windows

					HWND Hwnd = static_cast<HWND>(NativeWindowHandle); // Приводим тип к HWND

					// Получаем текущий стиль окна
					LONG_PTR CurrentStyle = GetWindowLongPtr(Hwnd, GWL_STYLE);

					// Создаем новый стиль, убирая флаги WS_SIZEBOX (рамка изменения размера)
					// и WS_MAXIMIZEBOX (кнопка "Развернуть") с помощью битовой операции И НЕ (& ~)
					LONG_PTR NewStyle = CurrentStyle & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX;

					// Проверяем, отличается ли новый стиль от старого (чтобы не вызывать WinAPI без надобности)
					if (NewStyle != CurrentStyle)
					{
						// Устанавливаем новый стиль для окна
						SetWindowLongPtr(Hwnd, GWL_STYLE, NewStyle);

						// Говорим Windows перерисовать рамку окна с учетом нового стиля
						// Флаги говорят: обновить рамку, не менять позицию, не менять размер, не менять Z-порядок
						SetWindowPos(Hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
						UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Window style modified successfully. Resizing disabled."));
					}
					else
					{
						UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Window style already prevents resizing or GetWindowLongPtr failed? Style: %p"), (void*)CurrentStyle);
					}

#else // Для других платформ просто выводим сообщение
					UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Window style modification not implemented for this platform."));
#endif
				}
				else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get native window handle.")); }
			}
			else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get game SWindow.")); }


			bIsInitialWindowSetupComplete = true;
			UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Initial setup complete flag set. Calling ShowStartScreen..."));

			ShowStartScreen();

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
	
	if (bDesiredResolutionCalculated) // Проверяем, успели ли мы рассчитать разрешение
	{
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (Settings)
		{
			FIntPoint CurrentSavedResolution = Settings->GetScreenResolution();
			EWindowMode::Type CurrentSavedMode = Settings->GetFullscreenMode();

			// Проверяем, отличаются ли сохраненные настройки от наших желаемых оконных
			if (CurrentSavedResolution != DesiredWindowedResolution || CurrentSavedMode != EWindowMode::Windowed)
			{
				UE_LOG(LogTemp, Warning, TEXT("Shutdown: Settings differ from desired windowed state. Forcing save of %dx%d Windowed."),
					DesiredWindowedResolution.X, DesiredWindowedResolution.Y);

				// Устанавливаем НАШИ оконные настройки
				Settings->SetScreenResolution(DesiredWindowedResolution);
				Settings->SetFullscreenMode(EWindowMode::Windowed);
				// Применять (ApplySettings) перед сохранением необязательно, SaveSettings обычно делает это
				Settings->SaveSettings(); // СОХРАНЯЕМ ИМЕННО ИХ!
				UE_LOG(LogTemp, Warning, TEXT("Shutdown: Desired windowed settings saved."));
			}
			else {
				UE_LOG(LogTemp, Log, TEXT("Shutdown: Saved settings already match desired windowed state."));
			}
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("Shutdown: Could not get GameUserSettings to save final state."));
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Shutdown: Desired windowed resolution was never calculated. Cannot force save."));
	}

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
	if (!bIsInitialWindowSetupComplete || bIsFullscreenWidget)
	{
		ApplyWindowMode(bIsFullscreenWidget);
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Applied window mode (Fullscreen: %s OR Initial setup not complete)"), bIsFullscreenWidget ? TEXT("True") : TEXT("False"));
	}
	else {
		// Пропускаем ApplyWindowMode для первого оконного виджета после завершения настройки
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Skipping ApplyWindowMode for initial windowed widget because initial setup is complete."));
	}
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
}


void UMyGameInstance::StartLoadLevelWithVideoWidget(FName LevelName)
{
	UE_LOG(LogTemp, Log, TEXT(">>> StartLoadLevelWithVideoWidget: ENTERING. LevelName='%s'"), *LevelName.ToString());

	// 1. Проверки
	if (!LoadingScreenClass) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithVideoWidget: LoadingScreenClass is not set!")); return; }
	if (LevelName.IsNone()) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithVideoWidget: LevelName is None!")); return; }
	if (!LoadingMediaPlayerAsset) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithVideoWidget: LoadingMediaPlayerAsset is not set!")); return; }
	if (!LoadingMediaSourceAsset) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithVideoWidget: LoadingMediaSourceAsset is not set!")); return; }

	// 2. Показываем виджет
	UE_LOG(LogTemp, Log, TEXT("StartLoadLevelWithVideoWidget: Showing loading screen widget '%s'..."), *LoadingScreenClass->GetName());
	UUserWidget* LoadingWidgetInstance = ShowWidget<UUserWidget>(LoadingScreenClass, true);
	if (!LoadingWidgetInstance) { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithVideoWidget: Failed to create/show LoadingScreenWidget! Fallback OpenLevel.")); UGameplayStatics::OpenLevel(this, LevelName); return; }

	// 3. Сброс флагов
	LevelToLoadAsync = LevelName;
	bIsLevelLoadComplete = false;
	bIsLoadingVideoFinished = false;
	UE_LOG(LogTemp, Log, TEXT("StartLoadLevelWithVideoWidget: State reset. LevelToLoad='%s'"), *LevelToLoadAsync.ToString());

	// 4. Настройка виджета (передача ссылок на плеер/источник)
	UClass* ActualWidgetClass = LoadingWidgetInstance->GetClass();
	if (ActualWidgetClass && ActualWidgetClass->IsChildOf(LoadingScreenClass))
	{
		// Установка плеера
		FProperty* MediaPlayerBaseProp = ActualWidgetClass->FindPropertyByName(FName("LoadingMediaPlayer"));
		FObjectProperty* MediaPlayerProp = CastField<FObjectProperty>(MediaPlayerBaseProp);
		if (MediaPlayerProp) { MediaPlayerProp->SetObjectPropertyValue_InContainer(LoadingWidgetInstance, LoadingMediaPlayerAsset); }
		else { UE_LOG(LogTemp, Warning, TEXT("StartLoadLevelWithVideoWidget: Could not find/cast 'LoadingMediaPlayer' property in widget.")); }
		// Установка источника
		FProperty* MediaSourceBaseProp = ActualWidgetClass->FindPropertyByName(FName("LoadingMediaSource"));
		FObjectProperty* MediaSourceProp = CastField<FObjectProperty>(MediaSourceBaseProp);
		if (MediaSourceProp) { MediaSourceProp->SetObjectPropertyValue_InContainer(LoadingWidgetInstance, LoadingMediaSourceAsset); }
		else { UE_LOG(LogTemp, Warning, TEXT("StartLoadLevelWithVideoWidget: Could not find/cast 'LoadingMediaSource' property in widget.")); }
	}
	else { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithVideoWidget: Widget class mismatch!")); }

	// 5. Запуск асинхронной загрузки
	FString PackagePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelName.ToString());
	UE_LOG(LogTemp, Log, TEXT("StartLoadLevelWithVideoWidget: Requesting async load for package '%s'..."), *PackagePath);
	FLoadPackageAsyncDelegate LoadCallback = FLoadPackageAsyncDelegate::CreateUObject(this, &UMyGameInstance::OnLevelPackageLoaded);
	if (LoadCallback.IsBound()) { LoadPackageAsync(PackagePath, LoadCallback, 0, PKG_ContainsMap); }
	else { UE_LOG(LogTemp, Error, TEXT("StartLoadLevelWithVideoWidget: Failed to bind LoadPackageAsync delegate!")); CheckAndFinalizeLevelTransition(); }

	UE_LOG(LogTemp, Log, TEXT("<<< StartLoadLevelWithVideoWidget: EXITING function."));
}

/** Коллбэк завершения асинхронной загрузки пакета */
void UMyGameInstance::OnLevelPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
	UE_LOG(LogTemp, Log, TEXT(">>> OnLevelPackageLoaded: ENTERING (Callback). Package='%s', Result=%s"), *PackageName.ToString(), *AsyncLoadingResultToString(Result));
	if (Result != EAsyncLoadingResult::Succeeded) { UE_LOG(LogTemp, Error, TEXT("OnLevelPackageLoaded: Async package load FAILED for '%s'!"), *PackageName.ToString()); }
	else { UE_LOG(LogTemp, Log, TEXT("OnLevelPackageLoaded: Async package load SUCCEEDED for '%s'."), *PackageName.ToString()); }
	bIsLevelLoadComplete = true;
	CheckAndFinalizeLevelTransition();
	UE_LOG(LogTemp, Log, TEXT("<<< OnLevelPackageLoaded: EXITING function."));
}

/** Вызывается из Blueprint виджета, когда видео завершилось */
void UMyGameInstance::NotifyLoadingVideoFinished()
{
	UE_LOG(LogTemp, Log, TEXT(">>> NotifyLoadingVideoFinished: ENTERING (Called by Widget)."));
	bIsLoadingVideoFinished = true;
	CheckAndFinalizeLevelTransition();
	UE_LOG(LogTemp, Log, TEXT("<<< NotifyLoadingVideoFinished: EXITING."));
}

/** Проверяет, можно ли завершить переход */
void UMyGameInstance::CheckAndFinalizeLevelTransition()
{
	UE_LOG(LogTemp, Verbose, TEXT(">>> CheckAndFinalizeLevelTransition: Checking: LevelLoadComplete=%s, VideoFinished=%s"),
		bIsLevelLoadComplete ? TEXT("True") : TEXT("False"), bIsLoadingVideoFinished ? TEXT("True") : TEXT("False"));

	if (bIsLevelLoadComplete && bIsLoadingVideoFinished)
	{
		UE_LOG(LogTemp, Log, TEXT("CheckAndFinalizeLevelTransition: ---> Conditions MET. Finalizing transition!"));

		// --- ОТПИСКА ОТ ДЕЛЕГАТА БОЛЬШЕ НЕ НУЖНА, ТАК КАК МЫ ЕГО НЕ ИСПОЛЬЗУЕМ ДЛЯ ЭТОГО ---

		// Сохраняем имя уровня перед сбросом CurrentTopLevelWidget
		FName LevelToOpen = LevelToLoadAsync;

		// Удаляем виджет загрузки
		if (CurrentTopLevelWidget != nullptr)
		{
			UE_LOG(LogTemp, Log, TEXT("CheckAndFinalizeLevelTransition: Removing loading screen widget: %s"), *CurrentTopLevelWidget->GetName());
			CurrentTopLevelWidget->RemoveFromParent();
			CurrentTopLevelWidget = nullptr;
		}

		// Сбрасываем переменные состояния СРАЗУ
		LevelToLoadAsync = NAME_None;
		bIsLevelLoadComplete = false;
		bIsLoadingVideoFinished = false;
		UE_LOG(LogTemp, Log, TEXT("CheckAndFinalizeLevelTransition: State flags reset."));

		// Открываем УЖЕ загруженный уровень
		if (!LevelToOpen.IsNone())
		{
			UE_LOG(LogTemp, Log, TEXT("CheckAndFinalizeLevelTransition: Calling OpenLevel for '%s'..."), *LevelToOpen.ToString());
			UGameplayStatics::OpenLevel(this, LevelToOpen);
		}
		else { UE_LOG(LogTemp, Error, TEXT("CheckAndFinalizeLevelTransition: LevelToOpen is None!")); }

	}
	else { UE_LOG(LogTemp, Verbose, TEXT("CheckAndFinalizeLevelTransition: ---> Conditions NOT MET. Waiting...")); }
	UE_LOG(LogTemp, Verbose, TEXT("<<< CheckAndFinalizeLevelTransition: EXITING."));
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
}


// =============================================================================
// Отправка HTTP Запросов на Сервер (Аутентификация)
// =============================================================================

void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password)
{
	// Логируем начало операции.
	UE_LOG(LogTemp, Log, TEXT("RequestLogin: Attempting login for user: %s"), *Username);

	// --- Шаг 1: Подготовка JSON тела запроса ---
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);

	// --- Шаг 2: Сериализация JSON в строку ---
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to serialize JSON body."));
		// Сообщаем об ошибке пользователю через UI.
		DisplayLoginError(TEXT("Ошибка клиента: Не удалось создать запрос")); 
		return;
	}

	// --- Шаг 3: Создание и настройка HTTP запроса ---
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/login"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	// --- Шаг 4: Привязка обработчика ответа ---
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnLoginResponseReceived);

	// --- Шаг 5: Отправка запроса ---
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayLoginError(TEXT("Ошибка сети: Не удалось начать запрос")); 
	}
	else {
		UE_LOG(LogTemp, Log, TEXT("RequestLogin: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/login")));
		// В этом месте можно было бы показать пользователю индикатор загрузки.
	}
}

// --- Регистрация ---

void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
	// Логируем начало операции.
	UE_LOG(LogTemp, Log, TEXT("RequestRegister: Attempting registration for user: %s, email: %s"), *Username, *Email);

	// --- Шаг 1: Подготовка JSON тела запроса ---
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);
	RequestJson->SetStringField(TEXT("email"), Email);

	// --- Шаг 2: Сериализация JSON в строку ---
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("RequestRegister: Failed to serialize JSON body."));
		DisplayRegisterError(TEXT("Ошибка клиента: Не удалось создать запрос")); 
		return;
	}

	// --- Шаг 3: Создание и настройка HTTP запроса ---
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/register"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	// --- Шаг 4: Привязка обработчика ответа ---
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnRegisterResponseReceived);

	// --- Шаг 5: Отправка запроса ---
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestRegister: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayRegisterError(TEXT("Ошибка сети: Не удалось начать запрос")); 
	}
	else {
		UE_LOG(LogTemp, Log, TEXT("RequestRegister: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/register")));
	}
}


// =============================================================================
// Обработка Ответов от Сервера (Колбэки HTTP Запросов)
// =============================================================================

void UMyGameInstance::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// --- Инициализация переменных для результата ---
	bool bLoginSuccess = false;
	FString ResponseErrorMsg = TEXT(""); // Используем локальную переменную

	// --- Шаг 1: Проверка базовой успешности запроса ---
	if (!bWasSuccessful || !Response.IsValid())
	{
		ResponseErrorMsg = TEXT("Сервер не доступен или проблемы с сетью");
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Request failed. Message: %s"), *ResponseErrorMsg);
		// DisplayLoginError(ResponseErrorMsg); // Можно оставить для логов
		// !!! ВЫЗЫВАЕМ ДЕЛЕГАТ С НЕУДАЧЕЙ и выходим !!!
		OnLoginAttemptCompleted.Broadcast(false, ResponseErrorMsg);
		return; // Выходим из функции
	}

	// --- Шаг 2: Анализ ответа сервера ---
	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	// --- Шаг 3: Обработка в зависимости от кода ответа ---
	if (ResponseCode == 200) // Успешный логин (200 OK).
	{
		TSharedPtr<FJsonObject> ResponseJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
		{
			int64 ReceivedUserId = -1;
			FString ReceivedUsername;
			if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
				ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername))
			{
				UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Login successful for user: %s (ID: %lld)"), *ReceivedUsername, ReceivedUserId);
				SetLoginStatus(true, ReceivedUserId, ReceivedUsername); // Обновляем статус как и раньше
				bLoginSuccess = true; // Успех!
				// --- !!! УДАЛЕН ВЫЗОВ ShowLoadingScreen() ИЛИ ДРУГОГО ПЕРЕХОДА !!! ---
			}
			else {
				ResponseErrorMsg = TEXT("Ошибка сервера: Неверный формат ответа");
				UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed parsing JSON fields. Message: %s"), *ResponseErrorMsg);
				// DisplayLoginError(ResponseErrorMsg);
			}
		}
		else {
			ResponseErrorMsg = TEXT("Ошибка сервера: Не удалось обработать ответ");
			UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed deserializing JSON. Message: %s"), *ResponseErrorMsg);
			// DisplayLoginError(ResponseErrorMsg);
		}
	}
	else if (ResponseCode == 401) // Ошибка аутентификации (401 Unauthorized).
	{
		ResponseErrorMsg = TEXT("Неверное имя пользователя или пароль");
		UE_LOG(LogTemp, Warning, TEXT("OnLoginResponseReceived: Login failed - Invalid credentials (401). Message: %s"), *ResponseErrorMsg);
		// DisplayLoginError(ResponseErrorMsg);
	}
	else // Другие ошибки сервера
	{
		ResponseErrorMsg = FString::Printf(TEXT("Ошибка сервера (Код: %d)"), ResponseCode);
		// ... (можно добавить парсинг сообщения из JSON, как было раньше) ...
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Server error. Message: %s"), *ResponseErrorMsg);
		// DisplayLoginError(ResponseErrorMsg);
	}

	// !!! ВЫЗЫВАЕМ ДЕЛЕГАТ С ФИНАЛЬНЫМ РЕЗУЛЬТАТОМ в конце функции !!!
	UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Broadcasting OnLoginAttemptCompleted. Success: %s, Message: %s"), bLoginSuccess ? TEXT("True") : TEXT("False"), *ResponseErrorMsg);
	OnLoginAttemptCompleted.Broadcast(bLoginSuccess, ResponseErrorMsg);
}


void UMyGameInstance::OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// --- Шаг 1: Проверка базовой успешности запроса ---
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayRegisterError(TEXT("Сервер не доступен или проблемы с сетью")); 
		return;
	}

	// --- Шаг 2: Анализ ответа сервера ---
	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	// --- Шаг 3: Обработка в зависимости от кода ответа ---
	if (ResponseCode == 201) // Успешная регистрация (201 Created).
	{
		UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Registration successful!"));
		ShowLoginScreen();
		// --- Показ сообщения об успехе на экране логина с небольшой задержкой ---
		FTimerHandle TempTimerHandle;
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(TempTimerHandle, [this]() {
				DisplayLoginSuccessMessage(TEXT("Регистрация успешна! Можете войти в аккаунт")); 
				}, 0.1f, false);
		}
		// --- Конец показа сообщения ---
	}
	else if (ResponseCode == 409) // Ошибка: Конфликт (имя пользователя или email уже заняты) (409 Conflict).
	{
		FString ErrorMessage = TEXT("Имя пользователя или Email уже существует"); // Переведено
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Ошибка регистрации: %s"), *ServerErrorMsg); 
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (409)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	else if (ResponseCode == 400) // Ошибка: Неверный запрос (ошибка валидации данных на сервере) (400 Bad Request).
	{
		FString ErrorMessage = TEXT("Предоставлены неверные данные"); 
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Ошибка регистрации: %s"), *ServerErrorMsg); 
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (400)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	else // Другие ошибки сервера.
	{
		FString ErrorMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d)"), ResponseCode);
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: %s"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
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

