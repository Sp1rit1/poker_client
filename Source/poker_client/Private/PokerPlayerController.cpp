#include "PokerPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h" // Не используется напрямую, но может быть полезен для контекста
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

    // Логика переключения режима ввода теперь более явно управляется в TryAggregateAndTriggerHUDUpdate
    // на основе того, чей ход и есть ли доступные действия для локального игрока.
    // Здесь мы только сбрасываем TOptional и сохраняем индекс ходящего.
}

void APokerPlayerController::HandlePlayerActionsAvailable(const TArray<EPlayerAction>& AllowedActions)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandlePlayerActionsAvailable: Received %d actions."), AllowedActions.Num());
    OptAllowedActions = AllowedActions;
    // Если это последний ожидаемый делегат до TryAggregate, можно вызвать его здесь,
    // но так как у нас их несколько, лучше вызывать TryAggregate в том, который приходит последним.
    // Либо, если порядок не гарантирован, вызывать TryAggregate в каждом, а он уже проверит все Opt.
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
        OptAllowedActions.IsSet() &&          // Массив доступных действий для ХОДЯЩЕГО игрока
        OptBetToCall.IsSet() &&             // Сумма, которую ХОДЯЩИЙ игрок должен добавить для колла
        OptMinRaiseAmount.IsSet() &&        // Минимальный ЧИСТЫЙ рейз для ХОДЯЩЕГО игрока (или мин. бет)
        OptMovingPlayerStack.IsSet() &&     // Стек ХОДЯЩЕГО игрока
        OptCurrentPot.IsSet() &&
        OptMovingPlayerCurrentBet.IsSet())) // Текущая ставка ХОДЯЩЕГО игрока в этом раунде
    {
        // Логируем, каких данных не хватает, если нужно для отладки
        FString MissingOpts = TEXT("Missing Opts: ");
        if (!OptMovingPlayerSeatIndex.IsSet()) MissingOpts += TEXT("SeatIdx ");
        if (!OptMovingPlayerName.IsSet()) MissingOpts += TEXT("Name ");
        if (!OptAllowedActions.IsSet()) MissingOpts += TEXT("Actions ");
        if (!OptBetToCall.IsSet()) MissingOpts += TEXT("ToCall ");
        if (!OptMinRaiseAmount.IsSet()) MissingOpts += TEXT("MinRaise ");
        if (!OptMovingPlayerStack.IsSet()) MissingOpts += TEXT("StackMoving ");
        if (!OptCurrentPot.IsSet()) MissingOpts += TEXT("Pot ");
        if (!OptMovingPlayerCurrentBet.IsSet()) MissingOpts += TEXT("CurrentBetMoving ");
        UE_LOG(LogTemp, Verbose, TEXT("TryAggregateAndTriggerHUDUpdate: Not all optional values from delegates are set yet. %s"), *MissingOpts);
        return;
    }

    // 2. Проверяем валидность HUD и его интерфейса
    if (!GameHUDWidgetInstance || !GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("TryAggregateAndTriggerHUDUpdate: HUD is not valid or does not implement IGameHUDInterface. Cannot update HUD."));
        // Сбрасываем TOptional, чтобы не пытаться обновить HUD с неполными данными в следующий раз, если HUD "сломался"
        OptMovingPlayerSeatIndex.Reset(); OptMovingPlayerName.Reset(); OptAllowedActions.Reset();
        OptBetToCall.Reset(); OptMinRaiseAmount.Reset(); OptMovingPlayerStack.Reset();
        OptCurrentPot.Reset(); OptMovingPlayerCurrentBet.Reset();
        return;
    }

    // 3. Получаем основные данные из TOptionals
    const int32 MovingPlayerSeatIndex = OptMovingPlayerSeatIndex.GetValue();
    const FString& MovingPlayerName = OptMovingPlayerName.GetValue();
    const TArray<EPlayerAction>& AllowedActionsForMovingPlayer = OptAllowedActions.GetValue();
    const int64 PotValue = OptCurrentPot.GetValue();
    const int64 StackOfMovingPlayer = OptMovingPlayerStack.GetValue();
    const int64 CurrentBetOfMovingPlayer = OptMovingPlayerCurrentBet.GetValue();
    // OptBetToCall и OptMinRaiseAmount - это ActualAmountToCallUI и MinPureRaiseValueUI для ХОДЯЩЕГО игрока

    const int32 LocalPlayerSeatIndex = 0; // Предполагаем, что локальный игрок всегда SeatIndex 0 в оффлайне
    const bool bIsLocalPlayerTurn = (MovingPlayerSeatIndex == LocalPlayerSeatIndex);

    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::TryAggregateAndTriggerHUDUpdate for Seat %d (%s). IsLocalPlayerTurn: %s"),
        MovingPlayerSeatIndex, *MovingPlayerName, bIsLocalPlayerTurn ? TEXT("true") : TEXT("false"));

    // 4. Получаем GameState для расчетов относительно локального игрока
    UOfflinePokerGameState* CurrentGameState = GetCurrentGameState();
    if (!CurrentGameState) {
        UE_LOG(LogTemp, Error, TEXT("TryAggregateAndTriggerHUDUpdate: CurrentGameState is NULL! Cannot perform calculations for local player."));
        // Сброс TOptional здесь тоже может быть уместен
        OptMovingPlayerSeatIndex.Reset(); OptMovingPlayerName.Reset(); OptAllowedActions.Reset();
        OptBetToCall.Reset(); OptMinRaiseAmount.Reset(); OptMovingPlayerStack.Reset();
        OptCurrentPot.Reset(); OptMovingPlayerCurrentBet.Reset();
        return;
    }

    // 5. Рассчитываем данные, специфичные для отображения ЛОКАЛЬНОМУ игроку
    int64 ActualLocalPlayerStack = 0;
    int64 LocalPlayerCurrentBetInThisRound = 0;
    if (CurrentGameState->Seats.IsValidIndex(LocalPlayerSeatIndex))
    {
        ActualLocalPlayerStack = CurrentGameState->Seats[LocalPlayerSeatIndex].Stack;
        LocalPlayerCurrentBetInThisRound = CurrentGameState->Seats[LocalPlayerSeatIndex].CurrentBet;
    }
    else { UE_LOG(LogTemp, Warning, TEXT("TryAggregateAndTriggerHUDUpdate: LocalPlayerSeatIndex %d is not valid in GameState->Seats!"), LocalPlayerSeatIndex); }

    // Сколько ЛОКАЛЬНОМУ игроку нужно ДОБАВИТЬ, чтобы заколлировать текущую ставку на столе
    int64 AmountLocalPlayerNeedsToAddForCall = CurrentGameState->CurrentBetToCall - LocalPlayerCurrentBetInThisRound;
    if (AmountLocalPlayerNeedsToAddForCall < 0) AmountLocalPlayerNeedsToAddForCall = 0;
    // Эта сумма не должна превышать стек локального игрока
    AmountLocalPlayerNeedsToAddForCall = FMath::Min(AmountLocalPlayerNeedsToAddForCall, ActualLocalPlayerStack);

    // Минимальный ЧИСТЫЙ рейз, который можно сделать на текущей улице (или мин. бет, если это первый бет)
    int64 MinPureRaiseOnTableForLocal = CurrentGameState->LastBetOrRaiseAmountInCurrentRound > 0
        ? CurrentGameState->LastBetOrRaiseAmountInCurrentRound
        : CurrentGameState->BigBlindAmount;


    // 6. СНАЧАЛА ОБНОВЛЯЕМ СОСТОЯНИЕ КНОПОК и переключаем режим ввода
    if (bIsLocalPlayerTurn && AllowedActionsForMovingPlayer.Num() > 0)
    {
        // Ход локального игрока, и есть доступные действия
        UE_LOG(LogTemp, Log, TEXT("   Local player's turn (Seat %d). Updating and Enabling action buttons."), LocalPlayerSeatIndex);
        IGameHUDInterface::Execute_UpdateActionButtons(
            GameHUDWidgetInstance.Get(),
            AllowedActionsForMovingPlayer // Передаем действия, доступные локальному игроку
        );

        if (!bIsInUIMode) // Если мы еще не в UI режиме (например, после хода бота)
        {
            UE_LOG(LogTemp, Log, TEXT("   Switching to UI Input Mode for local player's turn."));
            SwitchToUIInputMode(GameHUDWidgetInstance.Get());
        }
    }
    else // Ход бота ИЛИ у локального игрока нет действий (например, олл-ин или ошибка)
    {
        UE_LOG(LogTemp, Log, TEXT("   Not local player's turn OR no actions available for local (Seat %d). Disabling buttons."), MovingPlayerSeatIndex);
        IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance.Get());

        if (bIsInUIMode) // Если это не активный ход локального игрока И мы в UI режиме
        {
            UE_LOG(LogTemp, Log, TEXT("   Not local player's active turn and we are in UI mode, switching back to Game mode."));
            SwitchToGameInputMode();
        }
    }

    // 7. ЗАТЕМ ВСЕГДА ОБНОВЛЯЕМ ОСНОВНУЮ ИНФОРМАЦИЮ В HUD
    // (Имя ходящего, банк, стек локального игрока, и детали для колла/рейза относительно локального игрока)
    IGameHUDInterface::Execute_UpdateGameInfo(
        GameHUDWidgetInstance.Get(),
        MovingPlayerName,
        PotValue,
        ActualLocalPlayerStack,
        LocalPlayerCurrentBetInThisRound,
        CurrentGameState->CurrentBetToCall,    // Общая ставка для колла на столе
        MinPureRaiseOnTableForLocal            // Минимальный чистый рейз на столе
        // Мы убрали StackOfMovingPlayer и CurrentBetOfMovingPlayer, так как они для PlayerSeatVisualizer
    );
    UE_LOG(LogTemp, Log, TEXT("   Called UpdateGameInfo. MovingPlayer: %s, Pot: %lld, LocalStack: %lld, LocalBetInRound: %lld, TotalBetToCallForLocal: %lld, MinPureRaiseOnTableForLocal: %lld"),
        *MovingPlayerName, PotValue, ActualLocalPlayerStack, LocalPlayerCurrentBetInThisRound, CurrentGameState->CurrentBetToCall, MinPureRaiseOnTableForLocal);

    // 8. Обновляем все 3D-визуализаторы мест (стеки, имена)
    UpdateAllSeatVisualizersFromGameState();

    // Сброс TOptional переменных теперь происходит в HandlePlayerTurnStarted для нового цикла, что корректно.
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
            // Показываем карты лицом, если они есть.
            // Если игрок сфолдил, его PlayerResult.HoleCards все равно будут переданы,
            // а UI (WBP_GameHUD) добавит пометку "(сфолдил)".
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

        // Опционально: Скрыть элементы шоудауна, если они есть и интерфейс это поддерживает
        if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            // IGameHUDInterface::Execute_ClearShowdownDisplay(GameHUDWidgetInstance); // Если вы добавили эту функцию
        }
        // OnNewHandAboutToStartDelegate в OfflineManager должен позаботиться о скрытии карт на столах

        OfflineManager->StartNewHand();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestStartNewHandFromUI: Cannot start new hand: %s"), *ReasonWhyNot);
        // Уведомить HUD о причине
        if (GameHUDWidgetInstance && GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
        {
            // Если у вас есть ShowNotificationMessage в интерфейсе
            IGameHUDInterface::Execute_ShowNotificationMessage(GameHUDWidgetInstance, ReasonWhyNot, 5.0f);
            // Или используем историю, если нет отдельной функции уведомления
            // IGameHUDInterface::Execute_AddGameHistoryMessage(GameHUDWidgetInstance, FString::Printf(TEXT("SYSTEM: %s"), *ReasonWhyNot));
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

    ULevelTransitionManager* LTM = GI->GetLevelTransitionManager(); // Предполагаем, что у вас есть такой геттер
    if (!LTM)
    {
        UE_LOG(LogTemp, Error, TEXT("RequestReturnToMainMenu: LevelTransitionManager is null!"));
        return;
    }

    // Опционально: Уведомить OfflineGameManager о выходе из игры, если нужно что-то сбросить
    UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
    if (OfflineManager)
    {
        // OfflineManager->ResetGameForMenu(); // Создайте эту функцию в OfflineManager, если она нужна
                                            // Например, для сброса GameStateData или отписки от делегатов,
                                            // хотя GameInstance и его объекты обычно переживают смену уровня.
                                            // Для простоты пока можно пропустить, но для чистоты может понадобиться.
        UE_LOG(LogTemp, Log, TEXT("RequestReturnToMainMenu: (Optional) OfflineManager exists, consider resetting it if needed."));
    }

    // Вызываем переход на уровень меню с использованием ассетов заставки из GameInstance
    LTM->StartLoadLevelWithVideo(
        FName("MenuLevel"),                     // Имя вашего уровня главного меню
        GI->LoadingVideo_WidgetClass,      // Ассеты по умолчанию из GameInstance
        GI->LoadingVideo_MediaPlayer,
        GI->LoadingVideo_MediaSource,
        TEXT("")                                // Опции GameMode для MenuLevel не нужны, он использует свой дефолтный
    );
}