#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h" // Для UUserWidget и TSubclassOf
#include "PokerPlayerController.generated.h"

// Нет необходимости в прямом объявлении UWBP_GameHUD, так как мы будем использовать UUserWidget

UCLASS()
class POKER_CLIENT_API APokerPlayerController : public APlayerController // Замените YOURPROJECT_API
{
	GENERATED_BODY()

protected:
	/** Класс виджета игрового HUD, который будет создан. Назначается в Blueprint наследнике этого контроллера. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HUD")
	TSubclassOf<UUserWidget> GameHUDWidgetClass; // Используем базовый UUserWidget для класса

	/** Экземпляр созданного игрового HUD. */
	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD")
	UUserWidget* GameHUDWidgetInstance; // Используем базовый UUserWidget для экземпляра

	virtual void BeginPlay() override;

public:
	APokerPlayerController();

	// Функция для получения экземпляра HUD. Может потребоваться Cast в Blueprint, если нужны специфичные функции HUD.
	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	UUserWidget* GetGameHUD() const; // Возвращаем UUserWidget*

	// ... (остальные функции HandleFoldAction, ToggleInputMode и т.д. остаются такими же) ...
	UFUNCTION(BlueprintCallable, Category = "Poker Actions")
	void HandleFoldAction();

	UFUNCTION(BlueprintCallable, Category = "Poker Actions")
	void HandleCheckCallAction();

	UFUNCTION(BlueprintCallable, Category = "Poker Actions")
	void HandleBetRaiseAction(int64 Amount);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void ToggleInputMode();

protected:

	bool bIsMouseCursorVisible;
	virtual void SetupInputComponent() override;
	/** Обработчик события, когда OfflineGameManager запрашивает действие у игрока. */
	UFUNCTION() 
	void HandleActionRequested(int32 PlayerSeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack);
};