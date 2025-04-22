// MyGameInstance.cpp
#include "MyGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// --- ��������������� ������� ---

void UMyGameInstance::SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass)
{
    // --- ��� 1: �������� ����������� ������ ---
    if (!NewScreenClass)
    {
        UE_LOG(LogTemp, Error, TEXT("SwitchScreen FAILED: NewScreenClass is NULL!"));
        return;
    }
    // �������� ��� ������, ������� �������� ��������
    // ���������� GetName() ��� GetClass()->GetName() ��� ��������� �����
    UE_LOG(LogTemp, Log, TEXT("SwitchScreen Attempting to show: %s"), *NewScreenClass->GetName());

    // ������� ���������� �����
    if (CurrentScreenWidget && CurrentScreenWidget->IsValidLowLevel())
    {
        UE_LOG(LogTemp, Log, TEXT("SwitchScreen Removing previous widget: %s"), *CurrentScreenWidget->GetName());
        CurrentScreenWidget->RemoveFromParent();
        CurrentScreenWidget = nullptr;
    }
    else {
        UE_LOG(LogTemp, Log, TEXT("SwitchScreen No previous widget to remove."));
    }

    // ������� � ��������� ����� �����
    // --- ��� 2: �������� PlayerController ---
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("SwitchScreen FAILED: Could not get PlayerController!"));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("SwitchScreen Found PlayerController: %s"), *PlayerController->GetName());

    // --- ��� 3: ������� �������� ������� ---
    UE_LOG(LogTemp, Log, TEXT("SwitchScreen Calling CreateWidget..."));
    CurrentScreenWidget = CreateWidget<UUserWidget>(PlayerController, NewScreenClass);

    // --- ��� 4: �������� ���������� �������� ---
    if (CurrentScreenWidget)
    {
        UE_LOG(LogTemp, Log, TEXT("SwitchScreen Widget %s CREATED successfully! Adding to viewport."), *CurrentScreenWidget->GetName());
        CurrentScreenWidget->AddToViewport(); // ZOrder ����� �������� ��� �������������: AddToViewport(10);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SwitchScreen FAILED: CreateWidget returned NULL for class %s! Check if class is valid and assigned in BP_MyGameInstance."), *NewScreenClass->GetName());
    }
}

// --- �������� ������ ---

void UMyGameInstance::Init()
{
    Super::Init();
    UE_LOG(LogTemp, Warning, TEXT("===== MyGameInstance Init() CALLED ====="));
    // ShowStartScreen(); // <-- ��������������� ��� ������� ���� �����
    // ������������� �������� ��������
    SetLoginStatus(false, -1, TEXT(""));
    SetOfflineMode(false);
}

void UMyGameInstance::ShowStartScreen()
{
    // --- ��� 6: �������� ������ ShowStartScreen ---
    UE_LOG(LogTemp, Warning, TEXT("===== ShowStartScreen() CALLED ====="));
    // ������� ������, ���� ����� ������� ���� � ������ ��������
    if (GetWorld()) // ������� �������� �� ���������� GetWorld()
    {
        GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    }
    SwitchScreen(StartScreenClass);
    // ���������� ������� ��� �������� �� ��������� �����
    SetLoginStatus(false, -1, TEXT(""));
    SetOfflineMode(false);
}

// ... �������� ����������� UE_LOG(LogTemp, Warning, ...) � ������
// ������ ����� ������� Show...Screen(), ����� ����������� ��������� ...

void UMyGameInstance::ShowLoginScreen()
{
    UE_LOG(LogTemp, Warning, TEXT("===== ShowLoginScreen() CALLED ====="));
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(LoginScreenClass);
}

void UMyGameInstance::ShowRegisterScreen()
{
    UE_LOG(LogTemp, Warning, TEXT("===== ShowRegisterScreen() CALLED ====="));
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(RegisterScreenClass);
}

void UMyGameInstance::ShowLoadingScreen(float Duration)
{
    UE_LOG(LogTemp, Warning, TEXT("===== ShowLoadingScreen() CALLED (Duration: %.2f) ====="), Duration);
    SwitchScreen(LoadingScreenClass); // ������� ������� ������

    // ������ ��������� ������ ���� ���� ���
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
        World->GetTimerManager().SetTimer(
            LoadingScreenTimerHandle,
            this,
            &UMyGameInstance::OnLoadingScreenTimerComplete,
            Duration,
            false
        );
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("ShowLoadingScreen FAILED: GetWorld() returned NULL, cannot set timer!"));
    }
}

void UMyGameInstance::OnLoadingScreenTimerComplete()
{
    UE_LOG(LogTemp, Warning, TEXT("===== OnLoadingScreenTimerComplete() CALLED ====="));
    ShowMainMenu();
}

void UMyGameInstance::ShowMainMenu()
{
    UE_LOG(LogTemp, Warning, TEXT("===== ShowMainMenu() CALLED ====="));
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(MainMenuClass);
}


void UMyGameInstance::ShowProfileScreen()
{
    UE_LOG(LogTemp, Warning, TEXT("===== ShowProfileScreen() CALLED ====="));
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(ProfileScreenClass);
}


void UMyGameInstance::ShowSettingsScreen()
{
    UE_LOG(LogTemp, Warning, TEXT("===== ShowSettingsScreen() CALLED ====="));
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(SettingsScreenClass);
}

// --- ������ ���������� ���������� (���� ����� �������� ��� ����) ---
void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
    bIsLoggedIn = bNewIsLoggedIn;
    LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1;
    LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT("");
    if (bIsLoggedIn) { bIsInOfflineMode = false; }
    UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username=%s"), bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername);
}

void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
    bIsInOfflineMode = bNewIsOffline;
    if (bIsInOfflineMode) { SetLoginStatus(false, -1, TEXT("")); }
    UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"), bIsInOfflineMode ? TEXT("true") : TEXT("false"));
}




// --- �������� HTTP ������� ---
void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password)
{
    UE_LOG(LogTemp, Log, TEXT("Attempting login for user: %s"), *Username);

    // 1. �������� JSON �������
    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("username"), Username);
    RequestJson->SetStringField(TEXT("password"), Password);

    // 2. ������������ JSON � ������
    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    // 3. �������� HTTP �������
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/login")); // ���������� ApiBaseUrl �� .h
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);

    // 4. �������� ������� ��������� ������ (��������)
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnLoginResponseReceived);

    // 5. �������� �������
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to start login request."));
        // ���������� �������� ������ � UI?
        DisplayLoginError(TEXT("Network error: Could not start request."));
    }
    else {
        // �����������: �������� ��������� �������� � UI (���� ��� �� �������)
        // ClearLoginError(); // �������� ���������� ������
    }
}


void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
    UE_LOG(LogTemp, Log, TEXT("Attempting registration for user: %s, email: %s"), *Username, *Email);

    // 1. �������� JSON �������
    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("username"), Username);
    RequestJson->SetStringField(TEXT("password"), Password);
    RequestJson->SetStringField(TEXT("email"), Email); // ��������� email

    // 2. ������������ JSON � ������
    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    // 3. �������� HTTP �������
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/register")); // �������� �����������
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);

    // 4. �������� ������� ��������� ������
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMyGameInstance::OnRegisterResponseReceived);

    // 5. �������� �������
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to start registration request."));
        DisplayRegisterError(TEXT("Network error: Could not start request."));
    }
    else {
        // ClearRegisterError();
    }
}




void UMyGameInstance::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Login request failed or response invalid."));
        DisplayLoginError(TEXT("Network error or server unavailable."));
        return;
    }

    int32 ResponseCode = Response->GetResponseCode();
    FString ResponseBody = Response->GetContentAsString();

    UE_LOG(LogTemp, Log, TEXT("Login Response Code: %d"), ResponseCode);
    UE_LOG(LogTemp, Log, TEXT("Login Response Body: %s"), *ResponseBody);

    if (ResponseCode == 200) // HTTP OK
    {
        // ������ JSON �����
        TSharedPtr<FJsonObject> ResponseJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

        if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
        {
            int64 ReceivedUserId = -1;
            FString ReceivedUsername;

            if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
                ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername))
            {
                UE_LOG(LogTemp, Log, TEXT("Login successful for user: %s (ID: %lld)"), *ReceivedUsername, ReceivedUserId);
                SetLoginStatus(true, ReceivedUserId, ReceivedUsername); // ��������� ��������� � GameInstance
                ShowLoadingScreen(); // ���������� ����� ��������, ������� ����� ������� ������� ����
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse userId or username from login response."));
                DisplayLoginError(TEXT("Server error: Invalid response format."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to deserialize login response JSON."));
            DisplayLoginError(TEXT("Server error: Could not parse response."));
        }
    }
    else if (ResponseCode == 401) // Unauthorized
    {
        UE_LOG(LogTemp, Warning, TEXT("Login failed: Invalid credentials."));
        DisplayLoginError(TEXT("Login failed: Incorrect username or password."));
    }
    else
    {
        // ������ ������ (400, 404, 500 � �.�.)
        FString ErrorMessage = FString::Printf(TEXT("Login failed: Server error (Code: %d)"), ResponseCode);
        // ���������� ������� ��������� �� ������ �� ���� ������, ���� ������ ��� ����
        TSharedPtr<FJsonObject> ErrorJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
            FString ServerErrorMsg;
            if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) { // ������������, ��� ������ ���� ���� "message"
                ErrorMessage = FString::Printf(TEXT("Login failed: %s (Code: %d)"), *ServerErrorMsg, ResponseCode);
            }
        }
        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMessage);
        DisplayLoginError(ErrorMessage);
    }
}


void UMyGameInstance::OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Register request failed or response invalid."));
        DisplayRegisterError(TEXT("Network error or server unavailable."));
        return;
    }

    int32 ResponseCode = Response->GetResponseCode();
    FString ResponseBody = Response->GetContentAsString(); // ����� ��������� ��������� �� ������

    UE_LOG(LogTemp, Log, TEXT("Register Response Code: %d"), ResponseCode);
    UE_LOG(LogTemp, Log, TEXT("Register Response Body: %s"), *ResponseBody);


    if (ResponseCode == 201) // HTTP Created
    {
        UE_LOG(LogTemp, Log, TEXT("Registration successful!"));
        // �����! ��������� �� ����� ������ � ���������� ��� ���������
        ShowLoginScreen();
        // ��������� �������� ����� ������� ���������, ����� ����� ����� �������������?
        // ��� ����� �������� ����/�������� � ShowLoginScreen? ����� ���� ���:
        FTimerHandle TempTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TempTimerHandle, [this]() {
            DisplayLoginSuccessMessage(TEXT("Registration successful! Please log in."));
            }, 0.1f, false); // ��������� ��������
    }
    else if (ResponseCode == 409) // Conflict (Username/Email exists)
    {
        FString ErrorMessage = TEXT("Registration failed: Username or Email already exists.");
        // ��������� �������� ����� ���������� ��������� �� �������
        TSharedPtr<FJsonObject> ErrorJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
            FString ServerErrorMsg;
            if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
                ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMessage);
        DisplayRegisterError(ErrorMessage);
    }
    else if (ResponseCode == 400) // Bad Request (e.g., invalid email format, weak password from server validation)
    {
        FString ErrorMessage = TEXT("Registration failed: Invalid data provided.");
        // ��������� �������� ����� ���������� ��������� �� �������
        TSharedPtr<FJsonObject> ErrorJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid()) {
            FString ServerErrorMsg;
            if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg)) {
                ErrorMessage = FString::Printf(TEXT("Registration failed: %s"), *ServerErrorMsg);
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMessage);
        DisplayRegisterError(ErrorMessage);
    }
    else
    {
        // ������ ������
        FString ErrorMessage = FString::Printf(TEXT("Registration failed: Server error (Code: %d)"), ResponseCode);
        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMessage);
        DisplayRegisterError(ErrorMessage);
    }
}


void UMyGameInstance::DisplayLoginError(const FString& Message)
{
    if (CurrentScreenWidget)
    {
        // ��� ������� ������ ����� ��������� � ������ �������/������� � Blueprint �������!
        FName FunctionName = FName(TEXT("DisplayErrorMessage"));
        // ���������, ���� �� ����� ������� � �������
        if (CurrentScreenWidget->GetClass()->FindFunctionByName(FunctionName))
        {
            // ������� ��������� ��� �������� ��������� (������)
            struct FDisplayParams {
                FString Message;
            };
            FDisplayParams Params;
            Params.Message = Message;
            // �������� ������� � Blueprint
            CurrentScreenWidget->ProcessEvent(CurrentScreenWidget->GetClass()->FindFunctionByName(FunctionName), &Params);
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Function 'DisplayErrorMessage' not found in widget %s"), *CurrentScreenWidget->GetName());
        }
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("Cannot display login error, CurrentScreenWidget is null"));
    }
}

void UMyGameInstance::DisplayRegisterError(const FString& Message)
{
    if (CurrentScreenWidget)
    {
        FName FunctionName = FName(TEXT("DisplayErrorMessage"));
        if (CurrentScreenWidget->GetClass()->FindFunctionByName(FunctionName))
        {
            struct FDisplayParams { FString Message; };
            FDisplayParams Params;
            Params.Message = Message;
            CurrentScreenWidget->ProcessEvent(CurrentScreenWidget->GetClass()->FindFunctionByName(FunctionName), &Params);
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Function 'DisplayErrorMessage' not found in widget %s"), *CurrentScreenWidget->GetName());
        }
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("Cannot display register error, CurrentScreenWidget is null"));
    }
}

void UMyGameInstance::DisplayLoginSuccessMessage(const FString& Message)
{
    // ��� ������� ���������� ����� ShowLoginScreen(), ������� CurrentScreenWidget ������ ���� WBP_LoginScreen
    if (CurrentScreenWidget)
    {
        // ����� ������������ �� �� ������� DisplayErrorMessage, �� � ������ �������,
        // ��� ������� ��������� ������� � BP, ��������, DisplaySuccessMessage
        FName FunctionName = FName(TEXT("DisplaySuccessMessage")); // !!! �������� ��� ������� � WBP_LoginScreen Blueprint
        if (CurrentScreenWidget->GetClass()->FindFunctionByName(FunctionName))
        {
            struct FDisplayParams { FString Message; };
            FDisplayParams Params;
            Params.Message = Message;
            CurrentScreenWidget->ProcessEvent(CurrentScreenWidget->GetClass()->FindFunctionByName(FunctionName), &Params);
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Function 'DisplaySuccessMessage' not found in widget %s. Trying 'DisplayErrorMessage' instead."), *CurrentScreenWidget->GetName());
            // ������� �� DisplayErrorMessage, ���� DisplaySuccessMessage ���
            FunctionName = FName(TEXT("DisplayErrorMessage"));
            if (CurrentScreenWidget->GetClass()->FindFunctionByName(FunctionName))
            {
                struct FDisplayParams { FString Message; };
                FDisplayParams Params;
                Params.Message = Message;
                CurrentScreenWidget->ProcessEvent(CurrentScreenWidget->GetClass()->FindFunctionByName(FunctionName), &Params);
            }
            else {
                UE_LOG(LogTemp, Error, TEXT("Neither 'DisplaySuccessMessage' nor 'DisplayErrorMessage' found in widget %s"), *CurrentScreenWidget->GetName());
            }
        }
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("Cannot display login success message, CurrentScreenWidget is null"));
    }
}







