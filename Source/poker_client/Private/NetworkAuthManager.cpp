#include "NetworkAuthManager.h"
#include "MyGameInstance.h" // Убедитесь, что этот инклюд и путь к нему корректны
#include "NetworkDataTypes.h" // Для FPlayerFriendInfo, FPlayerStatsInfo

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "Serialization/JsonSerializer.h" // Для FJsonSerializer
#include "Dom/JsonObject.h"             // Для FJsonObject
#include "Dom/JsonValue.h"              // Для FJsonValue

// Вспомогательная функция для добавления Authorization хедера
// Помещена в анонимное пространство имен, чтобы быть локальной для этого .cpp файла
namespace
{
    void AddAuthorizationHeaderToRequest(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest, UMyGameInstance* GameInstance)
    {
        if (GameInstance && GameInstance->bIsLoggedIn && !GameInstance->GetAuthToken().IsEmpty())
        {
            HttpRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + GameInstance->GetAuthToken());
            UE_LOG(LogTemp, Verbose, TEXT("AddAuthorizationHeaderToRequest: Authorization Bearer token added."));
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("AddAuthorizationHeaderToRequest: No valid AuthToken or user not logged in. Header not added."));
        }
    }
}


UNetworkAuthManager::UNetworkAuthManager()
{
    OwningGameInstance = nullptr;
    // ApiBaseUrl инициализируется в Initialize
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
        UE_LOG(LogTemp, Warning, TEXT("UNetworkAuthManager::Initialize - ApiBaseUrl is empty! Will use default from GameInstance if available."));
        // Можно добавить логику получения ApiBaseUrl из GameInstance, если он там тоже хранится как дефолтный
        if (OwningGameInstance && !OwningGameInstance->ApiBaseUrl.IsEmpty())
        {
            ApiBaseUrl = OwningGameInstance->ApiBaseUrl;
            UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::Initialize - Using ApiBaseUrl from GameInstance: %s"), *ApiBaseUrl);
        }
    }
}

void UNetworkAuthManager::RequestLogin(const FString& Username, const FString& Password)
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("RequestLogin: OwningGameInstance is null.")); return; }
    if (ApiBaseUrl.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("RequestLogin: ApiBaseUrl is not set.")); OwningGameInstance->OnLoginAttemptCompleted.Broadcast(false, TEXT("Внутренняя ошибка: URL API не настроен.")); return; }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestLogin: User: %s"), *Username);

    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("username"), Username);
    RequestJson->SetStringField(TEXT("password"), Password);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestLogin: Failed to serialize JSON."));
        OwningGameInstance->OnLoginAttemptCompleted.Broadcast(false, TEXT("Ошибка создания запроса."));
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
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestLogin: ProcessRequest failed."));
        OwningGameInstance->OnLoginAttemptCompleted.Broadcast(false, TEXT("Ошибка сети при отправке запроса."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestLogin: Request sent."));
    }
}

void UNetworkAuthManager::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("OnLoginResponseReceived: OwningGameInstance is null.")); return; }

    bool bLoginSuccess = false;
    FString ResponseMessage = TEXT("Неизвестная ошибка входа.");
    FString ReceivedAuthToken = TEXT("");

    if (bWasSuccessful && Response.IsValid())
    {
        int32 ResponseCode = Response->GetResponseCode();
        FString ResponseBody = Response->GetContentAsString();
        UE_LOG(LogTemp, Log, TEXT("OnLoginResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);

        if (ResponseCode == 200)
        {
            TSharedPtr<FJsonObject> ResponseJson;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
            if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
            {
                int64 ReceivedUserId = -1;
                FString ReceivedUsername, ReceivedFriendCode;

                if (ResponseJson->TryGetNumberField(TEXT("userId"), ReceivedUserId) &&
                    ResponseJson->TryGetStringField(TEXT("username"), ReceivedUsername) &&
                    ResponseJson->TryGetStringField(TEXT("friendCode"), ReceivedFriendCode) &&
                    ResponseJson->TryGetStringField(TEXT("accessToken"), ReceivedAuthToken)) // <-- ИЗВЛЕКАЕМ accessToken
                {
                    UE_LOG(LogTemp, Log, TEXT("Login successful for: %s (ID: %lld, FriendCode: %s), Token (partial): %s"),
                        *ReceivedUsername, ReceivedUserId, *ReceivedFriendCode, *ReceivedAuthToken.Left(20));
                    OwningGameInstance->SetLoginStatus(true, ReceivedUserId, ReceivedUsername, ReceivedFriendCode, ReceivedAuthToken); // <-- ПЕРЕДАЕМ ТОКЕН
                    bLoginSuccess = true;
                    ResponseMessage = TEXT("Вход выполнен успешно!");
                }
                else { ResponseMessage = TEXT("Ошибка сервера: неверный формат ответа (отсутствуют userId, username, friendCode или accessToken)."); }
            }
            else { ResponseMessage = TEXT("Ошибка сервера: не удалось обработать ответ JSON."); }
        }
        else if (ResponseCode == 401) { ResponseMessage = TEXT("Неверное имя пользователя или пароль."); }
        else
        {
            // Улучшенный парсинг сообщения об ошибке с сервера
            TSharedPtr<FJsonObject> ErrorJson;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
            if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid() && ErrorJson->HasTypedField<EJson::String>(TEXT("message"))) {
                ResponseMessage = ErrorJson->GetStringField(TEXT("message"));
            }
            else if (!ResponseBody.IsEmpty()) {
                ResponseMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d): %s"), ResponseCode, *ResponseBody);
            }
            else {
                ResponseMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d)"), ResponseCode);
            }
        }
    }
    else { ResponseMessage = TEXT("Сервер не доступен или проблемы с сетью."); }

    if (OwningGameInstance->OnLoginAttemptCompleted.IsBound())
        OwningGameInstance->OnLoginAttemptCompleted.Broadcast(bLoginSuccess, ResponseMessage);
}


void UNetworkAuthManager::RequestRegister(const FString& Username, const FString& Password, const FString& Email)
{
    // Этот метод остается без изменений, так как регистрация обычно не включает возврат JWT.
    // Если ваша логика сервера изменилась и регистрация сразу возвращает токен,
    // то OnRegisterResponseReceived нужно будет обновить аналогично OnLoginResponseReceived.
    if (!OwningGameInstance) { /* ... */ return; }
    if (ApiBaseUrl.IsEmpty()) { /* ... */ return; }
    // ... (остальная часть вашего существующего кода RequestRegister) ...
    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestRegister for user: %s"), *Username);
    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("username"), Username);
    RequestJson->SetStringField(TEXT("password"), Password);
    RequestJson->SetStringField(TEXT("email"), Email);
    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer)) { /* ... */ OwningGameInstance->OnRegisterAttemptCompleted.Broadcast(false, TEXT("Ошибка создания запроса.")); return; }
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/auth/register"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UNetworkAuthManager::OnRegisterResponseReceived);
    if (!HttpRequest->ProcessRequest()) { /* ... */ OwningGameInstance->OnRegisterAttemptCompleted.Broadcast(false, TEXT("Ошибка сети.")); }
    else { UE_LOG(LogTemp, Log, TEXT("RequestRegister: Request sent.")); }
}

void UNetworkAuthManager::OnRegisterResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    // Этот метод остается без изменений, если сервер не возвращает токен при регистрации.
    // ... (ваш существующий код OnRegisterResponseReceived) ...
    if (!OwningGameInstance) { /* ... */ return; }
    bool bRegisterSuccess = false;
    FString ResultMessage = TEXT("Неизвестная ошибка регистрации.");
    if (bWasSuccessful && Response.IsValid()) {
        int32 ResponseCode = Response->GetResponseCode();
        FString ResponseBody = Response->GetContentAsString();
        UE_LOG(LogTemp, Log, TEXT("OnRegisterResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);
        if (ResponseCode == 201 || ResponseCode == 200) {
            bRegisterSuccess = true;
            ResultMessage = TEXT("Регистрация прошла успешно! Теперь вы можете войти.");
        }
        else {
            // Улучшенный парсинг сообщения об ошибке
            TSharedPtr<FJsonObject> ErrorJson;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
            if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid() && ErrorJson->HasTypedField<EJson::String>(TEXT("message"))) {
                ResultMessage = ErrorJson->GetStringField(TEXT("message"));
            }
            else if (!ResponseBody.IsEmpty()) {
                ResultMessage = FString::Printf(TEXT("Ошибка регистрации (Код: %d): %s"), ResponseCode, *ResponseBody);
            }
            else {
                ResultMessage = FString::Printf(TEXT("Ошибка регистрации (Код: %d)"), ResponseCode);
            }
        }
    }
    else { ResultMessage = TEXT("Сервер не доступен или проблемы с сетью."); }
    if (OwningGameInstance->OnRegisterAttemptCompleted.IsBound())
        OwningGameInstance->OnRegisterAttemptCompleted.Broadcast(bRegisterSuccess, ResultMessage);
}


void UNetworkAuthManager::RequestAddFriend(const FString& FriendCode)
{
    if (!OwningGameInstance || !OwningGameInstance->bIsLoggedIn) {
        UE_LOG(LogTemp, Warning, TEXT("RequestAddFriend: User not logged in."));
        if (OnAddFriendAttemptCompleted.IsBound()) OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Необходимо войти в аккаунт."));
        return;
    }
    if (ApiBaseUrl.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("RequestAddFriend: ApiBaseUrl is not set.")); if (OnAddFriendAttemptCompleted.IsBound()) OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Ошибка URL API.")); return; }
    if (FriendCode.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("RequestAddFriend: FriendCode is empty.")); if (OnAddFriendAttemptCompleted.IsBound()) OnAddFriendAttemptCompleted.Broadcast(false, TEXT("Код друга не может быть пустым.")); return; }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestAddFriend: Adding friend with code: %s"), *FriendCode);

    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("friendCode"), FriendCode);
    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    if (!FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer)) { /* ... ошибка сериализации ... */ return; }

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/friends/add"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    AddAuthorizationHeaderToRequest(HttpRequest, OwningGameInstance); // <--- ДОБАВЛЯЕМ JWT
    HttpRequest->SetContentAsString(RequestBodyString);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UNetworkAuthManager::OnAddFriendResponseReceived);

    if (!HttpRequest->ProcessRequest()) { /* ... ошибка отправки ... */ }
    else { UE_LOG(LogTemp, Log, TEXT("RequestAddFriend: Request sent.")); }
}

void UNetworkAuthManager::OnAddFriendResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    // Логика остается прежней, но теперь запрос был аутентифицирован через JWT
    // ... (ваш существующий код OnAddFriendResponseReceived, который уже должен хорошо парсить ошибки и успех) ...
    if (!OwningGameInstance) { /* ... */ return; }
    bool bAddSuccess = false;
    FString ResultMessage = TEXT("Неизвестная ошибка при добавлении друга.");
    if (bWasSuccessful && Response.IsValid()) {
        int32 ResponseCode = Response->GetResponseCode();
        FString ResponseBody = Response->GetContentAsString();
        UE_LOG(LogTemp, Log, TEXT("OnAddFriendResponseReceived: Code: %d, Body: %s"), ResponseCode, *ResponseBody);
        if (ResponseCode == 200 || ResponseCode == 201) {
            bAddSuccess = true;
            // Пытаемся извлечь "message" из JSON ответа, если есть
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
            if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid() && JsonObject->HasTypedField<EJson::String>(TEXT("message"))) {
                ResultMessage = JsonObject->GetStringField(TEXT("message"));
            }
            else {
                ResultMessage = TEXT("Друг успешно добавлен!");
            }
        }
        else {
            // Улучшенный парсинг сообщения об ошибке
            TSharedPtr<FJsonObject> ErrorJson;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
            if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid() && ErrorJson->HasTypedField<EJson::String>(TEXT("message"))) {
                ResultMessage = ErrorJson->GetStringField(TEXT("message"));
            }
            else if (!ResponseBody.IsEmpty()) {
                ResultMessage = FString::Printf(TEXT("Ошибка добавления друга (Код: %d): %s"), ResponseCode, *ResponseBody);
            }
            else {
                ResultMessage = FString::Printf(TEXT("Ошибка добавления друга (Код: %d)"), ResponseCode);
            }
        }
    }
    else { ResultMessage = TEXT("Сервер не доступен или проблемы с сетью."); }
    if (OnAddFriendAttemptCompleted.IsBound()) OnAddFriendAttemptCompleted.Broadcast(bAddSuccess, ResultMessage);
}

void UNetworkAuthManager::RequestFriendList()
{
    if (!OwningGameInstance || !OwningGameInstance->bIsLoggedIn) { /* ... ошибка, пользователь не залогинен ... */ if (OnFriendListReceivedDelegate.IsBound()) OnFriendListReceivedDelegate.Broadcast(false, {}, TEXT("Необходимо войти.")); return; }
    if (ApiBaseUrl.IsEmpty()) { /* ... ошибка URL ... */ if (OnFriendListReceivedDelegate.IsBound()) OnFriendListReceivedDelegate.Broadcast(false, {}, TEXT("Ошибка URL API.")); return; }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestFriendList..."));
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/friends/list"));
    AddAuthorizationHeaderToRequest(HttpRequest, OwningGameInstance); // <--- ДОБАВЛЯЕМ JWT
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UNetworkAuthManager::OnFriendListResponseReceived);

    if (!HttpRequest->ProcessRequest()) { /* ... ошибка отправки ... */ }
    else { UE_LOG(LogTemp, Log, TEXT("RequestFriendList: Request sent.")); }
}

void UNetworkAuthManager::OnFriendListResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    // Этот код вы уже должны были написать ранее, он остается практически таким же.
    // Главное, что запрос теперь идет с JWT.
    // ... (ваш существующий код OnFriendListResponseReceived для парсинга массива FPlayerFriendInfo) ...
    if (!OwningGameInstance) { /* ... */ return; }
    TArray<FPlayerFriendInfo> ReceivedFriends;
    FString ErrorMessage = TEXT("Неизвестная ошибка при получении списка друзей.");
    bool bSuccess = false;
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200) {
        FString ResponseBody = Response->GetContentAsString();
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        if (FJsonSerializer::Deserialize(Reader, JsonArray)) {
            for (const TSharedPtr<FJsonValue>& Value : JsonArray) {
                const TSharedPtr<FJsonObject>* FriendObject;
                if (Value->TryGetObject(FriendObject)) {
                    FPlayerFriendInfo FriendInfo;
                    (*FriendObject)->TryGetNumberField(TEXT("userId"), FriendInfo.UserId);
                    (*FriendObject)->TryGetStringField(TEXT("username"), FriendInfo.Username);
                    (*FriendObject)->TryGetStringField(TEXT("friendCode"), FriendInfo.FriendCode);
                    ReceivedFriends.Add(FriendInfo);
                }
            }
            bSuccess = true;
            ErrorMessage = TEXT("");
        }
        else { ErrorMessage = TEXT("Ошибка парсинга JSON списка друзей."); }
    }
    else { /* ... обработка ошибок сети или кода ответа ... */
        if (!Response.IsValid()) { ErrorMessage = TEXT("Сервер не доступен."); }
        else { ErrorMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d) (список друзей)."), Response->GetResponseCode()); }
    }
    if (OnFriendListReceivedDelegate.IsBound()) OnFriendListReceivedDelegate.Broadcast(bSuccess, ReceivedFriends, ErrorMessage);
}

void UNetworkAuthManager::RequestPlayerStats()
{
    if (!OwningGameInstance || !OwningGameInstance->bIsLoggedIn) { /* ... ошибка, пользователь не залогинен ... */ if (OnPlayerStatsReceivedDelegate.IsBound()) OnPlayerStatsReceivedDelegate.Broadcast(false, FPlayerStatsInfo(), TEXT("Необходимо войти.")); return; }
    if (ApiBaseUrl.IsEmpty()) { /* ... ошибка URL ... */ if (OnPlayerStatsReceivedDelegate.IsBound()) OnPlayerStatsReceivedDelegate.Broadcast(false, FPlayerStatsInfo(), TEXT("Ошибка URL API.")); return; }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestPlayerStats..."));
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetURL(ApiBaseUrl + TEXT("/stats/me"));
    AddAuthorizationHeaderToRequest(HttpRequest, OwningGameInstance); // <--- ДОБАВЛЯЕМ JWT
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UNetworkAuthManager::OnPlayerStatsResponseReceived);

    if (!HttpRequest->ProcessRequest()) { /* ... ошибка отправки ... */ }
    else { UE_LOG(LogTemp, Log, TEXT("RequestPlayerStats: Request sent.")); }
}

void UNetworkAuthManager::OnPlayerStatsResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    // Этот код вы уже должны были написать ранее, он остается практически таким же.
    // ... (ваш существующий код OnPlayerStatsResponseReceived для парсинга FPlayerStatsInfo) ...
    if (!OwningGameInstance) { /* ... */ return; }
    FPlayerStatsInfo ReceivedStats;
    FString ErrorMessage = TEXT("Неизвестная ошибка при получении статистики.");
    bool bSuccess = false;
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200) {
        FString ResponseBody = Response->GetContentAsString();
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        TSharedPtr<FJsonObject> JsonObject;
        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid()) {
            JsonObject->TryGetNumberField(TEXT("handsPlayed"), ReceivedStats.HandsPlayed);
            JsonObject->TryGetNumberField(TEXT("handsWon"), ReceivedStats.HandsWon);
            // ... парсинг других полей статистики ...
            bSuccess = true;
            ErrorMessage = TEXT("");
        }
        else { ErrorMessage = TEXT("Ошибка парсинга JSON статистики."); }
    }
    else { /* ... обработка ошибок сети или кода ответа ... */
        if (!Response.IsValid()) { ErrorMessage = TEXT("Сервер не доступен."); }
        else { ErrorMessage = FString::Printf(TEXT("Ошибка сервера (Код: %d) (статистика)."), Response->GetResponseCode()); }
    }
    if (OnPlayerStatsReceivedDelegate.IsBound()) OnPlayerStatsReceivedDelegate.Broadcast(bSuccess, ReceivedStats, ErrorMessage);
}

void UNetworkAuthManager::RequestRemoveFriendByCode(const FString& FriendCodeToRemove)
{
    if (!OwningGameInstance || !OwningGameInstance->bIsLoggedIn) {
        UE_LOG(LogTemp, Warning, TEXT("RequestRemoveFriendByCode: User not logged in."));
        if (OnRemoveFriendAttemptCompletedDelegate.IsBound()) {
            OnRemoveFriendAttemptCompletedDelegate.Broadcast(false, TEXT("Необходимо войти в аккаунт."));
        }
        return;
    }
    if (ApiBaseUrl.IsEmpty()) { /* ... ошибка URL ... */ return; }
    if (FriendCodeToRemove.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("RequestRemoveFriendByCode: FriendCodeToRemove is empty."));
        if (OnRemoveFriendAttemptCompletedDelegate.IsBound()) {
            OnRemoveFriendAttemptCompletedDelegate.Broadcast(false, TEXT("Код друга для удаления не может быть пустым."));
        }
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestRemoveFriendByCode: Removing friend with code: %s"), *FriendCodeToRemove);

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    HttpRequest->SetVerb(TEXT("DELETE"));
    // Формируем URL с friendCode как частью пути
    FString Url = FString::Printf(TEXT("%s/friends/remove-by-code/%s"), *ApiBaseUrl, *FriendCodeToRemove);
    HttpRequest->SetURL(Url);
    AddAuthorizationHeaderToRequest(HttpRequest, OwningGameInstance); // JWT токен

    // Для DELETE тело запроса обычно не нужно
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UNetworkAuthManager::OnRemoveFriendByCodeResponseReceived); // Используем новый обработчик

    if (!HttpRequest->ProcessRequest()) {
        UE_LOG(LogTemp, Error, TEXT("UNetworkAuthManager::RequestRemoveFriendByCode: Failed to start HTTP request."));
        if (OnRemoveFriendAttemptCompletedDelegate.IsBound()) {
            OnRemoveFriendAttemptCompletedDelegate.Broadcast(false, TEXT("Ошибка сети при удалении друга."));
        }
    }
    else {
        UE_LOG(LogTemp, Log, TEXT("UNetworkAuthManager::RequestRemoveFriendByCode: HTTP request sent to %s"), *Url);
    }
}

// Переименовываем или создаем новый обработчик
void UNetworkAuthManager::OnRemoveFriendByCodeResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    // Логика этого обработчика остается такой же, как была в OnRemoveFriendResponseReceived
    // Он просто сообщает об успехе или ошибке через OnRemoveFriendAttemptCompletedDelegate
    if (!OwningGameInstance) { UE_LOG(LogTemp, Error, TEXT("OnRemoveFriendByCodeResponseReceived: OwningGameInstance is null.")); return; }

    bool bRemoveSuccess = false;
    FString ResultMessage = TEXT("Неизвестная ошибка при удалении друга.");

    // ... (дальнейшая логика парсинга ответа и вызова OnRemoveFriendAttemptCompletedDelegate.Broadcast(...)) ...
    // Скопируйте сюда вашу существующую логику из OnRemoveFriendResponseReceived, она должна подойти.
    // Главное, что она вызывается правильным запросом.
    if (bWasSuccessful && Response.IsValid())
    {
        int32 ResponseCode = Response->GetResponseCode();
        FString ResponseBody = Response->GetContentAsString();
        if (ResponseCode == 200 || ResponseCode == 204) { // OK или No Content
            bRemoveSuccess = true;
            // ... парсинг опционального message из JSON ...
            ResultMessage = TEXT("Друг успешно удален.");
        }
        else {
            // ... парсинг JSON ошибки ...
            ResultMessage = FString::Printf(TEXT("Ошибка удаления друга (Код: %d)"), ResponseCode);
        }
    }
    else { ResultMessage = TEXT("Сервер не доступен или проблемы с сетью."); }

    if (OnRemoveFriendAttemptCompletedDelegate.IsBound()) {
        OnRemoveFriendAttemptCompletedDelegate.Broadcast(bRemoveSuccess, ResultMessage);
    }
}