#include "PokerPlayerController.h" // Убедитесь, что имя файла .h указано верно
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "MyGameInstance.h"        // Замените, если имя другое
#include "OfflineGameManager.h"    // Для подписки на делегат
#include "GameHUDInterface.h"      // Ваш C++ интерфейс HUD
#include "Kismet/GameplayStatics.h"

APokerPlayerController::APokerPlayerController()
{
    DefaultMappingContext = nullptr;
    LookUpAction = nullptr;
    TurnAction = nullptr;
    GameHUDClass = nullptr;
    GameHUDWidgetInstance = nullptr;
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
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
            }
        }
    }
    else if (IsLocalPlayerController())
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDClass is not set in Blueprint Defaults. Cannot create HUD."));
    }

    // Устанавливаем режим ввода "только игра" и скрываем курсор
    FInputModeGameOnly InputModeData;
    SetInputMode(InputModeData);
    bShowMouseCursor = false;
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Input mode set to GameOnly, cursor hidden."));

    // Настройка Enhanced Input
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: DefaultMappingContext added."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: DefaultMappingContext is not set in Blueprint!"));
        }
    }

    // Привязка Input Actions (теперь делаем это здесь, а не в SetupInputComponent, т.к. он не вызывается для Enhanced Input автоматически без PlayerInputComponent)
    // Однако, PlayerController сам создает InputComponent, поэтому SetupInputComponent все еще лучшее место.
    // Если вы хотите использовать SetupInputComponent, убедитесь, что он вызывается.
    // Для надежности, если SetupInputComponent может не вызываться или вы хотите полный контроль здесь:
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent)) // InputComponent создается базовым PlayerController
    {
        if (LookUpAction)
        {
            EnhancedInputComponent->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleLookUp);
        }
        if (TurnAction)
        {
            EnhancedInputComponent->BindAction(TurnAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleTurn);
        }
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Enhanced Input Actions bound in BeginPlay via InputComponent."));
    }
    else if (PlayerInput) // PlayerInput это UPlayerInput*, который создается PlayerController и содержит InputComponent
    {
        UEnhancedInputComponent* EnhancedInputComponentFromPlayerInput = Cast<UEnhancedInputComponent>(PlayerInput->GetOuter()); // Попытка получить через PlayerInput
        if (EnhancedInputComponentFromPlayerInput)
        {
            if (LookUpAction) EnhancedInputComponentFromPlayerInput->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleLookUp);
            if (TurnAction) EnhancedInputComponentFromPlayerInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleTurn);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Enhanced Input Actions bound in BeginPlay via PlayerInput."));
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("APokerPlayerController: Failed to get UEnhancedInputComponent for binding actions."));
        }
    }


    // Подписка на делегат OfflineGameManager
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager)
        {
            OfflineManager->OnActionRequestedDelegate.AddDynamic(this, &APokerPlayerController::HandleActionRequested);
        }
    }
}

// SetupInputComponent больше не нужен, если мы делаем привязку в BeginPlay, ИЛИ если он не вызывается.
// Если он вызывается, то привязку лучше оставить там.
// Стандартно APlayerController вызывает SetupInputComponent. Оставим его для правильности.
void APokerPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::SetupInputComponent CALLED")); // Проверить, вызывается ли

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (LookUpAction)
        {
            EnhancedInputComponent->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleLookUp);
            UE_LOG(LogTemp, Log, TEXT("Bound LookUpAction"));
        }
        if (TurnAction)
        {
            EnhancedInputComponent->BindAction(TurnAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleTurn);
            UE_LOG(LogTemp, Log, TEXT("Bound TurnAction"));
        }
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("APokerPlayerController::SetupInputComponent - Failed to Cast to UEnhancedInputComponent."));
    }
}


void APokerPlayerController::HandleLookUp(const FInputActionValue& Value)
{
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

void APokerPlayerController::HandleActionRequested(int32 SeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack)
{
    if (GameHUDWidgetInstance)
    {
        if (GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            // Предполагаем, что SeatIndex 0 - это всегда локальный игрок в оффлайн режиме
            if (SeatIndex == 0)
            {
                IGameHUDInterface::Execute_UpdateActionButtonsAndPlayerTurn(GameHUDWidgetInstance, SeatIndex, AllowedActions, BetToCall, MinRaiseAmount, PlayerStack);
            }
            else
            {
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
            }
        }
    }
}

// Функции-обработчики действий (заглушки)
void APokerPlayerController::HandleFoldAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleFoldAction called by UI."));
    // ...
}

void APokerPlayerController::HandleCheckCallAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleCheckCallAction called by UI."));
    // ...
}

void APokerPlayerController::HandleBetRaiseAction(int64 Amount)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleBetRaiseAction called by UI with Amount: %lld"), Amount);
    // ...
}