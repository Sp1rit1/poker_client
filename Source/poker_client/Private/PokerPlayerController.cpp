#include "PokerPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h" 
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "PokerDataTypes.h"
#include "MyGameInstance.h"
#include "OfflineGameManager.h"
#include "OfflinePokerGameState.h"
#include "GameHUDInterface.h"
#include "LevelTransitionManager.h"
#include "PlayerSeatVisualizerInterface.h" 
#include "CommunityCardDisplayInterface.h"
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
            OfflineManager->OnShowdownResultsDelegate.AddDynamic(this, &APokerPlayerController::HandleShowdownResults);
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

void APokerPlayerController::HandlePlayerTurnStarted(int32 MovingPlayerSeatIndex)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandlePlayerTurnStarted: Seat %d. Current UI Mode: %s"),
        MovingPlayerSeatIndex, bIsInUIMode ? TEXT("UI Mode") : TEXT("Game Mode"));

    OptMovingPlayerSeatIndex.Reset(); OptMovingPlayerName.Reset(); OptAllowedActions.Reset();
    OptBetToCall.Reset(); OptMinRaiseAmount.Reset(); OptMovingPlayerStack.Reset();
    OptCurrentPot.Reset(); OptMovingPlayerCurrentBet.Reset();

    OptMovingPlayerSeatIndex = MovingPlayerSeatIndex;
}

void APokerPlayerController::HandlePlayerActionsAvailable(const TArray<EPlayerAction>& AllowedActions)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandlePlayerActionsAvailable: Received %d actions."), AllowedActions.Num());
    OptAllowedActions = AllowedActions;
}

void APokerPlayerController::HandleTableStateInfo(const FString& MovingPlayerName, int64 CurrentPot)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleTableStateInfo: Player '%s', Pot %lld"), *MovingPlayerName, CurrentPot);
    OptMovingPlayerName = MovingPlayerName;
    OptCurrentPot = CurrentPot;
}

void APokerPlayerController::HandleActionUIDetails(int64 ActualAmountToCall, int64 MinPureRaise, int64 PlayerStackOfMovingPlayer, int64 CurrentBetOfMovingPlayer)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionUIDetails: ActualToCall %lld, MinPureRaise %lld, Stack %lld, CurrentBet %lld"),
        ActualAmountToCall, MinPureRaise, PlayerStackOfMovingPlayer, CurrentBetOfMovingPlayer);

    OptBetToCall = ActualAmountToCall;
    OptMinRaiseAmount = MinPureRaise;
    OptMovingPlayerStack = PlayerStackOfMovingPlayer;
    OptMovingPlayerCurrentBet = CurrentBetOfMovingPlayer;

    TryAggregateAndTriggerHUDUpdate();
}

void APokerPlayerController::TryAggregateAndTriggerHUDUpdate()
{
    // 1. Проверяем, что все необходимые данные от делегатов были получены
    if (!(OptMovingPlayerSeatIndex.IsSet() &&
        OptMovingPlayerName.IsSet() &&
        OptAllowedActions.IsSet() &&
        OptBetToCall.IsSet() &&             
        OptMinRaiseAmount.IsSet() &&        
        OptMovingPlayerStack.IsSet() &&     
        OptCurrentPot.IsSet() &&
        OptMovingPlayerCurrentBet.IsSet())) 
    {
        FString MissingOpts = TEXT("TryAggregateAndTriggerHUDUpdate: Not all optional values from delegates are set yet. Missing: ");
        if (!OptMovingPlayerSeatIndex.IsSet()) MissingOpts += TEXT("SeatIdx ");
        if (!OptMovingPlayerName.IsSet()) MissingOpts += TEXT("Name ");
        if (!OptAllowedActions.IsSet()) MissingOpts += TEXT("Actions ");
        if (!OptBetToCall.IsSet()) MissingOpts += TEXT("OptBetToCall(ForMovingPlayer) ");
        if (!OptMinRaiseAmount.IsSet()) MissingOpts += TEXT("OptMinRaise(ForMovingPlayer) ");
        if (!OptMovingPlayerStack.IsSet()) MissingOpts += TEXT("StackMoving ");
        if (!OptCurrentPot.IsSet()) MissingOpts += TEXT("Pot ");
        if (!OptMovingPlayerCurrentBet.IsSet()) MissingOpts += TEXT("CurrentBetMoving ");
        UE_LOG(LogTemp, Verbose, TEXT("%s"), *MissingOpts);
        return;
    }

    // 2. Проверяем валидность HUD и его интерфейса
    if (!GameHUDWidgetInstance || !GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("TryAggregateAndTriggerHUDUpdate: HUD is not valid or does not implement IGameHUDInterface. Cannot update HUD."));
        OptMovingPlayerSeatIndex.Reset(); OptMovingPlayerName.Reset(); OptAllowedActions.Reset();
        OptBetToCall.Reset(); OptMinRaiseAmount.Reset(); OptMovingPlayerStack.Reset();
        OptCurrentPot.Reset(); OptMovingPlayerCurrentBet.Reset();
        return;
    }

    // 3. Получаем основные данные из TOptionals (эти данные относятся к игроку, ЧЕЙ ХОД СЕЙЧАС)
    const int32 MovingPlayerSeatIndex = OptMovingPlayerSeatIndex.GetValue();
    const FString& MovingPlayerName = OptMovingPlayerName.GetValue();
    // const TArray<EPlayerAction>& AllowedActionsForMovingPlayer = OptAllowedActions.GetValue(); // Используется ниже
    const int64 CurrentPotOnTable = OptCurrentPot.GetValue();
    const int64 StackOfMovingPlayer = OptMovingPlayerStack.GetValue();
    const int64 CurrentBetOfMovingPlayerInRound = OptMovingPlayerCurrentBet.GetValue();

    const int64 CalculatedAmountToCallForMovingPlayer = OptBetToCall.GetValue();
    const int64 CalculatedMinPureRaiseForMovingPlayer = OptMinRaiseAmount.GetValue();

    const int32 LocalPlayerActualSeatIndex = 0;
    const bool bIsLocalPlayerTurn = (MovingPlayerSeatIndex == LocalPlayerActualSeatIndex);

    UOfflinePokerGameState* CurrentGameState = GetCurrentGameState();
    if (!CurrentGameState) { /* ... лог и выход ... */ return; }

    int64 ActualLocalPlayerStack = 0;
    int64 LocalPlayerCurrentBetInThisRound = 0;
    if (CurrentGameState->Seats.IsValidIndex(LocalPlayerActualSeatIndex))
    {
        ActualLocalPlayerStack = CurrentGameState->Seats[LocalPlayerActualSeatIndex].Stack;
        LocalPlayerCurrentBetInThisRound = CurrentGameState->Seats[LocalPlayerActualSeatIndex].CurrentBet;
    }

    int64 BetToCallForHUDDisplay;
    int64 MinRaiseForHUDDisplay;

    if (bIsLocalPlayerTurn)
    {
        // Если ход локального игрока, используем точные значения, 
        // рассчитанные для него в RequestPlayerAction и пришедшие через OnActionUIDetailsDelegate.
        BetToCallForHUDDisplay = CalculatedAmountToCallForMovingPlayer;
        MinRaiseForHUDDisplay = CalculatedMinPureRaiseForMovingPlayer;
    }
    else // Ход бота
    {
        // Показываем локальному игроку, сколько ему нужно было бы для колла против текущей ставки стола
        BetToCallForHUDDisplay = CurrentGameState->CurrentBetToCall - LocalPlayerCurrentBetInThisRound;
        if (BetToCallForHUDDisplay < 0) BetToCallForHUDDisplay = 0;
        BetToCallForHUDDisplay = FMath::Min(BetToCallForHUDDisplay, ActualLocalPlayerStack);

        // Показываем локальному игроку, какой сейчас минимальный чистый рейз на столе
        MinRaiseForHUDDisplay = CurrentGameState->LastBetOrRaiseAmountInCurrentRound > 0
            ? CurrentGameState->LastBetOrRaiseAmountInCurrentRound
            : CurrentGameState->BigBlindAmount;
    }

    // 1. Управляем кнопками и режимом ввода (эта логика остается прежней)
    const TArray<EPlayerAction>& AllowedActionsForCurrentTurn = OptAllowedActions.GetValue(); // Действия для того, чей ход
    if (bIsLocalPlayerTurn && AllowedActionsForCurrentTurn.Num() > 0)
    {
        IGameHUDInterface::Execute_UpdateActionButtons(GameHUDWidgetInstance.Get(), AllowedActionsForCurrentTurn);
    }
    else
    {
        IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance.Get());
    }

    // 2. Вызываем UpdateGameInfo
    IGameHUDInterface::Execute_UpdateGameInfo(
        GameHUDWidgetInstance.Get(),
        MovingPlayerName,
        CurrentPotOnTable,
        ActualLocalPlayerStack,
        LocalPlayerCurrentBetInThisRound,
        BetToCallForHUDDisplay, 
        MinRaiseForHUDDisplay   
    );
    UE_LOG(LogTemp, Warning, TEXT("CONTROLLER TryAggregate: To HUD->UpdateGameInfo: MovingPlayerName=%s, Pot=%lld, LocalStack=%lld, LocalBet=%lld, HUD_BetToCall=%lld, HUD_MinRaise=%lld"),
        *MovingPlayerName, CurrentPotOnTable, ActualLocalPlayerStack, LocalPlayerCurrentBetInThisRound, BetToCallForHUDDisplay, MinRaiseForHUDDisplay);

    UpdateAllSeatVisualizersFromGameState();
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
            UE_LOG(LogTemp, Verbose, TEXT("   Updating PlayerInfo for Seat %d (%s): Stack %lld"), SeatData.SeatIndex, *SeatData.PlayerName, SeatData.Stack);
            IPlayerSeatVisualizerInterface::Execute_UpdatePlayerInfo(FoundVisualizer, SeatData.PlayerName, SeatData.Stack);

            // ЗАДАЧА 2: Скрыть карманные карты тех, кто сфолдил
            if (SeatData.Status == EPlayerStatus::Folded)
            {
                UE_LOG(LogTemp, Verbose, TEXT("   Seat %d (%s) is FOLDED. Hiding their hole cards."), SeatData.SeatIndex, *SeatData.PlayerName);
                IPlayerSeatVisualizerInterface::Execute_HideHoleCards(FoundVisualizer);
            }
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
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleCommunityCardsUpdated received with %d community cards."), CommunityCards.Num());

    // 1. Обновляем 3D отображение общих карт (BP_CommunityCardArea)
    if (CommunityCardDisplayActor && CommunityCardDisplayActor->GetClass()->ImplementsInterface(UCommunityCardDisplayInterface::StaticClass()))
    {
        ICommunityCardDisplayInterface::Execute_UpdateCommunityCards(CommunityCardDisplayActor.Get(), CommunityCards);
        UE_LOG(LogTemp, Log, TEXT("   Called UpdateCommunityCards on BP_CommunityCardArea."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("   CommunityCardDisplayActor is null or does not implement ICommunityCardDisplayInterface. Cannot update 3D community cards."));
    }

    // 2. Обновляем текстовое отображение общих карт в WBP_GameHUD
    if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        IGameHUDInterface::Execute_UpdateCommunityCardsDisplay(GameHUDWidgetInstance.Get(), CommunityCards); // Вызов новой интерфейсной функции
        UE_LOG(LogTemp, Log, TEXT("   Called UpdateCommunityCardsDisplay on WBP_GameHUD."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("   GameHUDWidgetInstance is null or does not implement IGameHUDInterface. Cannot update text community cards in HUD."));
    }
}

void APokerPlayerController::HandleShowdownResults(const TArray<FShowdownPlayerInfo>& ShowdownPlayerResults, const FString& WinnerAnnouncement)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleShowdownResults received. Announcement: '%s'. Results count: %d"), *WinnerAnnouncement, ShowdownPlayerResults.Num());

    // 1. Уведомить HUD о результатах шоудауна (он сам разберется, как отобразить на основе статуса)
    if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        IGameHUDInterface::Execute_DisplayShowdownResults(GameHUDWidgetInstance.Get(), ShowdownPlayerResults, WinnerAnnouncement);
    }

    // 2. Обновить визуализаторы мест, чтобы ПОКАЗАТЬ КАРТЫ ЛИЦОМ для всех игроков в ShowdownResults
    //    (даже если они сфолдили, их PlayerStatusAtShowdown будет это отражать)
    TArray<AActor*> AllSeatVisualizerActors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UPlayerSeatVisualizerInterface::StaticClass(), AllSeatVisualizerActors);

    for (const FShowdownPlayerInfo& PlayerResult : ShowdownPlayerResults)
    {
        AActor* FoundVisualizer = nullptr;
        for (AActor* VisualizerActor : AllSeatVisualizerActors)
        {
            if (VisualizerActor && IPlayerSeatVisualizerInterface::Execute_GetSeatIndexRepresentation(VisualizerActor) == PlayerResult.SeatIndex)
            {
                FoundVisualizer = VisualizerActor;
                break;
            }
        }

        if (FoundVisualizer)
        {
            if (PlayerResult.HoleCards.Num() > 0)
            {
                IPlayerSeatVisualizerInterface::Execute_UpdateHoleCards(FoundVisualizer, PlayerResult.HoleCards, true);
            }
            else
            {
                IPlayerSeatVisualizerInterface::Execute_HideHoleCards(FoundVisualizer);
            }
        }
    }
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

void APokerPlayerController::RequestStartNewHandFromUI()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::RequestStartNewHandFromUI called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (!GI) {
        UE_LOG(LogTemp, Error, TEXT("RequestStartNewHandFromUI: MyGameInstance is null."));
        return;
    }

    UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
    if (!OfflineManager) {
        UE_LOG(LogTemp, Error, TEXT("RequestStartNewHandFromUI: OfflineGameManager is null."));
        return;
    }

    FString ReasonWhyNot;
    if (OfflineManager->CanStartNewHand(ReasonWhyNot))
    {
        UE_LOG(LogTemp, Log, TEXT("RequestStartNewHandFromUI: CanStartNewHand returned true. Calling StartNewHand()."));

        OfflineManager->StartNewHand();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestStartNewHandFromUI: Cannot start new hand: %s"), *ReasonWhyNot);
        // Уведомить HUD о причине
        if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            IGameHUDInterface::Execute_ShowNotificationMessage(GameHUDWidgetInstance, ReasonWhyNot, 5.0f);
        }
    }
}

void APokerPlayerController::RequestReturnToMainMenu()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: RequestReturnToMainMenu called."));

    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (!GI)
    {
        UE_LOG(LogTemp, Error, TEXT("RequestReturnToMainMenu: MyGameInstance is null!"));
        return;
    }

    ULevelTransitionManager* LTM = GI->GetLevelTransitionManager(); 
    if (!LTM)
    {
        UE_LOG(LogTemp, Error, TEXT("RequestReturnToMainMenu: LevelTransitionManager is null!"));
        return;
    }


    UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
    if (OfflineManager)
    {
        UE_LOG(LogTemp, Log, TEXT("RequestReturnToMainMenu: (Optional) OfflineManager exists, consider resetting it if needed."));
    }

    LTM->StartLoadLevelWithVideo(
        FName("MenuLevel"),                     
        GI->LoadingVideo_WidgetClass,      
        GI->LoadingVideo_MediaPlayer,
        GI->LoadingVideo_MediaSource,
        TEXT("")                                
    );
}