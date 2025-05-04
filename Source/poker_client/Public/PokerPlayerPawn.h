#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h" // Включаем для USceneComponent
#include "Camera/CameraComponent.h" // Включаем для UCameraComponent
#include "PokerPlayerPawn.generated.h" // Должен быть последним include

UCLASS()
class POKER_CLIENT_API APokerPlayerPawn : public APawn // <-- Замените YOURPROJECT_API на API вашего проекта
{
	GENERATED_BODY()

public:
	// Конструктор
	APokerPlayerPawn();

protected:

	// Компонент сцены, который будет служить корнем
	// VisibleAnywhere - виден в редакторе, BlueprintReadOnly - можно читать из BP, но не писать напрямую
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* SceneRoot;

	// Компонент камеры
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* CameraComponent;

public:
	// Геттеры для компонентов (хорошая практика, не обязательно для решения проблемы)
	/** Возвращает SceneRoot subobject **/
	FORCEINLINE class USceneComponent* GetSceneRoot() const { return SceneRoot; }
	/** Возвращает CameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetCameraComponent() const { return CameraComponent; }
};