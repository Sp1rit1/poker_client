#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/IHttpRequest.h" // Для FHttpRequestPtr и FHttpResponsePtr
#include "NetworkAuthManager.generated.h"

// Прямое объявление для UMyGameInstance, чтобы избежать циклической зависимости в заголовках
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAddFriendAttemptCompleted, bool, bSuccess, const FString&, Message);

UCLASS()
class POKER_CLIENT_API UNetworkAuthManager : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UNetworkAuthManager();

    void Initialize(UMyGameInstance* InGameInstance, const FString& InApiBaseUrl);

    UFUNCTION(BlueprintCallable, Category = "Network|Authentication") // Можно вызывать из BP, если понадобится
    void RequestLogin(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Network|Authentication")
    void RequestRegister(const FString& Username, const FString& Password, const FString& Email);

    UPROPERTY(BlueprintAssignable, Category = "Network|Friends")
    FOnAddFriendAttemptCompleted OnAddFriendAttemptCompleted;

    UFUNCTION(BlueprintCallable, Category = "Network|Friends")
    void RequestAddFriend(const FString& FriendCode);

protected:

    void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    void OnAddFriendResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:

    UPROPERTY() 
    TObjectPtr<UMyGameInstance> OwningGameInstance; 
    FString ApiBaseUrl;
};