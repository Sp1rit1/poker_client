#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"           // ��� TSubclassOf � TObjectPtr
#include "GameFramework/GameUserSettings.h" // ��� ���������� ����������� ����
#include "Engine/Engine.h"                  // ��� GEngine
#include "Interfaces/IHttpRequest.h"        // ��� HTTP ��������
#include "Interfaces/IHttpResponse.h"       // ��� HTTP �������
#include "TimerManager.h"                   // ��� FTimerHandle � ���������� ���������
// --- ������������ �������, ���� �� �� ������������ ����� ��� �������� JSON �������� ---
// #include "Json.h"
// #include "JsonUtilities.h"
// #include "Serialization/JsonSerializer.h"
// #include "JsonObjectConverter.h"
// --- ����� ������������ �������� ---

#include "MyGameInstance.generated.h" // ������ ���� ��������� ��������

// Forward declaration, ����� �� �������� ���� .h ������� �����
class UUserWidget;

UCLASS()
class POKER_CLIENT_API UMyGameInstance : public UGameInstance // �������� YOURPROJECTNAME_API �� ���� (��������, POKER_CLIENT_API)
{
	GENERATED_BODY()

public: // �������� ����� � �� Blueprint (���� UFUNCTION/UPROPERTY)

	// --- ���������� ��������� ���� ---
	// �������� ��� ������ �� Blueprint
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsLoggedIn = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int64 LoggedInUserId = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FString LoggedInUsername;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsInOfflineMode = false;

	// --- ������ �������� (����������� � BP_MyGameInstance) ---
	// �������� ��� ������ �� Blueprint, ������������� ������ � defaults
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> WindowContainerClass; // �����: ����� �������-����������

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
	TSubclassOf<UUserWidget> SettingsScreenClass; // ���� ������������

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Classes")
	TSubclassOf<UUserWidget> ProfileScreenClass; // ���� ������������

	// --- ������� ��������� (���������� �� Blueprint ��� C++) ---
	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowStartScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowLoginScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowRegisterScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowLoadingScreen(float Duration = 2.0f); // ������������ �� ���������

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowSettingsScreen(); // ���� ������������

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void ShowProfileScreen(); // ���� ������������

	// --- ������� ���������� ���������� ---
	UFUNCTION(BlueprintCallable, Category = "State")
	void SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername);

	UFUNCTION(BlueprintCallable, Category = "State")
	void SetOfflineMode(bool bNewIsOffline);

	// --- ������� ��� ������� �������� (���������� �� Blueprint) ---
	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestLogin(const FString& Username, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestRegister(const FString& Username, const FString& Password, const FString& Email);

	// ������� URL ��� API �������������� (����� ������� UPROPERTY ��� ��������� � Editor)
	// UPROPERTY(EditDefaultsOnly, Category="Network")
	FString ApiBaseUrl = TEXT("http://localhost:8080/api/auth");

	// --- ���������������� ������� GameInstance ---
	virtual void Init() override;
	virtual void Shutdown() override; // ��������� ��� ���������� ������ �������� ����

protected: // �������� ������ �� ����� ������ � ��� �����������

	// --- ������� ���������� �������� ---
	// ���������� Transient, ����� ��� �� ����������� � ����������� ������ ��� �������
	// ���������� TObjectPtr ��� ������������ ���������� ����������� UE
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentTopLevelWidget = nullptr; // �������������: ��� CurrentScreenWidget

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentContainerInstance = nullptr; // �����: ��������� �� ��������� ����������

	// --- ������ ��� ������ �������� ---
	FTimerHandle LoadingScreenTimerHandle;
	void OnLoadingScreenTimerComplete(); // Callback ��� �������

	// --- ����������� ������� HTTP ---
	// ��� ������� ���������� �� ���������� HTTP ��������
	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// --- ������� ��� ������ Blueprint �� C++ (����������� ���������) ---
	// �������� ������� � ��������� ������� � CurrentTopLevelWidget ��� CurrentContainerInstance
	void DisplayLoginError(const FString& Message);
	void DisplayRegisterError(const FString& Message);
	void DisplayLoginSuccessMessage(const FString& Message); // ��� ��������� ����� �������� �����������

	// --- ��������������� ������� ��� ���������� ����� � ������ ---
	/**
	 * ������������� ����� ����� � ��������� ������� ����.
	 * @param bIsUIOnly True ��� ������ UI Only (������� �������), False ��� Game And UI (�������������).
	 * @param bShowMouse True ����� �������� ������, False ����� ������.
	 */
	void SetupInputMode(bool bIsUIOnly, bool bShowMouse);

	/**
	 * ��������� ������� ��� ������������� ����� � ��������� ���� ����.
	 * @param bWantFullscreen True ��� ��������� WindowedFullscreen, False ��� Windowed.
	 */
	void ApplyWindowMode(bool bWantFullscreen);

	/**
	 * ��������� ��������������� ������� ��� ��������, ������ ������� � ���������� ����������.
	 * ������������� ����� ����, ����/����, ������� ������ ������, ������� � ��������� �����.
	 * @param WidgetClassToShow ����� ������� ��� �����������.
	 * @param bIsFullscreenWidget True, ���� ������ ������ ���� � ������������� ������, False ��� ��������.
	 * @return ��������� �� ��������� ������ ��� nullptr � ������ ������.
	 */
	template <typename T = UUserWidget> // �� ��������� UUserWidget, �� ����� ������� ���������� ���
	T* ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget);

	// ��������� ������ ������ �� �����, ��� ���������� ������� � protected
	// private:
	// void SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass); // ������ ����� - ������
};