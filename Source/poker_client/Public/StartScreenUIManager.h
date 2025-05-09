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
     * @param InLoadingScreenClass Класс виджета экрана загрузки с видео.
     * @param InLoadingMediaPlayerAsset Ассет MediaPlayer для экрана загрузки.
     * @param InLoadingMediaSourceAsset Ассет MediaSource для экрана загрузки.
     */
    void Initialize(
        UMyGameInstance* InGameInstance,
        TSubclassOf<UUserWidget> InStartScreenClass,
        TSubclassOf<UUserWidget> InLoginScreenClass,
        TSubclassOf<UUserWidget> InRegisterScreenClass,
        TSubclassOf<UUserWidget> InLoadingScreenClass,
        UMediaPlayer* InLoadingMediaPlayerAsset,
        UMediaSource* InLoadingMediaSourceAsset
    );

    // --- Функции Навигации Стартовых Экранов ---
    UFUNCTION(BlueprintCallable, Category = "UI Navigation|StartScreens")
    void ShowStartScreen();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|StartScreens")
    void ShowLoginScreen();

    UFUNCTION(BlueprintCallable, Category = "UI Navigation|StartScreens")
    void ShowRegisterScreen();

    /**
     * Запускает процесс загрузки нового уровня с показом виджета с видео.
     * @param LevelName Имя уровня для загрузки (без префиксов пути).
     */
    UFUNCTION(BlueprintCallable, Category = "UI Navigation|Loading")
    void StartLoadLevelWithVideoWidget(FName LevelName);

    /**
     * Вызывается из Blueprint виджета экрана загрузки, когда видео завершило проигрывание.
     */
    UFUNCTION(BlueprintCallable, Category = "UI Navigation|Loading")
    void NotifyLoadingVideoFinished();

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

    /**
     * Коллбэк, вызываемый по завершении асинхронной загрузки пакета уровня.
     */
    void OnLevelPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);

    /**
     * Проверяет, завершены ли загрузка уровня и проигрывание видео, и если да, осуществляет переход на новый уровень.
     */
    void CheckAndFinalizeLevelTransition();

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

    UPROPERTY()
    TSubclassOf<UUserWidget> LoadingScreenClass;

    // --- Ассеты для экрана загрузки (передаются при инициализации) ---
    UPROPERTY()
    TObjectPtr<UMediaPlayer> LoadingMediaPlayerAsset;

    UPROPERTY()
    TObjectPtr<UMediaSource> LoadingMediaSourceAsset;

    // --- Состояние текущего UI и загрузки ---
    UPROPERTY()
    TObjectPtr<UUserWidget> CurrentTopLevelWidget; // Текущий отображаемый виджет (Start, Login, Register или Loading)

    FName LevelToLoadAsync;        // Имя уровня, который асинхронно загружается
    bool bIsLevelLoadComplete;     // Флаг завершения загрузки пакета уровня
    bool bIsLoadingVideoFinished;  // Флаг завершения проигрывания видео

    // Вспомогательная функция для преобразования EAsyncLoadingResult в строку
    FString AsyncLoadingResultToString(EAsyncLoadingResult::Type Result);
};