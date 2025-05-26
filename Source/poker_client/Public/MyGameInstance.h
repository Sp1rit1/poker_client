#pragma once

#include "CoreMinimal.h"
#include "PokerDataTypes.h"
#include "Engine/GameInstance.h"

class UStartScreenUIManager;
class UNetworkAuthManager;
class UOfflineGameManager;
class UMenuScreenUIManager;
class ULevelTransitionManager;
class UUserWidget; // Базовый класс виджетов
class UMediaPlayer;
class UMediaSource;
#include "MyGameInstance.generated.h"

// Делегаты для событий аутентификации (остаются здесь, т.к. это глобальные события приложения)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginAttemptCompleted, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRegisterAttemptCompleted, bool, bSuccess, const FString&, Message);


UCLASS()
class POKER_CLIENT_API UMyGameInstance : public UGameInstance 
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

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Audio Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float MenuLevelMusicVolume = 0.5f; 

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Audio Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float GameLevelMusicVolume = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Start Screens")
    TSubclassOf<UUserWidget> StartScreenClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Start Screens")
    TSubclassOf<UUserWidget> LoginScreenClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Start Screens")
    TSubclassOf<UUserWidget> RegisterScreenClass;

    // --- Классы виджетов для основных экранов

    UPROPERTY(EditDefaultsOnly, Category = "UI|Menu Screens")
    TSubclassOf<UUserWidget> MainMenuClass; 

    UPROPERTY(EditDefaultsOnly, Category = "UI|Menu Screens")
    TSubclassOf<UUserWidget> OfflineLobbyClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Menu Screens")
    TSubclassOf<UUserWidget> OnlineLobbyClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Menu Screens")
    TSubclassOf<UUserWidget> ProfileScreenClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Menu Screens")
    TSubclassOf<UUserWidget> SettingsClass;

    // --- Ассеты для Заставки "Screensaver" (например, Старт -> Меню) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Transitions|Screensaver")
    TSubclassOf<UUserWidget> Screensaver_WidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Transitions|Screensaver")
    TObjectPtr<UMediaPlayer> Screensaver_MediaPlayer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Transitions|Screensaver")
    TObjectPtr<UMediaSource> Screensaver_MediaSource;

    // --- Ассеты для Заставки "LoadingVideo" (например, Меню -> Игра) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Transitions|LoadingVideo")
    TSubclassOf<UUserWidget> LoadingVideo_WidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Transitions|LoadingVideo")
    TObjectPtr<UMediaPlayer> LoadingVideo_MediaPlayer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Transitions|LoadingVideo")
    TObjectPtr<UMediaSource> LoadingVideo_MediaSource;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Offline Game Settings") 
    int32 PendingOfflineNumBots = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Offline Game Settings")
    int64 PendingOfflineInitialStack = 1000;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Offline Game Settings")
    int64 PendingSmallBlind = 50;

    UPROPERTY(BlueprintReadWrite, Category = "Offline Game Settings")
    TArray<FBotPersonalitySettings> PendingBotPersonalities;

    // --- Делегаты для событий аутентификации ---

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
    UMenuScreenUIManager* GetMenuScreenUIManager() const { return MenuScreenUIManagerInstance; }

    UFUNCTION(BlueprintPure, Category = "Managers")
    ULevelTransitionManager* GetLevelTransitionManager() const { return LevelTransitionManagerInstance; }

    // Обновленная функция для установки статуса логина
    UFUNCTION(BlueprintCallable, Category = "User Session")
    void SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername, const FString& NewFriendCode);

    UFUNCTION(BlueprintCallable, Category = "User Session")
    void SetOfflineMode(bool bNewIsOffline);

    UFUNCTION(BlueprintCallable, Category = "Window Settings")
    void ApplyWindowMode(bool bWantFullscreen);

    UPROPERTY(BlueprintReadOnly, Category = "Window Settings")
    bool bIsInitialWindowSetupComplete = false;

    void DelayedInitialResize(); // Вызывается таймером

protected:

    // --- Менеджеры ---
    UPROPERTY()
    TObjectPtr<UStartScreenUIManager> StartScreenUIManagerInstance; 

    UPROPERTY()
    TObjectPtr<UNetworkAuthManager> NetworkAuthManagerInstance; 

    UPROPERTY() 
        TObjectPtr<UOfflineGameManager> OfflineGameManager;

    UPROPERTY()
    TObjectPtr<UMenuScreenUIManager> MenuScreenUIManagerInstance;

    UPROPERTY()
    TObjectPtr<ULevelTransitionManager> LevelTransitionManagerInstance;

    FTimerHandle ResizeTimerHandle;
    FIntPoint DesiredWindowedResolution; 
    bool bDesiredResolutionCalculated = false;
};