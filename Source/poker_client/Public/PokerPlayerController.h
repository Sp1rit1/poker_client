#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PokerDataTypes.h" // Для EPlayerAction
#include "PokerPlayerController.generated.h" // Убедитесь, что это имя вашего файла

// Прямые объявления для UPROPERTY
class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UGameHUDInterface; // Прямое объявление C++ интерфейса

UCLASS()
class POKER_CLIENT_API APokerPlayerController : public APlayerController // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    APokerPlayerController();

    // --- Enhanced Input Свойства ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Mappings")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions")
    UInputAction* LookUpAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input|Actions")
    UInputAction* TurnAction;

    // --- UI Свойства ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> GameHUDClass;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* GameHUDWidgetInstance;

protected:
    virtual void BeginPlay() override;
    // SetupPlayerInputComponent теперь не нужен, так как привязка будет в BeginPlay
    // virtual void SetupPlayerInputComponent() override; // Закомментировано или удалено

    void SetupInputComponent() override;

    // Функции-обработчики для Input Actions
    void HandleLookUp(const struct FInputActionValue& Value);
    void HandleTurn(const struct FInputActionValue& Value);

    // Функция-обработчик делегата от OfflineGameManager
    UFUNCTION()
    void HandleActionRequested(int32 SeatIndex, const TArray<EPlayerAction>& AllowedActions, int64 BetToCall, int64 MinRaiseAmount, int64 PlayerStack);

public:
    // Функции-заглушки для обработки нажатий кнопок HUD
    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleFoldAction();

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleCheckCallAction();

    UFUNCTION(BlueprintCallable, Category = "Player Actions")
    void HandleBetRaiseAction(int64 Amount);
};