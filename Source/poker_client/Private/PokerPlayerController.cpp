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
            OfflineManager->OnActualHoleCardsDealtDelegate.AddDynamic(this, &APokerPlayerController::HandleActualHoleCardsDealt);
            OfflineManager->OnNewHandAboutToStartDelegate.AddDynamic(this, &APokerPlayerController::HandleNewHandAboutToStart);
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
    if (OptMovingPlayerSeatIndex.IsSet() &&
        OptMovingPlayerName.IsSet() &&
        OptAllowedActions.IsSet() &&
        OptBetToCall.IsSet() &&
        OptMinRaiseAmount.IsSet() &&
        OptMovingPlayerStack.IsSet() &&
        OptCurrentPot.IsSet())
    {
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate for Seat %d (%s)."),
            OptMovingPlayerSeatIndex.GetValue(), *OptMovingPlayerName.GetValue());

        if (!GameHUDWidgetInstance || !GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            UE_LOG(LogTemp, Warning, TEXT("TryAggregateAndTriggerHUDUpdate: HUD not valid or no IGameHUDInterface."));
            OptMovingPlayerSeatIndex.Reset(); // Предотвратить повторный вызов с теми же данными
            return;
        }

        IGameHUDInterface::Execute_UpdatePlayerTurnInfo(
            GameHUDWidgetInstance.Get(),
            OptMovingPlayerName.GetValue(),
            OptCurrentPot.GetValue(),
            OptBetToCall.GetValue(),
            OptMinRaiseAmount.GetValue(),
            OptMovingPlayerStack.GetValue()
        );

        IGameHUDInterface::Execute_UpdateActionButtons(
            GameHUDWidgetInstance.Get(),
            OptAllowedActions.GetValue()
        );

        // ОБНОВЛЯЕМ ВИЗУАЛИЗАТОРЫ МЕСТ ЗДЕСЬ, ЧТОБЫ ОНИ ОТОБРАЖАЛИ АКТУАЛЬНЫЕ СТЕКИ ПОСЛЕ ДЕЙСТВИЙ (например, блайндов)
        UpdateAllSeatVisualizersFromGameState();

        // Переключение в UI режим, чтобы игрок (или мы за бота) мог нажать кнопку
        if (!bIsInUIMode)
        {
            SwitchToUIInputMode(GameHUDWidgetInstance.Get());
        }
        // Сброс TOptional переменных теперь происходит в HandlePlayerTurnStarted.
    }
}

void APokerPlayerController::UpdateAllSeatVisualizersFromGameState()
{
    UOfflinePokerGameState* GameState = GetCurrentGameState();
    if (!GameState)
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateAllSeatVisualizersFromGameState: GameState is NULL. Cannot update visuals."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::UpdateAllSeatVisualizersFromGameState - Updating PlayerInfo for %d seats and checking for folded cards."), GameState->GetSeatsArray().Num());

    TArray<AActor*> AllSeatVisualizerActorsOnLevel;
    // Получаем все акторы на уровне, которые реализуют наш интерфейс визуализатора места
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UPlayerSeatVisualizerInterface::StaticClass(), AllSeatVisualizerActorsOnLevel);

    // Проходим по каждому игроку, который активен в текущей раздаче (присутствует в GameState->Seats)
    for (const FPlayerSeatData& SeatData : GameState->GetSeatsArray())
    {
        AActor* FoundVisualizer = nullptr;
        // Ищем на сцене визуализатор, соответствующий текущему SeatData по его SeatIndexRepresentation
        for (AActor* VisualizerActor : AllSeatVisualizerActorsOnLevel)
        {
            if (VisualizerActor && VisualizerActor->GetClass()->ImplementsInterface(UPlayerSeatVisualizerInterface::StaticClass()))
            {
                // Вызываем интерфейсную функцию GetSeatIndexRepresentation, чтобы сравнить
                if (IPlayerSeatVisualizerInterface::Execute_GetSeatIndexRepresentation(VisualizerActor) == SeatData.SeatIndex)
                {
                    FoundVisualizer = VisualizerActor;
                    break; // Нашли нужный визуализатор, выходим из внутреннего цикла
                }
            }
        }

        if (FoundVisualizer)
        {
            // ЗАДАЧА 1: Вызвать UpdatePlayerInfo для всех активных игроков
            // Мы предполагаем, что если игрок есть в GameState->Seats, то его визуализатор должен быть видим
            // и его информация (имя, стек) должна быть обновлена.
            // Видимость самого актора BP_PlayerSeatVisualizer устанавливается один раз в BP_OfflineGameMode.
            UE_LOG(LogTemp, Verbose, TEXT("   Updating PlayerInfo for Seat %d (%s): Stack %lld"), SeatData.SeatIndex, *SeatData.PlayerName, SeatData.Stack);
            IPlayerSeatVisualizerInterface::Execute_UpdatePlayerInfo(FoundVisualizer, SeatData.PlayerName, SeatData.Stack);

            // ЗАДАЧА 2: Скрыть карманные карты тех, кто сфолдил
            if (SeatData.Status == EPlayerStatus::Folded)
            {
                UE_LOG(LogTemp, Verbose, TEXT("   Seat %d (%s) is FOLDED. Hiding their hole cards."), SeatData.SeatIndex, *SeatData.PlayerName);
                IPlayerSeatVisualizerInterface::Execute_HideHoleCards(FoundVisualizer);
            }
            // Важно: Эта функция НЕ должна отвечать за ПОКАЗ карт (UpdateHoleCards).
            // Показ карт (лицом для локального, рубашкой для других) происходит ОДИН РАЗ
            // в APokerPlayerController::HandleActualHoleCardsDealt() после их раздачи.
            // На шоудауне будет отдельная логика показа карт.
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("UpdateAllSeatVisualizersFromGameState: Visualizer for SeatIndex %d (%s) NOT FOUND on level."), SeatData.SeatIndex, *SeatData.PlayerName);
        }
    }
}

void APokerPlayerController::HandleActualHoleCardsDealt()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActualHoleCardsDealt CALLED."));
    UOfflinePokerGameState* GameState = GetCurrentGameState();
    if (!GameState)
    {
        UE_LOG(LogTemp, Error, TEXT("HandleActualHoleCardsDealt: GameState is NULL."));
        return;
    }

    TArray<AActor*> SeatVisualizerActors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UPlayerSeatVisualizerInterface::StaticClass(), SeatVisualizerActors);
    UE_LOG(LogTemp, Log, TEXT("HandleActualHoleCardsDealt: Found %d visualizers. Processing %d seats in GameState."), SeatVisualizerActors.Num(), GameState->GetSeatsArray().Num());

    const int32 LocalPlayerSeatIndex = 0; // Предположение для оффлайн игры

    for (const FPlayerSeatData& SeatData : GameState->GetSeatsArray())
    {
        AActor* FoundVisualizer = nullptr;
        for (AActor* VisualizerActor : SeatVisualizerActors)
        {
            if (VisualizerActor && VisualizerActor->GetClass()->ImplementsInterface(UPlayerSeatVisualizerInterface::StaticClass()) &&
                IPlayerSeatVisualizerInterface::Execute_GetSeatIndexRepresentation(VisualizerActor) == SeatData.SeatIndex)
            {
                FoundVisualizer = VisualizerActor;
                break;
            }
        }

        if (FoundVisualizer)
        {
            UE_LOG(LogTemp, Log, TEXT("HandleActualHoleCardsDealt: Processing Seat %d (%s). Status: %s. HoleCards.Num: %d"),
                SeatData.SeatIndex, *SeatData.PlayerName, *UEnum::GetValueAsString(SeatData.Status), SeatData.HoleCards.Num());

            // Карты показываем или скрываем в зависимости от статуса и количества карт
            // Игрок должен быть в игре (не Folded, не SittingOut), и у него должно быть 2 карты
            if (SeatData.bIsSittingIn &&
                SeatData.Status != EPlayerStatus::Folded &&
                SeatData.Status != EPlayerStatus::SittingOut &&
                SeatData.Status != EPlayerStatus::Waiting && // Waiting обычно до раздачи карт
                SeatData.HoleCards.Num() == 2)
            {
                bool bShowFace = (SeatData.SeatIndex == LocalPlayerSeatIndex);
                UE_LOG(LogTemp, Log, TEXT("   -> Updating hole cards for Seat %d. ShowFace: %s"), SeatData.SeatIndex, bShowFace ? TEXT("true") : TEXT("false"));
                IPlayerSeatVisualizerInterface::Execute_UpdateHoleCards(FoundVisualizer, SeatData.HoleCards, bShowFace);
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("   -> Hiding hole cards for Seat %d. (Status: %s, NumCards: %d, SittingIn: %d)"),
                    SeatData.SeatIndex, *UEnum::GetValueAsString(SeatData.Status), SeatData.HoleCards.Num(), SeatData.bIsSittingIn);
                IPlayerSeatVisualizerInterface::Execute_HideHoleCards(FoundVisualizer);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("HandleActualHoleCardsDealt: Visualizer for Seat %d NOT FOUND."), SeatData.SeatIndex);
        }
    }
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

void APokerPlayerController::HandleNewHandAboutToStart()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleNewHandAboutToStart received. Hiding all hole cards."));
    TArray<AActor*> SeatVisualizerActors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UPlayerSeatVisualizerInterface::StaticClass(), SeatVisualizerActors);
    for (AActor* VisualizerActor : SeatVisualizerActors)
    {
        if (VisualizerActor && VisualizerActor->GetClass()->ImplementsInterface(UPlayerSeatVisualizerInterface::StaticClass()))
        {
            IPlayerSeatVisualizerInterface::Execute_HideHoleCards(VisualizerActor);
        }
    }
    // Также здесь можно скрыть общие карты, если они не скрываются в другом месте
    if (CommunityCardDisplayActor && CommunityCardDisplayActor->GetClass()->ImplementsInterface(UCommunityCardDisplayInterface::StaticClass()))
    {
        ICommunityCardDisplayInterface::Execute_HideCommunityCards(CommunityCardDisplayActor.Get());
    }
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
            UpdateAllSeatVisualizersFromGameState();
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