// MyGameInstance.cpp
#include "MyGameInstance.h" // Ваш заголовочный файл
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h" // Убедитесь, что инклюд есть

// --- Вспомогательная функция ---

void UMyGameInstance::SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass)
{
    // Удаляем предыдущий экран
    if (CurrentScreenWidget && CurrentScreenWidget->IsValidLowLevel())
    {
        CurrentScreenWidget->RemoveFromParent();
        CurrentScreenWidget = nullptr;
    }

    // Создаем и добавляем новый экран
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

// --- Основные Методы ---

void UMyGameInstance::Init()
{
    Super::Init(); // Важно вызвать родительский Init
    ShowStartScreen(); // Показываем стартовый экран при запуске
}

void UMyGameInstance::ShowStartScreen()
{
    // Очищаем таймер, если вдруг перешли сюда с экрана загрузки
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(StartScreenClass);
    // Сбрасываем статусы при возврате на стартовый экран
    SetLoginStatus(false, -1, TEXT(""));
    SetOfflineMode(false);
}

void UMyGameInstance::ShowLoginScreen()
{
    // Очищаем таймер на всякий случай
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(LoginScreenClass);
}

void UMyGameInstance::ShowRegisterScreen()
{
    // Очищаем таймер, если вдруг перешли сюда с экрана загрузки
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(RegisterScreenClass); // Используем новую переменную класса
}

void UMyGameInstance::ShowLoadingScreen(float Duration)
{
    // Показываем виджет загрузки
    SwitchScreen(LoadingScreenClass);

    // Очищаем старый таймер, если он был
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);

    // Запускаем новый таймер
    GetWorld()->GetTimerManager().SetTimer(
        LoadingScreenTimerHandle,          // Хендл для управления таймером
        this,                              // Объект, чей метод будет вызван
        &UMyGameInstance::OnLoadingScreenTimerComplete, // Указатель на метод для вызова
        Duration,                          // Задержка в секундах
        false                              // false = таймер сработает один раз
    );
    UE_LOG(LogTemp, Log, TEXT("Showing Loading Screen. Timer set for %.2f seconds."), Duration);
}

void UMyGameInstance::OnLoadingScreenTimerComplete()
{
    // Этот метод будет вызван автоматически по истечению времени таймера
    UE_LOG(LogTemp, Log, TEXT("Loading screen timer finished."));
    // Показываем главное меню (SwitchScreen сам удалит экран загрузки)
    ShowMainMenu();
}

void UMyGameInstance::ShowMainMenu()
{
    // Очищаем таймер на всякий случай (если сюда попали не по таймеру)
    GetWorld()->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
    SwitchScreen(MainMenuClass);
    // Возможно, здесь понадобится обновить UI главного меню
    // в зависимости от bIsLoggedIn или bIsInOfflineMode,
    // но это можно сделать и в самом виджете WBP_MainMenu
}

// --- Методы Управления Состоянием ---

void UMyGameInstance::SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername)
{
    // (Реализация остается той же, что и раньше)
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
    // (Реализация остается той же, что и раньше)
    bIsInOfflineMode = bNewIsOffline;
    if (bIsInOfflineMode)
    {
        SetLoginStatus(false, -1, TEXT(""));
    }
    UE_LOG(LogTemp, Log, TEXT("Offline Mode Status Updated: IsOffline=%s"),
        bIsInOfflineMode ? TEXT("true") : TEXT("false"));
}

// --- Реализации HTTP методов (будут в Шаге 2.7) ---

void UMyGameInstance::RequestLogin(const FString& Username, const FString& Password)
{
    UE_LOG(LogTemp, Warning, TEXT("RequestLogin function called but not implemented yet."));
    // Здесь будет логика HTTP запроса...
    // При УСПЕШНОМ ответе сервера нужно будет вызвать НЕ ShowMainMenu(),
    // а ShowLoadingScreen()!
    // Примерно так:
    // if (Success) {
    //     SetLoginStatus(true, ReceivedUserId, ReceivedUsername);
    //     ShowLoadingScreen(5.0f); // Запускаем экран загрузки на 5 секунд
    // } else { ... обработка ошибки ... }
}

void UMyGameInstance::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
    UE_LOG(LogTemp, Warning, TEXT("RequestRegister function called but not implemented yet."));
    // Здесь будет логика HTTP запроса...
    // При УСПЕШНОМ ответе сервера (201 Created) обычно просто показываем сообщение
    // на экране логина или ничего не делаем, оставаясь на нем.
}

// ... реализации OnLoginResponseReceived и OnRegisterResponseReceived ...