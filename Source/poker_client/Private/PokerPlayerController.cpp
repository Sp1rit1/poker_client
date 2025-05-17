#include "PokerPlayerController.h" // Убедитесь, что имя вашего .h файла здесь
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h" // Для GetAuthGameMode()
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "MyGameInstance.h"        // Замените, если имя вашего GameInstance другое
#include "OfflineGameManager.h"    // Для подписки на делегат
#include "OfflinePokerGameState.h" // Для доступа к GameState через OfflineManager
#include "GameHUDInterface.h"      // Ваш C++ интерфейс HUD
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h" // Для FOutputDeviceNull

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
                IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
                UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Initial DisableButtons called on HUD."));
            }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: Failed to create GameHUDWidgetInstance from GameHUDClass.")); }
    }
    else if (IsLocalPlayerController()) { UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDClass is not set in Blueprint Defaults. Cannot create HUD.")); }

    // Начальная установка игрового режима
    SwitchToGameInputMode();

    // Подписка на делегат OfflineGameManager
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager)
        {
            // Привязываем функцию с 7 параметрами к делегату с 7 параметрами
            OfflineManager->OnActionRequestedDelegate.AddDynamic(this, &APokerPlayerController::HandleActionRequested);
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Subscribed to OnActionRequestedDelegate."));
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
        // if (UIInputMappingContext) { Subsystem->AddMappingContext(UIInputMappingContext, 1); } 
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Switched to UI Input Mode. Cursor shown. WidgetToFocus: %s"), *GetNameSafe(WidgetToFocus));
}

// ОБНОВЛЕННАЯ СИГНАТУРА ФУНКЦИИ ДЛЯ СООТВЕТСТВИЯ ДЕЛЕГАТУ
void APokerPlayerController::HandleActionRequested(int32 MovingPlayerSeatIndex, const FString& MovingPlayerName, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 MovingPlayerStack, int64 CurrentPot)
{
    // Логирование
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionRequested: SeatIndex=%d, Name='%s', Pot=%lld, Stack(MovingPlayer)=%lld, BetToCall=%lld, MinRaise=%lld"),
        MovingPlayerSeatIndex, *MovingPlayerName, CurrentPot, MovingPlayerStack, BetToCall, MinRaiseAmount);
    FString ActionsString = TEXT("Allowed Actions: ");
    for (EPlayerAction Action : AllowedActions) { UEnum* EnumPtr = StaticEnum<EPlayerAction>(); if (EnumPtr) { ActionsString += EnumPtr->GetNameStringByValue(static_cast<int64>(Action)) + TEXT(", "); } }
    UE_LOG(LogTemp, Log, TEXT("Actions: %s"), *ActionsString);

    if (!GameHUDWidgetInstance || !GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::HandleActionRequested - HUD not valid or no IGameHUDInterface."));
        return;
    }

    // --- Получаем актуальный стек ЛОКАЛЬНОГО игрока (Seat 0) для передачи в HUD ---
    int64 ActualLocalPlayerStack = 0;
    const int32 LocalPlayerSeatIndex = 0;

    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>(); // GI уже должен быть получен один раз для этой функции
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
    {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        if (GameState->Seats.IsValidIndex(LocalPlayerSeatIndex))
        {
            ActualLocalPlayerStack = GameState->Seats[LocalPlayerSeatIndex].Stack;
        }
        // Имя ходящего игрока (MovingPlayerName) уже передано в функцию как параметр
    }
    else // Если GI или другие компоненты невалидны, устанавливаем стек локального в 0 для безопасности
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::HandleActionRequested - Could not get GameInstance/OfflineManager/GameState to fetch LocalPlayerStack. Defaulting to 0."));
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionRequested - For HUD: LocalPlayerActualStack (Seat 0)=%lld"), ActualLocalPlayerStack);


    // 1. Обновляем основную информацию в HUD, используя новую сигнатуру интерфейса
    //    Убедитесь, что ваша интерфейсная функция UpdatePlayerTurnInfo принимает:
    //    const FString& MovingPlayerName, int64 CurrentPot, int64 CurrentBetToCall, int64 MinimumRaise, int64 LocalPlayerStackToDisplay
    IGameHUDInterface::Execute_UpdatePlayerTurnInfo(
        GameHUDWidgetInstance,
        MovingPlayerName,           // Имя того, чей ход (уже передано в эту функцию)
        CurrentPot,                 // Актуальный банк
        BetToCall,                  // Ставка для колла для ходящего игрока
        MinRaiseAmount,             // Мин. рейз для ходящего игрока
        ActualLocalPlayerStack      // <--- ПЕРЕДАЕМ ВСЕГДА СТЕК ЛОКАЛЬНОГО ИГРОКА
    );

    // 2. Обновляем состояние кнопок действий
    if (MovingPlayerSeatIndex == LocalPlayerSeatIndex) // Ход локального игрока
    {
        IGameHUDInterface::Execute_UpdateActionButtons(GameHUDWidgetInstance, AllowedActions);

        // Показ карт локального игрока (логика остается)
        // GI уже получен выше
        if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
        {
            UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
            if (GameState->GetCurrentGameStage() >= EGameStage::Preflop && GameState->Seats.IsValidIndex(LocalPlayerSeatIndex))
            {
                const FPlayerSeatData& LocalPlayerData = GameState->Seats[LocalPlayerSeatIndex];
                if (LocalPlayerData.HoleCards.Num() == 2) { OnLocalPlayerCardsDealt_BP(LocalPlayerData.HoleCards); }
            }
        }
    }
    else // Ход другого игрока (бота)
    {
        IGameHUDInterface::Execute_DisableButtons(GameHUDWidgetInstance);
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Bot's turn (Seat %d). Buttons disabled. Input mode UNCHANGED for local player."), MovingPlayerSeatIndex);
    }

    // 3. Обновляем ВСЕ PlayerSeatVisualizers через GameMode
    AGameModeBase* CurrentGameModeBase = GetWorld()->GetAuthGameMode();
    if (CurrentGameModeBase)
    {
        FOutputDeviceNull Ar;
        CurrentGameModeBase->CallFunctionByNameWithArguments(TEXT("RefreshAllSeatVisualizersUI_Event"), Ar, nullptr, true);
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Requested GameMode to refresh all seat visualizers."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: Could not get GameMode to refresh seat visualizers."));
    }
}

// Функции-обработчики действий (вызываются из WBP_GameHUD)
// Они НЕ МЕНЯЮТ режим ввода здесь. Возврат в игровой режим - по Tab/Escape из HUD.
void APokerPlayerController::HandleFoldAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleFoldAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState()) {
        if (GI->GetOfflineGameManager()->GetGameState()->GetCurrentTurnSeatIndex() == 0) {
            GI->GetOfflineGameManager()->ProcessPlayerAction(EPlayerAction::Fold, 0);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleFoldAction: Not local player's turn.")); }
    }
}

void APokerPlayerController::HandleCheckCallAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleCheckCallAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState()) {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        if (GameState->GetCurrentTurnSeatIndex() == 0) {
            const FPlayerSeatData& PlayerSeat = GameState->GetSeatData(0);
            EPlayerAction ActionToTake = (GameState->GetCurrentBetToCall() > PlayerSeat.CurrentBet) ? EPlayerAction::Call : EPlayerAction::Check;
            GI->GetOfflineGameManager()->ProcessPlayerAction(ActionToTake, 0);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleCheckCallAction: Not local player's turn.")); }
    }
}

void APokerPlayerController::HandleBetRaiseAction(int64 Amount)
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandleBetRaiseAction called by UI with Amount: %lld"), Amount);
    if (Amount <= 0) { UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: Amount is not positive.")); return; }
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState()) {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        if (GameState->GetCurrentTurnSeatIndex() == 0) {
            const FPlayerSeatData& PlayerSeat = GameState->GetSeatData(0);
            // Корректное определение Bet или Raise должно быть в OfflineGameManager::ProcessPlayerAction на основе текущего состояния ставок.
            // Для упрощения здесь, если есть ставка для колла - это Raise, иначе Bet. Но это не всегда верно.
            // Лучше, если UI передает точное EPlayerAction, или ProcessPlayerAction определяет его сам.
            // Пока оставим так, но это место для потенциального улучшения.
            EPlayerAction ActionToTake = (GameState->GetCurrentBetToCall() > PlayerSeat.CurrentBet || GameState->GetCurrentBetToCall() > 0) ? EPlayerAction::Raise : EPlayerAction::Bet;
            GI->GetOfflineGameManager()->ProcessPlayerAction(ActionToTake, Amount);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandleBetRaiseAction: Not local player's turn.")); }
    }
}

void APokerPlayerController::HandlePostBlindAction()
{
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: HandlePostBlindAction called by UI."));
    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI && GI->GetOfflineGameManager() && GI->GetOfflineGameManager()->GetGameState())
    {
        UOfflinePokerGameState* GameState = GI->GetOfflineGameManager()->GetGameState();
        int32 CurrentTurnSeat = GameState->GetCurrentTurnSeatIndex();
        // Добавлена проверка IsValidIndex для GameState->Seats
        if (CurrentTurnSeat == 0 && GameState->Seats.IsValidIndex(CurrentTurnSeat) &&
            (GameState->GetCurrentGameStage() == EGameStage::WaitingForSmallBlind || GameState->GetCurrentGameStage() == EGameStage::WaitingForBigBlind) &&
            (GameState->Seats[CurrentTurnSeat].Status == EPlayerStatus::MustPostSmallBlind || GameState->Seats[CurrentTurnSeat].Status == EPlayerStatus::MustPostBigBlind))
        {
            GI->GetOfflineGameManager()->ProcessPlayerAction(EPlayerAction::PostBlind, 0);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandlePostBlindAction: Not local player's turn or invalid state.")); }
    }
}