#include "PokerPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "MyGameInstance.h"
#include "OfflineGameManager.h"
#include "GameHUDInterface.h" // Ваш C++ интерфейс HUD
#include "Kismet/GameplayStatics.h"

APokerPlayerController::APokerPlayerController()
{
    bIsUIModeActive = false;
    GameHUDClass = nullptr;
    GameHUDWidgetInstance = nullptr;
}

void APokerPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalPlayerController() && GameHUDClass)
    {
        GameHUDWidgetInstance = CreateWidget<UUserWidget>(this, GameHUDClass);
        if (GameHUDWidgetInstance)
        {
            GameHUDWidgetInstance->AddToViewport();
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: GameHUDWidgetInstance created and added to viewport."));

            // Начальная деактивация кнопок через интерфейс
            if (GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
            {
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
                UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Initial DisableButtons called on HUD."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: Failed to create GameHUDWidgetInstance from GameHUDClass."));
        }
    }
    else if (IsLocalPlayerController())
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDClass is not set in Blueprint Defaults. Cannot create HUD."));
    }

    bIsUIModeActive = false;
    SetInputModeGameOnlyAdvanced();

    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager)
        {
            OfflineManager->OnActionRequestedDelegate.AddDynamic(this, &APokerPlayerController::HandleActionRequested);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Subscribed to OnActionRequestedDelegate."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: OfflineGameManager is null in GameInstance, cannot subscribe."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameInstance is not UMyGameInstance type."));
    }
}

void APokerPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (InputComponent)
    {
        InputComponent->BindAxis("LookUp", this, &APokerPlayerController::LookUp);
        InputComponent->BindAxis("Turn", this, &APokerPlayerController::Turn);
        InputComponent->BindAction("ToggleCursorMode", IE_Pressed, this, &APokerPlayerController::ToggleInputMode);
    }
}

void APokerPlayerController::LookUp(float Value)
{
    if (!bIsUIModeActive && Value != 0.0f)
    {
        APawn* const ControlledPawn = GetPawn();
        if (ControlledPawn)
        {
            ControlledPawn->AddControllerPitchInput(Value);
        }
    }
}

void APokerPlayerController::Turn(float Value)
{
    if (!bIsUIModeActive && Value != 0.0f)
    {
        APawn* const ControlledPawn = GetPawn();
        if (ControlledPawn)
        {
            ControlledPawn->AddControllerYawInput(Value);
        }
    }
}

void APokerPlayerController::ToggleInputMode()
{
    bIsUIModeActive = !bIsUIModeActive;
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::ToggleInputMode - bIsUIModeActive is now %s"), bIsUIModeActive ? TEXT("true") : TEXT("false"));

    if (bIsUIModeActive)
    {
        // При переходе в UI режим, если это не ход игрока, кнопки должны быть выключены.
        // Вызов DisableButtons здесь может быть избыточен, если UpdateActionButtonsAndPlayerTurn
        // корректно обрабатывает ситуацию, когда нет доступных действий.
        // Но если вы хотите гарантированно их выключить при открытии курсора (если это не активный ход):
        // if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        // {
        //    // Возможно, проверить, действительно ли сейчас НЕ ход игрока, прежде чем вызывать
        //    IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
        // }
        SetInputModeUIOnlyAdvanced(GameHUDWidgetInstance, false);
    }
    else
    {
        SetInputModeGameOnlyAdvanced();
    }
}

void APokerPlayerController::SetInputModeGameOnlyAdvanced()
{
    FInputModeGameOnly InputModeData;
    SetInputMode(InputModeData);
    bShowMouseCursor = false;
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: SetInputModeGameOnlyAdvanced called. Cursor hidden."));
}

void APokerPlayerController::SetInputModeUIOnlyAdvanced(UUserWidget* InWidgetToFocus, bool bLockMouseToViewport)
{
    FInputModeUIOnly InputModeData;
    if (InWidgetToFocus)
    {
        InputModeData.SetWidgetToFocus(InWidgetToFocus->TakeWidget());
    }
    InputModeData.SetLockMouseToViewportBehavior(bLockMouseToViewport ? EMouseLockMode::LockAlways : EMouseLockMode::DoNotLock);

    SetInputMode(InputModeData);
    bShowMouseCursor = true;
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: SetInputModeUIOnlyAdvanced called. Cursor shown. WidgetToFocus: %s"), *GetNameSafe(InWidgetToFocus));
}

void APokerPlayerController::HandleActionRequested(int32 SeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionRequested for Seat %d. BetToCall: %lld, MinRaise: %lld, Stack: %lld"), SeatIndex, BetToCall, MinRaiseAmount, PlayerStack);

    if (GameHUDWidgetInstance)
    {
        if (GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            // Проверяем, является ли текущий контроллер контроллером игрока, для которого запрошено действие
            // В оффлайн-игре с одним локальным игроком, PlayerIndex обычно 0.
            // Если SeatIndex (из OfflineGameManager) соответствует нашему локальному игроку (обычно место 0):
            if (SeatIndex == 0) // Упрощенная проверка для локального игрока на месте 0
            {
                IGameHUDInterface::Execute_UpdateActionButtonsAndPlayerTurn(GameHUDWidgetInstance, SeatIndex, AllowedActions, BetToCall, MinRaiseAmount, PlayerStack);
            }
            else
            {
                // Если это ход бота, или другого игрока (в будущем онлайн режиме),
                // то для локального игрока все кнопки действий должны быть выключены.
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
                // Дополнительно можно обновить HUD, чтобы показать, чей ход (например, "Bot 1 is thinking...")
                // Это можно сделать, расширив UpdateActionButtonsAndPlayerTurn или добавив новую функцию в интерфейс.
                // Пока что просто выключаем кнопки.
                UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Not local player's turn (Seat %d). Disabling buttons."), SeatIndex);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDWidgetInstance (%s) does not implement IGameHUDInterface!"), *GetNameSafe(GameHUDWidgetInstance));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDWidgetInstance is null, cannot update HUD actions."));
    }
}

// Функции-обработчики действий
void APokerPlayerController::HandleFoldAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleFoldAction called by UI."));
    // ... (ваша логика вызова ProcessPlayerAction) ...
}

void APokerPlayerController::HandleCheckCallAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleCheckCallAction called by UI."));
    // ... (ваша логика вызова ProcessPlayerAction) ...
}

void APokerPlayerController::HandleBetRaiseAction(int64 Amount)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleBetRaiseAction called by UI with Amount: %lld"), Amount);
    // ... (ваша логика вызова ProcessPlayerAction) ...
}