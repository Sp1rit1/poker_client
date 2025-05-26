#include "PokerPlayerPawn.h" // Убедитесь, что имя файла .h указано верно
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

// Реализация конструктора
APokerPlayerPawn::APokerPlayerPawn()
{
	// Отключаем Tick для оптимизации, если он не нужен
	PrimaryActorTick.bCanEverTick = false;

	// Создаем компонент сцены по умолчанию, который будет служить корнем.
	// У акторов должен быть RootComponent.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	if (SceneRoot) // Проверяем, что компонент успешно создан
	{
		RootComponent = SceneRoot;
		// Устанавливаем корневой компонент как Movable (Перемещаемый) по умолчанию
		RootComponent->SetMobility(EComponentMobility::Movable);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APokerPlayerPawn: Failed to create SceneRoot component!"));
	}


	// Создаем компонент камеры.
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	if (CameraComponent && RootComponent) // Проверяем, что камера и корень существуют
	{
		// Прикрепляем компонент камеры к корневому компоненту.
		CameraComponent->SetupAttachment(RootComponent);
		// Явно устанавливаем камеру как Movable (Перемещаемый)
		CameraComponent->SetMobility(EComponentMobility::Movable);
	}
	else
	{
		if (!CameraComponent) UE_LOG(LogTemp, Error, TEXT("APokerPlayerPawn: Failed to create CameraComponent!"));
		if (!RootComponent) UE_LOG(LogTemp, Error, TEXT("APokerPlayerPawn: Cannot attach CameraComponent because RootComponent is null!"));
	}
}