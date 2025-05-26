#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h" 
#include "Camera/CameraComponent.h" 
#include "PokerPlayerPawn.generated.h" 

UCLASS()
class POKER_CLIENT_API APokerPlayerPawn : public APawn 
{
	GENERATED_BODY()

public:
	// Конструктор
	APokerPlayerPawn();

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* SceneRoot;

	// Компонент камеры
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* CameraComponent;

public:
	FORCEINLINE class USceneComponent* GetSceneRoot() const { return SceneRoot; }
	FORCEINLINE class UCameraComponent* GetCameraComponent() const { return CameraComponent; }
};