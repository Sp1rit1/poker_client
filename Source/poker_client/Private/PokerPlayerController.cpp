#include "PokerPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "MyGameInstance.h"        // Замените, если имя вашего GameInstance другое
#include "OfflineGameManager.h"    // Для подписки на делегат
#include "OfflinePokerGameState.h" // Для доступа к GameState через OfflineManager
#include "GameHUDInterface.h"      // Ваш C++ интерфейс HUD
#include "Kismet/GameplayStatics.h"

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
                // Начальная деактивация кнопок и, возможно, инициализация "пустого" состояния HUD
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
                // Вызов InitializePotDisplay здесь может быть преждевременным, если банк еще 0.
                // Лучше это сделать в BP_PokerGameMode после InitializeGame.
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
    SwitchToGameInputMode();

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
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: OfflineManager is null in GameInstance, cannot subscribe."));
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
        if (ToggleToUIAction)
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
    if (bIsInUIMode) return;

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
    if (bIsInUIMode) return;

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
    // Эта функция вызывается, когда мы В ИГРОВОМ РЕЖИМЕ и хотим перейти в UI
    if (!bIsInUIMode)
    {
        SwitchToUIInputMode(GameHUDWidgetInstance);
    }
    // Если мы уже в UI режиме, то эта привязка из IMC_PlayerControls не должна была сработать,
    // так как этот IMC удаляется. Переключение обратно из UI в игру должно обрабатываться
    // внутри WBP_GameHUD (например, по OnKeyDown для Escape или Tab).
}

void APokerPlayerController::SwitchToGameInputMode()
{
    FInputModeGameOnly InputModeData;
    SetInputMode(InputModeData);
    bShowMouseCursor = false;
    bIsInUIMode = false;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        Subsystem->ClearAllMappings(); // Очищаем все, чтобы избежать наложения с UI контекстом
        if (PlayerInputMappingContext) // PlayerInputMappingContext - это ваш IMC_PlayerControls
        {
            Subsystem->AddMappingContext(PlayerInputMappingContext, 0); // Добавляем игровой контекст
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
    FInputModeUIOnly InputModeData; // Используем UIOnly, так как в UI режиме не должно быть игрового ввода для осмотра
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
        // Удаляем игровой контекст, чтобы действия из него (осмотр) не работали в UI режиме
        if (PlayerInputMappingContext)
        {
            Subsystem->RemoveMappingContext(PlayerInputMappingContext);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: PlayerInputMappingContext REMOVED."));
        }
        // Если у вас есть отдельный IMC для UI (например, IMC_UI_Interactions, содержащий Tab/Esc для возврата в игру),
        // то его нужно добавить здесь:
        // if (UIMappingContext) { Subsystem->AddMappingContext(UIMappingContext, 1); } // с более высоким приоритетом
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Switched to UI Input Mode. Cursor shown. WidgetToFocus: %s"), *GetNameSafe(WidgetToFocus));
}

void APokerPlayerController::HandleActionRequested(int32 SeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack, int64 CurrentPot)
{
    // Логирование полученных данных (очень полезно для отладки)
    UE_LOG(LogTemp, Log,
        TEXT("APokerPlayerController::HandleActionRequested: SeatIndex=%d, Pot=%lld, Stack=%lld, BetToCall=%lld, MinRaise=%lld"),
        SeatIndex, CurrentPot, PlayerStack, BetToCall, MinRaiseAmount);

    FString ActionsString = TEXT("Allowed Actions: ");
    for (EPlayerAction Action : AllowedActions)
    {
        UEnum* EnumPtr = StaticEnum<EPlayerAction>();
        if (EnumPtr)
        {
            ActionsString += EnumPtr->GetNameStringByValue(static_cast<int64>(Action)) + TEXT(", ");
        }
    }
    UE_LOG(LogTemp, Log, TEXT("%s"), *ActionsString);


    if (!GameHUDWidgetInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::HandleActionRequested - GameHUDWidgetInstance is null. Cannot update HUD."));
        return;
    }

    // Проверяем, реализует ли наш HUD нужный интерфейс
    if (!GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass())) // Замените UGameHUDInterface на ваше имя C++ класса интерфейса
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::HandleActionRequested - GameHUDWidgetInstance (%s) does not implement IGameHUDInterface!"), *GetNameSafe(GameHUDWidgetInstance));
        return;
    }

    // 1. Обновляем основную информацию о ходе и состоянии стола в HUD.
    // Эта информация отображается всегда, независимо от того, чей ход (локального игрока или бота).
    IGameHUDInterface::Execute_UpdatePlayerTurnInfo( // Замените IGameHUDInterface и UpdatePlayerTurnInfo на ваши точные имена
        GameHUDWidgetInstance,    // Target
        SeatIndex,                // ForPlayerSeatIndex (чей сейчас ход по данным от GameManager)
        CurrentPot,               // CurrentPot
        BetToCall,                // CurrentBetToCall
        MinRaiseAmount,           // MinimumRaise
        PlayerStack               // PlayerStack (стек игрока, чей сейчас ход)
    );

    // 2. Обновляем состояние кнопок действий в зависимости от того, чей ход.
    // Предполагаем, что локальный игрок всегда имеет SeatIndex 0 в оффлайн-режиме.
    // Вам может понадобиться более сложная логика для определения, является ли SeatIndex ходом локального игрока,
    // особенно если вы планируете мультиплеер или возможность играть за разные места.
    // Например, можно хранить LocalPlayerSeatIndex в PlayerController.
    const int32 LocalPlayerSeatIndex = 0; // Пока что жестко задаем для оффлайна

    if (SeatIndex == LocalPlayerSeatIndex)
    {
        // Это ход локального игрока. Обновляем кнопки доступными действиями.
        IGameHUDInterface::Execute_UpdateActionButtons( // Замените IGameHUDInterface и UpdateActionButtons на ваши точные имена
            GameHUDWidgetInstance,    // Target
            AllowedActions            // AllowedActions (массив действий, которые может совершить локальный игрок)
        );

    }
    else
    {
        // Это ход другого игрока (бота). Деактивируем все кнопки действий для локального игрока.
        IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance); // Замените IGameHUDInterface и DisableButtons на ваши точные имена
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionRequested - Not local player's turn (Seat %d). Disabling buttons for local player."), SeatIndex);

    }
}

// Функции-обработчики действий (вызываются из WBP_GameHUD)
void APokerPlayerController::HandleFoldAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleFoldAction called by UI."));
    if (bIsInUIMode) // Действие должно переключать обратно в игровой режим
    {
        SwitchToGameInputMode();
    }
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState()) { // Изменено на GetGameState()
        int32 CurrentTurnSeat = GI->GetOfflineGameManager()->GetGameState()->GetCurrentTurnSeatIndex(); // Используем геттер
        if (CurrentTurnSeat == 0) { // Убедимся, что это ход локального игрока
            GI->GetOfflineGameManager()->ProcessPlayerAction(EPlayerAction::Fold, 0);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleFoldAction: Not local player's turn!")); }
    }
}

void APokerPlayerController::HandleCheckCallAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleCheckCallAction called by UI."));
    if (bIsInUIMode)
    {
        SwitchToGameInputMode();
    }
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState()) {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        int32 CurrentTurnSeat = GameState->GetCurrentTurnSeatIndex();
        if (CurrentTurnSeat == 0) { // Ход локального игрока
            FPlayerSeatData PlayerSeat = GameState->GetSeatData(CurrentTurnSeat);
            EPlayerAction ActionToTake = EPlayerAction::Check; // По умолчанию
            int64 AmountToCall = 0;

            if (GameState->GetCurrentBetToCall() > PlayerSeat.CurrentBet) {
                ActionToTake = EPlayerAction::Call;
                AmountToCall = FMath::Min(PlayerSeat.Stack, GameState->GetCurrentBetToCall() - PlayerSeat.CurrentBet);
            }
            // ProcessPlayerAction должна сама вычислить сумму для колла, если мы передаем EPlayerAction::Call и Amount = 0.
            // Либо мы передаем точную сумму, которую игрок должен доставить.
            // Для простоты, если это Call, ProcessPlayerAction должен будет вычислить сумму.
            // Сейчас ProcessPlayerAction не принимает Amount для Call.
            // Давайте пока для Call будем передавать 0, а ProcessPlayerAction разберется.
            GI->GetOfflineGameManager()->ProcessPlayerAction(ActionToTake, 0); // Amount 0 для Check/Call
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleCheckCallAction: Not local player's turn!")); }
    }
}

void APokerPlayerController::HandleBetRaiseAction(int64 Amount)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleBetRaiseAction called by UI with Amount: %lld"), Amount);
    if (Amount <= 0) { UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: Amount is not positive.")); return; }

    if (bIsInUIMode)
    {
        SwitchToGameInputMode();
    }
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState()) {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        int32 CurrentTurnSeat = GameState->GetCurrentTurnSeatIndex();
        if (CurrentTurnSeat == 0) { // Ход локального игрока
            FPlayerSeatData PlayerSeat = GameState->GetSeatData(CurrentTurnSeat);
            EPlayerAction ActionToTake = EPlayerAction::Bet; // По умолчанию

            if (GameState->GetCurrentBetToCall() > PlayerSeat.CurrentBet) { // Если уже есть ставка для колла, то это Raise
                ActionToTake = EPlayerAction::Raise;
            }
            // TODO: Валидация суммы ставки (Amount) относительно MinRaiseAmount, стека игрока и т.д.
            // Эту валидацию лучше делать в ProcessPlayerAction.
            GI->GetOfflineGameManager()->ProcessPlayerAction(ActionToTake, Amount);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: Not local player's turn!")); }
    }
}