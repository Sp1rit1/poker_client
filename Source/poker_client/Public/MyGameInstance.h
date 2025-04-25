#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"           
#include "GameFramework/GameUserSettings.h" 
#include "Engine/Engine.h"                 
#include "Interfaces/IHttpRequest.h"        
#include "Interfaces/IHttpResponse.h"       
#include "TimerManager.h"                 
#include "MyGameInstance.generated.h" 


class UUserWidget;

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
	FString LoggedInUsername;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsInOfflineMode = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> WindowContainerClass; 

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> StartScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> LoginScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> RegisterScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> LoadingScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> MainMenuClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> SettingsScreenClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> ProfileScreenClass;

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowStartScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowLoginScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowRegisterScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowLoadingScreen(float Duration = 7.0f); 

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

	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestLogin(const FString& Username, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestRegister(const FString& Username, const FString& Password, const FString& Email);

	FString ApiBaseUrl = TEXT("http://localhost:8080/api/auth");

	virtual void Init() override;
	virtual void Shutdown() override; 

protected: 

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentTopLevelWidget = nullptr; 

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentContainerInstance = nullptr; 

	FTimerHandle LoadingScreenTimerHandle;
	void OnLoadingScreenTimerComplete(); 

	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);


	UUserWidget* FindWidgetInContainer(TSubclassOf<UUserWidget> WidgetClassToFind) const;
	void DisplayLoginError(const FString& Message);
	void DisplayRegisterError(const FString& Message);
	void DisplayLoginSuccessMessage(const FString& Message); 

	void SetupInputMode(bool bIsUIOnly, bool bShowMouse);

	void ApplyWindowMode(bool bWantFullscreen);

	template <typename T = UUserWidget> 
	T* ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget);

};