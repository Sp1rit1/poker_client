#include "NetworkAuthManager.h"
#include "MyGameInstance.h" // Включаем заголовок нашего GameInstance

// Инклуды для HTTP и JSON (как были в вашем MyGameInstance.cpp)
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h" // IHttpRequest уже включен через NetworkAuthManager.h
#include "Json.h"
#include "Serialization/JsonSerializer.h"

UNetworkAuthManager::UNetworkAuthManager()
{
    OwningGameInstance = nullptr;
    // ApiBaseUrl будет инициализирован в Initialize()
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
        // Возможно, стоит вызвать делегат с ошибкой здесь
        OwningGameInstance->OnLoginAttemptCompleted.Broadcast(false, TEXT("Внутренняя ошибка: не удалось создать запрос."));
        return;
    }

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/auth/login")); // Используем сохраненный ApiBaseUrl
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
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/auth/register")); // Используем сохраненный ApiBaseUrl
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
            FString ReceivedFriendCode; // Добавлено для кода друга

            // Извлекаем friendCode из ответа сервера (предполагается, что он там есть)
            if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
                ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername) &&
                ResponseJson->TryGetStringField(TEXT("friendCode"), ReceivedFriendCode)) // Изменил с token на friendCode
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
                        (*ErrorObject)->TryGetStringField(TEXT("defaultMessage"), FieldError); // Стандартное поле для @Valid
                        ValidationErrors += FieldError + TEXT("; ");
                    }
                    else if (ErrorValue->Type == EJson::String) { // Если ошибки просто строками
                        ValidationErrors += ErrorValue->AsString() + TEXT("; ");
                    }
                }
                if (!ValidationErrors.IsEmpty())
                {
                    ResponseMessage = ValidationErrors.LeftChop(2); // Убираем последний "; "
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
                    else if (ErrorValue->Type == EJson::String) { // Простая строка
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