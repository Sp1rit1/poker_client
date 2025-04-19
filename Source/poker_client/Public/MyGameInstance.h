// MyGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
// ����� ����������� ��� UI � HTTP:
// #include "Blueprint/UserWidget.h"
// #include "Interfaces/IHttpRequest.h"
#include "MyGameInstance.generated.h" // ������ ���� ���������

UCLASS() // ������ ��� ������� ��������� UE
class POKERCLIENT_API UMyGameInstance : public UGameInstance // �������� POKERCLIENT_API!
{
    GENERATED_BODY() // ������������ ������

public:
    /** ����, ������������, ��������� �� ������������ � ������ ������.
     *  ����� � ��������� � ������ �� Blueprints. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Authentication State")
    bool bIsLoggedIn = false;

    /** ID ������������, ���������� �� ������� ����� ��������� �����.
     *  -1 ��������, ��� ������������ �� ���������. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Authentication State")
    int64 LoggedInUserId = -1; // ���������� 64 ����, ��� Long � Java

    /** ��� ������������, ���������� �� ������� ����� ��������� �����. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Authentication State")
    FString LoggedInUsername = TEXT(""); // ������������� ������ �������

    /** ����, ������������, ������ �� ������������ ������� �����. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Mode")
    bool bIsInOfflineMode = false;

    // --- ����� ��� ������� ����� (������ �� �������, HTTP �����������) ---

    // --- ����� ��� ������� ���������� ������� (���������, �������) ---

};