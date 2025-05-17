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

    SwitchToGameInputMode(); // Начинаем в игровом режиме

    UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
    if (GI)
    {
        UOfflineGameManager* OfflineManager = GI->GetOfflineGameManager();
        if (OfflineManager)
        {
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
        // if (UIMappingContext) { Subsystem->AddMappingContext(UIMappingContext, 1); } 
    }
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Switched to UI Input Mode. Cursor shown. WidgetToFocus: %s"), *GetNameSafe(WidgetToFocus));
}

void APokerPlayerController::HandleActionRequested(int32 SeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack, int64 CurrentPot)
{
    // Логирование
    UE_LOG(LogTemp, Log, TEXT("APokerPlayerController::HandleActionRequested: SeatIndex=%d, Pot=%lld, Stack=%lld, BetToCall=%lld, MinRaise=%lld"),
        SeatIndex, CurrentPot, PlayerStack, BetToCall, MinRaiseAmount);
    FString ActionsString = TEXT("Allowed Actions: ");
    for (EPlayerAction Action : AllowedActions) { UEnum* EnumPtr = StaticEnum<EPlayerAction>(); if (EnumPtr) { ActionsString += EnumPtr->GetNameStringByValue(static_cast<int64>(Action)) + TEXT(", "); } }
    UE_LOG(LogTemp, Log, TEXT("Actions: %s"), *ActionsString);

    if (!GameHUDWidgetInstance || !GameHUDWidgetInstance->GetClass()->ImplementsInterface(UGameHUDInterface::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController::HandleActionRequested - HUD not valid or no IGameHUDInterface."));
        return;
    }

    // 1. Всегда обновляем информацию о ходе в HUD
    IGameHUDInterface::Execute_UpdatePlayerTurnInfo(GameHUDWidgetInstance, SeatIndex, CurrentPot, BetToCall, MinRaiseAmount, PlayerStack);

    const int32 LocalPlayerSeatIndex = 0;
    if (SeatIndex == LocalPlayerSeatIndex) // Ход локального игрока
    {
        IGameHUDInterface::Execute_UpdateActionButtons(GameHUDWidgetInstance, AllowedActions);

        // Если мы не в UI режиме (например, только начался наш ход), автоматически переключаемся в UI.
        // Если уже были в UI (например, игрок сам нажал Tab/S), остаемся в нем.
        if (!bIsInUIMode)
        {
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Local player's turn, NOT in UI mode, switching TO UI input mode."));
            SwitchToUIInputMode(GameHUDWidgetInstance);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Local player's turn, ALREADY in UI mode. Ensuring cursor is visible."));
            bShowMouseCursor = true; // На всякий случай, если другой код мог его скрыть
        }

        // Показ карт локального игрока
        UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
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
        UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: Bot's turn (Seat %d). Buttons disabled. Input mode UNCHANGED for local player."), SeatIndex);
        // Режим ввода локального игрока НЕ МЕНЯЕТСЯ автоматически.
    }

    // Обновляем ВСЕ PlayerSeatVisualizers через GameMode
    AGameModeBase* CurrentGameModeBase = GetWorld()->GetAuthGameMode();
    if (CurrentGameModeBase)
    {
        FOutputDeviceNull Ar;
        CurrentGameModeBase->CallFunctionByNameWithArguments(TEXT("RefreshAllSeatVisualizersUI_Event"), Ar, nullptr, true);
    }
}

// Функции-обработчики действий (вызываются из WBP_GameHUD)
// ТЕПЕРЬ ОНИ НЕ МЕНЯЮТ РЕЖИМ ВВОДА. Игрок остается в UI-режиме.
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
        if (CurrentTurnSeat == 0 &&
            (GameState->GetCurrentGameStage() == EGameStage::WaitingForSmallBlind || GameState->GetCurrentGameStage() == EGameStage::WaitingForBigBlind) &&
            (GameState->Seats.IsValidIndex(CurrentTurnSeat) &&
                (GameState->Seats[CurrentTurnSeat].Status == EPlayerStatus::MustPostSmallBlind || GameState->Seats[CurrentTurnSeat].Status == EPlayerStatus::MustPostBigBlind)))
        {
            GI->GetOfflineGameManager()->ProcessPlayerAction(EPlayerAction::PostBlind, 0);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("HandlePostBlindAction: Not local player's turn or invalid state.")); }
    }
}