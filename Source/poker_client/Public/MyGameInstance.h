// MyGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
// Позже понадобятся для UI и HTTP:
// #include "Blueprint/UserWidget.h"
// #include "Interfaces/IHttpRequest.h"
#include "MyGameInstance.generated.h" // Должен быть последним

UCLASS() // Макрос для системы отражений UE
class POKERCLIENT_API UMyGameInstance : public UGameInstance // Замените POKERCLIENT_API!
{
    GENERATED_BODY() // Обязательный макрос

public:
    /** Флаг, показывающий, залогинен ли пользователь в данный момент.
     *  Виден в редакторе и читаем из Blueprints. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Authentication State")
    bool bIsLoggedIn = false;

    /** ID пользователя, полученный от сервера после успешного входа.
     *  -1 означает, что пользователь не залогинен. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Authentication State")
    int64 LoggedInUserId = -1; // Используем 64 бита, как Long в Java

    /** Имя пользователя, полученное от сервера после успешного входа. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Authentication State")
    FString LoggedInUsername = TEXT(""); // Инициализация пустой строкой

    /** Флаг, показывающий, выбрал ли пользователь оффлайн режим. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Mode")
    bool bIsInOfflineMode = false;

    // --- Место для будущих полей (ссылки на виджеты, HTTP обработчики) ---

    // --- Место для будущих объявлений методов (навигация, запросы) ---

};