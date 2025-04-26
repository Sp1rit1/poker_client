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
// ������������� � ���������� ������ GameInstance
// =============================================================================


/*@brief <������� ��������>
����������: ������������� �������, ������������ �������� ��������(������, �������, ����������, enum � �.�.), � ������� ��������� �����������.
@param <���_���������> <��������>
����������: ��������� ������� �������� ������� ��� ������.
@return <��������>
����������: ��������� ��������, ������������ �������� ��� �������.
@warning <�������� ��������������>
����������: �������� ������ �������������� ��� ���������� � ������������� ���������, �������� �������� ��� ������ �������� �������������.
@note <�������� �������>
����������: ��������� �������������� ������� ��� ����������, ������� �� �������� �� �������� ���������, �� ���������������, �� ����� ���� ������� ��� ���������.
*/


/**
 * @brief ���������� ���� ��� ��� �������� ���������� GameInstance (� ������ ����).
 * ��������� ����� ��� ������������� ���������� ���������� � ������,
 * ������� ������ ������������ �� ���������� ���� ������ ����.
 */
void UMyGameInstance::Init()
{
	// ����������� �������� Init() �������� ������ UGameInstance.
	Super::Init();

	// --- �������� ��������� ������� ���� ---
	// ������� ��������� UOfflineGameManager. 'this' (������� UMyGameInstance)
	// ���������� ��� Outer (��������), �������� ��������� �����.
	OfflineGameManager = NewObject<UOfflineGameManager>(this);
	// ���������, ������� �� ��� ������ ������.
	if (OfflineGameManager)
	{
		// �������� �����.
		UE_LOG(LogTemp, Log, TEXT("OfflineGameManager created successfully."));
	}
	else {
		// �������� ����������� ������, ���� ������� �� �������.
		UE_LOG(LogTemp, Error, TEXT("Failed to create OfflineGameManager!"));
	}

	// �������� ���������� �������� ������������� GameInstance.
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Initialized."));
}

/**
 * @brief ���������� ��� ����������� ���������� GameInstance (��� �������� ����).
 * ����� ��� ������������ ��������, ���������� ��������� � �.�.
 */
void UMyGameInstance::Shutdown()
{
	// �������� ������ �������� ���������� ������.
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown."));
	// ����������� �������� Shutdown() �������� ������.
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
 * @brief ��������� ������� ��� ���������������� ��������, ������ � ���������� ��������� �������� ������.
 * ������� ���������� ������ �������� ������ ����� ������� ������.
 * ����������� ����� ���� � ����� � ����������� �� ����� bIsFullscreenWidget.
 * @tparam T ��� �������, ������� ����� ������� (�� ��������� UUserWidget). ��������� ������� ���������� ��� ���������.
 * @param WidgetClassToShow ����� ������� (TSubclassOf<UUserWidget>), ��������� �������� ����� �������.
 * @param bIsFullscreenWidget ���� true, ������ ��������� ������������� (����������� ������������� ����� ����, ���� GameAndUI, ���� ������).
 *                            ���� false, ������ ��������� "�������" (����������� ������� �����, ���� UIOnly, ���� ��������).
 * @return T* ��������� �� ��������� ��������� ������� ���� T, ��� nullptr � ������ ������.
 */
template <typename T>
T* UMyGameInstance::ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget)
{
	// �������� PlayerController.
	APlayerController* PC = GetFirstLocalPlayerController();
	// ��������� PlayerController � ���������� ����� �������.
	if (!PC) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: PlayerController is null.")); return nullptr; }
	if (!WidgetClassToShow) { UE_LOG(LogTemp, Error, TEXT("ShowWidget: WidgetClassToShow is null.")); return nullptr; }

	// --- ��� 1: ��������� ������ ���� � ����� ---
	ApplyWindowMode(bIsFullscreenWidget);                 // ������������� �������/������������� �����.
	SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget); // ������������� ����� ����� � ��������� ���� (������������� �� bIsFullscreenWidget).

	// --- ��� 2: �������� ����������� ������� �������� ������ ---
	// ���� ���� ��������� �� ���������� ������.
	if (CurrentTopLevelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous top level widget: %s"), *CurrentTopLevelWidget->GetName());
		// ������� ������ �� �������� � ������.
		CurrentTopLevelWidget->RemoveFromParent();
	}
	// �������� ��������� �� ������ �������.
	CurrentTopLevelWidget = nullptr;
	CurrentContainerInstance = nullptr; // ����� ���������� ��������� �� ���������, ���� �� ��� �������.

	// --- ��� 3: �������� � ����� ������ ������� ---
	// ������� ��������� ������� ������� ������. CreateWidget ���������� ��� UUserWidget*, �� ��� ��� ������� ���������, �� ����� �������� � T*.
	T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
	// ���������, ������� �� ������ ������.
	if (NewWidget)
	{
		// ��������� ������ � �������� ������� ����.
		NewWidget->AddToViewport();
		// ��������� ��������� �� ����� ������ ��� ������� ������ �������� ������.
		CurrentTopLevelWidget = NewWidget;
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Added new widget: %s"), *WidgetClassToShow->GetName());
		// ���������� ��������� �� ��������� ������.
		return NewWidget;
	}
	else
	{
		// �������� ������ �������� �������.
		UE_LOG(LogTemp, Error, TEXT("ShowWidget: Failed to create widget: %s"), *WidgetClassToShow->GetName());
		// ���������� nullptr.
		return nullptr;
	}
}


// =============================================================================
// ������� ��������� ����� ��������
// =============================================================================

/**
 * @brief ���������� ��������� ����� ���������� (��������, � �������� "�����", "�������", "�����").
 * ���������� ������-��������� ��� �������� �������� ������.
 */
void UMyGameInstance::ShowStartScreen()
{
	// ���������, ��� ������ �������� ���������� � ���������� ������ ������ � ���������� GameInstance (� Blueprint).
	if (!WindowContainerClass || !StartScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: WindowContainerClass or StartScreenClass is not set!"));
		return; // ���������, ���� ������ �� ������.
	}

	// �������� ShowWidget ��� ����������� �������-���������� � ������� ������ (false).
	UUserWidget* Container = ShowWidget<UUserWidget>(WindowContainerClass, false);

	// ���� ��������� ��� ������� ������ � �������.
	if (Container)
	{
		// ��������� ��������� �� ������� ��������� ����������.
		CurrentContainerInstance = Container;

		// --- ����� Blueprint-������� ������ ���������� ��� ��������� �������� ---
		FName FunctionName = FName(TEXT("SetContentWidget")); // ��� ������� � Blueprint (WBP_WindowContainer).
		// ���� ������� �� ����� � ������ �������-����������.
		UFunction* Function = CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName);
		// ���� ������� �������.
		if (Function)
		{
			// �������������� ��������� ��� �������� ���������� � Blueprint-�������.
			// ��������� ������ ��������� � ����������� ������� � BP.
			struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
			FSetContentParams Params;
			Params.WidgetClassToSet = StartScreenClass; // ��������� ����� ���������� ������ ��� ��������.
			// �������� Blueprint-������� ����� ������� ��������� ProcessEvent.
			CurrentContainerInstance->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Log, TEXT("ShowStartScreen: Set content to StartScreenClass."));
		}
		else {
			// ������, ���� ������������ ������� �� ������� � ������� ����������.
			UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: Function 'SetContentWidget' not found in %s!"), *WindowContainerClass->GetName());
		}
	}
	// ������������� ������ ������ ��������, ���� �� ��� ������� (��������, ��� �������� �� ����).
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief ���������� ����� ����� ������������ (�����/������).
 * ���������� ������-���������.
 */
void UMyGameInstance::ShowLoginScreen()
{
	// �������� ������� ������� ���������� � ������ ������.
	if (!WindowContainerClass || !LoginScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: WindowContainerClass or LoginScreenClass is not set!"));
		return;
	}

	// --- ������ �����������������/�������� ���������� ---
	// �������� �������� ������� ��������� ����������.
	UUserWidget* Container = CurrentContainerInstance;
	// ���� ���������� ��� ��� ������� ������ �������� ������ - ��� �� ��� ���������
	// (������, ��� ������� �����-�� ������ ������, ��������, �������������).
	if (!Container || CurrentTopLevelWidget != CurrentContainerInstance)
	{
		// ����� ������� � �������� ��������� ������.
		Container = ShowWidget<UUserWidget>(WindowContainerClass, false);
		// ���� ������� ������, ��������� ���������.
		if (Container) CurrentContainerInstance = Container;
		else return; // ���� �� ������� ������� ���������, �������.
	}
	else {
		// ��������� ��� �������, ������ ��������, ��� ����� ����� � ���� ��������� ��� UI.
		SetupInputMode(true, true);
	}

	// --- ��������� �������� ������ ���������� ---
	FName FunctionName = FName(TEXT("SetContentWidget")); // ��� ������� � BP.
	// ���� ������� � ������ �������� ����������.
	UFunction* Function = CurrentContainerInstance ? CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName) : nullptr;
	if (Function)
	{
		// ������� ��������� � �������� �������, ��������� LoginScreenClass.
		struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; };
		FSetContentParams Params;
		Params.WidgetClassToSet = LoginScreenClass;
		CurrentContainerInstance->ProcessEvent(Function, &Params);
		UE_LOG(LogTemp, Log, TEXT("ShowLoginScreen: Set content to LoginScreenClass."));
	}
	else { UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: Could not find SetContentWidget in container.")); }
	// ������������� ������ ��������.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

/**
 * @brief ���������� ����� ����������� ������ ������������.
 * ���������� ������-���������.
 */
void UMyGameInstance::ShowRegisterScreen()
{
	// �������� ������� �������.
	if (!WindowContainerClass || !RegisterScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowRegisterScreen: WindowContainerClass or RegisterScreenClass is not set!"));
		return;
	}
	// ������ ���������/�������� ���������� ���������� ShowLoginScreen.
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

	// ��������� RegisterScreenClass ��� �������� ����������.
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
	// ������������� ������ ��������.
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
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
// ��������������� ������� ��� �������������� � ��������� � ����������
// =============================================================================

/**
 * @brief ������� �������� ������ ���������� ������ ������ �������-����������.
 * ���������� ��������� ��� ������� � ��������� �������, ����������� � ����� 'ContentSlotBorder'.
 * @param WidgetClassToFind ����� �������, ������� �� ����.
 * @return UUserWidget* ��������� �� ��������� ������ ��� nullptr, ���� �� ������ ��� ��������� ������.
 * @note ������� �������� `const`, ��� ��� ��� �� �������� ��������� GameInstance.
 */
UUserWidget* UMyGameInstance::FindWidgetInContainer(TSubclassOf<UUserWidget> WidgetClassToFind) const
{
	// ��������� ������� ��������� ���������� � �������� ������.
	if (!CurrentContainerInstance || !WidgetClassToFind)
	{
		return nullptr; // ������ ������ ��� �����.
	}

	// ��� ���������� (UPROPERTY) � ������ WBP_WindowContainer, ������� ��������� �� UBorder, �������� ������.
	FName BorderVariableName = FName(TEXT("ContentSlotBorder"));
	// ���������� ������� ��������� UE ��� ������ �������� (����������) ���� FObjectProperty �� �����.
	FObjectProperty* BorderProp = FindFProperty<FObjectProperty>(CurrentContainerInstance->GetClass(), BorderVariableName);

	// ���� �������� (���������� UBorder) ������� � ������ ����������.
	if (BorderProp)
	{
		// �������� �������� ����� �������� (��������� �� UObject) �� ����������� ���������� ����������.
		UObject* BorderObject = BorderProp->GetObjectPropertyValue_InContainer(CurrentContainerInstance);
		// �������� �������� (cast) ���� UObject � UPanelWidget (������� ����� ��� ��������, ������� �������� ��������, ������� UBorder).
		UPanelWidget* ContentPanel = Cast<UPanelWidget>(BorderObject);

		// ���� ���������� ������� � � ������ ���� �������� ��������.
		if (ContentPanel && ContentPanel->GetChildrenCount() > 0)
		{
			// �������� ������ (� ������������, ��� ������������) �������� ������ ������ UBorder.
			UWidget* ChildWidget = ContentPanel->GetChildAt(0);
			// ���������, ��� �������� ������ ���������� � ��� ����� �������� ��� ����������� �� WidgetClassToFind.
			if (ChildWidget && ChildWidget->IsA(WidgetClassToFind))
			{
				// ���� �� ���������, �������� ��������� � UUserWidget � ���������� ���.
				return Cast<UUserWidget>(ChildWidget);
			}
			// �������� ��������������, ���� �������� ������ ����, �� �� �� ���� ������, ������� �� ������.
			else if (ChildWidget) {
				UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: Child widget %s is not of expected class %s"),
					*ChildWidget->GetName(), *WidgetClassToFind->GetName());
			}
		}
		// �������� ��������������, ���� � ������ (Border) ��� �������� ��������.
		else if (ContentPanel) {
			UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: ContentSlotBorder has no children."));
		}
		// �������� ��������������, ���� ������, �� ������� ��������� ContentSlotBorder, �� �������� UPanelWidget (������������, �� ��������).
		else {
			UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: Failed to cast ContentSlotBorder object to UPanelWidget."));
		}
	}
	// �������� ������, ���� ���������� � ������ 'ContentSlotBorder' �� ������� � ������ ����������.
	// ���������, ��� � WBP_WindowContainer ���� UBorder, �� ������ 'ContentSlotBorder', � � ���� ������� ���� 'Is Variable'.
	else {
		UE_LOG(LogTemp, Error, TEXT("FindWidgetInContainer: Could not find FObjectProperty 'ContentSlotBorder' in %s. Make sure the variable exists and is public/UPROPERTY()."),
			*CurrentContainerInstance->GetClass()->GetName());
	}

	// ���������� nullptr �� ���� �������, ����� ������ �� ��� ������� ������.
	return nullptr;
}


/**
 * @brief ������� �������� ������ ������ � �������� �� ��� Blueprint-������� DisplayErrorMessage.
 * @param Message ��������� �� ������ ��� �����������.
 */
void UMyGameInstance::DisplayLoginError(const FString& Message)
{
	// �������� ��������� �� ������.
	UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: %s"), *Message);

	// ���� �������� ��������� WBP_LoginScreen ������ ������ �������-����������.
	UUserWidget* LoginWidget = FindWidgetInContainer(LoginScreenClass);

	// ���� ������ ������ ������ (�.�., �� ������ ������� ������ ����������).
	if (LoginWidget)
	{
		// ��� Blueprint-�������, ���������� �� ����������� ������ � WBP_LoginScreen.
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		// ���� ��� ������� �� ����� � ������ ������� ������.
		UFunction* Function = LoginWidget->GetClass()->FindFunctionByName(FunctionName);
		// ���� ������� �������.
		if (Function)
		{
			// �������������� ��������� ��� ������ ������� (��������� ���� �������� ���� FString).
			struct FDisplayParams { FString Message; }; // ��������� ������ ��������������� ��������� ������� � BP.
			FDisplayParams Params;
			Params.Message = Message; // �������� ���������.
			// �������� Blueprint-������� �� ���������� ���������� ������� ������.
			LoginWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginError: Called DisplayErrorMessage on WBP_LoginScreen."));
		}
		else {
			// ������: � WBP_LoginScreen ��� ������� � ����� ������.
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginError: Function 'DisplayErrorMessage' not found in WBP_LoginScreen!"));
		}
	}
	else {
		// ��������������: �� ������� ����� �������� ������ ������ ��� ����������� ������
		// (��������, ������������ ��� ������� �� ������ �����).
		UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: Could not find active WBP_LoginScreen inside container to display message."));
	}
}

/**
 * @brief ������� �������� ������ ����������� � �������� �� ��� Blueprint-������� DisplayErrorMessage.
 * @param Message ��������� �� ������ ��� �����������.
 */
void UMyGameInstance::DisplayRegisterError(const FString& Message)
{
	// �������� ������.
	UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: %s"), *Message);

	// ���� �������� ������ ����������� ������ ����������.
	UUserWidget* RegisterWidget = FindWidgetInContainer(RegisterScreenClass);

	// ���� ������ ������.
	if (RegisterWidget)
	{
		// ���� ������� DisplayErrorMessage � WBP_RegisterScreen.
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		UFunction* Function = RegisterWidget->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			// ������� ��������� � �������� �������.
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			RegisterWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayRegisterError: Called DisplayErrorMessage on WBP_RegisterScreen."));
		}
		else {
			// ������: ������� �� �������.
			UE_LOG(LogTemp, Error, TEXT("DisplayRegisterError: Function 'DisplayErrorMessage' not found in WBP_RegisterScreen!"));
		}
	}
	else {
		// ��������������: ������ �� ������.
		UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: Could not find active WBP_RegisterScreen inside container to display message."));
	}
}

/**
 * @brief ������� �������� ������ ������ � �������� �� ��� Blueprint-������� DisplaySuccessMessage (��� DisplayErrorMessage ��� �������� �������).
 * ������������ ��� ������ ��������� �� �������� ����������� �� ������ ������.
 * @param Message ��������� �� ������ ��� �����������.
 */
void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
	// �������� ��������� �� ������.
	UE_LOG(LogTemp, Log, TEXT("DisplayLoginSuccessMessage: %s"), *Message);

	// ���� �������� ������ ������ (�.�. ���������� ��������� ������ ���).
	UUserWidget* LoginWidget = FindWidgetInContainer(LoginScreenClass);

	// ���� ������ ������.
	if (LoginWidget)
	{
		// ����� �������: ���������������� ��� ������ � �������� (������� ������� ������).
		FName SuccessFuncName = FName(TEXT("DisplaySuccessMessage"));
		FName ErrorFuncName = FName(TEXT("DisplayErrorMessage"));

		// �������� ����� ����������� ������� ��� ��������� �� ������.
		UFunction* FunctionToCall = LoginWidget->GetClass()->FindFunctionByName(SuccessFuncName);
		// ���� ��� �� �������...
		if (!FunctionToCall)
		{
			// ...�������� ��� � �������� ����� ����������� ������� ��� ��������� �� �������.
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Function '%s' not found, falling back to '%s'."), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
			FunctionToCall = LoginWidget->GetClass()->FindFunctionByName(ErrorFuncName);
		}

		// ���� ������� ����� ���� �� ���� �� ������� (������ ��� ������).
		if (FunctionToCall)
		{
			// ������� ��������� � �������� ��������� �������.
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			LoginWidget->ProcessEvent(FunctionToCall, &Params);
			// ��������, ����� ������ ������� ���� �������.
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Called %s on WBP_LoginScreen."), *FunctionToCall->GetName());
		}
		else {
			// ������: � WBP_LoginScreen ��� �� ������� ������, �� ������� ������. �� ����� �������� ���������.
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginSuccessMessage: Neither '%s' nor '%s' found in WBP_LoginScreen!"), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
		}
	}
	else {
		// ��������������: �� ������ �������� ������ ������.
		UE_LOG(LogTemp, Warning, TEXT("DisplayLoginSuccessMessage: Could not find active WBP_LoginScreen inside container to display message."));
	}
}