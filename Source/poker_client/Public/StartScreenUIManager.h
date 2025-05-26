#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/LatentActionManager.h" 
#include "StartScreenUIManager.generated.h"

class UMyGameInstance;
class UUserWidget;
class UMediaPlayer;
class UMediaSource;
struct FTimerHandle; 

UCLASS()
class POKER_CLIENT_API UStartScreenUIManager : public UObject 
{
    GENERATED_BODY()

public:
    UStartScreenUIManager();

    void Initialize(
        UMyGameInstance* InGameInstance,
        TSubclassOf<UUserWidget> InStartScreenClass,
        TSubclassOf<UUserWidget> InLoginScreenClass,
        TSubclassOf<UUserWidget> InRegisterScreenClass
    );

    // --- Функции Навигации Стартовых Экранов ---
    UFUNCTION(BlueprintCallable, Category = "UI Navigation|StartScreens")
    void ShowStartScreen();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|StartScreens")
    void ShowLoginScreen();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|StartScreens")
    void ShowRegisterScreen();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|StartScreens")
    void TriggerTransitionToMenuLevel();

protected:

    template <typename T>
    T* ShowWidget(TSubclassOf<UUserWidget> WidgetClassToShow, bool bIsFullscreenWidget);

private:
    UPROPERTY()
    TObjectPtr<UMyGameInstance> OwningGameInstance;

    // --- Классы виджетов (передаются при инициализации) ---
    UPROPERTY()
    TSubclassOf<UUserWidget> StartScreenClass;

    UPROPERTY()
    TSubclassOf<UUserWidget> LoginScreenClass;

    UPROPERTY()
    TSubclassOf<UUserWidget> RegisterScreenClass;

    // --- Состояние текущего UI и загрузки ---
    UPROPERTY()
    TObjectPtr<UUserWidget> CurrentTopLevelWidget; // Текущий отображаемый виджет (Start, Login, Register)

};