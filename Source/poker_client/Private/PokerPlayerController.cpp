#include "PokerPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h" // Не используется напрямую, но может быть полезен для контекста
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "MyGameInstance.h"
#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "GameHUDInterface.h"
#include "PlayerSeatVisualizerInterface.h" // Включаем интерфейс для мест
#include "CommunityCardDisplayInterface.h"
#include "Kismet/GameplayStatics.h"   // Для GetAllActorsWithInterface и GetActorOfClass
#include "Misc/OutputDeviceNull.h"    // Для CallFunctionByNameWithArguments (если бы использовали)

APokerPlayerController::APokerPlayerController()
{
    PlayerInputMappingContext = nullptr;
    LookUpAction = nullptr;
    TurnAction = nullptr;
    ToggleToUIAction = nullptr;
    GameHUDClass = nullptr;
    GameHUDWidgetInstance = nullptr;
    CommunityCardDisplayActor = nullptr;
    bIsInUIMode = false;

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
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: GameHUDWidgetInstance created."));
            if (GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
            {
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance.Get());
                UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Initial DisableButtons called on HUD."));
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: Failed to create GameHUDWidgetInstance.")); }
    }
    else if (IsLocalPlayerController()) { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDClass not set.")); }

    // Находим актора для отображения общих карт
    // Рекомендуется искать по конкретному классу вашего BP_CommunityCardArea, если он есть,
    // или если он один, можно оставить поиск по AActor и проверку интерфейса.
    // Для большей надежности, если у вас есть C++ базовый класс для BP_CommunityCardArea, используйте его.
    // AYourCommunityCardAreaBaseClass* FoundCCA = Cast<AYourCommunityCardAreaBaseClass>(UGameplayStatics::GetActorOfClass(GetWorld(), AYourCommunityCardAreaBaseClass::StaticClass()));
    // if (FoundCCA && FoundCCA->GetClass()->ImplementsInterface(UCommunityCardDisplayInterface::StaticClass())) { ... }
    // Пока оставим как было, но имейте в виду для улучшения:
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UCommunityCardDisplayInterface::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        CommunityCardDisplayActor = FoundActors[0]; // Берем первый найденный
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Found CommunityCardDisplayActor: %s via Interface Search"), *CommunityCardDisplayActor->GetName());
        ICommunityCardDisplayInterface::Execute_HideCommunityCards(CommunityCardDisplayActor.Get());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: CommunityCardDisplayActor implementing ICommunityCardDisplayInterface NOT FOUND."));
        CommunityCardDisplayActor = nullptr;
    }

    SwitchToGameInputMode();

    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager)
        {
            OfflineManager->OnPlayerTurnStartedDelegate.AddDynamic(this, &APokerPlayerController::HandlePlayerTurnStarted);
            OfflineManager->OnPlayerActionsAvailableDelegate.AddDynamic(this, &APokerPlayerController::HandlePlayerActionsAvailable);
            OfflineManager->OnTableStateInfoDelegate.AddDynamic(this, &APokerPlayerController::HandleTableStateInfo);
            OfflineManager->OnActionUIDetailsDelegate.AddDynamic(this, &APokerPlayerController::HandleActionUIDetails);
            OfflineManager->OnGameHistoryEventDelegate.AddDynamic(this, &APokerPlayerController::HandleGameHistoryEvent);
            OfflineManager->OnCommunityCardsUpdatedDelegate.AddDynamic(this, &APokerPlayerController::HandleCommunityCardsUpdated);
            OfflineManager->OnShowdownDelegate.AddDynamic(this, &APokerPlayerController::HandleShowdown);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Subscribed to all OfflineManager delegates."));
        }
        else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: OfflineManager is null.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameInstance is not UMyGameInstance.")); }
}

void APokerPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::SetupInputComponent CALLED"));
    if (UEnhancedInputComponent* EnhancedInputComp = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (LookUpAction) EnhancedInputComp->BindAction(LookUpAction.Get(), ETriggerEvent::Triggered, this, &APokerPlayerController::HandleLookUp);
        else { UE_LOG(LogTemp, Warning, TEXT("SetupInputComponent: LookUpAction is null.")); }

        if (TurnAction) EnhancedInputComp->BindAction(TurnAction.Get(), ETriggerEvent::Triggered, this, &APokerPlayerController::HandleTurn);
        else { UE_LOG(LogTemp, Warning, TEXT("SetupInputComponent: TurnAction is null.")); }

        if (ToggleToUIAction) EnhancedInputComp->BindAction(ToggleToUIAction.Get(), ETriggerEvent::Started, this, &APokerPlayerController::HandleToggleToUI);
        else { UE_LOG(LogTemp, Warning, TEXT("SetupInputComponent: ToggleToUIAction is null.")); }
    }
    else { UE_LOG(LogTemp, Error, TEXT("APokerPlayerController::SetupInputComponent - Failed to Cast to UEnhancedInputComponent.")); }
}

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
    if (!bIsInUIMode)
    {
        SwitchToUIInputMode(GameHUDWidgetInstance.Get());
    }
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
        if (PlayerInputMappingContext) { Subsystem->AddMappingContext(PlayerInputMappingContext.Get(), 0); }
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
        if (PlayerInputMappingContext) { Subsystem->RemoveMappingContext(PlayerInputMappingContext.Get()); }
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Switched to UI Input Mode. Cursor shown. WidgetToFocus: %s"), *GetNameSafe(WidgetToFocus));
}

UOfflinePokerGameState* APokerPlayerController::GetCurrentGameState() const
{
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager())
    {
        return GI->GetOfflineGameManager()->GetGameState();
    }
    return nullptr;
}

// --- Обработчики делегатов от OfflineGameManager ---
void APokerPlayerController::HandlePlayerTurnStarted(int32 MovingPlayerSeatIndex)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandlePlayerTurnStarted: Seat %d"), MovingPlayerSeatIndex);
    OptMovingPlayerSeatIndex.Reset();
    OptMovingPlayerName.Reset();
    OptAllowedActions.Reset();
    OptBetToCall.Reset();
    OptMinRaiseAmount.Reset();
    OptMovingPlayerStack.Reset();
    OptCurrentPot.Reset();
    OptMovingPlayerSeatIndex = MovingPlayerSeatIndex;
}

void APokerPlayerController::HandlePlayerActionsAvailable(const TArray<EPlayerAction>& AllowedActions)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandlePlayerActionsAvailable: %d actions"), AllowedActions.Num());
    OptAllowedActions = AllowedActions;
}

void APokerPlayerController::HandleTableStateInfo(const FString& MovingPlayerName, int64 CurrentPot)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleTableStateInfo: Player '%s', Pot %lld"), *MovingPlayerName, CurrentPot);
    OptMovingPlayerName = MovingPlayerName;
    OptCurrentPot = CurrentPot;
}

void APokerPlayerController::HandleActionUIDetails(int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStackOfMovingPlayer)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionUIDetails: BetToCall %lld, MinRaise %lld, Stack %lld"), BetToCall, MinRaiseAmount, PlayerStackOfMovingPlayer);
    OptBetToCall = BetToCall;
    OptMinRaiseAmount = MinRaiseAmount;
    OptMovingPlayerStack = PlayerStackOfMovingPlayer;
    TryAggregateAndTriggerHUDUpdate();
}

void APokerPlayerController::TryAggregateAndTriggerHUDUpdate()
{
    // Проверяем, что все необходимые TOptional данные были установлены
    if (OptMovingPlayerSeatIndex.IsSet() &&
        OptMovingPlayerName.IsSet() &&
        OptAllowedActions.IsSet() &&
        OptBetToCall.IsSet() &&
        OptMinRaiseAmount.IsSet() &&
        OptMovingPlayerStack.IsSet() &&
        OptCurrentPot.IsSet())
    {
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate - All data aggregated. Updating HUD and Seat Visualizers for Seat %d (%s)."),
            OptMovingPlayerSeatIndex.GetValue(), *OptMovingPlayerName.GetValue());

        // Проверяем валидность HUD и реализацию интерфейса
        if (!GameHUDWidgetInstance || !GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate - HUD not valid or does not implement IGameHUDInterface."));
            // Важно сбросить хотя бы один TOptional, чтобы предотвратить повторный вызов с невалидным HUD
            // Сброс всех происходит в HandlePlayerTurnStarted, но здесь можно для безопасности.
            OptMovingPlayerSeatIndex.Reset();
            return;
        }

        // 1. Обновляем основную информацию о ходе и банке в HUD
        // Эта информация важна всегда, независимо от того, чей ход.
        IGameHUDInterface::Execute_UpdatePlayerTurnInfo(
            GameHUDWidgetInstance.Get(),         // Target
            OptMovingPlayerName.GetValue(),      // Имя игрока, чей ход
            OptCurrentPot.GetValue(),            // Текущий банк
            OptBetToCall.GetValue(),             // Сумма для колла
            OptMinRaiseAmount.GetValue(),        // Минимальный рейз/бет
            OptMovingPlayerStack.GetValue()      // Стек игрока, чей ход
        );

        // 2. Обновляем кнопки действий для ТЕКУЩЕГО ХОДЯЩЕГО ИГРОКА (независимо от того, локальный он или нет - для ручного теста)
        // Передаем ему доступные действия.
        IGameHUDInterface::Execute_UpdateActionButtons(
            GameHUDWidgetInstance.Get(),         // Target
            OptAllowedActions.GetValue()         // Доступные действия для OptMovingPlayerSeatIndex
        );

        // 3. Управление режимом ввода: ВСЕГДА переключаемся в UI-режим, чтобы мы могли нажать кнопку за текущего игрока.
        // Локальный игрок или бот, которым мы управляем вручную, потребует взаимодействия с UI.
        if (!bIsInUIMode) // Переключаемся, только если еще не в UI режиме
        {
            SwitchToUIInputMode(GameHUDWidgetInstance.Get());
        }
        // else // Если мы уже в UI режиме, ничего не делаем с режимом ввода
        // {
        //    UE_LOG(LogTemp, Log, TEXT("TryAggregateAndTriggerHUDUpdate: Already in UI mode."));
        // }
        // Обратный переход в GameInputMode теперь будет происходить в HandleFoldAction, HandleCheckCallAction и т.д.
        // ПОСЛЕ того, как действие игрока (даже если это бот под нашим управлением) отправлено.

        // 4. Обновляем ВСЕ PlayerSeatVisualizers, чтобы отразить текущее состояние стеков, имен и т.д.
        UpdateAllSeatVisualizersFromGameState();

        // Сброс TOptional переменных теперь происходит в HandlePlayerTurnStarted,
        // что гарантирует, что TryAggregateAndTriggerHUDUpdate не сработает снова, пока не придет новый полный набор данных.
    }
    // else // Если не все данные собраны, ничего не делаем, ждем следующего делегата.
    // {
    //    UE_LOG(LogTemp, Verbose, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate - Not all optional data is set yet. Waiting..."));
    // }
}

void APokerPlayerController::UpdateAllSeatVisualizersFromGameState()
{
    UOfflinePokerGameState* GameState = GetCurrentGameState();
    if (!GameState)
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::UpdateAllSeatVisualizersFromGameState: GameState is null."));
        return;
    }

    TArray<AActor*> SeatVisualizerActors;
    // Ищем акторы, которые реализуют интерфейс
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UPlayerSeatVisualizerInterface::StaticClass(), SeatVisualizerActors);

    if (SeatVisualizerActors.Num() == 0 && GameState->GetNumSeats() > 0) // Добавил проверку на GetNumSeats > 0, чтобы не спамить лог если игроков нет
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::UpdateAllSeatVisualizersFromGameState: No actors found implementing IPlayerSeatVisualizerInterface, but GameState has seats."));
        // return; // Можно раскомментировать, если это критично, но лучше пусть HUD обновится
    }

    // Сначала можно скрыть все, чтобы потом показать только активные
    // Это полезно, если количество игроков может меняться от руки к руке (редко в простом покере)
    // или если вы хотите быть уверены, что старые данные неактивных мест не видны.
    // Для MVP можно и без этого, если BP_PokerGameMode корректно скрывает лишние при инициализации.
    /*
    for (AActor* VisualizerActor : SeatVisualizerActors)
    {
        if (VisualizerActor)
        {
            IPlayerSeatVisualizerInterface::Execute_SetSeatVisibility(VisualizerActor, false);
            IPlayerSeatVisualizerInterface::Execute_HideHoleCards(VisualizerActor);
        }
    }
    */

    const int32 LocalPlayerSeatIndex = 0;

    for (const FPlayerSeatData& SeatData : GameState->GetSeatsArray())
    {
        // Ищем визуализатор для текущего SeatData.SeatIndex
        AActor* FoundVisualizer = nullptr;
        for (AActor* VisualizerActor : SeatVisualizerActors)
        {
            if (VisualizerActor) // Добавил проверку на валидность VisualizerActor
            {
                if (IPlayerSeatVisualizerInterface::Execute_GetSeatIndexRepresentation(VisualizerActor) == SeatData.SeatIndex)
                {
                    FoundVisualizer = VisualizerActor;
                    break;
                }
            }
        }

        if (FoundVisualizer)
        {
            // Устанавливаем видимость в зависимости от того, "сидит" ли игрок за столом
            // (bIsSittingIn), и есть ли он вообще в GameStateData->Seats (что уже подразумевается циклом).
            // Логика скрытия неактивных мест уже должна быть в BP_PokerGameMode::BeginPlay при первоначальной настройке.
            // Здесь мы просто обновляем данные для тех, кто в игре.
            IPlayerSeatVisualizerInterface::Execute_SetSeatVisibility(FoundVisualizer, SeatData.bIsSittingIn && (SeatData.Status != EPlayerStatus::SittingOut && SeatData.Status != EPlayerStatus::Waiting));


            IPlayerSeatVisualizerInterface::Execute_UpdatePlayerInfo(FoundVisualizer, SeatData.PlayerName, SeatData.Stack);

            bool bShowFace = (SeatData.SeatIndex == LocalPlayerSeatIndex && SeatData.Status != EPlayerStatus::Folded);
            if (SeatData.HoleCards.Num() > 0 && SeatData.Status != EPlayerStatus::Folded)
            {
                IPlayerSeatVisualizerInterface::Execute_UpdateHoleCards(FoundVisualizer, SeatData.HoleCards, bShowFace);
            }
            else
            {
                IPlayerSeatVisualizerInterface::Execute_HideHoleCards(FoundVisualizer);
            }
        }
        // else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::UpdateAllSeatVisualizersFromGameState: No visualizer found for SeatIndex %d"), SeatData.SeatIndex); }
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Updated all seat visualizers via C++."));
}


void APokerPlayerController::HandleGameHistoryEvent(const FString& HistoryMessage)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleGameHistoryEvent received: %s"), *HistoryMessage);
    if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        IGameHUDInterface::Execute_AddGameHistoryMessage(GameHUDWidgetInstance.Get(), HistoryMessage);
    }
}

void APokerPlayerController::HandleCommunityCardsUpdated(const TArray<FCard>& CommunityCards)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleCommunityCardsUpdated received. Num cards: %d"), CommunityCards.Num());
    if (CommunityCardDisplayActor && CommunityCardDisplayActor->GetClass()->ImplementsInterface(UCommunityCardDisplayInterface::StaticClass()))
    {
        ICommunityCardDisplayInterface::Execute_UpdateCommunityCards(CommunityCardDisplayActor.Get(), CommunityCards);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: CommunityCardDisplayActor is null or does not implement interface."));
    }
    // После обновления общих карт, состояние игроков (стеки) обычно не меняется до следующего раунда ставок.
    // Но если вдруг ваша логика это предполагает, или просто для консистентности:
    // UpdateAllSeatVisualizersFromGameState(); // Это обновит стеки и имена, но карты игроков останутся те же.
}

void APokerPlayerController::HandleShowdown(const TArray<int32>& ShowdownPlayerSeatIndices)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleShowdown received for %d players."), ShowdownPlayerSeatIndices.Num());

    UOfflinePokerGameState* GameState = GetCurrentGameState();
    if (!GameState)
    {
        UE_LOG(LogTemp, Error, TEXT("HandleShowdown: GameState is null. Cannot proceed."));
        return;
    }

    if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        IGameHUDInterface::Execute_AddGameHistoryMessage(GameHUDWidgetInstance.Get(), TEXT("--- SHOWDOWN ---"));
        IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance.Get());
    }

    TArray<AActor*> AllSeatVisualizerActors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UPlayerSeatVisualizerInterface::StaticClass(), AllSeatVisualizerActors);

    for (int32 PlayerSeatIdxInShowdown : ShowdownPlayerSeatIndices)
    {
        if (GameState->Seats.IsValidIndex(PlayerSeatIdxInShowdown))
        {
            const FPlayerSeatData& PlayerData = GameState->Seats[PlayerSeatIdxInShowdown];
            AActor* FoundVisualizer = nullptr;
            for (AActor* VisualizerActor : AllSeatVisualizerActors)
            {
                if (VisualizerActor && IPlayerSeatVisualizerInterface::Execute_GetSeatIndexRepresentation(VisualizerActor) == PlayerSeatIdxInShowdown)
                {
                    FoundVisualizer = VisualizerActor;
                    break;
                }
            }

            if (FoundVisualizer)
            {
                // На шоудауне показываем карты лицом, если они есть и игрок не сфолдил (хотя в ShowdownPlayerSeatIndices обычно только активные)
                if (PlayerData.HoleCards.Num() > 0 && PlayerData.Status != EPlayerStatus::Folded)
                {
                    IPlayerSeatVisualizerInterface::Execute_UpdateHoleCards(FoundVisualizer, PlayerData.HoleCards, true);
                }
                else
                {
                    IPlayerSeatVisualizerInterface::Execute_HideHoleCards(FoundVisualizer);
                }
            }
        }
    }
    // Обновление стеков после AwardPot будет инициировано через следующий вызов TryAggregateAndTriggerHUDUpdate,
    // который, в свою очередь, вызовет UpdateAllSeatVisualizersFromGameState.
}

// --- Функции-обработчики действий игрока ---
void APokerPlayerController::HandleFoldAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleFoldAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
    {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        int32 ActingPlayerSeat = GameState->GetCurrentTurnSeatIndex();
        if (ActingPlayerSeat != -1)
        {
            GI->GetOfflineGameManager()->ProcessPlayerAction(ActingPlayerSeat, EPlayerAction::Fold, 0);
            if (ActingPlayerSeat == 0 && bIsInUIMode) SwitchToGameInputMode(); // Если локальный игрок сделал ход, возвращаем игровой режим
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleFoldAction: CurrentTurnSeat is -1.")); }
    }
    else { UE_LOG(LogTemp, Warning, TEXT("HandleFoldAction: Critical component is null.")); }
}

void APokerPlayerController::HandleCheckCallAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleCheckCallAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
    {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        int32 ActingPlayerSeat = GameState->GetCurrentTurnSeatIndex();
        if (ActingPlayerSeat != -1 && GameState->Seats.IsValidIndex(ActingPlayerSeat))
        {
            const FPlayerSeatData& PlayerSeat = GameState->Seats[ActingPlayerSeat];
            EPlayerAction ActionToTake = (GameState->GetCurrentBetToCall() > PlayerSeat.CurrentBet) ? EPlayerAction::Call : EPlayerAction::Check;
            GI->GetOfflineGameManager()->ProcessPlayerAction(ActingPlayerSeat, ActionToTake, 0); // Сумма для Call вычисляется в ProcessPlayerAction
            if (ActingPlayerSeat == 0 && bIsInUIMode) SwitchToGameInputMode();
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleCheckCallAction: CurrentTurnSeat %d is invalid or GameState/OfflineManager null."), ActingPlayerSeat); }
    }
}

void APokerPlayerController::HandleBetRaiseAction(int64 Amount)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleBetRaiseAction called by UI with Amount: %lld"), Amount);
    if (Amount <= 0) { UE_LOG(LogTemp, Warning, TEXT("Bet/Raise Amount should be positive.")); return; }

    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
    {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        int32 ActingPlayerSeat = GameState->GetCurrentTurnSeatIndex();
        if (ActingPlayerSeat != -1 && GameState->Seats.IsValidIndex(ActingPlayerSeat))
        {
            const FPlayerSeatData& PlayerSeat = GameState->Seats[ActingPlayerSeat];
            // Определяем, Bet это или Raise. Если CurrentBetToCall == 0 ИЛИ PlayerSeat.CurrentBet == GameState->CurrentBetToCall, то это Bet.
            // Иначе это Raise.
            EPlayerAction ActionToTake = (GameState->GetCurrentBetToCall() == 0 || PlayerSeat.CurrentBet == GameState->GetCurrentBetToCall()) ? EPlayerAction::Bet : EPlayerAction::Raise;
            GI->GetOfflineGameManager()->ProcessPlayerAction(ActingPlayerSeat, ActionToTake, Amount);
            if (ActingPlayerSeat == 0 && bIsInUIMode) SwitchToGameInputMode();
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: CurrentTurnSeat %d is invalid or GameState/OfflineManager null."), ActingPlayerSeat); }
    }
}

void APokerPlayerController::HandlePostBlindAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandlePostBlindAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
    {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        int32 ActingPlayerSeat = GameState->GetCurrentTurnSeatIndex();
        if (ActingPlayerSeat != -1 && GameState->Seats.IsValidIndex(ActingPlayerSeat))
        {
            const FPlayerSeatData& CurrentPlayerData = GameState->Seats[ActingPlayerSeat];
            if ((GameState->GetCurrentGameStage() == EGameStage::WaitingForSmallBlind && CurrentPlayerData.Status == EPlayerStatus::MustPostSmallBlind) ||
                (GameState->GetCurrentGameStage() == EGameStage::WaitingForBigBlind && CurrentPlayerData.Status == EPlayerStatus::MustPostBigBlind))
            {
                GI->GetOfflineGameManager()->ProcessPlayerAction(ActingPlayerSeat, EPlayerAction::PostBlind, 0);
                // SwitchToGameInputMode() здесь НЕ нужен, так как ход сразу перейдет к следующему
                // и TryAggregateAndTriggerHUDUpdate сам решит, переключать ли режим.
            }
            else {
                UE_LOG(LogTemp, Warning, TEXT("HandlePostBlindAction: Invalid state for posting blind. Current Stage: %s, Player Status: %s"),
                    *UEnum::GetValueAsString(GameState->GetCurrentGameStage()),
                    *UEnum::GetValueAsString(CurrentPlayerData.Status));
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandlePostBlindAction: CurrentTurnSeat %d is invalid."), ActingPlayerSeat); }
    }
}