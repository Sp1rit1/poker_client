// MyGameInstance.cpp
#include "MyGameInstance.h" // ��� ������������ ����
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h" // ���������, ��� ������ ����

// --- ��������������� ������� ---

void UMyGameInstance::SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass)
{
    // ������� ���������� �����
    if (CurrentScreenWidget && CurrentScreenWidget->IsValidLowLevel())
    {
        CurrentScreenWidget->RemoveFromParent();
        CurrentScreenWidget = nullptr;
    }

    // ������� � ��������� ����� �����
    if (NewScreenClass)
    {
        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
        if (PlayerController)
        {
            CurrentScreenWidget = CreateWidget<UUserWidget>(PlayerController, NewScreenClass);
            if (CurrentScreenWidget)
            {
                CurrentScreenWidget->AddToViewport();
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to create widget for class %s"), *NewScreenClass->GetName());
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SwitchScreen called with null NewScreenClass"));
    }
}

// --- �������� ������ ---

void UMyGameInstance::Init()
{
    Super::Init(); // ����� ������� ������������ Init
    ShowStartScreen(); // ���������� ��������� ����� ��� �������
}

void UMyGameInstance::ShowStartScreen()
{
    // ������� ������, ���� ����� ������� ���� � ������ ��������
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(StartScreenClass);
    // ���������� ������� ��� �������� �� ��������� �����
    SetLoginStatus(false, -1, TEXT(""));
    SetOfflineMode(false);
}

void UMyGameInstance::ShowLoginScreen()
{
    // ������� ������ �� ������ ������
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(LoginScreenClass);
}

void UMyGameInstance::ShowRegisterScreen()
{
    // ������� ������, ���� ����� ������� ���� � ������ ��������
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(RegisterScreenClass); // ���������� ����� ���������� ������
}

void UMyGameInstance::ShowLoadingScreen(float Duration)
{
    // ���������� ������ ��������
    SwitchScreen(LoadingScreenClass);

    // ������� ������ ������, ���� �� ���
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);

    // ��������� ����� ������
    GetWorld()->GetTimerManager().SetTimer(
        LoadingScreenTimerHandle,          // ����� ��� ���������� ��������
        this,                              // ������, ��� ����� ����� ������
        &UMyGameInstance::OnLoadingScreenTimerComplete, // ��������� �� ����� ��� ������
        Duration,                          // �������� � ��������
        false                              // false = ������ ��������� ���� ���
    );
    UE_LOG(LogTemp, Log, TEXT("Showing Loading Screen. Timer set for %.2f seconds."), Duration);
}

void UMyGameInstance::OnLoadingScreenTimerComplete()
{
    // ���� ����� ����� ������ ������������� �� ��������� ������� �������
    UE_LOG(LogTemp, Log, TEXT("Loading screen timer finished."));
    // ���������� ������� ���� (SwitchScreen ��� ������ ����� ��������)
    ShowMainMenu();
}

void UMyGameInstance::ShowMainMenu()
{
    // ������� ������ �� ������ ������ (���� ���� ������ �� �� �������)
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(MainMenuClass);
    // ��������, ����� ����������� �������� UI �������� ����
    // � ����������� �� bIsLoggedIn ��� bIsInOfflineMode,
    // �� ��� ����� ������� � � ����� ������� WBP_MainMenu
}

// --- ������ ���������� ���������� ---

void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
    // (���������� �������� ��� ��, ��� � ������)
    bIsLoggedIn = bNewIsLoggedIn;
    LoggedInUserId = bNewIsLoggedIn ? NewUserId : -1;
    LoggedInUsername = bNewIsLoggedIn ? NewUsername : TEXT("");
    if (bIsLoggedIn)
    {
        bIsInOfflineMode = false;
    }
    UE_LOG(LogTemp, Log, TEXT("Login Status Updated: LoggedIn=%s, UserID=%lld, Username=%s"),
        bIsLoggedIn ? TEXT("true") : TEXT("false"), LoggedInUserId, *LoggedInUsername);
}

void UMyGameInstance::SetOfflineMode(bool bNewIsOffline)
{
    // (���������� �������� ��� ��, ��� � ������)
    bIsInOfflineMode = bNewIsOffline;
    if (bIsInOfflineMode)
    {
        SetLoginStatus(false, -1, TEXT(""));
    }
    UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"),
        bIsInOfflineMode ? TEXT("true") : TEXT("false"));
}

// --- ���������� HTTP ������� (����� � ���� 2.7) ---

void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password)
{
    UE_LOG(LogTemp, Warning, TEXT("RequestLogin function called but not implemented yet."));
    // ����� ����� ������ HTTP �������...
    // ��� �������� ������ ������� ����� ����� ������� �� ShowMainMenu(),
    // � ShowLoadingScreen()!
    // �������� ���:
    // if (Success) {
    //     SetLoginStatus(true, ReceivedUserId, ReceivedUsername);
    //     ShowLoadingScreen(5.0f); // ��������� ����� �������� �� 5 ������
    // } else { ... ��������� ������ ... }
}

void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
    UE_LOG(LogTemp, Warning, TEXT("RequestRegister function called but not implemented yet."));
    // ����� ����� ������ HTTP �������...
    // ��� �������� ������ ������� (201 Created) ������ ������ ���������� ���������
    // �� ������ ������ ��� ������ �� ������, ��������� �� ���.
}

// ... ���������� OnLoginResponseReceived � OnRegisterResponseReceived ...