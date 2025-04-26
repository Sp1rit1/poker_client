#pragma once // ������������ ��������� ��������� ����� ��� ����������, ������������ ������ ���������� �����������

#include "CoreMinimal.h" // �������� ������������ ���� UE, ���������� ������� ���� � �������
#include "Engine/GameInstance.h" // ����������� �������� ������ GameInstance, �� �������� �� �����������
#include "Blueprint/UserWidget.h" // ��� ������ � ��������� UMG
#include "GameFramework/GameUserSettings.h" // ��� ���������� ������ � �����������
#include "Engine/Engine.h"  //  ����������� ����������� ������� GameEngine               
#include "Interfaces/IHttpRequest.h" // ��������� ��� �������� � ���������� HTTP-��������        
#include "Interfaces/IHttpResponse.h" // ��������� ��� ��������� HTTP-�������      
#include "TimerManager.h" // ���������� ���������
#include "OfflineGameManager.h"  // ����������� ����� � ������� ������
#include "MyGameInstance.generated.h" // // ��������������� ������������ ����, ���������� ���, ��������������� Unreal Header Tool ��� ��������� ������� ���������. ������ ���� ��������� ����������


class UUserWidget; 

UCLASS() // ������, ���������� ���� ����� ��� ������� ��������� Unreal Engine.

// POKER_CLIENT_API - ������ ��� �������� ������, ����� �� ���� �������� ��� ������ �������. ��� ����� �������� �������� � ������ ��� ���������� ���������� ���������� � �������
class POKER_CLIENT_API UMyGameInstance : public UGameInstance 
{
	GENERATED_BODY() // ������, ���������� ��� �������, ���������� UCLASS(), ��������� ���, ��������������� UHT. ������ ���� ������ ������� � ���� ������.

public: 

	// --- ���������� ��������� ���� ---


// UPROPERTY: ������, �������� ���������� ������� ��� ������� ��������� UE.
// VisibleAnywhere: ���������� ����� ������ � ��������� �� ������ Details ��� ������ ���������� ����� ������� (�� ������ �������������).
// BlueprintReadOnly: ���������� ����� ������ �� Blueprint, �� ������ ��������.
// Category = "name": ���������� ��� ���������� � ��������� "name" � ���������.

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsLoggedIn = false; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int64 LoggedInUserId = -1; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FString LoggedInUsername;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsInOfflineMode = false; 

	// EditDefaultsOnly: ���������� ����� ������������� ������ � ��������� Class Defaults (��� Blueprint-����������), �� �� �� ��������� ����������� � ������
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	// TSubclassOf<UUserWidget>: ��� ������, �������� ������ �� ����� ������� (Blueprint), � �� �� ��� ���������. WindowContainerClass - �����-��������� ��� ������ �������� 
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Managers")
	UOfflineGameManager* OfflineGameManager; 


	// --- ������� ��������� (���������� �� Blueprint ��� C++) ---
	
// UFUNCTION: ������, �������� ������� ������� ��� ������� ��������� UE.

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

	FString ApiBaseUrl = TEXT("http://localhost:8080/api/auth");  // ������� URL ��� API ��������������

	virtual void Init() override; // ����������� (���������������� � �������� �������), ��������������� (override) �� UGameInstance ������� ������������� GameInstance
	virtual void Shutdown() override; // ������� ���������� ������ GameInstance

protected: 

	// --- ������� ���������� �������� ---
	// Transient: ���������, ��� �������� ���� ���������� �� ������ ����������� ��� ������������ (�������������� ��������� ������� � ������ ��� ����������). ��� ����� ���������������� ��� �������.
	// TObjectPtr<UUserWidget>: ����������� ��� ������ ��������� UE ��� �������� UObject (������� �������). ������������� ����������, ���� ������ ������.

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentTopLevelWidget = nullptr;  // ��������� �� ������� ������ �������� ������ (��������� ��� �������������)

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentContainerInstance = nullptr; // ��������� �� ������� ��������� �������-����������

	FTimerHandle LoadingScreenTimerHandle; // ��������� ��� �������� �������������� ������� � ���������� ��
	void OnLoadingScreenTimerComplete();  // ������� ��������� ������ (callback), ������� ����� ������� �� ���������� ������� LoadingScreenTimerHandle.


	// --- ����������� ������� HTTP ---
	// FHttpRequestPtr, FHttpResponsePtr: ���� ����� ���������� ��� �������� HTTP ������� � ������
	// ���������� ������� ��������� ������ ��� ��������� ������ �� ������ ������ � �����������.
	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);


	// --- ������� ��� ������ Blueprint �� C++ (����������� ���������) ---
	// ��������������� ������� ��� ������ ����������� ������� ������ ���������� �� ������. const ����� ����������� ������ ��������, ��� ������ ����� �� �������� ���������(����) �������
	UUserWidget* FindWidgetInContainer(TSubclassOf<UUserWidget> WidgetClassToFind) const; 
	void DisplayLoginError(const FString& Message); 
	void DisplayRegisterError(const FString& Message);
	void DisplayLoginSuccessMessage(const FString& Message); 

	// --- ��������������� ������� ��� ���������� ����� � ������ ---

	//������������� ����� ����� � ��������� ������� ����.
	// bIsUIOnly True ��� ������ UI Only (������� �������), False ��� Game And UI (�������������).
	// bShowMouse True ����� �������� ������, False ����� ������.

	void SetupInputMode(bool bIsUIOnly, bool bShowMouse);

	void ApplyWindowMode(bool bWantFullscreen);

	// ��������� ��������������� ������� ��� ��������, ������ ������� � ���������� ����������.
	template <typename T = UUserWidget> 
	T* ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget);

};