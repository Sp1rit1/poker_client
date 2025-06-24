#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/IHttpRequest.h" // Для FHttpRequestPtr и FHttpResponsePtr
#include "NetworkDataTypes.h"
#include "NetworkAuthManager.generated.h"

// Прямое объявление для UMyGameInstance, чтобы избежать циклической зависимости в заголовках
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAddFriendAttemptCompleted, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFriendListReceivedSignature, bool, bSuccess, const TArray<FPlayerFriendInfo>&, Friends, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerStatsReceivedSignature, bool, bSuccess, const FPlayerStatsInfo&, Stats, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRemoveFriendAttemptCompletedSignature, bool, bSuccess, const FString&, Message);


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

    UPROPERTY(BlueprintAssignable, Category = "Network|Friends")
    FOnFriendListReceivedSignature OnFriendListReceivedDelegate; 

    UPROPERTY(BlueprintAssignable, Category = "Network|Stats")
    FOnPlayerStatsReceivedSignature OnPlayerStatsReceivedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Network|Friends")
    FOnRemoveFriendAttemptCompletedSignature OnRemoveFriendAttemptCompletedDelegate; 

    UFUNCTION(BlueprintCallable, Category = "Network|Friends")
    void RequestRemoveFriendByCode(const FString& FriendCodeToRemove);

    UFUNCTION(BlueprintCallable, Category = "Network|Friends")
    void RequestFriendList();

    UFUNCTION(BlueprintCallable, Category = "Network|Stats")
    void RequestPlayerStats();

protected:

    void OnFriendListResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful); 

    void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    void OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    void OnAddFriendResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    void OnPlayerStatsResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    void OnRemoveFriendByCodeResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:

    UPROPERTY() 
    TObjectPtr<UMyGameInstance> OwningGameInstance; 
    FString ApiBaseUrl;
};