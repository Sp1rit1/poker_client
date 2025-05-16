#include "PokerPlayerController.h" // Убедитесь, что имя вашего .h файла здесь
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "MyGameInstance.h"        // Замените, если имя вашего GameInstance другое
#include "OfflineGameManager.h"    // Для подписки на делегат
#include "GameHUDInterface.h"      // Ваш C++ интерфейс HUD
#include "Kismet/GameplayStatics.h" // Для GetGameInstance

APokerPlayerController::APokerPlayerController()
{
    PlayerInputMappingContext = nullptr;
    LookUpAction = nullptr;
    TurnAction = nullptr;
    ToggleToUIAction = nullptr;
    GameHUDClass = nullptr;
    GameHUDWidgetInstance = nullptr;
    bIsInUIMode = false; // Начинаем в игровом режиме
}

void APokerPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Создаем и отображаем HUD
    if (IsLocalPlayerController() && GameHUDClass)
    {
        GameHUDWidgetInstance = CreateWidget<UUserWidget>(this, GameHUDClass);
        if (GameHUDWidgetInstance)
        {
            GameHUDWidgetInstance->AddToViewport();
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: GameHUDWidgetInstance created and added to viewport."));
            if (GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
            {
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance); // Начальная деактивация кнопок
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

    // Начальная установка игрового режима
    SwitchToGameInputMode(); // Эта функция теперь также устанавливает bIsInUIMode = false;

    // Подписка на делегат OfflineGameManager
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
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::SetupInputComponent CALLED"));

    if (UEnhancedInputComponent* EnhancedInputComp = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (LookUpAction)
        {
            EnhancedInputComp->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleLookUp);
            UE_LOG(LogTemp, Log, TEXT("Bound LookUpAction"));
        }
        if (TurnAction)
        {
            EnhancedInputComp->BindAction(TurnAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleTurn);
            UE_LOG(LogTemp, Log, TEXT("Bound TurnAction"));
        }
        if (ToggleToUIAction) // Это действие для перехода ИЗ игры В UI
        {
            EnhancedInputComp->BindAction(ToggleToUIAction, ETriggerEvent::Started, this, &APokerPlayerController::HandleToggleToUI);
            UE_LOG(LogTemp, Log, TEXT("Bound ToggleToUIAction"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("APokerPlayerController::SetupInputComponent - Failed to Cast to UEnhancedInputComponent."));
    }
}

void APokerPlayerController::HandleLookUp(const FInputActionValue& Value)
{
    if (bIsInUIMode) return; // Не вращаем в UI режиме

    const float LookAxisValue = Value.Get<float>();
    if (LookAxisValue != 0.0f)
    {
        APawn* const ControlledPawn = GetPawn();
        if (ControlledPawn)
        {
            ControlledPawn->AddControllerPitchInput(LookAxisValue);
        }
    }
}

void APokerPlayerController::HandleTurn(const FInputActionValue& Value)
{
    if (bIsInUIMode) return; // Не вращаем в UI режиме

    const float TurnAxisValue = Value.Get<float>();
    if (TurnAxisValue != 0.0f)
    {
        APawn* const ControlledPawn = GetPawn();
        if (ControlledPawn)
        {
            ControlledPawn->AddControllerYawInput(TurnAxisValue);
        }
    }
}

void APokerPlayerController::HandleToggleToUI(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::HandleToggleToUI called. Current bIsInUIMode before toggle: %s"), bIsInUIMode ? TEXT("true") : TEXT("false"));
    if (!bIsInUIMode) // Только если мы В ИГРОВОМ режиме, переключаемся в UI
    {
        SwitchToUIInputMode(GameHUDWidgetInstance);
    }
    // Если мы уже в UI режиме, это действие из PlayerInputMappingContext не должно было сработать,
    // так как этот контекст должен быть удален при переходе в SwitchToUIInputMode.
}

void APokerPlayerController::SwitchToGameInputMode()
{
    FInputModeGameOnly InputModeData;
    SetInputMode(InputModeData);
    bShowMouseCursor = false;
    bIsInUIMode = false;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        // Если вы явно удаляли PlayerInputMappingContext в SwitchToUIInputMode,
        // или использовали ClearAllMappings, то здесь его нужно снова добавить.
        // Если PlayerInputMappingContext никогда не удалялся из сабсистемы, этот AddMappingContext может быть лишним
        // или вызвать добавление дубликата (хотя обычно AddMappingContext обрабатывает это).
        // Для надежности, если он мог быть удален:
        Subsystem->ClearAllMappings(); // Очищаем все предыдущие, чтобы избежать наложения
        if (PlayerInputMappingContext)
        {
            Subsystem->AddMappingContext(PlayerInputMappingContext, 0);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: PlayerInputMappingContext ADDED."));
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: PlayerInputMappingContext is NULL, cannot add in SwitchToGameInputMode."));
        }
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Switched to Game Input Mode. Cursor hidden."));
}

void APokerPlayerController::SwitchToUIInputMode(UUserWidget* WidgetToFocus)
{
    FInputModeUIOnly InputModeData;
    if (WidgetToFocus)
    {
        InputModeData.SetWidgetToFocus(WidgetToFocus->TakeWidget());
    }
    InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputModeData);
    bShowMouseCursor = true;
    bIsInUIMode = true;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        // Удаляем игровой контекст, чтобы действия из него не работали в UI режиме
        if (PlayerInputMappingContext)
        {
            Subsystem->RemoveMappingContext(PlayerInputMappingContext);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: PlayerInputMappingContext REMOVED."));
        }
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Switched to UI Input Mode. Cursor shown. WidgetToFocus: %s"), *GetNameSafe(WidgetToFocus));
}

void APokerPlayerController::HandleActionRequested(int32 SeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionRequested for Seat %d. BetToCall: %lld, MinRaise: %lld, Stack: %lld"), SeatIndex, BetToCall, MinRaiseAmount, PlayerStack);

    if (GameHUDWidgetInstance)
    {
        if (GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            if (SeatIndex == 0) // Предполагаем, что локальный игрок всегда SeatIndex 0
            {
                IGameHUDInterface::Execute_UpdateActionButtonsAndPlayerTurn(GameHUDWidgetInstance, SeatIndex, AllowedActions, BetToCall, MinRaiseAmount, PlayerStack);

                // Опционально: если это наш ход, и мы не в UI режиме, автоматически переключиться в UI
                // if (!bIsInUIMode)
                // {
                //     SwitchToUIInputMode(GameHUDWidgetInstance);
                // }
            }
            else
            {
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
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

// Функции-обработчики действий (заглушки)
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