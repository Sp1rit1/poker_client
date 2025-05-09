#include "PokerPlayerController.h"
// #include "WBP_GameHUD.h" // Этот инклюд теперь не нужен, если WBP_GameHUD - это чистый Blueprint

#include "Kismet/GameplayStatics.h"
#include "MyGameInstance.h" 
#include "OfflineGameManager.h" 


APokerPlayerController::APokerPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	bIsMouseCursorVisible = false;
}


void APokerPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	bIsMouseCursorVisible = false;

	if (GameHUDWidgetClass)
	{
		// Создаем экземпляр виджета, используя базовый класс UUserWidget
		GameHUDWidgetInstance = CreateWidget<UUserWidget>(this, GameHUDWidgetClass);

		if (GameHUDWidgetInstance)
		{
			GameHUDWidgetInstance->AddToViewport();
			UE_LOG(LogTemp, Log, TEXT("APokerPlayerController: GameHUDWidgetInstance created and added to viewport."));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("APokerPlayerController: Failed to create GameHUDWidgetInstance!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("APokerPlayerController: GameHUDWidgetClass is not set!"));
	}
}

UUserWidget* APokerPlayerController::GetGameHUD() const // Возвращаем UUserWidget*
{
	return GameHUDWidgetInstance;
}

void APokerPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent(); // Вызываем реализацию базового класса APlayerController

	// InputComponent создается и инициализируется в APlayerController::PostInitializeComponents()
	// и затем передается сюда. Мы должны проверить, что он существует.
	if (InputComponent)
	{
		// Включаем ввод для этого контроллера, если он еще не включен
		EnableInput(this); // Важно, чтобы контроллер мог обрабатывать ввод

		InputComponent->BindAction("ToggleCursor", IE_Pressed, this, &APokerPlayerController::ToggleInputMode);
		// ... другие ваши BindAction ...
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APokerPlayerController::SetupInputComponent - InputComponent is NULL!"));
	}
}

// Пример ToggleInputMode
void APokerPlayerController::ToggleInputMode()
{
	bIsMouseCursorVisible = !bIsMouseCursorVisible;
	bShowMouseCursor = bIsMouseCursorVisible;

	if (bIsMouseCursorVisible)
	{
		FInputModeGameAndUI InputModeData;
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputModeData.SetHideCursorDuringCapture(false);
		SetInputMode(InputModeData);
		UE_LOG(LogTemp, Log, TEXT("Input Mode: GameAndUI, Cursor Visible."));
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		UE_LOG(LogTemp, Log, TEXT("Input Mode: GameOnly, Cursor Hidden."));
	}
}

// Заглушки для Обработки Действий Игрока
void APokerPlayerController::HandleFoldAction()
{
	UE_LOG(LogTemp, Log, TEXT("PlayerController: Fold Action Triggered"));
	// Логика вызова OfflineGameManager будет здесь
}

void APokerPlayerController::HandleCheckCallAction()
{
	UE_LOG(LogTemp, Log, TEXT("PlayerController: Check/Call Action Triggered"));
	// Логика вызова OfflineGameManager будет здесь
}

void APokerPlayerController::HandleBetRaiseAction(int64 Amount)
{
	UE_LOG(LogTemp, Log, TEXT("PlayerController: Bet/Raise Action Triggered with Amount: %lld"), Amount);
	// Логика вызова OfflineGameManager будет здесь
}