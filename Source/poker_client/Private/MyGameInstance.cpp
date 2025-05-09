#include "MyGameInstance.h"
#include "StartScreenUIManager.h"   // Включаем заголовки наших новых менеджеров
#include "NetworkAuthManager.h"
#include "OfflineGameManager.h"   // OfflineGameManager остался

// --- Основные инклуды Unreal Engine (как были) ---
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
// Blueprint/UserWidget.h, Components/PanelWidget.h и т.д. теперь больше нужны в StartScreenUIManager

// --- Для работы с окнами (как были) ---
#include "SlateBasics.h"
#include "GenericPlatform/GenericApplication.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#endif


UMyGameInstance::UMyGameInstance()
{
    // Инициализация указателей на менеджеры
    StartScreenUIManagerInstance = nullptr;
    NetworkAuthManagerInstance = nullptr;
    OfflineGameManager = nullptr;

    // Инициализация флагов и переменных состояния (как было)
    bIsInitialWindowSetupComplete = false;
    DesiredWindowedResolution = FIntPoint::ZeroValue;
    bDesiredResolutionCalculated = false;
    bIsLoggedIn = false;
    LoggedInUsername = TEXT("");
    LoggedInUserId = -1;
    LoggedInFriendCode = TEXT("");
    bIsInOfflineMode = false;

    // ApiBaseUrl инициализируется значением по умолчанию из .h или может быть изменена в Blueprint
}

void UMyGameInstance::Init()
{
    Super::Init();
    UE_LOG(LogTemp, Log, TEXT("UMyGameInstance::Init() Started."));

    // Логирование начального состояния окна (как было)
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
            RegisterScreenClass,
            LoadingScreenClass,
            LoadingMediaPlayerAsset,
            LoadingMediaSourceAsset
        );
        UE_LOG(LogTemp, Log, TEXT("StartScreenUIManager created and initialized."));
    }
    else { UE_LOG(LogTemp, Error, TEXT("Failed to create StartScreenUIManager!")); }

    NetworkAuthManagerInstance = NewObject<UNetworkAuthManager>(this);
    if (NetworkAuthManagerInstance) {
        NetworkAuthManagerInstance->Initialize(this, ApiBaseUrl);
        UE_LOG(LogTemp, Log, TEXT("NetworkAuthManager created and initialized."));
    }
    else { UE_LOG(LogTemp, Error, TEXT("Failed to create NetworkAuthManager!")); }

    OfflineGameManager = NewObject<UOfflineGameManager>(this); // OfflineGameManager остался
    if (OfflineGameManager) { UE_LOG(LogTemp, Log, TEXT("OfflineGameManager created successfully.")); }
    else { UE_LOG(LogTemp, Error, TEXT("Failed to create OfflineGameManager!")); }


    // Запуск таймера для DelayedInitialResize (как было)
    const float ResizeDelay = 0.1f;
    GetTimerManager().SetTimer(ResizeTimerHandle, this, &UMyGameInstance::DelayedInitialResize, ResizeDelay, false);
    UE_LOG(LogTemp, Log, TEXT("Init: Timer scheduled for DelayedInitialResize in %.2f seconds."), ResizeDelay);

    UE_LOG(LogTemp, Log, TEXT("UMyGameInstance::Init() Finished."));
}

void UMyGameInstance::DelayedInitialResize()
{
    UE_LOG(LogTemp, Warning, TEXT("--- UMyGameInstance::DelayedInitialResize() Called ---"));
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
            bDesiredResolutionCalculated = true;
            UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Stored desired windowed resolution %dx%d"), DesiredWindowedResolution.X, DesiredWindowedResolution.Y);

            UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Forcing resolution to %dx%d and mode to Windowed."), TargetResolution.X, TargetResolution.Y);
            Settings->SetScreenResolution(TargetResolution);
            Settings->SetFullscreenMode(TargetMode);
            Settings->ApplySettings(false);
            Settings->SaveSettings();
            UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Settings applied and saved."));

            TSharedPtr<SWindow> GameWindow = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
            if (GameWindow.IsValid())
            {
                void* NativeWindowHandle = GameWindow->GetNativeWindow() ? GameWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
                if (NativeWindowHandle != nullptr)
                {
                    UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Attempting to modify window style..."));
#if PLATFORM_WINDOWS
                    HWND Hwnd = static_cast<HWND>(NativeWindowHandle);
                    LONG_PTR CurrentStyle = GetWindowLongPtr(Hwnd, GWL_STYLE);
                    LONG_PTR NewStyle = CurrentStyle & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX;
                    if (NewStyle != CurrentStyle)
                    {
                        SetWindowLongPtr(Hwnd, GWL_STYLE, NewStyle);
                        SetWindowPos(Hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
                        UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Window style modified. Resizing disabled."));
                    }
                    else { UE_LOG(LogTemp, Log, TEXT("DelayedInitialResize: Window style already prevents resizing.")); }
#else
                    UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Window style modification not implemented for this platform."));
#endif
                }
                else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get native window handle.")); }
            }
            else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get game SWindow.")); }

            bIsInitialWindowSetupComplete = true;
            UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Initial setup complete flag set. Calling ShowStartScreen..."));

            // Вызываем ShowStartScreen через новый менеджер
            if (StartScreenUIManagerInstance)
            {
                StartScreenUIManagerInstance->ShowStartScreen();
            }
            else { UE_LOG(LogTemp, Error, TEXT("DelayedInitialResize: StartScreenUIManagerInstance is null! Cannot show start screen.")); }

        }
        else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: FSlateApplication not initialized. Skipping resize.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get GameUserSettings!")); }
    UE_LOG(LogTemp, Warning, TEXT("--- UMyGameInstance::DelayedInitialResize() Finished ---"));
}

void UMyGameInstance::Shutdown()
{
    UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown started."));
    if (bDesiredResolutionCalculated)
    {
        UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
        if (Settings)
        {
            FIntPoint CurrentSavedResolution = Settings->GetScreenResolution();
            EWindowMode::Type CurrentSavedMode = Settings->GetFullscreenMode();
            if (CurrentSavedResolution != DesiredWindowedResolution || CurrentSavedMode != EWindowMode::Windowed)
            {
                UE_LOG(LogTemp, Warning, TEXT("Shutdown: Settings differ. Forcing save of %dx%d Windowed."),
                    DesiredWindowedResolution.X, DesiredWindowedResolution.Y);
                Settings->SetScreenResolution(DesiredWindowedResolution);
                Settings->SetFullscreenMode(EWindowMode::Windowed);
                Settings->SaveSettings();
                UE_LOG(LogTemp, Warning, TEXT("Shutdown: Desired windowed settings saved."));
            }
            else { UE_LOG(LogTemp, Log, TEXT("Shutdown: Saved settings already match desired.")); }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("Shutdown: Could not get GameUserSettings.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("Shutdown: Desired resolution not calculated. Cannot force save.")); }

    Super::Shutdown();
}

// --- Вспомогательные Функции для Управления Окном и Вводом (остаются здесь) ---
void UMyGameInstance::SetupInputMode(bool bIsUIOnly, bool bShowMouse)
{
    APlayerController* PC = GetFirstLocalPlayerController();
    if (PC)
    {
        PC->SetShowMouseCursor(bShowMouse);
        if (bIsUIOnly)
        {
            FInputModeUIOnly InputModeData;
            InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputModeData);
            UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to UI Only. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
        }
        else
        {
            FInputModeGameAndUI InputModeData;
            InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputModeData.SetHideCursorDuringCapture(false);
            PC->SetInputMode(InputModeData);
            UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to Game And UI. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
        }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("SetupInputMode: PlayerController is null.")); }
}

void UMyGameInstance::ApplyWindowMode(bool bWantFullscreen)
{
    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (Settings)
    {
        EWindowMode::Type CurrentMode = Settings->GetFullscreenMode();
        EWindowMode::Type TargetMode = bWantFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
        bool bModeChanged = false;
        if (CurrentMode != TargetMode)
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
        else
        {
            if (bDesiredResolutionCalculated) // Используем наше рассчитанное разрешение для оконного режима
            {
                TargetResolution = DesiredWindowedResolution;
            }
            else // Запасной вариант, если расчет не прошел
            {
                TargetResolution = Settings->GetLastConfirmedScreenResolution();
                if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
                {
                    TargetResolution = FIntPoint(Settings->GetDefaultResolution().X, Settings->GetDefaultResolution().Y);
                }
                if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
                {
                    TargetResolution = FIntPoint(324, 486); // Абсолютный запасной вариант
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
            Settings->ApplySettings(false);
            UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Settings Applied."));
        }
        else { UE_LOG(LogTemp, Verbose, TEXT("ApplyWindowMode: No change needed.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("ApplyWindowMode: Could not get GameUserSettings.")); }
}

// --- Управление Глобальным Состоянием Игры (остается здесь) ---
void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername, const FString& NewFriendCode)
{
    bIsLoggedIn = bNewIsLoggedIn;
    LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1;
    LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT("");
    LoggedInFriendCode = bNewIsLoggedIn ? NewFriendCode : TEXT(""); // Добавлено сохранение кода друга
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
        SetLoginStatus(false, -1, TEXT(""), TEXT("")); // Сбрасываем все данные логина
    }
    UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));
}