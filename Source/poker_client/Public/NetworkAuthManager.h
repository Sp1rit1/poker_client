#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/IHttpRequest.h" // Для FHttpRequestPtr и FHttpResponsePtr
#include "NetworkAuthManager.generated.h"

// Прямое объявление для UMyGameInstance, чтобы избежать циклической зависимости в заголовках
class UMyGameInstance;

UCLASS()
class POKER_CLIENT_API UNetworkAuthManager : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UNetworkAuthManager();

    /**
     * Инициализирует менеджер, устанавливая ссылку на GameInstance и базовый URL API.
     * @param InGameInstance Указатель на владеющий GameInstance.
     * @param InApiBaseUrl Базовый URL для API запросов.
     */
    void Initialize(UMyGameInstance* InGameInstance, const FString& InApiBaseUrl);

    /**
     * Отправляет запрос на вход пользователя.
     * @param Username Имя пользователя.
     * @param Password Пароль пользователя.
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Authentication") // Можно вызывать из BP, если понадобится
        void RequestLogin(const FString& Username, const FString& Password);

    /**
     * Отправляет запрос на регистрацию нового пользователя.
     * @param Username Имя пользователя.
     * @param Password Пароль пользователя.
     * @param Email Email пользователя.
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Authentication")
    void RequestRegister(const FString& Username, const FString& Password, const FString& Email);

protected:
    /**
     * Обработчик ответа на запрос входа.
     * Вызывается по завершении HTTP запроса.
     */
    void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    /**
     * Обработчик ответа на запрос регистрации.
     * Вызывается по завершении HTTP запроса.
     */
    void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:
    // Указатель на владеющий GameInstance для доступа к его состоянию и делегатам
    UPROPERTY() // UPROPERTY здесь для защиты от сборки мусора, если менеджер живет долго
    TObjectPtr<UMyGameInstance> OwningGameInstance; // Используем TObjectPtr для современных версий UE

    // Базовый URL для API запросов
    FString ApiBaseUrl;
};