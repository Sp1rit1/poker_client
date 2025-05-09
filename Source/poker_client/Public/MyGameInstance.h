#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
// Прямые объявления для менеджеров и виджетов, чтобы не включать их .h здесь
class UStartScreenUIManager;
class UNetworkAuthManager;
class UOfflineGameManager;
class UGameScreenUIManager;
class UUserWidget; // Базовый класс виджетов
class UMediaPlayer;
class UMediaSource;
#include "MyGameInstance.generated.h"

// Делегаты для событий аутентификации (остаются здесь, т.к. это глобальные события приложения)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginAttemptCompleted, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRegisterAttemptCompleted, bool, bSuccess, const FString&, Message);


UCLASS()
class POKER_CLIENT_API UMyGameInstance : public UGameInstance // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UMyGameInstance();
    virtual void Init() override;
    virtual void Shutdown() override;

    // --- Глобальное Состояние Игры (Логин, Оффлайн Режим) ---
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "User Session")
    bool bIsLoggedIn = false;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "User Session")
    FString LoggedInUsername = TEXT("");

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "User Session")
    int64 LoggedInUserId = -1;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "User Session")
    FString LoggedInFriendCode = TEXT(""); // Добавлено

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "User Session")
    bool bIsInOfflineMode = false;

    // --- Классы виджетов для стартовых экранов (настраиваются в Blueprint GameInstance) ---
// Они нужны здесь, чтобы их можно было назначить в редакторе и передать в StartScreenUIManager
    UPROPERTY(EditDefaultsOnly, Category = "UI|Start Screens")
    TSubclassOf<UUserWidget> StartScreenClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Start Screens")
    TSubclassOf<UUserWidget> LoginScreenClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Start Screens")
    TSubclassOf<UUserWidget> RegisterScreenClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Loading Screen")
    TSubclassOf<UUserWidget> LoadingScreenClass;

    // --- Ассеты для экрана загрузки (настраиваются в Blueprint GameInstance) ---
    UPROPERTY(EditDefaultsOnly, Category = "UI|Loading Screen")
    TObjectPtr<UMediaPlayer> LoadingMediaPlayerAsset;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Loading Screen")
    TObjectPtr<UMediaSource> LoadingMediaSourceAsset;

    // --- Классы виджетов для основных экранов

    UPROPERTY(EditDefaultsOnly, Category = "UI|Game Screens")
    TSubclassOf<UUserWidget> GameMainMenuClass; 

    UPROPERTY(EditDefaultsOnly, Category = "UI|Game Screens")
    TSubclassOf<UUserWidget> GameOfflineLobbyClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Game Screens")
    TSubclassOf<UUserWidget> GameOnlineLobbyClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Game Screens")
    TSubclassOf<UUserWidget> GameProfileScreenClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Game Screens")
    TSubclassOf<UUserWidget> GameSettingsClass;


    // --- Делегаты для событий аутентификации ---
// Виджеты будут подписываться на них
    UPROPERTY(BlueprintAssignable, Category = "Network|Authentication")
    FOnLoginAttemptCompleted OnLoginAttemptCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Network|Authentication")
    FOnRegisterAttemptCompleted OnRegisterAttemptCompleted;

    // --- Базовый URL API (остается здесь, т.к. может использоваться и другими сетевыми менеджерами) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Network")
    FString ApiBaseUrl = TEXT("http://localhost:8080/api"); 


    // --- Функции менеджеров ---
    UFUNCTION(BlueprintPure, Category = "Managers")
    UStartScreenUIManager* GetStartScreenUIManager() const { return StartScreenUIManagerInstance; }

    UFUNCTION(BlueprintPure, Category = "Managers")
    UNetworkAuthManager* GetNetworkAuthManager() const { return NetworkAuthManagerInstance; }

    UFUNCTION(BlueprintPure, Category = "Managers")
    UOfflineGameManager* GetOfflineGameManager() const { return OfflineGameManager; }

    UFUNCTION(BlueprintPure, Category = "Managers")
    UGameScreenUIManager* GetGameScreenUIManager() const { return GameScreenUIManagerInstance; }

    // Обновленная функция для установки статуса логина
    UFUNCTION(BlueprintCallable, Category = "User Session")
    void SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername, const FString& NewFriendCode);

    UFUNCTION(BlueprintCallable, Category = "User Session")
    void SetOfflineMode(bool bNewIsOffline);


    // --- Настройки Окна (остаются здесь, т.к. это глобальные настройки приложения) ---
    void DelayedInitialResize(); // Вызывается таймером
    void ApplyWindowMode(bool bWantFullscreen);
    void SetupInputMode(bool bIsUIOnly, bool bShowMouse);

    UPROPERTY(BlueprintReadOnly, Category = "Window Settings")
    bool bIsInitialWindowSetupComplete = false;

protected:

    // --- Менеджеры ---
    UPROPERTY()
    TObjectPtr<UStartScreenUIManager> StartScreenUIManagerInstance; 

    UPROPERTY()
    TObjectPtr<UNetworkAuthManager> NetworkAuthManagerInstance; 

    UPROPERTY() // OfflineGameManager остался здесь
        TObjectPtr<UOfflineGameManager> OfflineGameManager;

    UPROPERTY()
    TObjectPtr<UGameScreenUIManager> GameScreenUIManagerInstance;

    FTimerHandle ResizeTimerHandle;
    FIntPoint DesiredWindowedResolution; // Для сохранения при выходе
    bool bDesiredResolutionCalculated = false;
};