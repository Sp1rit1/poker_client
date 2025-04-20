// MyGameInstance.cpp
#include "MyGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// --- Вспомогательная функция ---

void UMyGameInstance::SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass)
{
    // --- ЛОГ 1: Проверка переданного класса ---
    if (!NewScreenClass)
    {
        UE_LOG(LogTemp, Error, TEXT("SwitchScreen FAILED: NewScreenClass is NULL!"));
        return;
    }
    // Логируем имя класса, который пытаемся показать
    // Используем GetName() или GetClass()->GetName() для получения имени
    UE_LOG(LogTemp, Log, TEXT("SwitchScreen Attempting to show: %s"), *NewScreenClass->GetName());

    // Удаляем предыдущий экран
    if (CurrentScreenWidget && CurrentScreenWidget->IsValidLowLevel())
    {
        UE_LOG(LogTemp, Log, TEXT("SwitchScreen Removing previous widget: %s"), *CurrentScreenWidget->GetName());
        CurrentScreenWidget->RemoveFromParent();
        CurrentScreenWidget = nullptr;
    }
    else {
        UE_LOG(LogTemp, Log, TEXT("SwitchScreen No previous widget to remove."));
    }

    // Создаем и добавляем новый экран
    // --- ЛОГ 2: Проверка PlayerController ---
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("SwitchScreen FAILED: Could not get PlayerController!"));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("SwitchScreen Found PlayerController: %s"), *PlayerController->GetName());

    // --- ЛОГ 3: Попытка создания виджета ---
    UE_LOG(LogTemp, Log, TEXT("SwitchScreen Calling CreateWidget..."));
    CurrentScreenWidget = CreateWidget<UUserWidget>(PlayerController, NewScreenClass);

    // --- ЛОГ 4: Проверка результата создания ---
    if (CurrentScreenWidget)
    {
        UE_LOG(LogTemp, Log, TEXT("SwitchScreen Widget %s CREATED successfully! Adding to viewport."), *CurrentScreenWidget->GetName());
        CurrentScreenWidget->AddToViewport(); // ZOrder можно добавить при необходимости: AddToViewport(10);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SwitchScreen FAILED: CreateWidget returned NULL for class %s! Check if class is valid and assigned in BP_MyGameInstance."), *NewScreenClass->GetName());
    }
}

// --- Основные Методы ---

void UMyGameInstance::Init()
{
    Super::Init();
    UE_LOG(LogTemp, Warning, TEXT("===== MyGameInstance Init() CALLED ====="));
    // ShowStartScreen(); // <-- ЗАКОММЕНТИРУЙТЕ ИЛИ УДАЛИТЕ ЭТОТ ВЫЗОВ
    // Инициализация статусов остается
    SetLoginStatus(false, -1, TEXT(""));
    SetOfflineMode(false);
}

void UMyGameInstance::ShowStartScreen()
{
    // --- ЛОГ 6: Проверка вызова ShowStartScreen ---
    UE_LOG(LogTemp, Warning, TEXT("===== ShowStartScreen() CALLED ====="));
    // Очищаем таймер, если вдруг перешли сюда с экрана загрузки
    if (GetWorld()) // Добавим проверку на валидность GetWorld()
    {
        GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    }
    SwitchScreen(StartScreenClass);
    // Сбрасываем статусы при возврате на стартовый экран
    SetLoginStatus(false, -1, TEXT(""));
    SetOfflineMode(false);
}

// ... добавьте аналогичные UE_LOG(LogTemp, Warning, ...) в начало
// других ваших функций Show...Screen(), чтобы отслеживать навигацию ...

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
    SwitchScreen(LoadingScreenClass); // Сначала покажем виджет

    // Таймер запускаем только если есть мир
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

// --- Методы Управления Состоянием (логи можно оставить как есть) ---
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

// --- Заглушки HTTP Методов ---
void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password) { UE_LOG(LogTemp, Warning, TEXT("RequestLogin function called but not implemented yet.")); }
void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email) { UE_LOG(LogTemp, Warning, TEXT("RequestRegister function called but not implemented yet.")); }