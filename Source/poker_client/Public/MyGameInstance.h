
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h" 
#include "Blueprint/UserWidget.h" 
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h" 
#include "Components/TextBlock.h" 
#include "Components/EditableTextBox.h"
#include "Kismet/GameplayStatics.h"
#include "MyGameInstance.generated.h" 

UCLASS()
class POKER_CLIENT_API UMyGameInstance : public UGameInstance 
{
    GENERATED_BODY()

public:

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsLoggedIn = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    int64 LoggedInUserId = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    FString LoggedInUsername = TEXT("");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsInOfflineMode = false;






































    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> StartScreenClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> LoginScreenClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> RegisterScreenClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> LoadingScreenClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> MainMenuClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> SettingsScreenClass; 


    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> ProfileScreenClass; 

    UPROPERTY() 
        TObjectPtr<UUserWidget> CurrentScreenWidget = nullptr;

private:

    FTimerHandle LoadingScreenTimerHandle; 

    void OnLoadingScreenTimerComplete();

public:


    virtual void Init() override;

    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowStartScreen();

    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowLoginScreen();

    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowRegisterScreen();

    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowLoadingScreen(float Duration = 5.0f);


    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowMainMenu();

    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowSettingsScreen(); 


    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowProfileScreen(); 


    UFUNCTION(BlueprintCallable, Category = "State")
    void SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername);


    UFUNCTION(BlueprintCallable, Category = "State")
    void SetOfflineMode(bool bNewIsOffline);


    FString ApiBaseUrl = TEXT("http://localhost:8080/api/auth");

    UFUNCTION(BlueprintCallable, Category = "Network")
    void RequestLogin(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Network")
    void RequestRegister(const FString& Username, const FString& Password, const FString& Email);



    void DisplayLoginError(const FString& Message);

    void DisplayRegisterError(const FString& Message);

    void DisplayLoginSuccessMessage(const FString& Message);

private:

    void SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass);

    void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful); 
    void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

};