// MyGameInstance.cpp

#include "MyGameInstance.h"
#include "StartScreenUIManager.h"
#include "NetworkAuthManager.h"
#include "OfflineGameManager.h"
#include "MenuScreenUIManager.h"
#include "LevelTransitionManager.h"
#include "GameFramework/PlayerController.h" // Уже был, но важен для GetFirstLocalPlayerController
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h" // Для UGameplayStatics::GetCurrentLevelName
#include "TimerManager.h"
#include "SlateBasics.h"
#include "GenericPlatform/GenericApplication.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h" // Убедитесь, что этот порядок инклюдов не конфликтует, если есть другой Allow/Hide
#endif

UMyGameInstance::UMyGameInstance()
{
    // Инициализация указателей на менеджеры
    StartScreenUIManagerInstance = nullptr;
    NetworkAuthManagerInstance = nullptr;
    OfflineGameManager = nullptr;
    MenuScreenUIManagerInstance = nullptr;
    LevelTransitionManagerInstance = nullptr; // Добавил инициализацию

    // Инициализация флагов и переменных состояния
    bIsInitialWindowSetupComplete = false;
    DesiredWindowedResolution = FIntPoint::ZeroValue;
    bDesiredResolutionCalculated = false;
    bIsLoggedIn = false;
    LoggedInUsername = TEXT("");
    LoggedInUserId = -1;
    LoggedInFriendCode = TEXT("");
    bIsInOfflineMode = true; // Начинаем в оффлайн режиме по умолчанию
}

void UMyGameInstance::Init()
{
    Super::Init();
    UE_LOG(LogTemp, Log, TEXT("UMyGameInstance::Init() Started."));

    UGameUserSettings* InitialSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (InitialSettings) {
        UE_LOG(LogTemp, Warning, TEXT("Init Start Check: CurrentRes=%dx%d | CurrentMode=%d"),
            InitialSettings->GetScreenResolution().X, InitialSettings->GetScreenResolution().Y, (int32)InitialSettings->GetFullscreenMode());
    }
    else { UE_LOG(LogTemp, Warning, TEXT("Init Start Check: Settings=NULL")); }

    // --- Создание и инициализация менеджеров ---
    StartScreenUIManagerInstance = NewObject<UStartScreenUIManager>(this);
    if (StartScreenUIManagerInstance) {
        StartScreenUIManagerInstance->Initialize(
            this,
            StartScreenClass,
            LoginScreenClass,
            RegisterScreenClass
        );
        UE_LOG(LogTemp, Log, TEXT("StartScreenUIManager created and initialized."));
    }
    else { UE_LOG(LogTemp, Error, TEXT("Failed to create StartScreenUIManager!")); }

    MenuScreenUIManagerInstance = NewObject<UMenuScreenUIManager>(this);
    if (MenuScreenUIManagerInstance) {
        MenuScreenUIManagerInstance->Initialize(
            this,
            MainMenuClass,
            OfflineLobbyClass,
            OnlineLobbyClass,
            ProfileScreenClass,
            SettingsClass
        );
        UE_LOG(LogTemp, Log, TEXT("MenuScreenUIManager created and initialized.")); // Исправлено имя
    }
    else { UE_LOG(LogTemp, Error, TEXT("Failed to create MenuScreenUIManager!")); } // Исправлено имя

    NetworkAuthManagerInstance = NewObject<UNetworkAuthManager>(this);
    if (NetworkAuthManagerInstance) {
        NetworkAuthManagerInstance->Initialize(this, ApiBaseUrl);
        UE_LOG(LogTemp, Log, TEXT("NetworkAuthManager created and initialized."));
    }
    else { UE_LOG(LogTemp, Error, TEXT("Failed to create NetworkAuthManager!")); }

    OfflineGameManager = NewObject<UOfflineGameManager>(this);
    if (OfflineGameManager) { UE_LOG(LogTemp, Log, TEXT("OfflineGameManager created successfully.")); }
    else { UE_LOG(LogTemp, Error, TEXT("Failed to create OfflineGameManager!")); }

    LevelTransitionManagerInstance = NewObject<ULevelTransitionManager>(this);
    if (LevelTransitionManagerInstance)
    {
        LevelTransitionManagerInstance->Initialize(this);
        UE_LOG(LogTemp, Log, TEXT("LevelTransitionManager created and initialized."));
    }
    else { UE_LOG(LogTemp, Error, TEXT("Failed to create LevelTransitionManagerInstance!")); }

    // Запуск таймера для DelayedInitialResize (будет выполняться всегда, но логика внутри DelayedInitialResize будет условной)
    const float ResizeDelay = 0.1f; // Можно увеличить, если нужно больше времени на загрузку уровня
    GetTimerManager().SetTimer(ResizeTimerHandle, this, &UMyGameInstance::DelayedInitialResize, ResizeDelay, false);
    UE_LOG(LogTemp, Log, TEXT("Init: Timer scheduled for DelayedInitialResize in %.2f seconds."), ResizeDelay);

    UE_LOG(LogTemp, Log, TEXT("UMyGameInstance::Init() Finished."));
}

void UMyGameInstance::DelayedInitialResize()
{
    UE_LOG(LogTemp, Warning, TEXT("--- UMyGameInstance::DelayedInitialResize() Called ---"));

    // Получаем имя текущего загруженного уровня
    FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(GetWorld(), true); // true - для удаления префикса PIE

    // ИЗМЕНЕНИЕ: Выполняем изменение размера и стиля окна ТОЛЬКО для StartLevel
    if (CurrentLevelName.Equals(TEXT("StartLevel"), ESearchCase::IgnoreCase)) // Замените "StartLevel" на точное имя вашего стартового уровня
    {
        UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Current level is StartLevel. Proceeding with window modifications."));

        UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
        if (Settings)
        {
            UE_LOG(LogTemp, Warning, TEXT("DelayedResize Start: CurrentRes=%dx%d | CurrentMode=%d"),
                Settings->GetScreenResolution().X, Settings->GetScreenResolution().Y, (int32)Settings->GetFullscreenMode());

            FDisplayMetrics DisplayMetrics;
            if (FSlateApplication::IsInitialized())
            {
                FSlateApplication::Get().GetCachedDisplayMetrics(DisplayMetrics);
                const float DesiredWidthFraction = 0.16875f;
                const float DesiredHeightFraction = 0.45f;
                int32 CalculatedWidth = FMath::RoundToInt(DisplayMetrics.PrimaryDisplayWidth * DesiredWidthFraction);
                int32 CalculatedHeight = FMath::RoundToInt(DisplayMetrics.PrimaryDisplayHeight * DesiredHeightFraction);
                const int32 MinWidth = 324;
                const int32 MinHeight = 486;
                CalculatedWidth = FMath::Max(CalculatedWidth, MinWidth);
                CalculatedHeight = FMath::Max(CalculatedHeight, MinHeight);
                FIntPoint TargetResolution(CalculatedWidth, CalculatedHeight);
                EWindowMode::Type TargetMode = EWindowMode::Windowed;

                DesiredWindowedResolution = TargetResolution;
                bDesiredResolutionCalculated = true; // Флаг, что мы рассчитали и применили это разрешение
                UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Stored desired windowed resolution %dx%d for StartLevel"), DesiredWindowedResolution.X, DesiredWindowedResolution.Y);

                UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Forcing resolution to %dx%d and mode to Windowed for StartLevel."), TargetResolution.X, TargetResolution.Y);
                Settings->SetScreenResolution(TargetResolution);
                Settings->SetFullscreenMode(TargetMode);
                Settings->ApplySettings(false); // Применяем, но не подтверждаем, чтобы не мешать пользователю, если он изменит в игре
                // Settings->SaveSettings(); // Сохраняем, чтобы при следующем запуске эти настройки были

                TSharedPtr<SWindow> GameWindow = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
                if (GameWindow.IsValid())
                {
                    void* NativeWindowHandle = GameWindow->GetNativeWindow() ? GameWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
                    if (NativeWindowHandle != nullptr)
                    {
                        UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Attempting to modify window style for StartLevel..."));
#if PLATFORM_WINDOWS
                        HWND Hwnd = static_cast<HWND>(NativeWindowHandle);
                        LONG_PTR CurrentStyle = GetWindowLongPtr(Hwnd, GWL_STYLE);
                        LONG_PTR NewStyle = CurrentStyle & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX; // Запрещаем изменение размера и кнопку максимизации
                        if (NewStyle != CurrentStyle)
                        {
                            SetWindowLongPtr(Hwnd, GWL_STYLE, NewStyle);
                            // Обновляем фрейм окна, чтобы изменения стиля применились
                            SetWindowPos(Hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
                            UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Window style modified for StartLevel. Resizing disabled."));
                        }
                        else { UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Window style for StartLevel already prevents resizing.")); }
#else
                        UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Window style modification not implemented for this platform."));
#endif
                    }
                    else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get native window handle for StartLevel.")); }
                }
                else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get game SWindow for StartLevel.")); }
            }
            else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: FSlateApplication not initialized on StartLevel. Skipping resize.")); }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get GameUserSettings on StartLevel!")); }

        // Вызов ShowStartScreen ТЕПЕРЬ ЗАВИСИТ от вашего StartLevelGameMode
        // Этот флаг bIsInitialWindowSetupComplete может быть полезен для StartScreenUIManager, чтобы он знал, когда показывать UI
        bIsInitialWindowSetupComplete = true;
        UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Initial window setup for StartLevel complete flag set."));
        // if (StartScreenUIManagerInstance)
        // {
        //     StartScreenUIManagerInstance->ShowStartScreen(); // Если вы хотите, чтобы GI инициировал показ
        // }

    }
    else // Если это не StartLevel
    {
        UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Current level is %s, not StartLevel. Skipping window modifications."), *CurrentLevelName);
        // Здесь можно сбросить флаг или установить дефолтные разрешения для других уровней, если нужно
        bIsInitialWindowSetupComplete = true; // Считаем, что для других уровней настройка не нужна или уже сделана
    }
    UE_LOG(LogTemp, Warning, TEXT("--- UMyGameInstance::DelayedInitialResize() Finished ---"));
}

void UMyGameInstance::Shutdown()
{
    UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown started."));
    // Логика сохранения настроек окна при выходе остается,
    // она полезна, если вы хотите, чтобы оконный режим и его размер сохранялись между сессиями
    // только для StartLevel или для всех уровней.
    // Если вы хотите, чтобы только настройки StartLevel сохранялись, добавьте проверку на текущий уровень здесь тоже.
    // Но обычно настройки окна глобальны.
    if (bDesiredResolutionCalculated) // Этот флаг теперь будет true только если мы были на StartLevel и применили кастомный размер
    {
        UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
        if (Settings)
        {
            // Если мы хотим, чтобы игра всегда закрывалась с нашим DesiredWindowedResolution,
            // если оно было установлено для StartLevel.
            // Если игра закрывается из полноэкранного режима на другом уровне, это вернет ее в оконный.
            // Возможно, эту логику стоит пересмотреть, чтобы она была менее навязчивой.
            // Пока оставим как есть: если мы меняли размер для StartLevel, пытаемся его сохранить.
            FIntPoint CurrentSavedResolution = Settings->GetScreenResolution();
            EWindowMode::Type CurrentSavedMode = Settings->GetFullscreenMode();

            if (CurrentSavedResolution != DesiredWindowedResolution || CurrentSavedMode != EWindowMode::Windowed)
            {
                UE_LOG(LogTemp, Warning, TEXT("Shutdown: Settings differ from StartLevel's desired. Forcing save of %dx%d Windowed."),
                    DesiredWindowedResolution.X, DesiredWindowedResolution.Y);
                Settings->SetScreenResolution(DesiredWindowedResolution);
                Settings->SetFullscreenMode(EWindowMode::Windowed);
                Settings->ApplySettings(false); // Применяем, чтобы SaveSettings сохранил актуальное
                Settings->SaveSettings();
                UE_LOG(LogTemp, Warning, TEXT("Shutdown: Desired windowed settings (from StartLevel config) saved."));
            }
            else { UE_LOG(LogTemp, Log, TEXT("Shutdown: Saved settings already match StartLevel's desired.")); }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("Shutdown: Could not get GameUserSettings.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("Shutdown: Desired resolution for StartLevel was not calculated/applied. No specific save action.")); }

    Super::Shutdown();
}

// --- УДАЛЕНА ФУНКЦИЯ SetupInputMode ---

// --- Функция ApplyWindowMode остается, т.к. может использоваться из UI настроек ---
void UMyGameInstance::ApplyWindowMode(bool bWantFullscreen)
{
    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (Settings)
    {
        EWindowMode::Type TargetMode = bWantFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
        bool bModeChanged = false;

        if (Settings->GetFullscreenMode() != TargetMode)
        {
            Settings->SetFullscreenMode(TargetMode);
            bModeChanged = true;
            UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting FullscreenMode to %s"), *UEnum::GetValueAsString(TargetMode));
        }

        FIntPoint TargetResolution;
        if (bWantFullscreen)
        {
            TargetResolution = Settings->GetDesktopResolution();
        }
        else // Оконный режим
        {
            // Если мы ранее рассчитали и применили кастомное разрешение для StartLevel, используем его
            if (bDesiredResolutionCalculated && DesiredWindowedResolution.X > 0 && DesiredWindowedResolution.Y > 0)
            {
                TargetResolution = DesiredWindowedResolution;
            }
            else // Иначе используем стандартные сохраненные или дефолтные значения
            {
                TargetResolution = Settings->GetLastConfirmedScreenResolution();
                if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
                {
                    TargetResolution = Settings->GetDefaultResolution(); // Разрешение из настроек проекта
                }
                if (TargetResolution.X <= 0 || TargetResolution.Y <= 0) // Абсолютный крайний случай
                {
                    TargetResolution = FIntPoint(1280, 720); // Или ваши минимальные размеры
                }
            }
        }

        if (Settings->GetScreenResolution() != TargetResolution)
        {
            Settings->SetScreenResolution(TargetResolution);
            bModeChanged = true;
            UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting Resolution to %dx%d"), TargetResolution.X, TargetResolution.Y);
        }

        if (bModeChanged)
        {
            Settings->ApplySettings(false); // Применяем без подтверждения, чтобы пользователь мог отменить из UI
            // Settings->SaveSettings(); // Обычно SaveSettings вызывается после подтверждения пользователем в UI
            UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Settings Applied (not saved yet)."));
        }
        else { UE_LOG(LogTemp, Verbose, TEXT("ApplyWindowMode: No change needed.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("ApplyWindowMode: Could not get GameUserSettings.")); }
}

// --- Функции SetLoginStatus и SetOfflineMode остаются без изменений ---
void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername, const FString& NewFriendCode)
{
    bIsLoggedIn = bNewIsLoggedIn;
    LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1;
    LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT("");
    LoggedInFriendCode = bNewIsLoggedIn ? NewFriendCode : TEXT("");
    if (bIsLoggedIn) {
        bIsInOfflineMode = false;
    }
    UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username='%s', FriendCode='%s'"),
        bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername, *LoggedInFriendCode);
}

void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
    bIsInOfflineMode = bNewIsOffline;
    if (bIsInOfflineMode) {
        SetLoginStatus(false, -1, TEXT(""), TEXT(""));
    }
    UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));
}
