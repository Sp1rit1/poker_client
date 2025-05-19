#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/LatentActionManager.h" // Для EAsyncLoadingResult в коллбэке
#include "StartScreenUIManager.generated.h"

// Прямые объявления
class UMyGameInstance;
class UUserWidget;
class UMediaPlayer;
class UMediaSource;
struct FTimerHandle; // Для ResizeTimerHandle, если он будет здесь управляться (хотя лучше в GI)

UCLASS()
class POKER_CLIENT_API UStartScreenUIManager : public UObject // Замените YOURPROJECT_API
{
    GENERATED_BODY()

public:
    UStartScreenUIManager();

    /**
     * Инициализирует менеджер UI стартовых экранов.
     * @param InGameInstance Указатель на владеющий GameInstance.
     * @param InStartScreenClass Класс виджета стартового экрана.
     * @param InLoginScreenClass Класс виджета экрана входа.
     * @param InRegisterScreenClass Класс виджета экрана регистрации.
     */
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
    /**
     * Шаблонная функция для показа виджетов верхнего уровня.
     * Удаляет предыдущий виджет и показывает новый.
     * @param WidgetClassToShow Класс виджета для показа.
     * @param bIsFullscreenWidget Флаг полноэкранного режима (влияет на окно и ввод).
     * @return Указатель на созданный виджет или nullptr.
     */
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