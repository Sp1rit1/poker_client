#include "PokerPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MyGameInstance.h" 
#include "OfflineGameManager.h" 
#include "GameHUDInterface.h"     // <-- ДОБАВЬТЕ ЭТОТ ИНКЛЮД
#include "PokerDataTypes.h"       // <-- ДОБАВЬТЕ ЭТОТ ИНКЛЮД (для EPlayerAction в HandleActionRequested, если GameHUDInterface.h его не подтянет транзитивно)

APokerPlayerController::APokerPlayerController()
{
    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
    bIsMouseCursorVisible = false;
}

void APokerPlayerController::BeginPlay()
{
    Super::BeginPlay();

    ToggleInputMode(); // Устанавливаем начальный режим через вашу функцию ToggleInputMode (чтобы bIsMouseCursorVisible была false)

    if (GameHUDWidgetClass)
    {
        GameHUDWidgetInstance = CreateWidget<UUserWidget>(this, GameHUDWidgetClass);
        if (GameHUDWidgetInstance)
        {
            GameHUDWidgetInstance->AddToViewport();
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: GameHUDWidgetInstance created and added to viewport."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("APokerPlayerController: Failed to create GameHUDWidgetInstance!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDWidgetClass is not set!"));
    }

    // --- ПОДПИСКА НА ДЕЛЕГАТ ---
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager)
        {
            // FOnActionRequestedSignature - это имя типа делегата, объявленного в OfflineGameManager.h
            OfflineManager->OnActionRequestedDelegate.AddDynamic(this, &APokerPlayerController::HandleActionRequested);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Subscribed to OnActionRequestedDelegate."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: OfflineGameManager is null in GameInstance, cannot subscribe to OnActionRequestedDelegate."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameInstance is not UMyGameInstance."));
    }
    // ---------------------------
}

UUserWidget* APokerPlayerController::GetGameHUD() const
{
    return GameHUDWidgetInstance;
}

void APokerPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (InputComponent)
    {
        EnableInput(this);
        InputComponent->BindAction("ToggleCursor", IE_Pressed, this, &APokerPlayerController::ToggleInputMode);
        // ... другие ваши BindAction ...
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("APokerPlayerController::SetupInputComponent - InputComponent is NULL!"));
    }
}

void APokerPlayerController::ToggleInputMode()
{
    // Ваша реализация ToggleInputMode выглядит хорошо.
    // Можно немного ее уточнить для ясности начального состояния, если BeginPlay всегда устанавливает GameOnly.
    bIsMouseCursorVisible = !bIsMouseCursorVisible; // Инвертируем состояние

    if (bIsMouseCursorVisible)
    {
        FInputModeGameAndUI InputModeData;
        InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputModeData.SetHideCursorDuringCapture(false); // Чтобы курсор не пропадал при клике в UI
        SetInputMode(InputModeData);
        bShowMouseCursor = true; // Явно показываем
        UE_LOG(LogTemp, Log, TEXT("Input Mode: GameAndUI, Cursor Visible."));
    }
    else
    {
        SetInputMode(FInputModeGameOnly());
        bShowMouseCursor = false; // Явно скрываем
        UE_LOG(LogTemp, Log, TEXT("Input Mode: GameOnly, Cursor Hidden."));
    }
}

// --- РЕАЛИЗАЦИЯ ФУНКЦИИ-ОБРАБОТЧИКА ДЕЛЕГАТА ---
void APokerPlayerController::HandleActionRequested(int32 PlayerSeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionRequested for Seat %d. BetToCall: %lld, MinRaise: %lld, Stack: %lld"),
        PlayerSeatIndex, BetToCall, MinRaiseAmount, PlayerStack);

    for (EPlayerAction Action : AllowedActions)
    {
        const UEnum* EnumPtr = StaticEnum<EPlayerAction>();
        FString ActionString = EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(Action)) : TEXT("UnknownAction");
        UE_LOG(LogTemp, Log, TEXT("Allowed Action: %s"), *ActionString);
    }

    if (GameHUDWidgetInstance)
    {
        // Проверяем, реализует ли наш виджет HUD интерфейс IGameHUDInterface
        if (GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            // Вызываем функцию интерфейса, которая будет реализована в Blueprint (WBP_GameHUD)
            IGameHUDInterface::Execute_UpdateActionButtonsAndPlayerTurn(GameHUDWidgetInstance, PlayerSeatIndex, AllowedActions, BetToCall, MinRaiseAmount, PlayerStack);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Called UpdateActionButtonsAndPlayerTurn via IGameHUDInterface."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDWidgetInstance (%s) does not implement IGameHUDInterface. Cannot update action buttons."), *GameHUDWidgetInstance->GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDWidgetInstance is null in HandleActionRequested."));
    }
}
// -------------------------------------------------

// Заглушки для Обработки Действий Игрока (остаются, как у вас, вызов ProcessPlayerAction будет позже)
void APokerPlayerController::HandleFoldAction()
{
    UE_LOG(LogTemp, Log, TEXT("PlayerController: Fold Action Triggered"));
    // UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    // if (GI && GI->GetOfflineGameManager())
    // {
    //     GI->GetOfflineGameManager()->ProcessPlayerAction(GI->GetOfflineGameManager()->GetGameState()->CurrentTurnSeat, EPlayerAction::Fold, 0);
    // }
}

void APokerPlayerController::HandleCheckCallAction()
{
    UE_LOG(LogTemp, Log, TEXT("PlayerController: Check/Call Action Triggered"));
    // ... (логика вызова ProcessPlayerAction с определением Check или Call) ...
}

void APokerPlayerController::HandleBetRaiseAction(int64 Amount)
{
    UE_LOG(LogTemp, Log, TEXT("PlayerController: Bet/Raise Action Triggered with Amount: %lld"), Amount);
    // ... (логика вызова ProcessPlayerAction с определением Bet или Raise) ...
}