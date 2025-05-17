#include "PokerPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "MyGameInstance.h"
#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "GameHUDInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"

APokerPlayerController::APokerPlayerController()
{
    PlayerInputMappingContext = nullptr;
    LookUpAction = nullptr;
    TurnAction = nullptr;
    ToggleToUIAction = nullptr;
    GameHUDClass = nullptr;
    GameHUDWidgetInstance = nullptr;
    bIsInUIMode = false;

    // Инициализируем TOptional переменные (они по умолчанию не установлены)
    OptMovingPlayerSeatIndex.Reset();
    OptMovingPlayerName.Reset();
    OptAllowedActions.Reset();
    OptBetToCall.Reset();
    OptMinRaiseAmount.Reset();
    OptMovingPlayerStack.Reset();
    OptCurrentPot.Reset();
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
            if (GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
            {
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
                UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Initial DisableButtons called on HUD."));
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: Failed to create GameHUDWidgetInstance from GameHUDClass.")); }
    }
    else if (IsLocalPlayerController()) { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDClass is not set in Blueprint Defaults. Cannot create HUD.")); }

    SwitchToGameInputMode();

    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager)
        {
            // Удаляем старую подписку, если она была
            // OfflineManager->OnActionRequestedDelegate.RemoveDynamic(this, &APokerPlayerController::HandleActionRequested); // Если была такая функция

            // Подписываемся на новые делегаты
            OfflineManager->OnPlayerTurnStartedDelegate.AddDynamic(this, &APokerPlayerController::HandlePlayerTurnStarted);
            OfflineManager->OnPlayerActionsAvailableDelegate.AddDynamic(this, &APokerPlayerController::HandlePlayerActionsAvailable);
            OfflineManager->OnTableStateInfoDelegate.AddDynamic(this, &APokerPlayerController::HandleTableStateInfo);
            OfflineManager->OnActionUIDetailsDelegate.AddDynamic(this, &APokerPlayerController::HandleActionUIDetails);
            OfflineManager->OnGameHistoryEventDelegate.AddDynamic(this, &APokerPlayerController::HandleGameHistoryEvent);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Subscribed to new OfflineManager delegates."));
        }
        else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: OfflineManager is null in GameInstance, cannot subscribe.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameInstance is not UMyGameInstance type.")); }
}

void APokerPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::SetupInputComponent CALLED"));
    if (UEnhancedInputComponent* EnhancedInputComp = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (LookUpAction) EnhancedInputComp->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleLookUp);
        if (TurnAction) EnhancedInputComp->BindAction(TurnAction, ETriggerEvent::Triggered, this, &APokerPlayerController::HandleTurn);
        if (ToggleToUIAction) EnhancedInputComp->BindAction(ToggleToUIAction, ETriggerEvent::Started, this, &APokerPlayerController::HandleToggleToUI);
    }
    else { UE_LOG(LogTemp, Error, TEXT("APokerPlayerController::SetupInputComponent - Failed to Cast to UEnhancedInputComponent.")); }
}

// --- Функции обработки ввода и режимов (остаются без изменений) ---
void APokerPlayerController::HandleLookUp(const FInputActionValue& Value)
{
    if (bIsInUIMode) return;
    const float LookAxisValue = Value.Get<float>();
    if (LookAxisValue != 0.0f) { if (APawn* const CP = GetPawn()) { CP->AddControllerPitchInput(LookAxisValue); } }
}

void APokerPlayerController::HandleTurn(const FInputActionValue& Value)
{
    if (bIsInUIMode) return;
    const float TurnAxisValue = Value.Get<float>();
    if (TurnAxisValue != 0.0f) { if (APawn* const CP = GetPawn()) { CP->AddControllerYawInput(TurnAxisValue); } }
}

void APokerPlayerController::HandleToggleToUI(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::HandleToggleToUI called. Current bIsInUIMode before toggle: %s"), bIsInUIMode ? TEXT("true") : TEXT("false"));
    if (!bIsInUIMode) { SwitchToUIInputMode(GameHUDWidgetInstance); }
}

void APokerPlayerController::SwitchToGameInputMode()
{
    FInputModeGameOnly InputModeData;
    SetInputMode(InputModeData);
    bShowMouseCursor = false;
    bIsInUIMode = false;
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        Subsystem->ClearAllMappings();
        if (PlayerInputMappingContext) { Subsystem->AddMappingContext(PlayerInputMappingContext, 0); }
        else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: PlayerInputMappingContext is NULL in SwitchToGameInputMode.")); }
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Switched to Game Input Mode. Cursor hidden."));
}

void APokerPlayerController::SwitchToUIInputMode(UUserWidget* WidgetToFocus)
{
    FInputModeUIOnly InputModeData;
    if (WidgetToFocus) { InputModeData.SetWidgetToFocus(WidgetToFocus->TakeWidget()); }
    InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputModeData);
    bShowMouseCursor = true;
    bIsInUIMode = true;
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (PlayerInputMappingContext) { Subsystem->RemoveMappingContext(PlayerInputMappingContext); }
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Switched to UI Input Mode. Cursor shown. WidgetToFocus: %s"), *GetNameSafe(WidgetToFocus));
}

// --- Новые обработчики делегатов и логика агрегации ---

void APokerPlayerController::HandlePlayerTurnStarted(int32 MovingPlayerSeatIndex)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandlePlayerTurnStarted: Seat %d"), MovingPlayerSeatIndex);
    OptMovingPlayerSeatIndex = MovingPlayerSeatIndex;
    // Сбрасываем другие опциональные значения, так как начался новый "запрос действия"
    OptMovingPlayerName.Reset();
    OptAllowedActions.Reset();
    OptBetToCall.Reset();
    OptMinRaiseAmount.Reset();
    OptMovingPlayerStack.Reset();
    OptCurrentPot.Reset();
    // TryAggregateAndTriggerHUDUpdate(); // Не вызываем, ждем остальные данные
}

void APokerPlayerController::HandlePlayerActionsAvailable(const TArray<EPlayerAction>& AllowedActions)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandlePlayerActionsAvailable: %d actions"), AllowedActions.Num());
    OptAllowedActions = AllowedActions;
    // TryAggregateAndTriggerHUDUpdate(); // Не вызываем, ждем остальные данные
}

void APokerPlayerController::HandleTableStateInfo(const FString& MovingPlayerName, int64 CurrentPot)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleTableStateInfo: Player '%s', Pot %lld"), *MovingPlayerName, CurrentPot);
    OptMovingPlayerName = MovingPlayerName;
    OptCurrentPot = CurrentPot;
    // TryAggregateAndTriggerHUDUpdate(); // Не вызываем, ждем остальные данные
}

void APokerPlayerController::HandleActionUIDetails(int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStackOfMovingPlayer)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionUIDetails: BetToCall %lld, MinRaise %lld, Stack %lld"), BetToCall, MinRaiseAmount, PlayerStackOfMovingPlayer);
    OptBetToCall = BetToCall;
    OptMinRaiseAmount = MinRaiseAmount;
    OptMovingPlayerStack = PlayerStackOfMovingPlayer;

    // Это последний из основных делегатов с данными для UI хода, теперь можно попытаться обновить HUD
    TryAggregateAndTriggerHUDUpdate();
}

void APokerPlayerController::TryAggregateAndTriggerHUDUpdate()
{
    // Проверяем, что все необходимые данные получены
    if (OptMovingPlayerSeatIndex.IsSet() &&
        OptMovingPlayerName.IsSet() &&
        OptAllowedActions.IsSet() &&
        OptBetToCall.IsSet() &&
        OptMinRaiseAmount.IsSet() &&
        OptMovingPlayerStack.IsSet() &&
        OptCurrentPot.IsSet())
    {
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate - All data aggregated. Updating HUD."));

        if (!GameHUDWidgetInstance || !GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate - HUD not valid or no IGameHUDInterface."));
            OptMovingPlayerSeatIndex.Reset(); // Сбрасываем, чтобы не вызывать повторно с теми же данными
            return;
        }

        // Получаем актуальный стек ЛОКАЛЬНОГО игрока (Seat 0)
        int64 ActualLocalPlayerStack = 0;
        const int32 LocalPlayerSeatIndex = 0;

        UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
        if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
        {
            UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
            if (GameState->Seats.IsValidIndex(LocalPlayerSeatIndex))
            {
                ActualLocalPlayerStack = GameState->Seats[LocalPlayerSeatIndex].Stack;
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate - Could not get GameState for LocalPlayerStack.")); }

        // 1. Обновляем основную информацию о ходе и банке
        IGameHUDInterface::Execute_UpdatePlayerTurnInfo(
            GameHUDWidgetInstance,
            OptMovingPlayerName.GetValue(),
            OptCurrentPot.GetValue(),
            OptBetToCall.GetValue(),
            OptMinRaiseAmount.GetValue(),
            ActualLocalPlayerStack // Стек локального игрока
        );

        // 2. Обновляем кнопки действий
        if (OptMovingPlayerSeatIndex.GetValue() == LocalPlayerSeatIndex) // Ход локального игрока
        {
            IGameHUDInterface::Execute_UpdateActionButtons(GameHUDWidgetInstance, OptAllowedActions.GetValue());

            // Показ карт локального игрока (если они были только что розданы на префлопе)
            // Эту логику можно вызывать и в другом месте, если карты обновляются независимо
            if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
            {
                UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
                if (GameState->GetCurrentGameStage() == EGameStage::Preflop && GameState->Seats.IsValidIndex(LocalPlayerSeatIndex))
                {
                    // Проверяем, что это первый вызов HandleActionRequested для префлопа (карты только что розданы)
                    // Можно добавить флаг в GameState или PlayerState, чтобы не вызывать это каждый раз
                    const FPlayerSeatData& LocalPlayerData = GameState->Seats[LocalPlayerSeatIndex];
                    if (LocalPlayerData.HoleCards.Num() == 2) { OnLocalPlayerCardsDealt_BP(LocalPlayerData.HoleCards); }
                }
            }
        }
        else // Ход другого игрока (бота)
        {
            IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
        }

        // 3. Обновляем ВСЕ PlayerSeatVisualizers через GameMode (как и раньше)
        AGameModeBase* CurrentGameModeBase = GetWorld()->GetAuthGameMode();
        if (CurrentGameModeBase)
        {
            FOutputDeviceNull Ar;
            CurrentGameModeBase->CallFunctionByNameWithArguments(TEXT("RefreshAllSeatVisualizersUI_Event"), Ar, nullptr, true);
        }

        // Сбрасываем TOptional переменные, чтобы следующее обновление HUD ожидало новых данных
        OptMovingPlayerSeatIndex.Reset();
        OptMovingPlayerName.Reset();
        OptAllowedActions.Reset();
        OptBetToCall.Reset();
        OptMinRaiseAmount.Reset();
        OptMovingPlayerStack.Reset();
        OptCurrentPot.Reset();
    }
    else
    {
        // Не все данные еще пришли, ждем следующего вызова делегата
        // UE_LOG(LogTemp, Verbose, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate - Not all data available yet."));
    }
}


void APokerPlayerController::HandleGameHistoryEvent(const FString& HistoryMessage)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleGameHistoryEvent received: %s"), *HistoryMessage);
    if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        IGameHUDInterface::Execute_AddGameHistoryMessage(GameHUDWidgetInstance, HistoryMessage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::HandleGameHistoryEvent - HUD not valid or no IGameHUDInterface to add message."));
    }
}

// --- Функции-обработчики действий игрока (HandleFoldAction и т.д. остаются такими же) ---
// ... (ваш существующий код для HandleFoldAction, HandleCheckCallAction, HandleBetRaiseAction, HandlePostBlindAction) ...
void APokerPlayerController::HandleFoldAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleFoldAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager && OfflineManager->GetGameState())
        {
            UOfflinePokerGameState* GameState = OfflineManager->GetGameState();
            // Проверяем, что это ход локального игрока (предполагаем, что локальный игрок всегда SeatIndex 0)
            // и что игра не в состоянии, когда действия не принимаются (хотя это больше забота OfflineManager)
            if (GameState->GetCurrentTurnSeatIndex() == 0)
            {
                // UI должен был быть обновлен так, что кнопка Fold активна только если Fold есть в AllowedActions.
                // Дополнительная проверка на AllowedActions здесь избыточна, если UI работает правильно.
                OfflineManager->ProcessPlayerAction(EPlayerAction::Fold, 0);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("HandleFoldAction: Not local player's (Seat 0) turn. CurrentTurnSeat: %d"), GameState->GetCurrentTurnSeatIndex());
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleFoldAction: OfflineManager or GameState is null.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("HandleFoldAction: GameInstance is null.")); }
}

void APokerPlayerController::HandleCheckCallAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleCheckCallAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager && OfflineManager->GetGameState())
        {
            UOfflinePokerGameState* GameState = OfflineManager->GetGameState();
            if (GameState->GetCurrentTurnSeatIndex() == 0) // Ход локального игрока
            {
                if (GameState->Seats.IsValidIndex(0))
                {
                    const FPlayerSeatData& PlayerSeat = GameState->Seats[0]; // Получаем данные локального игрока
                    EPlayerAction ActionToTake;

                    // Определяем, это Check или Call, на основе текущего состояния ставок
                    // Это должно совпадать с логикой, по которой UI активировал кнопку Check или Call
                    if (GameState->GetCurrentBetToCall() > PlayerSeat.CurrentBet)
                    {
                        ActionToTake = EPlayerAction::Call;
                    }
                    else
                    {
                        ActionToTake = EPlayerAction::Check;
                    }
                    // Сумма для Call будет вычислена в ProcessPlayerAction. Для Check сумма 0.
                    OfflineManager->ProcessPlayerAction(ActionToTake, 0);
                }
                else { UE_LOG(LogTemp, Error, TEXT("HandleCheckCallAction: Local player (Seat 0) data is invalid.")); }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("HandleCheckCallAction: Not local player's (Seat 0) turn. CurrentTurnSeat: %d"), GameState->GetCurrentTurnSeatIndex());
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleCheckCallAction: OfflineManager or GameState is null.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("HandleCheckCallAction: GameInstance is null.")); }
}

void APokerPlayerController::HandleBetRaiseAction(int64 Amount)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleBetRaiseAction called by UI with Amount: %lld"), Amount);
    if (Amount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: Amount must be positive. Action not sent."));
        // TODO: Возможно, уведомить HUD об ошибке ввода
        return;
    }

    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager && OfflineManager->GetGameState())
        {
            UOfflinePokerGameState* GameState = OfflineManager->GetGameState();
            if (GameState->GetCurrentTurnSeatIndex() == 0) // Ход локального игрока
            {
                if (GameState->Seats.IsValidIndex(0))
                {
                    const FPlayerSeatData& PlayerSeat = GameState->Seats[0];
                    EPlayerAction ActionToTake;

                    // Определяем, это Bet или Raise
                    // UI должен был активировать кнопку "Bet" или "Raise" на основе AllowedActions
                    // Здесь мы можем сделать более простую проверку для отправки правильного типа действия
                    if (GameState->GetCurrentBetToCall() > 0 || PlayerSeat.CurrentBet > 0) // Если уже есть ставка в этом раунде (от нас или от других) или есть что коллировать
                    {
                        ActionToTake = EPlayerAction::Raise;
                    }
                    else
                    {
                        ActionToTake = EPlayerAction::Bet;
                    }
                    // Валидация суммы Amount (минимальный бет/рейз, не больше стека) должна быть в ProcessPlayerAction.
                    OfflineManager->ProcessPlayerAction(ActionToTake, Amount);
                }
                else { UE_LOG(LogTemp, Error, TEXT("HandleBetRaiseAction: Local player (Seat 0) data is invalid.")); }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: Not local player's (Seat 0) turn. CurrentTurnSeat: %d"), GameState->GetCurrentTurnSeatIndex());
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: OfflineManager or GameState is null.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: GameInstance is null.")); }
}

void APokerPlayerController::HandlePostBlindAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandlePostBlindAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager && OfflineManager->GetGameState())
        {
            UOfflinePokerGameState* GameState = OfflineManager->GetGameState();
            int32 CurrentTurnSeat = GameState->GetCurrentTurnSeatIndex();

            // Проверяем, что это ход локального игрока (0) и что состояние игры/игрока корректно для постановки блайнда
            if (CurrentTurnSeat == 0 && GameState->Seats.IsValidIndex(CurrentTurnSeat))
            {
                const FPlayerSeatData& CurrentPlayerData = GameState->Seats[CurrentTurnSeat];
                if ((GameState->GetCurrentGameStage() == EGameStage::WaitingForSmallBlind && CurrentPlayerData.Status == EPlayerStatus::MustPostSmallBlind) ||
                    (GameState->GetCurrentGameStage() == EGameStage::WaitingForBigBlind && CurrentPlayerData.Status == EPlayerStatus::MustPostBigBlind))
                {
                    OfflineManager->ProcessPlayerAction(EPlayerAction::PostBlind, 0);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("HandlePostBlindAction: Invalid state for posting blind. CurrentTurn: %d, Stage: %s, Status: %s"),
                        CurrentTurnSeat, *UEnum::GetValueAsString(GameState->GetCurrentGameStage()), *UEnum::GetValueAsString(CurrentPlayerData.Status));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("HandlePostBlindAction: Not local player's (Seat 0) turn or invalid seat index. CurrentTurnSeat: %d"), CurrentTurnSeat);
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandlePostBlindAction: OfflineManager or GameState is null.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("HandlePostBlindAction: GameInstance is null.")); }
}