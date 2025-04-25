#include "MyGameInstance.h" // �������� �� ��� ������ .h �����, ���� ����������

// --- �������� ������� UE ---
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h" // ����� �����������
#include "TimerManager.h" // ��� ��������

// --- ������� ��� HTTP � JSON ---
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
// #include "JsonObjectConverter.h" // ����������������, ���� �����������

// --- ������� ��� UMG (������ ���� ����� ����������� ������� � C++) ---
// #include "Components/TextBlock.h"
// #include "Components/EditableTextBox.h"


// =============================================================================
// ������������� � ����������
// =============================================================================

void UMyGameInstance::Init()
{
	Super::Init();
	// �� ���������� ����� ������ �����, ���� ������ �� GameMode::BeginPlay
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Initialized."));
}

void UMyGameInstance::Shutdown()
{
	// �����������: ������� ������� ����� ��� ������ �� ����
	// ApplyWindowMode(false);
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown."));
	Super::Shutdown();
}

// =============================================================================
// ��������������� ������� ��� UI/����/�����
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
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); // �� �����������
			// InputModeData.SetWidgetToFocus(CurrentContainerInstance ? CurrentContainerInstance->TakeWidget() : nullptr); // ����� ������ ����� ����������
			PC->SetInputMode(InputModeData);
			UE_LOG(LogTemp, Verbose, TEXT("Input Mode set to UI Only. Mouse Visible: %s"), bShowMouse ? TEXT("True") : TEXT("False"));
		}
		else // GameAndUI ��� �������������� ������
		{
			FInputModeGameAndUI InputModeData;
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); // ����� �� �����������, ���� ����� �������� �� ����
			InputModeData.SetHideCursorDuringCapture(false); // �� �������� ������������� ��� �����
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
		// ���������� WindowedFullscreen ��� ������� "�������������" �����
		EWindowMode::Type TargetMode = bWantFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;

		bool bModeChanged = false;
		if (CurrentMode != TargetMode)
		{
			Settings->SetFullscreenMode(TargetMode);
			bModeChanged = true; // ������ ����� ����
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting FullscreenMode to %s"), *UEnum::GetValueAsString(TargetMode));
		}

		// ������������� ���������� ������ ���� ������ ����� ��� ���� ������� �� �������������
		FIntPoint TargetResolution;
		if (bWantFullscreen)
		{
			TargetResolution = Settings->GetDesktopResolution();
		}
		else
		{
			// ���������� ����������, ����������� ��� "LastConfirmed", ��� ���������, ��� ��������
			TargetResolution = Settings->GetLastConfirmedScreenResolution();
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(Settings->GetDefaultResolution().X, Settings->GetDefaultResolution().Y);
			}
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(720, 540); // ��������, ���� ��� ��������� ���������
			}
		}

		if (Settings->GetScreenResolution() != TargetResolution)
		{
			Settings->SetScreenResolution(TargetResolution);
			bModeChanged = true; // ������ ����������
			UE_LOG(LogTemp, Log, TEXT("ApplyWindowMode: Setting Resolution to %dx%d"), TargetResolution.X, TargetResolution.Y);
		}

		// ��������� ���������, ������ ���� ���-�� ����������
		if (bModeChanged)
		{
			Settings->ApplySettings(false); // false - �� ��������� �������������
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

	// 1. ������������� ����� ���� � �����/����
	// ������������� ����� �� ����� �������, ����� �������� ��������/������������� �������
	ApplyWindowMode(bIsFullscreenWidget);
	SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget); // UIOnly+������ ��� ��������, GameUI+���������� ��� fullscreen

	// 2. ������� ������ ������ �������� ������
	if (CurrentTopLevelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous top level widget: %s"), *CurrentTopLevelWidget->GetName());
		CurrentTopLevelWidget->RemoveFromParent();
	}
	CurrentTopLevelWidget = nullptr;
	CurrentContainerInstance = nullptr; // ��������� ����� �� �������, ���� �� ���������� ����� ������ �������� ������

	// 3. ������� � ��������� ����� ������
	T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
	if (NewWidget)
	{
		NewWidget->AddToViewport();
		CurrentTopLevelWidget = NewWidget; // ���������� ����� ������
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
// ������� ���������
// =============================================================================

void UMyGameInstance::ShowStartScreen()
{
	if (!WindowContainerClass || !StartScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: WindowContainerClass or StartScreenClass is not set!"));
		return;
	}
	// ���������� ��������� (�� �������� ��� UUserWidget*)
	UUserWidget* Container = ShowWidget<UUserWidget>(WindowContainerClass, false); // false = ������� �����

	if (Container)
	{
		CurrentContainerInstance = Container; // ���������� ������ �� ���������
		// �������� ������� 'SetContentWidget' ������ ���������� ����������
		FName FunctionName = FName(TEXT("SetContentWidget")); // ��� ������� � WBP_WindowContainer
		UFunction* Function = CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; }; // ��������� ��� ���������
			FSetContentParams Params;
			Params.WidgetClassToSet = StartScreenClass; // �������� ����� ���������� ������
			CurrentContainerInstance->ProcessEvent(Function, &Params); // �������� BP �������
			UE_LOG(LogTemp, Log, TEXT("ShowStartScreen: Set content to StartScreenClass."));
		} else { UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: Function 'SetContentWidget' not found in %s!"), *WindowContainerClass->GetName()); }
	}
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); // ������������� ������ ��������
}

void UMyGameInstance::ShowLoginScreen()
{
	if (!WindowContainerClass || !LoginScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: WindowContainerClass or LoginScreenClass is not set!"));
		return;
	}
	// �������� ���������������� ������������ ���������, ���� �� �������
	UUserWidget* Container = CurrentContainerInstance;
	if (!Container || CurrentTopLevelWidget != CurrentContainerInstance)
	{
		// ���������� ��� ��� �� ���������, ������� ������
		Container = ShowWidget<UUserWidget>(WindowContainerClass, false); // false = ������� �����
		if (Container) CurrentContainerInstance = Container;
		else return; // �� ������� ������� ���������
	} else {
		// ��������� ��� ����, ������ �������� ����/���� ��� UI
		SetupInputMode(true, true);
	}

	// �������� 'SetContentWidget' � ����������
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
	// ������ ���������� ShowLoginScreen, �� � RegisterScreenClass
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

	// ���������� ������ �������� � ������������� ������
	UUserWidget* LoadingWidget = ShowWidget<UUserWidget>(LoadingScreenClass, true); // true = fullscreen

	if (LoadingWidget)
	{
		// ��������� ������ ��� �������� ����� "��������"
		GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); // ������� ������ ������
		GetWorld()->GetTimerManager().SetTimer(LoadingScreenTimerHandle, this, &UMyGameInstance::OnLoadingScreenTimerComplete, Duration, false);
		UE_LOG(LogTemp, Log, TEXT("ShowLoadingScreen: Timer Started for %.2f seconds."), Duration);
	}
}

void UMyGameInstance::OnLoadingScreenTimerComplete()
{
	UE_LOG(LogTemp, Log, TEXT("Loading Screen Timer Complete. Showing Main Menu."));
	ShowMainMenu(); // ��������� � ������� ����
}

void UMyGameInstance::ShowMainMenu()
{
	if (!MainMenuClass) { UE_LOG(LogTemp, Error, TEXT("ShowMainMenu: MainMenuClass is not set!")); return; }
	// ���������� ������� ���� � ������������� ������
	ShowWidget<UUserWidget>(MainMenuClass, true); // true = fullscreen
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); // ������������� ������ ��������, ���� �� ��� ���
}

void UMyGameInstance::ShowSettingsScreen()
{
	if (!SettingsScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowSettingsScreen: SettingsScreenClass is not set!")); return; }
	// ������, ������ �� ���� ����� ���� ������������� ��� � ����������
	// ������ ��� ��������������:
	ShowWidget<UUserWidget>(SettingsScreenClass, true);
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

void UMyGameInstance::ShowProfileScreen()
{
	if (!ProfileScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowProfileScreen: ProfileScreenClass is not set!")); return; }
	// ������, ������ �� ���� ����� ���� ������������� ��� � ����������
	// ������ ��� ��������������:
	ShowWidget<UUserWidget>(ProfileScreenClass, true);
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

// =============================================================================
// ���������� ���������� ����
// =============================================================================

void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
	bIsLoggedIn = bNewIsLoggedIn;
	LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1;
	LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT("");
	if (bIsLoggedIn) {
		bIsInOfflineMode = false; // ������ ���� ����������� � � �������� ������������
	}
	UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username=%s"), bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername);
}

void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
	bIsInOfflineMode = bNewIsOffline;
	if (bIsInOfflineMode) {
		SetLoginStatus(false, -1, TEXT("")); // ������� �� ������ ��� �������� � �������
	}
	UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));

    if(bIsInOfflineMode)
    {
        // ��� �������� � ������� �����, ������ ���������� ������� ���� (��� ����� ������� ����)
        // ���������, ��� ��� ����������� ���� � ������ ����� (��������, fullscreen)
        ShowMainMenu();
    }
}

// =============================================================================
// ������ HTTP ��������
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

	// ����������� ���������� ������
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnLoginResponseReceived);

	// ���������� ������
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayLoginError(TEXT("Network error: Could not start request."));
	} else {
		UE_LOG(LogTemp, Log, TEXT("RequestLogin: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/login")));
		// ����� �������� ��������� �������� � UI, ���� �����
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
// ��������� ������� HTTP
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
			// ���������� TryGet... ��� ����������� ����������
			if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
				ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername))
			{
				UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Login successful for user: %s (ID: %lld)"), *ReceivedUsername, ReceivedUserId);
				SetLoginStatus(true, ReceivedUserId, ReceivedUsername);
				// ��������� �� ����� �������� -> ������� ����
				ShowLoadingScreen(); // ���������� �������� Duration �� ���������
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
	} else { // ������ ������
		FString ErrorMessage = FString::Printf(TEXT("Login failed: Server error (Code: %d)"), ResponseCode);
		// ������� ������� ��������� �� JSON ���� ������
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Login failed: %s"), *ServerErrorMsg); // ���������� ��������� �������
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
		// ��������� �� ����� ������, ����� ������������ ��� �����
		ShowLoginScreen();
		// ��������� ������ ��� ������ ��������� �� ������ ����� ����, ��� ����� ������ �����������
		FTimerHandle TempTimerHandle;
		if(GetWorld()) // ��������� ���������� World ����� �������������� TimerManager
		{
			GetWorld()->GetTimerManager().SetTimer(TempTimerHandle, [this]() {
				DisplayLoginSuccessMessage(TEXT("Registration successful! Please log in."));
			}, 0.1f, false); // ��������� ��������
		}
	} else if (ResponseCode == 409) // Conflict
	{
		FString ErrorMessage = TEXT("Registration failed: Username or Email already exists."); // ��������� �� ���������
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
	} else if (ResponseCode == 400) // Bad Request (������ ��������� �� �������)
	{
		FString ErrorMessage = TEXT("Registration failed: Invalid data provided.");
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			// ��������� ������� ��������� ���������, ���� ������ ��� ����
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
			// ����� ����� ������� ������ errors, ���� ������ ��� ���������� ��� ��������� ������ �� �����
		}
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (400)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	} else { // ������ ������
		FString ErrorMessage = FString::Printf(TEXT("Registration failed: Server error (Code: %d)"), ResponseCode);
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: %s"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
}


// =============================================================================
// ����� ������� ����������� ��������� � Blueprint
// =============================================================================

// ��������������� ������� ��� ������ ������� � ������� �������
void CallWidgetFunction(UUserWidget* Widget, FName FunctionName, const FString& Message)
{
    if (!Widget) { UE_LOG(LogTemp, Warning, TEXT("CallWidgetFunction: Widget is null for function %s"), *FunctionName.ToString()); return; }

    UFunction* Function = Widget->GetClass()->FindFunctionByName(FunctionName);
    if (Function)
    {
        // ��������� ��� �������� ��������� FString
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
	// ��������� ��� ��� �������� ����� �������
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
    // ������ ����������� ���������� ��� ��
    CallWidgetFunction(CurrentTopLevelWidget, FName(TEXT("DisplayErrorMessage")), Message);
}

void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
    UE_LOG(LogTemp, Log, TEXT("DisplayLoginSuccessMessage: %s"), *Message);
    // ��������� �� ������ ����������� ���������� �� ������ ������ (������� ������ ���� �������)
    // �������� ������� "DisplaySuccessMessage", ���� ��� - ������� "DisplayErrorMessage"
    FName SuccessFuncName = FName(TEXT("DisplaySuccessMessage"));
    FName ErrorFuncName = FName(TEXT("DisplayErrorMessage"));
    if (CurrentTopLevelWidget && CurrentTopLevelWidget->GetClass()->FindFunctionByName(SuccessFuncName))
    {
        CallWidgetFunction(CurrentTopLevelWidget, SuccessFuncName, Message);
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Function '%s' not found, falling back to '%s'."), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
        CallWidgetFunction(CurrentTopLevelWidget, ErrorFuncName, Message); // ������� �� ������� ������
    }
}