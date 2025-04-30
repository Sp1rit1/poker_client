// �������� ������������ ���� ������ ���������� GameInstance.
#include "MyGameInstance.h"

// --- �������� ������� Unreal Engine ---
// ����� ��� ���������� ��������� �������: ����, ����������� ����, �������� ��������.
#include "GameFramework/PlayerController.h"
// ����� ��� ������� � ��������� �������� ���� ������������ (����������, ����� ����).
#include "GameFramework/GameUserSettings.h"
// ���������� ������ ������, ����� ������� ����� �������� ������ �� ������ �����������, ������� GameUserSettings.
#include "Engine/Engine.h"
// ����� ����������� ����������� ������� ��� ����� ������� ����� (��������� �����������, ���������� �������� � �.�.).
#include "Kismet/GameplayStatics.h"
// ������� ��� ���������� ��������� (������������ ��� ��������������).
#include "TimerManager.h"
// ������� ����� ��� ���� ��������, ����������� � UMG (Unreal Motion Graphics).
#include "Blueprint/UserWidget.h"
// ������� ����� ��� ��������-����������� (��� Border, VerticalBox � �.�.), ��������� �������� � ��������� ����������.
#include "Components/PanelWidget.h"
// ��������� ��� ������ � �������� ��������� Unreal (������������ �����), ������������ ��� ������ ������� �� �����.
#include "UObject/UnrealType.h"
// �������� ��� ����� ��� ���������� ������� ������� ����.
#include "OfflineGameManager.h"

// --- ������� ��� ������ � HTTP � JSON ---
// ������ Unreal Engine, ��������������� ���������������� ��� HTTP ��������.
#include "HttpModule.h"
// ��������� ��� ��������, ��������� � �������� HTTP �������.
#include "Interfaces/IHttpRequest.h"
// ��������� ��� ��������� � ������� ������ �� HTTP ������.
#include "Interfaces/IHttpResponse.h"
// ������� ������ ��� ������������� JSON ������ (FJsonObject, FJsonValue).
#include "Json.h"
// ��������������� ������� ��� ������ � JSON (�� ������������ �������� � ���� ����, �� ����� ���� �������).
#include "JsonUtilities.h"
// ����� ��� �������������� ����� ����������� ������ C++ (����� FJsonObject) � ��������� �������������� JSON.
#include "Serialization/JsonSerializer.h"


// =============================================================================
// ���������� ������� UMyGameInstance
// =============================================================================

/**
 * @brief ����� ������������� GameInstance.
 * ��������� ������ ��� ���������� ��������� �������� ����.
 * ����� ����� ����� ���������������� ������ ���������� ���������.
 */
void UMyGameInstance::Init()
{
	// 1. ����� ���������� �������� ������ - ����������� ������ �����.
	Super::Init();
	UE_LOG(LogTemp, Log, TEXT("UMyGameInstance::Init() Started."));

	// 2. ����������� ���������� ��������� ���� (��� �������)
	// ��� �������, ����� ���������� � ����� ����������� *��* ������������ ������ �������.
	UGameUserSettings* InitialSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (InitialSettings) {
		UE_LOG(LogTemp, Warning, TEXT("Init Start Check: CurrentRes=%dx%d | CurrentMode=%d"),
			InitialSettings->GetScreenResolution().X, InitialSettings->GetScreenResolution().Y, (int32)InitialSettings->GetFullscreenMode());
	}
	else { UE_LOG(LogTemp, Warning, TEXT("Init Start Check: Settings=NULL")); }

	// 3. ������ ������� ��� ������ ������� DelayedInitialResize
	// ���������� GetTimerManager() - ���������� �������� ��������, ��������� �� UObject/UGameInstance.
	// Timer ����� ������ ���� ��� (false � ��������� ���������) ����� 0.5 �������.
	// �������� �����, ����� ���� ������ ����� ��������� ���� ����������� ������������� ���� � �������� .ini.
	const float ResizeDelay = 0.1f; // �������� � �������� (����� ���������, 0.2-0.5 ������ ����������)
	GetTimerManager().SetTimer(ResizeTimerHandle, this, &UMyGameInstance::DelayedInitialResize, ResizeDelay, false);
	UE_LOG(LogTemp, Log, TEXT("Init: Timer scheduled for DelayedInitialResize in %.2f seconds."), ResizeDelay);

	// 4. ������������� ������ ����� ���������� ������ (������)
	OfflineGameManager = NewObject<UOfflineGameManager>(this);
	if (OfflineGameManager) { UE_LOG(LogTemp, Log, TEXT("OfflineGameManager created successfully.")); }
	else { UE_LOG(LogTemp, Error, TEXT("Failed to create OfflineGameManager!")); }

	UE_LOG(LogTemp, Log, TEXT("UMyGameInstance::Init() Finished."));
}

/**
 * @brief �������, ���������� �������� ��� ��������� � ���������� �������� ����.
 * ����������� ����� ��������� ��������, ����� ������ �������� ������ �������������.
 */
void UMyGameInstance::DelayedInitialResize()
{
	// �������� ������ ���������� ���������� ������� (���������� Error ������� ��� ������� ������ � ����� ��� �������).
	UE_LOG(LogTemp, Warning, TEXT("--- DelayedInitialResize() Called ---"));

	// 1. �������� ��������� �� ������ �������� ������������.
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	// ���������, ������� �� ��� ��������.
	if (Settings)
	{
		// 2. �������� ��������� ���� ����� ������ �����������.
		UE_LOG(LogTemp, Warning, TEXT("DelayedResize Start: CurrentRes=%dx%d | CurrentMode=%d"),
			Settings->GetScreenResolution().X, Settings->GetScreenResolution().Y, (int32)Settings->GetFullscreenMode());

		// 3. �������� ���������� � �������.
		FDisplayMetrics DisplayMetrics;
		// ���������, ���������������� �� ������� Slate, ������ ��� �������� �������.
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetCachedDisplayMetrics(DisplayMetrics);

			// 4. ������ �������� ��������� ���� � ����� �� ������� ������.
			const float DesiredWidthFraction = 0.22f;  // 15% ������
			const float DesiredHeightFraction = 0.45f; // 30% ������

			// 5. ������������ ������� ���������� � ��������.
			int32 CalculatedWidth = FMath::RoundToInt(DisplayMetrics.PrimaryDisplayWidth * DesiredWidthFraction);
			int32 CalculatedHeight = FMath::RoundToInt(DisplayMetrics.PrimaryDisplayHeight * DesiredHeightFraction);

			// 6. ������������� ����������� ������� ����, ����� �������� ������� ���������� ����.
			const int32 MinWidth = 422;  // ����������� ������
			const int32 MinHeight = 486; // ����������� ������
			CalculatedWidth = FMath::Max(CalculatedWidth, MinWidth);
			CalculatedHeight = FMath::Max(CalculatedHeight, MinHeight);

			// 7. ��������� ������� ���������: ���������� � ������� �����.
			FIntPoint TargetResolution(CalculatedWidth, CalculatedHeight);
			EWindowMode::Type TargetMode = EWindowMode::Windowed; // ����������� Windowed

			// 8. ������������� ������������� � ��������� ���������.
			// �� �� ������ �������� if(Current != Target), ����� �������������� ��������� � ���������
			// ������ ��������, ���� ���� ��� �������� ������� � ��������������.
			UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Forcing resolution to %dx%d and mode to Windowed."), TargetResolution.X, TargetResolution.Y);

			// ������������� �������� � ������� ��������.
			Settings->SetScreenResolution(TargetResolution);
			Settings->SetFullscreenMode(TargetMode);

			// ��������� ��������� � ��������� ���� ����. false - �� ����� �������������.
			Settings->ApplySettings(false);

			// ��������� ����������� ��������� � ���� GameUserSettings.ini ������������.
			// ��� �������� ���, ����� ��������� ����������� � �������������� ��� ��������� ��������.
			Settings->SaveSettings();

			UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Settings applied and saved."));

			// 9. (�����������, ��� �������) ��������� ��������� ����� ����� ����������.
			FPlatformProcess::Sleep(0.1f); // ���� ������� ��������� ����� �� ������ ������.
			FIntPoint SettingsAfterApply = Settings->GetScreenResolution();
			EWindowMode::Type ModeAfterApply = Settings->GetFullscreenMode();
			UE_LOG(LogTemp, Warning, TEXT("DelayedResize End Check: CurrentRes=%dx%d | CurrentMode=%d"),
				SettingsAfterApply.X, SettingsAfterApply.Y, (int32)ModeAfterApply);

		}
		else {
			// ������: �� ������� �������� ������� �������.
			UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: FSlateApplication not initialized yet, cannot get display metrics. Skipping resize."));
		}
	}
	else {
		// ������: �� ������� �������� GameUserSettings.
		UE_LOG(LogTemp, Warning, TEXT("DelayedInitialResize: Could not get GameUserSettings!"));
	}
	// �������� ���������� �������.
	UE_LOG(LogTemp, Warning, TEXT("--- DelayedInitialResize() Finished ---"));
}


/**
 * @brief ����� ���������� ������ GameInstance.
 */
void UMyGameInstance::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown started."));
	// ����� �������� ����� ��� ��� �������, ���� �����.

	// ����������� �������� Shutdown �������� ������.
	Super::Shutdown();
}


// =============================================================================
// ��������������� ������� ��� ���������� UI, ����� � ������
// =============================================================================

/**
 * @brief ����������� ����� ����� ��� PlayerController � ��������� ������� ����.
 * @param bIsUIOnly ���� true, ��������������� ����� "������ UI" (���� �������������� ������ ���������). ���� false, "���� � UI".
 * @param bShowMouse ��������� ���������� ���������� ������� ����.
 */
void UMyGameInstance::SetupInputMode(bool bIsUIOnly, bool bShowMouse)
{
	// �������� ��������� �� ������� ���������� ������-�����������.
	APlayerController* PC = GetFirstLocalPlayerController();
	// ������ ��������� ��������� ����� ��������������.
	if (PC)
	{
		// ������������� ��������� �������.
		PC->SetShowMouseCursor(bShowMouse);
		// �������� ����� �����.
		if (bIsUIOnly)
		{
			// ����� "������ UI": ���� ���� ������ �� �������, ���� "�� �����".
			FInputModeUIOnly InputModeData;
			// �� ����������� ������ � �������� ����, ����� ����� ���� ����� �������� ��� ���.
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// ��������� �����.
			PC->SetInputMode(InputModeData);
			// �������� (Verbose - ��������� ���).
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to UI Only. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
		else
		{
			// ����� "���� � UI": ���� ���� � �� �������, � � ���� (���� �� ���������� ���������).
			FInputModeGameAndUI InputModeData;
			// ����� �� ��������� ������.
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// �� �������� ������ ������������� ��� ����� � ������� ���� (�� ��������� �� �������).
			InputModeData.SetHideCursorDuringCapture(false);
			// ��������� �����.
			PC->SetInputMode(InputModeData);
			// ��������.
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to Game And UI. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
	}
	else {
		// ��������������, ���� PlayerController �� ������.
		UE_LOG(LogTemp, Warning, TEXT("SetupInputMode: PlayerController is null."));
	}
}

/**
 * @brief ������������� ����� ���� (������� ��� �������������) � ��������������� ����������.
 * @param bWantFullscreen ���� true, ��������������� ������������� ����� (��� �����), ����� �������.
 */
void UMyGameInstance::ApplyWindowMode(bool bWantFullscreen)
{
	// �������� ������ � ���������� ������������ ����� GEngine.
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	// ���������, ��� ��������� ��������.
	if (Settings)
	{
		// ���������� ������� � ������� ������ ����.
		EWindowMode::Type CurrentMode = Settings->GetFullscreenMode();
		// ���������� WindowedFullscreen ��� �������������� ������ (������ ������ �������).
		EWindowMode::Type TargetMode = bWantFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;

		// ���� ��� ������������ ���������.
		bool bModeChanged = false;
		// ���� ����� ����� ��������.
		if (CurrentMode != TargetMode)
		{
			Settings->SetFullscreenMode(TargetMode); // ������������� ����� �����.
			bModeChanged = true;                    // ��������, ��� ���� ���������.
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting FullscreenMode to %s"), *UEnum::GetValueAsString(TargetMode)); // ��������.
		}

		// ���������� ������� ����������.
		FIntPoint TargetResolution;
		if (bWantFullscreen)
		{
			// ��� �������������� ������ ���������� �������� ���������� �������� �����.
			TargetResolution = Settings->GetDesktopResolution();
		}
		else
		{
			// ��� �������� ������ ������� �������� ����� ��������� �������������� ����������.
			TargetResolution = Settings->GetLastConfirmedScreenResolution();
			// ���� ��� ���������, ����� ���������� �� ��������� �� �������� �������.
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(Settings->GetDefaultResolution().X, Settings->GetDefaultResolution().Y);
			}
			// ���� � ��� ���������, ���������� ������ �������� �������� (���� "�������" ����������).
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(720, 540); // �������� ��������.
			}
		}

		// ���� ������� ���������� ���������� �� ��������.
		if (Settings->GetScreenResolution() != TargetResolution)
		{
			Settings->SetScreenResolution(TargetResolution); // ������������� ����� ����������.
			bModeChanged = true;                             // ��������, ��� ���� ���������.
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting Resolution to %dx%d"), TargetResolution.X, TargetResolution.Y); // ��������.
		}

		// ���� ���� �����-���� ��������� (������ ��� ����������).
		if (bModeChanged)
		{
			// ��������� ��� ���������� ���������. false - �� ��������� �������������.
			Settings->ApplySettings(false);
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Settings Applied."));
		}
		else {
			// ��������, ���� �������� ������ �� ��������.
			UE_LOG(LogTemp, Verbose, TEXT("ApplyWindowMode: Window Mode and Resolution already match target. No change needed."));
		}

	}
	else {
		// ��������������, ���� �� ������� �������� GameUserSettings.
		UE_LOG(LogTemp, Warning, TEXT("ApplyWindowMode: Could not get GameUserSettings."));
	}
}

/**
 * @brief ��������� ������� ��� ������ �������� (����������)
 * ������� ���������� ������ � ���������� �����.
 * @param WidgetClassToShow ����� ������� ��� ������.
 * @param bIsFullscreenWidget ���� �������������� ������ (������ �� ���� � ����).
 * @return ��������� �� ��������� ������ ��� nullptr.
 */
template <typename T>
T* UMyGameInstance::ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget)
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: PlayerController is null.")); return nullptr; }
	if (!WidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: WidgetClassToShow is null.")); return nullptr; }

	// --- ��� 1: ������� ������ ������ ---
	if (CurrentTopLevelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous widget: %s"), *CurrentTopLevelWidget->GetName());
		CurrentTopLevelWidget->RemoveFromParent();
	}
	CurrentTopLevelWidget = nullptr;
	// CurrentContainerInstance = nullptr; // ������ �������, �.�. ���������� ������ ���

	// --- ��� 2: ������ ����� ���� � ����� ---
	ApplyWindowMode(bIsFullscreenWidget);
	SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget); // ���� UI ��� ��������, Game+UI ��� ��������������

	// --- ��� 3: ������� � ���������� ����� ������ ---
	T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
	if (NewWidget)
	{
		NewWidget->AddToViewport();
		CurrentTopLevelWidget = NewWidget; // ��������� ��������� �� ����� ������
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Added new widget: %s"), *WidgetClassToShow->GetName());
		return NewWidget;
	}
	else { UE_LOG(LogTemp, Error, TEXT("ShowWidget: Failed to create widget: %s"), *WidgetClassToShow->GetName()); return nullptr; }
}


// =============================================================================
// ������� ��������� ����� ��������
// =============================================================================

// =============================================================================
// ������� ��������� ����� �������� (����������)
// =============================================================================

/**
 * @brief ���������� ��������� ����� ���������� ��������.
 */
void UMyGameInstance::ShowStartScreen()
{
	// ���������, ��� ����� ���������� ������ ����� � ���������� GameInstance (� Blueprint).
	if (!StartScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: StartScreenClass is not set!"));
		return; // ���������, ���� ����� �� �����.
	}

	// �������� ShowWidget ��� ����������� ���������� ������ �������� � ������� ������ (false).
	// ShowWidget ���� ������ ���������� CurrentTopLevelWidget.
	UUserWidget* StartWidget = ShowWidget<UUserWidget>(StartScreenClass, false);

	// ���������, ������� �� ������ � ������� ������.
	if (!StartWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: Failed to create/show StartScreen!"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ShowStartScreen: StartScreen displayed successfully."));
	}

	// ������������� ������ ������ ��������, ���� �� ��� �������.
	GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief ���������� ����� ����� ������������ ��������.
 */
void UMyGameInstance::ShowLoginScreen()
{
	// �������� ������� ������ ������ ������.
	if (!LoginScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: LoginScreenClass is not set!"));
		return;
	}

	// ���������� ����� ������ �������� � ������� ������ (false).
	UUserWidget* LoginWidget = ShowWidget<UUserWidget>(LoginScreenClass, false);

	if (!LoginWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: Failed to create/show LoginScreen!"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ShowLoginScreen: LoginScreen displayed successfully."));
	}

	// ������������� ������ ��������.
	GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief ���������� ����� ����������� ������ ������������ ��������.
 */
void UMyGameInstance::ShowRegisterScreen()
{
	// �������� ������� ������.
	if (!RegisterScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: RegisterScreenClass is not set!"));
		return;
	}

	// ���������� ����� ����������� �������� � ������� ������ (false).
	UUserWidget* RegisterWidget = ShowWidget<UUserWidget>(RegisterScreenClass, false);

	if (!RegisterWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: Failed to create/show RegisterScreen!"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ShowRegisterScreen: RegisterScreen displayed successfully."));
	}

	// ������������� ������ ��������.
	GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}


/**
 * @brief ���������� ������������� ������ "��������" �� �������� �����.
 * �� ��������� ������� ������������� �������� OnLoadingScreenTimerComplete (������� ������� ������� ����).
 * @param Duration ����� � ��������, �� ������� ������������ ����� ��������.
 */
void UMyGameInstance::ShowLoadingScreen(float Duration)
{
	// �������� ������ ������� ��������.
	if (!LoadingScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowLoadingScreen: LoadingScreenClass is not set!")); return; }

	// ���������� ������ �������� � ������������� ������ (true).
	UUserWidget* LoadingWidget = ShowWidget<UUserWidget>(LoadingScreenClass, true);

	// ���� ������ ������� ������.
	if (LoadingWidget)
	{
		// ������������� ���������� ������ (���� ���).
		GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
		// ��������� ����� ������.
		GetWorld()->GetTimerManager().SetTimer(
			LoadingScreenTimerHandle, // ����� ������� ��� ����������� ��� ���������.
			this,                     // ������, �� ������� ����� ������ �����.
			&UMyGameInstance::OnLoadingScreenTimerComplete, // ��������� �� �����-������.
			Duration,                 // �������� ����� �������.
			false);                   // false - ������ �� �������������.
		UE_LOG(LogTemp, Log, TEXT("ShowLoadingScreen: Timer Started for %.2f seconds."), Duration);
	}
}

/**
 * @brief �����, ���������� �� ���������� ������� ������ ��������.
 * ��������� ���� �� ������� �����.
 */
void UMyGameInstance::OnLoadingScreenTimerComplete()
{
	UE_LOG(LogTemp, Log, TEXT("Loading Screen Timer Complete. Showing Main Menu."));
	// ������ �������� ������� ������ �������� ����.
	ShowMainMenu();
}

/**
 * @brief ���������� ������� ���� ���� (������������� �����).
 */
void UMyGameInstance::ShowMainMenu()
{
	// �������� ������.
	if (!MainMenuClass) { UE_LOG(LogTemp, Error, TEXT("ShowMainMenu: MainMenuClass is not set!")); return; }
	// ���������� ������ �������� ���� � ������������� ������ (true).
	ShowWidget<UUserWidget>(MainMenuClass, true);
	// ������������� ������ ��������, ���� �� ��� �������.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief ���������� ����� �������� (������������� �����).
 */
void UMyGameInstance::ShowSettingsScreen()
{
	// �������� ������.
	if (!SettingsScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowSettingsScreen: SettingsScreenClass is not set!")); return; }
	// ����� �������.
	ShowWidget<UUserWidget>(SettingsScreenClass, true);
	// ��������� �������.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief ���������� ����� ������� ������������ (������������� �����).
 */
void UMyGameInstance::ShowProfileScreen()
{
	// �������� ������.
	if (!ProfileScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowProfileScreen: ProfileScreenClass is not set!")); return; }
	// ����� �������.
	ShowWidget<UUserWidget>(ProfileScreenClass, true);
	// ��������� �������.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}


// =============================================================================
// ���������� ���������� ���������� ���� (�����, ������� �����)
// =============================================================================

/**
 * @brief ��������� ������ ������ ������������ � ��������� ������ (ID, ���).
 * @param bNewIsLoggedIn ����� ������ ������ (true - ���������, false - ���).
 * @param NewUserId ID ������������ (���� ���������), ����� -1.
 * @param NewUsername ��� ������������ (���� ���������), ����� ������ ������.
 */
void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
	// ������������� �������� ���������� ���������.
	bIsLoggedIn = bNewIsLoggedIn;
	LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1; // ���������� ��������� �������� ��� ��������� ID ��� �������� �� ���������.
	LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT(""); // ������������� ��� ��� ������ ������.
	// ���������� ���������: ���� ������������ ���������, �� �� ��������� � ������� ������.
	if (bIsLoggedIn) {
		bIsInOfflineMode = false;
	}
	// �������� ���������� ���������.
	UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username=%s"), bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername);
}

/**
 * @brief ������������� ��� ������� ���� ������� ������.
 * ��� �������� � ������� ����� ���������� ������ ������.
 * @param bNewIsOffline ����� ������ ������� ������ (true - �������, false - ��������).
 */
void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
	// ������������� ���� ������� ������.
	bIsInOfflineMode = bNewIsOffline;
	// ���� ������������ ������ ������� �����, �� �� ����� ���� ������������ ���������.
	if (bIsInOfflineMode) {
		// �������� SetLoginStatus, ����� �������� ������ ������.
		SetLoginStatus(false, -1, TEXT(""));
	}
	// �������� ��������� �������.
	UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));

	// ���� ������� � ������� �����.
	if (bIsInOfflineMode)
	{
		// ���������� ������� ���� (�� �������� ����� ����� ����� ������� ������� ����).
		ShowMainMenu();
	}
}


// =============================================================================
// �������� HTTP �������� �� ������ (��������������)
// =============================================================================

/**
 * @brief ���������� ����������� HTTP POST ������ �� ������ ��� �������������� ������������.
 * @param Username ��������� ������������� ���.
 * @param Password ��������� ������������� ������.
 */
void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password)
{
	// �������� ������ ��������.
	UE_LOG(LogTemp, Log, TEXT("RequestLogin: Attempting login for user: %s"), *Username);

	// --- ��� 1: ���������� JSON ���� ������� ---
	// ������� ����� ��������� �� ������ JSON. MakeShareable ������ ��� �����.
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	// ��������� JSON ������ ������ "username" � "password".
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);

	// --- ��� 2: ������������ JSON � ������ ---
	FString RequestBody; // ������ ��� �������� ���������� ������������.
	// ������� JsonWriter, ������� ����� ������ � ������ RequestBody.
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	// ��������� ������������. ��������� ���������.
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		// ������ ������������ - ��������� ��������.
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to serialize JSON body."));
		// �������� �� ������ ������������ ����� UI.
		DisplayLoginError(TEXT("Client error: Could not create request."));
		return;
	}

	// --- ��� 3: �������� � ��������� HTTP ������� ---
	// �������� ������ �� �������� HTTP ������.
	FHttpModule& HttpModule = FHttpModule::Get();
	// ������� ������ �������. �������� ESPMode::ThreadSafe ����� ��� ����������� ��������.
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	// ����������� ��������� �������:
	HttpRequest->SetVerb(TEXT("POST"));                            // HTTP �����.
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/login"));             // URL ��������� ������.
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json")); // ��� ������������� ��������.
	HttpRequest->SetContentAsString(RequestBody);                   // ���� ������� (��� JSON).

	// --- ��� 4: �������� ����������� ������ ---
	// ������������� �������, ������� ����� �������, ����� ������ ������� (��� ���������� ������ ����).
	// BindUObject(this, ...) ����������� ����� OnLoginResponseReceived �������� ������� UMyGameInstance.
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnLoginResponseReceived);

	// --- ��� 5: �������� ������� ---
	// ���������� �������� �������. ��� ����������� ��������.
	if (!HttpRequest->ProcessRequest())
	{
		// ���� ������ ���� �� ������� ������ ���������� (��������, �������� � HttpModule).
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayLoginError(TEXT("Network error: Could not start request."));
	}
	else {
		// ������ ������� ��������� (�� ����� ��� �� �������).
		UE_LOG(LogTemp, Log, TEXT("RequestLogin: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/login")));
		// � ���� ����� ����� ���� �� �������� ������������ ��������� ��������.
	}
}

/**
 * @brief ���������� ����������� HTTP POST ������ �� ������ ��� ����������� ������ ������������.
 * @param Username �������� ��� ������������.
 * @param Password �������� ������.
 * @param Email ����� ����������� ����� ������������.
 */
void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
	// �������� ������ ��������.
	UE_LOG(LogTemp, Log, TEXT("RequestRegister: Attempting registration for user: %s, email: %s"), *Username, *Email);

	// --- ��� 1: ���������� JSON ���� ������� ---
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField(TEXT("username"), Username);
	RequestJson->SetStringField(TEXT("password"), Password);
	RequestJson->SetStringField(TEXT("email"), Email); // ��������� email � ������.

	// --- ��� 2: ������������ JSON � ������ ---
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("RequestRegister: Failed to serialize JSON body."));
		DisplayRegisterError(TEXT("Client error: Could not create request."));
		return;
	}

	// --- ��� 3: �������� � ��������� HTTP ������� ---
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(ApiBaseUrl + TEXT("/register")); // URL ��������� �����������.
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	// --- ��� 4: �������� ����������� ������ ---
	// ����������� ������ �����-������, ����������� ��� ������ �� �����������.
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnRegisterResponseReceived);

	// --- ��� 5: �������� ������� ---
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
// ��������� ������� �� ������� (������� HTTP ��������)
// =============================================================================

/**
 * @brief ����� ��������� ������ (callback), ������� ����������� �� ���������� HTTP ������� �� �����.
 * ����������� ����� ������� � ��������� ��������� ���� ��� ���������� ������.
 * @param Request ��������� �� �������� ������ (����� �������� ���. ����, ���� �����).
 * @param Response ��������� �� ����� �������.
 * @param bWasSuccessful ����, ����������� �� ����� ���������� ������� �� ������� ������ (�� �������� �������� �����!).
 */
void UMyGameInstance::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// --- ��� 1: �������� ������� ���������� ������� ---
	// ���� ������ �� ������ �� ������ ���� ��� ������ ������ ���������.
	if (!bWasSuccessful || !Response.IsValid())
	{
		// ��������, �������� � ����������� ��� ������ ����������.
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayLoginError(TEXT("Network error or server unavailable."));
		// ����� ����� ���� �� ������ ��������� ��������, ���� �� �����������.
		return; // �������.
	}

	// --- ��� 2: ������ ������ ������� ---
	// �������� HTTP ��� ������ (e.g., 200, 401).
	int32 ResponseCode = Response->GetResponseCode();
	// �������� ���� ������ ��� ������.
	FString ResponseBody = Response->GetContentAsString();
	// �������� ���������� ������.
	UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	// --- ��� 3: ��������� � ����������� �� ���� ������ ---
	// �������� ����� (200 OK).
	if (ResponseCode == 200)
	{
		// �������� ���������� ���� ������ ��� JSON.
		TSharedPtr<FJsonObject> ResponseJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
		{
			// JSON ������� ���������. �������� ������� ����������� ������ (userId, username).
			int64 ReceivedUserId = -1;
			FString ReceivedUsername;
			// ���������� TryGet... ��� ����������� ����������.
			if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
				ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername))
			{
				// ������ ������� ���������!
				UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Login successful for user: %s (ID: %lld)"), *ReceivedUsername, ReceivedUserId);
				// ��������� ���������� ��������� GameInstance.
				SetLoginStatus(true, ReceivedUserId, ReceivedUsername);
				// ���������� ������� � ���������� ������ (����� ��������).
				ShowLoadingScreen();
			}
			else {
				// ������: JSON ����������, �� �� �������� ������ �����.
				UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to parse 'userId' or 'username' from JSON response."));
				DisplayLoginError(TEXT("Server error: Invalid response format."));
			}
		}
		else {
			// ������: �� ������� ���������� ���� ������ ��� JSON.
			UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to deserialize JSON response. Body: %s"), *ResponseBody);
			DisplayLoginError(TEXT("Server error: Could not parse response."));
		}
	}
	// ������ �������������� (401 Unauthorized).
	else if (ResponseCode == 401)
	{
		// �������� ����� ��� ������.
		UE_LOG(LogTemp, Warning, TEXT("OnLoginResponseReceived: Login failed - Invalid credentials (401)."));
		// �������� ������������.
		DisplayLoginError(TEXT("Login failed: Incorrect username or password."));
	}
	// ������ ������ ������� (e.g., 500 Internal Server Error).
	else {
		// ��������� ��������� �� ������ �� ���������.
		FString ErrorMessage = FString::Printf(TEXT("Login failed: Server error (Code: %d)"), ResponseCode);
		// �������� �������� ����� ���������� ��������� �� ���� ������ (���� ������ ���� JSON � �������).
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				// ���������� ��������� �� �������.
				ErrorMessage = FString::Printf(TEXT("Login failed: %s"), *ServerErrorMsg);
			}
		}
		// �������� � ���������� ������.
		UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: %s"), *ErrorMessage);
		DisplayLoginError(ErrorMessage);
	}
	// ����� ����� ���� �� ������ ��������� ��������.
}

/**
 * @brief ����� ��������� ������ (callback), ������� ����������� �� ���������� HTTP ������� �� �����������.
 * ����������� ����� ������� � ���� ��������� �� ����� ������, ���� ���������� ������.
 * @param Request ��������� �� �������� ������.
 * @param Response ��������� �� ����� �������.
 * @param bWasSuccessful ���� ������ ���������� ������� �� ������� ������.
 */
void UMyGameInstance::OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// --- ��� 1: �������� ������� ���������� ������� ---
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: Request failed or response invalid. Connection error?"));
		DisplayRegisterError(TEXT("Network error or server unavailable."));
		return;
	}

	// --- ��� 2: ������ ������ ������� ---
	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

	// --- ��� 3: ��������� � ����������� �� ���� ������ ---
	// �������� ����������� (201 Created).
	if (ResponseCode == 201)
	{
		UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Registration successful!"));
		// �������������� ������������ �� ����� ������.
		ShowLoginScreen();
		// --- ����� ��������� �� ������ �� ������ ������ � ��������� ��������� ---
		FTimerHandle TempTimerHandle; // ��������� ����� ��� �������.
		// ��������� ���������� World ����� �������������� �������.
		if (GetWorld())
		{
			// ��������� ������, ������� �������� ������-������� ����� 0.1 �������.
			GetWorld()->GetTimerManager().SetTimer(TempTimerHandle, [this]() {
				// ������-������� �������� ����� ��������� �� ������.
				// [this] ����������� ��������� �� ������� ������ UMyGameInstance.
				DisplayLoginSuccessMessage(TEXT("Registration successful! Please log in."));
				}, 0.1f, false); // false - ������ �� �������������.
		}
		// --- ����� ������ ��������� ---
	}
	// ������: �������� (��� ������������ ��� email ��� ������) (409 Conflict).
	else if (ResponseCode == 409)
	{
		// ��������� �� ���������.
		FString ErrorMessage = TEXT("Registration failed: Username or Email already exists.");
		// �������� ������� ����� ��������� ��������� �� ������ �������.
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
		}
		// �������� � ���������� ������.
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (409)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	// ������: �������� ������ (������ ��������� ������ �� �������) (400 Bad Request).
	else if (ResponseCode == 400)
	{
		// ��������� �� ���������.
		FString ErrorMessage = TEXT("Registration failed: Invalid data provided.");
		// �������� ������� ����� ��������� ���������.
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
			// ����� ����� ���� �� �������� ������� ����������� ������ ��������� �� �����, ���� ������ �� �������������.
		}
		// �������� � ���������� ������.
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (400)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	// ������ ������ �������.
	else {
		FString ErrorMessage = FString::Printf(TEXT("Registration failed: Server error (Code: %d)"), ResponseCode);
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: %s"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
	// ����� ����� ���� �� ������ ��������� ��������.
}



// =============================================================================
// ��������������� ������� ��� �������������� � ��������� 
// =============================================================================


/**
 * @brief ������� �������� ������ ������ (�������� CurrentTopLevelWidget)
 * � �������� �� ��� Blueprint-������� DisplayErrorMessage.
 * @param Message ��������� �� ������ ��� �����������.
 */
void UMyGameInstance::DisplayLoginError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: %s"), *Message);

	// ���������, ��� ������� ������ �������� ������ ����������,
	// � ��� �� ����� ���������� ����� (LoginScreenClass).
	if (CurrentTopLevelWidget && LoginScreenClass && CurrentTopLevelWidget->IsA(LoginScreenClass))
	{
		// ��������� �� ������� ������ (��� ��������).
		UUserWidget* LoginWidget = CurrentTopLevelWidget;

		// ��� Blueprint-������� ��� ������.
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		// ���� ������� � ������ �������.
		UFunction* Function = LoginWidget->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			// ������� ��������� � �������� ������� ����� ProcessEvent.
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			LoginWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginError: Called DisplayErrorMessage on %s."), *LoginWidget->GetName());
		}
		else {
			// ������: ������� �� ������� � Blueprint �������.
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginError: Function 'DisplayErrorMessage' not found in %s!"), *LoginScreenClass->GetName());
		}
	}
	else {
		// �������� �������, �� ������� �� ������� �������� ������.
		if (!CurrentTopLevelWidget) {
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: CurrentTopLevelWidget is null when trying to display error."));
		}
		else if (!LoginScreenClass) {
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: LoginScreenClass is null variable in GameInstance."));
		}
		else {
			// ������� ������ �� �������� �������� ������.
			UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: CurrentTopLevelWidget (%s) is not the expected LoginScreenClass (%s). Cannot display error."),
				*CurrentTopLevelWidget->GetName(), *LoginScreenClass->GetName());
		}
	}
}

/**
 * @brief ������� �������� ������ ����������� (�������� CurrentTopLevelWidget)
 * � �������� �� ��� Blueprint-������� DisplayErrorMessage.
 * @param Message ��������� �� ������ ��� �����������.
 */
void UMyGameInstance::DisplayRegisterError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: %s"), *Message);

	// ��������� ������� ������ �������� ������ �� ��� RegisterScreenClass.
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
 * @brief ������� �������� ������ ������ (�������� CurrentTopLevelWidget)
 * � �������� �� ��� Blueprint-������� DisplaySuccessMessage (��� DisplayErrorMessage).
 * @param Message ��������� �� ������ ��� �����������.
 */
void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("DisplayLoginSuccessMessage: %s"), *Message);

	// ��������� ������� ������ �������� ������ �� ��� LoginScreenClass.
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

