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
void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password) { UE_LOG(LogTemp, Warning, TEXT("RequestLogin function called but not implemented yet.")); }
void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email) { UE_LOG(LogTemp, Warning, TEXT("RequestRegister function called but not implemented yet.")); }