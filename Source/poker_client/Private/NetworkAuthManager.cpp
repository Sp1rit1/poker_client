#include "NetworkAuthManager.h"
#include "MyGameInstance.h" 

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h" 
#include "Json.h"
#include "Serialization/JsonSerializer.h"

UNetworkAuthManager::UNetworkAuthManager()
{
    OwningGameInstance = nullptr;

}

void UNetworkAuthManager::Initialize(UMyGameInstance* InGameInstance, const FString& InApiBaseUrl)
{
    OwningGameInstance = InGameInstance;
    ApiBaseUrl = InApiBaseUrl;

    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::Initialize - OwningGameInstance is null!"));
    }
    if (ApiBaseUrl.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("UNetworkAuthManager::Initialize - ApiBaseUrl is empty!"));
    }
}

void UNetworkAuthManager::RequestLogin(const FString& Username, const FString& Password)
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("RequestLogin: OwningGameInstance is null. Cannot proceed."));
        return;
    }
    if (ApiBaseUrl.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("RequestLogin: ApiBaseUrl is not set. Cannot proceed."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestLogin: Attempting login for user: %s"), *Username);

    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("username"), Username);
    RequestJson->SetStringField(TEXT("password"), Password);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestLogin: Failed to serialize JSON body."));

        OwningGameInstance->OnLoginAttemptCompleted.Broadcast(false, TEXT("Внутренняя ошибка: не удалось создать запрос."));
        return;
    }

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/auth/login")); 
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);

    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UNetworkAuthManager::OnLoginResponseReceived);

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestLogin: Failed to start HTTP request (ProcessRequest failed)."));
        OwningGameInstance->OnLoginAttemptCompleted.Broadcast(false, TEXT("Ошибка сети: не удалось отправить запрос."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestLogin: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/auth/login")));
    }
}

void UNetworkAuthManager::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("RequestRegister: OwningGameInstance is null. Cannot proceed."));
        return;
    }
    if (ApiBaseUrl.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("RequestRegister: ApiBaseUrl is not set. Cannot proceed."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestRegister: Attempting registration for user: %s, email: %s"), *Username, *Email);

    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("username"), Username);
    RequestJson->SetStringField(TEXT("password"), Password);
    RequestJson->SetStringField(TEXT("email"), Email);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestRegister: Failed to serialize JSON body."));
        OwningGameInstance->OnRegisterAttemptCompleted.Broadcast(false, TEXT("Внутренняя ошибка: не удалось создать запрос."));
        return;
    }

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/auth/register")); 
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);

    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UNetworkAuthManager::OnRegisterResponseReceived);

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestRegister: Failed to start HTTP request (ProcessRequest failed)."));
        OwningGameInstance->OnRegisterAttemptCompleted.Broadcast(false, TEXT("Ошибка сети: не удалось отправить запрос."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestRegister: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/auth/register")));
    }
}



void UNetworkAuthManager::RequestAddFriend(const FString& FriendCode)
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("RequestAddFriend: OwningGameInstance is null. Cannot proceed."));
        if (OnAddFriendAttemptCompleted.IsBound())
        {
            OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Внутренняя ошибка: нет GameInstance."));
        }
        return;
    }
    if (ApiBaseUrl.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("RequestAddFriend: ApiBaseUrl is not set. Cannot proceed."));
        if (OnAddFriendAttemptCompleted.IsBound())
        {
            OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Внутренняя ошибка: не задан URL API."));
        }
        return;
    }
    if (FriendCode.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestAddFriend: FriendCode is empty."));
        if (OnAddFriendAttemptCompleted.IsBound())
        {
            OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Код друга не может быть пустым."));
        }
        return;
    }

    if (!OwningGameInstance->bIsLoggedIn)
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestAddFriend: User not logged in."));
        if (OnAddFriendAttemptCompleted.IsBound())
        {
            OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Для добавления друга необходимо войти в аккаунт."));
        }
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestAddFriend: Attempting to add friend with code: %s"), *FriendCode);

    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("friendCode"), FriendCode);

    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestAddFriend: Failed to serialize JSON body."));
        if (OnAddFriendAttemptCompleted.IsBound())
        {
            OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Внутренняя ошибка: не удалось создать запрос."));
        }
        return;
    }

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/friends/add"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->SetContentAsString(RequestBodyString);

    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UNetworkAuthManager::OnAddFriendResponseReceived);

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestAddFriend: Failed to start HTTP request."));
        if (OnAddFriendAttemptCompleted.IsBound())
        {
            OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Ошибка сети: не удалось отправить запрос."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestAddFriend: HTTP request sent to %s"), *(ApiBaseUrl + TEXT("/friends/add")));
    }
}




void UNetworkAuthManager::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: OwningGameInstance is null. Cannot process response."));
        return;
    }

    bool bLoginSuccess = false;
    FString ResponseMessage = TEXT("");

    if (!bWasSuccessful || !Response.IsValid())
    {
        ResponseMessage = TEXT("Сервер не доступен или проблемы с сетью");
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::OnLoginResponseReceived: Request failed. Message: %s"), *ResponseMessage);
        OwningGameInstance->OnLoginAttemptCompleted.Broadcast(false, ResponseMessage);
        return;
    }

    int32 ResponseCode = Response->GetResponseCode();
    FString ResponseBody = Response->GetContentAsString();
    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::OnLoginResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

    if (ResponseCode == 200)
    {
        TSharedPtr<FJsonObject> ResponseJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
        {
            int64 ReceivedUserId = -1;
            FString ReceivedUsername;
            FString ReceivedFriendCode; 

            // Извлекаем friendCode из ответа сервера (предполагается, что он там есть)
            if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
                ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername) &&
                ResponseJson->TryGetStringField(TEXT("friendCode"), ReceivedFriendCode)) 
            {
                UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::OnLoginResponseReceived: Login successful for user: %s (ID: %lld, FriendCode: %s)"), *ReceivedUsername, ReceivedUserId, *ReceivedFriendCode);
                // Обновляем глобальное состояние через GameInstance
                OwningGameInstance->SetLoginStatus(true, ReceivedUserId, ReceivedUsername, ReceivedFriendCode); // Передаем friendCode
                bLoginSuccess = true;
                ResponseMessage = TEXT("");
            }
            else
            {
                ResponseMessage = TEXT("Ошибка сервера: Неверный формат ответа (отсутствуют userId, username или friendCode).");
                UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::OnLoginResponseReceived: Failed parsing JSON fields. Message: %s"), *ResponseMessage);
            }
        }
        else
        {
            ResponseMessage = TEXT("Ошибка сервера: Не удалось обработать ответ JSON.");
            UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::OnLoginResponseReceived: Failed deserializing JSON. Message: %s"), *ResponseMessage);
        }
    }
    else if (ResponseCode == 401)
    {
        ResponseMessage = TEXT("Неверное имя пользователя или пароль.");
        UE_LOG(LogTemp, Warning, TEXT("UNetworkAuthManager::OnLoginResponseReceived: Login failed - Invalid credentials (401). Message: %s"), *ResponseMessage);
    }
    else
    {
        ResponseMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d)"), ResponseCode);
        TSharedPtr<FJsonObject> ErrorJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid())
        {
            FString ServerErrorMsg;
            // Пытаемся извлечь сообщение из стандартных полей Spring Boot ошибки
            if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg) || ErrorJson->TryGetStringField(TEXT("error"), ServerErrorMsg))
            {
                ResponseMessage = ServerErrorMsg; // Используем сообщение от сервера
            }
            // Если есть поле "errors" (для ошибок валидации)
            const TArray<TSharedPtr<FJsonValue>>* ErrorsArray;
            if (ErrorJson->TryGetArrayField(TEXT("errors"), ErrorsArray))
            {
                FString ValidationErrors;
                for (const TSharedPtr<FJsonValue>& ErrorValue : *ErrorsArray)
                {
                    const TSharedPtr<FJsonObject>* ErrorObject;
                    if (ErrorValue->TryGetObject(ErrorObject))
                    {
                        FString FieldError;
                        (*ErrorObject)->TryGetStringField(TEXT("defaultMessage"), FieldError); 
                        ValidationErrors += FieldError + TEXT("; ");
                    }
                    else if (ErrorValue->Type == EJson::String) { 
                        ValidationErrors += ErrorValue->AsString() + TEXT("; ");
                    }
                }
                if (!ValidationErrors.IsEmpty())
                {
                    ResponseMessage = ValidationErrors.LeftChop(2); 
                }
            }
        }
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::OnLoginResponseReceived: Server error. Final Message: %s"), *ResponseMessage);
    }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::OnLoginResponseReceived: Broadcasting OnLoginAttemptCompleted. Success: %s, Message: %s"), bLoginSuccess ? TEXT("True") : TEXT("False"), *ResponseMessage);
    OwningGameInstance->OnLoginAttemptCompleted.Broadcast(bLoginSuccess, ResponseMessage);
}

void UNetworkAuthManager::OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("OnRegisterResponseReceived: OwningGameInstance is null. Cannot process response."));
        return;
    }

    bool bRegisterSuccess = false;
    FString ResultMessage = TEXT("");

    if (!bWasSuccessful || !Response.IsValid())
    {
        ResultMessage = TEXT("Сервер не доступен или проблемы с сетью.");
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::OnRegisterResponseReceived: Request failed or response invalid. Message: %s"), *ResultMessage);
        OwningGameInstance->OnRegisterAttemptCompleted.Broadcast(false, ResultMessage);
        return;
    }

    int32 ResponseCode = Response->GetResponseCode();
    FString ResponseBody = Response->GetContentAsString();
    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::OnRegisterResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

    if (ResponseCode == 201 || ResponseCode == 200) // 201 Created или иногда 200 OK для регистрации
    {
        UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::OnRegisterResponseReceived: Registration successful!"));
        bRegisterSuccess = true;
        ResultMessage = TEXT("Регистрация прошла успешно! Теперь вы можете войти.");
    }
    else
    {
        // Формируем сообщение по умолчанию на основе кода
        if (ResponseCode == 409) // Conflict
        {
            ResultMessage = TEXT("Имя пользователя или Email уже существует.");
        }
        else if (ResponseCode == 400) // Bad Request (часто ошибки валидации)
        {
            ResultMessage = TEXT("Предоставлены неверные данные.");
        }
        else
        {
            ResultMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d)"), ResponseCode);
        }

        // Пытаемся извлечь более детальное сообщение из JSON ответа, как в OnLoginResponseReceived
        TSharedPtr<FJsonObject> ErrorJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid())
        {
            FString ServerErrorMsg;
            if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg) || ErrorJson->TryGetStringField(TEXT("error"), ServerErrorMsg))
            {
                if (!ServerErrorMsg.IsEmpty()) ResultMessage = ServerErrorMsg; // Используем сообщение от сервера, если оно есть
            }

            const TArray<TSharedPtr<FJsonValue>>* ErrorsArray;
            if (ErrorJson->TryGetArrayField(TEXT("errors"), ErrorsArray) && ErrorsArray->Num() > 0)
            {
                FString ValidationErrors;
                for (const TSharedPtr<FJsonValue>& ErrorValue : *ErrorsArray)
                {
                    const TSharedPtr<FJsonObject>* ErrorObject;
                    FString FieldError;
                    if (ErrorValue->TryGetObject(ErrorObject)) // Структура с defaultMessage
                    {
                        (*ErrorObject)->TryGetStringField(TEXT("defaultMessage"), FieldError);
                    }
                    else if (ErrorValue->Type == EJson::String) { 
                        FieldError = ErrorValue->AsString();
                    }

                    if (!FieldError.IsEmpty()) {
                        if (!ValidationErrors.IsEmpty()) ValidationErrors += TEXT("\n"); // Новая строка для каждого сообщения
                        ValidationErrors += FieldError;
                    }
                }
                if (!ValidationErrors.IsEmpty())
                {
                    ResultMessage = ValidationErrors;
                }
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("UNetworkAuthManager::OnRegisterResponseReceived: Registration failed. Final Message: %s"), *ResultMessage);
    }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::OnRegisterResponseReceived: Broadcasting OnRegisterAttemptCompleted. Success: %s, Message: %s"), bRegisterSuccess ? TEXT("True") : TEXT("False"), *ResultMessage);
    OwningGameInstance->OnRegisterAttemptCompleted.Broadcast(bRegisterSuccess, ResultMessage);
}


void UNetworkAuthManager::OnAddFriendResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!OwningGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("OnAddFriendResponseReceived: OwningGameInstance is null. Cannot process response or broadcast."));
        return;
    }

    bool bAddSuccess = false;
    FString ResultMessage = TEXT("Неизвестная ошибка при добавлении друга.");

    if (!bWasSuccessful || !Response.IsValid())
    {
        ResultMessage = TEXT("Сервер не доступен или проблемы с сетью.");
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::OnAddFriendResponseReceived: Request failed or response invalid. Message: %s"), *ResultMessage);
        if (OnAddFriendAttemptCompleted.IsBound()) { OnAddFriendAttemptCompleted.Broadcast(false, ResultMessage); }
        return;
    }

    int32 ResponseCode = Response->GetResponseCode();
    FString ResponseBody = Response->GetContentAsString();
    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::OnAddFriendResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

    if (ResponseCode == 200 || ResponseCode == 201)
    {
        bAddSuccess = true;
        TSharedPtr<FJsonObject> ResponseJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid() && ResponseJson->HasField(TEXT("message")))
        {
            ResultMessage = ResponseJson->GetStringField(TEXT("message"));
        }
        else {
            // Если сервер не вернул "message", но код 200/201, считаем успехом
            ResultMessage = TEXT("Друг успешно добавлен!");
        }
        UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::OnAddFriendResponseReceived: Friend added. Message: %s"), *ResultMessage);
    }
    else
    {
        // Логика парсинга JSON ошибки, как в OnLoginResponseReceived
        TSharedPtr<FJsonObject> ErrorJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid())
        {
            FString ServerErrorMsg;
            if (ErrorJson->TryGetStringField(TEXT("message"), ServerErrorMsg) || ErrorJson->TryGetStringField(TEXT("error"), ServerErrorMsg))
            {
                if (!ServerErrorMsg.IsEmpty()) ResultMessage = ServerErrorMsg;
            }
        }
        else if (!ResponseBody.IsEmpty())
        {
            ResultMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d): %s"), ResponseCode, *ResponseBody);
        }
        else
        {
            ResultMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d)"), ResponseCode);
        }
        UE_LOG(LogTemp, Warning, TEXT("UNetworkAuthManager::OnAddFriendResponseReceived: Failed to add friend. Final Message: %s"), *ResultMessage);
    }

    if (OnAddFriendAttemptCompleted.IsBound()) // Проверяем перед вызовом
    {
        OnAddFriendAttemptCompleted.Broadcast(bAddSuccess, ResultMessage);
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("OnAddFriendResponseReceived: OnAddFriendAttemptCompleted delegate is not bound. UI will not be notified."));
    }
}