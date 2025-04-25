#include "MyGameInstance.h" 

#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h" 
#include "TimerManager.h" 
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h" 
#include "UObject/UnrealType.h" 

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"


void UMyGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Initialized."));
}

void UMyGameInstance::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("MyGameInstance Shutdown."));
	Super::Shutdown();
}


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
			TargetResolution = Settings->GetLastConfirmedScreenResolution();
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(Settings->GetDefaultResolution().X, Settings->GetDefaultResolution().Y);
			}
			if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
			{
				TargetResolution = FIntPoint(720, 540); 
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

	ApplyWindowMode(bIsFullscreenWidget);
	SetupInputMode(!bIsFullscreenWidget, !bIsFullscreenWidget); 

	if (CurrentTopLevelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ShowWidget: Removing previous top level widget: %s"), *CurrentTopLevelWidget->GetName());
		CurrentTopLevelWidget->RemoveFromParent();
	}
	CurrentTopLevelWidget = nullptr;
	CurrentContainerInstance = nullptr; 

	T* NewWidget = CreateWidget<T>(PC, WidgetClassToShow);
	if (NewWidget)
	{
		NewWidget->AddToViewport();
		CurrentTopLevelWidget = NewWidget;
		UE_LOG(LogTemp, Log, TEXT("ShowWidget: Added new widget: %s"), *WidgetClassToShow->GetName());
		return NewWidget;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ShowWidget: Failed to create widget: %s"), *WidgetClassToShow->GetName());
		return nullptr;
	}
}


void UMyGameInstance::ShowStartScreen()
{
	if (!WindowContainerClass || !StartScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: WindowContainerClass or StartScreenClass is not set!"));
		return;
	}

	UUserWidget* Container = ShowWidget<UUserWidget>(WindowContainerClass, false);

	if (Container)
	{
		CurrentContainerInstance = Container; 

		FName FunctionName = FName(TEXT("SetContentWidget"));
		UFunction* Function = CurrentContainerInstance->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			struct FSetContentParams { TSubclassOf<UUserWidget> WidgetClassToSet; }; 
			FSetContentParams Params;
			Params.WidgetClassToSet = StartScreenClass;
			CurrentContainerInstance->ProcessEvent(Function, &Params); 
			UE_LOG(LogTemp, Log, TEXT("ShowStartScreen: Set content to StartScreenClass."));
		} else { UE_LOG(LogTemp, Error, TEXT("ShowStartScreen: Function 'SetContentWidget' not found in %s!"), *WindowContainerClass->GetName()); }
	}
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); 
}

void UMyGameInstance::ShowLoginScreen()
{
	if (!WindowContainerClass || !LoginScreenClass) {
		UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: WindowContainerClass or LoginScreenClass is not set!"));
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
		Params.WidgetClassToSet = LoginScreenClass;
		CurrentContainerInstance->ProcessEvent(Function, &Params);
		UE_LOG(LogTemp, Log, TEXT("ShowLoginScreen: Set content to LoginScreenClass."));
	} else { UE_LOG(LogTemp, Error, TEXT("ShowLoginScreen: Could not find SetContentWidget in container.")); }
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

void UMyGameInstance::ShowRegisterScreen()
{
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

	UUserWidget* LoadingWidget = ShowWidget<UUserWidget>(LoadingScreenClass, true); 

	if (LoadingWidget)
	{
		GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); 
		GetWorld()->GetTimerManager().SetTimer(LoadingScreenTimerHandle, this, &UMyGameInstance::OnLoadingScreenTimerComplete, Duration, false);
		UE_LOG(LogTemp, Log, TEXT("ShowLoadingScreen: Timer Started for %.2f seconds."), Duration);
	}
}

void UMyGameInstance::OnLoadingScreenTimerComplete()
{
	UE_LOG(LogTemp, Log, TEXT("Loading Screen Timer Complete. Showing Main Menu."));
	ShowMainMenu(); 
}

void UMyGameInstance::ShowMainMenu()
{
	if (!MainMenuClass) { UE_LOG(LogTemp, Error, TEXT("ShowMainMenu: MainMenuClass is not set!")); return; }
	ShowWidget<UUserWidget>(MainMenuClass, true);
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle); 
}

void UMyGameInstance::ShowSettingsScreen()
{
	if (!SettingsScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowSettingsScreen: SettingsScreenClass is not set!")); return; }
	ShowWidget<UUserWidget>(SettingsScreenClass, true);
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}

void UMyGameInstance::ShowProfileScreen()
{
	if (!ProfileScreenClass) { UE_LOG(LogTemp, Error, TEXT("ShowProfileScreen: ProfileScreenClass is not set!")); return; }
	ShowWidget<UUserWidget>(ProfileScreenClass, true);
	GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
}


void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
	bIsLoggedIn = bNewIsLoggedIn;
	LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1;
	LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT("");
	if (bIsLoggedIn) {
		bIsInOfflineMode = false; 
	}
	UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username=%s"), bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername);
}

void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
	bIsInOfflineMode = bNewIsOffline;
	if (bIsInOfflineMode) {
		SetLoginStatus(false, -1, TEXT(""));
	}
	UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));

    if(bIsInOfflineMode)
    {
        ShowMainMenu();
    }
}


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

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnLoginResponseReceived);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestLogin: Failed to start HTTP request (ProcessRequest failed)."));
		DisplayLoginError(TEXT("Network error: Could not start request."));
	} else {
		UE_LOG(LogTemp, Log, TEXT("RequestLogin: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/login")));
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

	if (ResponseCode == 200) 
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
				SetLoginStatus(true, ReceivedUserId, ReceivedUsername);
				ShowLoadingScreen(); 
			} else {
				UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to parse 'userId' or 'username' from JSON response."));
				DisplayLoginError(TEXT("Server error: Invalid response format."));
			}
		} else {
			UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: Failed to deserialize JSON response. Body: %s"), *ResponseBody);
			DisplayLoginError(TEXT("Server error: Could not parse response."));
		}
	} else if (ResponseCode == 401) 
	{
		UE_LOG(LogTemp, Warning, TEXT("OnLoginResponseReceived: Login failed - Invalid credentials (401)."));
		DisplayLoginError(TEXT("Login failed: Incorrect username or password."));
	} else { 
		FString ErrorMessage = FString::Printf(TEXT("Login failed: Server error (Code: %d)"), ResponseCode);
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Login failed: %s"), *ServerErrorMsg); 
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

	if (ResponseCode == 201) 
	{
		UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Registration successful!"));
		ShowLoginScreen();
		FTimerHandle TempTimerHandle;
		if(GetWorld()) 
		{
			GetWorld()->GetTimerManager().SetTimer(TempTimerHandle, [this]() {
				DisplayLoginSuccessMessage(TEXT("Registration successful! Please log in."));
			}, 0.1f, false);
		}
	} else if (ResponseCode == 409) 
	{
		FString ErrorMessage = TEXT("Registration failed: Username or Email already exists."); 
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
	} else if (ResponseCode == 400)
	{
		FString ErrorMessage = TEXT("Registration failed: Invalid data provided.");
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
			FString ServerErrorMsg;
			if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
				ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("OnRegisterResponseReceived: %s (400)"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	} else { 
		FString ErrorMessage = FString::Printf(TEXT("Registration failed: Server error (Code: %d)"), ResponseCode);
		UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: %s"), *ErrorMessage);
		DisplayRegisterError(ErrorMessage);
	}
}



UUserWidget* UMyGameInstance::FindWidgetInContainer(TSubclassOf<UUserWidget> WidgetClassToFind) const
{
	if (!CurrentContainerInstance || !WidgetClassToFind)
	{
		return nullptr;
	}

	FName BorderVariableName = FName(TEXT("ContentSlotBorder"));
	FObjectProperty* BorderProp = FindFProperty<FObjectProperty>(CurrentContainerInstance->GetClass(), BorderVariableName);

	if (BorderProp)
	{
		UObject* BorderObject = BorderProp->GetObjectPropertyValue_InContainer(CurrentContainerInstance);
		UPanelWidget* ContentPanel = Cast<UPanelWidget>(BorderObject); 

		if (ContentPanel && ContentPanel->GetChildrenCount() > 0)
		{
			UWidget* ChildWidget = ContentPanel->GetChildAt(0);
			if (ChildWidget && ChildWidget->IsA(WidgetClassToFind))
			{
				return Cast<UUserWidget>(ChildWidget);
			}
			else if (ChildWidget) {
				UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: Child widget %s is not of expected class %s"),
					*ChildWidget->GetName(), *WidgetClassToFind->GetName());
			}
		}
		else if (ContentPanel) {
			UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: ContentSlotBorder has no children."));
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("FindWidgetInContainer: Failed to cast ContentSlotBorder object to UPanelWidget."));
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("FindWidgetInContainer: Could not find FObjectProperty 'ContentSlotBorder' in %s. Make sure the variable exists and is public/UPROPERTY()."),
			*CurrentContainerInstance->GetClass()->GetName());
	}

	return nullptr; 
}


void UMyGameInstance::DisplayLoginError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: %s"), *Message);

	UUserWidget* LoginWidget = FindWidgetInContainer(LoginScreenClass);

	if (LoginWidget)
	{
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		UFunction* Function = LoginWidget->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			LoginWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginError: Called DisplayErrorMessage on WBP_LoginScreen."));
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginError: Function 'DisplayErrorMessage' not found in WBP_LoginScreen!"));
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("DisplayLoginError: Could not find active WBP_LoginScreen inside container to display message."));
	}
}

void UMyGameInstance::DisplayRegisterError(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: %s"), *Message);

	UUserWidget* RegisterWidget = FindWidgetInContainer(RegisterScreenClass);

	if (RegisterWidget)
	{
		FName FunctionName = FName(TEXT("DisplayErrorMessage"));
		UFunction* Function = RegisterWidget->GetClass()->FindFunctionByName(FunctionName);
		if (Function)
		{
			int a = 2;
			struct FDisplayParams { FString Message; };
			FDisplayParams Params;
			Params.Message = Message;
			RegisterWidget->ProcessEvent(Function, &Params);
			UE_LOG(LogTemp, Verbose, TEXT("DisplayRegisterError: Called DisplayErrorMessage on WBP_RegisterScreen."));
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("DisplayRegisterError: Function 'DisplayErrorMessage' not found in WBP_RegisterScreen!"));
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("DisplayRegisterError: Could not find active WBP_RegisterScreen inside container to display message."));
	}
}

void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("DisplayLoginSuccessMessage: %s"), *Message);

	UUserWidget* LoginWidget = FindWidgetInContainer(LoginScreenClass);

	if (LoginWidget)
	{
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
			UE_LOG(LogTemp, Verbose, TEXT("DisplayLoginSuccessMessage: Called %s on WBP_LoginScreen."), *FunctionToCall->GetName());
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("DisplayLoginSuccessMessage: Neither '%s' nor '%s' found in WBP_LoginScreen!"), *SuccessFuncName.ToString(), *ErrorFuncName.ToString());
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("DisplayLoginSuccessMessage: Could not find active WBP_LoginScreen inside container to display message."));
	}
}