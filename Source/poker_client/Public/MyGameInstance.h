// MyGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h" // <--- ���������: ���������� ��� FTimerHandle � GetTimerManager()
#include "Blueprint/UserWidget.h" // ��� UUserWidget � TSubclassOf
// #include "Interfaces/IHttpRequest.h" // ������� ����� ��� ���� 2.7
#include "MyGameInstance.generated.h" // ������ ���� ���������

UCLASS()
class POKER_CLIENT_API UMyGameInstance : public UGameInstance // �������� POKERCLIENT_API!
{
    GENERATED_BODY()

public:
    // --- ��������� �������������� � ������ ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsLoggedIn = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    int64 LoggedInUserId = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    FString LoggedInUsername = TEXT("");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsInOfflineMode = false;

    // --- UI ��������� ---

    // ����� ������� ��� ���������� ������ (����������� � Blueprint)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> StartScreenClass;

    // ����� ������� ��� ������ ������ (����������� � Blueprint)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> LoginScreenClass;

    // ����� ������� ��� ������ �������� (����������� � Blueprint) - �����
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> LoadingScreenClass;

    // ����� ������� ��� �������� ���� (����������� � Blueprint)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Screen Classes")
    TSubclassOf<UUserWidget> MainMenuClass;

    // ��������� �� ������� ������������ ������ (����� ��� �������)
    UPROPERTY() // �� ����� ������ �����, ������ ������ ������
        TObjectPtr<UUserWidget> CurrentScreenWidget = nullptr;

    // ������� ������ ���������, �.�. �� ��������� ��� GameInstance
private:
    // --- ������ ��� ������ �������� ---
    FTimerHandle LoadingScreenTimerHandle; // ������ ������������� �������

    /** �������, ������� ����� ������� �� ���������� ������� �������� */
    void OnLoadingScreenTimerComplete();

public:
    // --- ������ ���������� UI ---

    /** ������������� GameInstance (���������� ��� ������ ����) */
    virtual void Init() override;

    /** �������� ��������� ����� */
    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowStartScreen();

    /** �������� ����� ������ */
    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowLoginScreen();

    /** �������� ����� �������� � ��������� ������ */
    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowLoadingScreen(float Duration = 5.0f); // <-- ���������� ����� �������� ������������ �����/��������

    /** �������� ������� ���� */
    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    void ShowMainMenu();

    // --- ������ ���������� ���������� ---

    /** ���������� ������ ������ (���������� ����� ��������� ������ �� �������) */
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetLoginStatus(bool bNewIsLoggedIn, int64 NewUserId, const FString& NewUsername);

    /** ���������� ������� ����� (���������� �� ������ "�������") */
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetOfflineMode(bool bNewIsOffline);

    // --- ������ ��� HTTP �������� (���������� ��� ���� 2.7) ---
    FString ApiBaseUrl = TEXT("http://localhost:8080/api/auth");

    UFUNCTION(BlueprintCallable, Category = "Network")
    void RequestLogin(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Network")
    void RequestRegister(const FString& Username, const FString& Password, const FString& Email);

private:
    // --- ��������� ������ ---

    /** ��������������� ������� ��� ����� ������ */
    void SwitchScreen(TSubclassOf<UUserWidget> NewScreenClass);

    // ���������� ������������ HTTP (���������� ����� � ���� 2.7)
    // void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    // void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

};