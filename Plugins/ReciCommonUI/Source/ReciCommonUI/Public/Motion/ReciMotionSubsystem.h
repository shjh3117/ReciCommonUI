// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "ReciMotionSubsystem.generated.h"

class UWidget;

/**
 * One local player's motion scale, which every Reci motion of the player's widgets follows: 1 is full motion, 0
 * turns it off (a viewport or carousel moves straight to its target, and a released drag does not glide), and a
 * value between shortens durations, time constants and glides by that factor. A drag itself stays 1:1.
 *
 * The value lives only as long as the local player: the plugin stores nothing, so a game that offers a motion
 * preference stores it and sets it on each player every session. Until then the player follows the project's
 * Default Motion Scale (Project Settings > Plugins > Reci Common UI > Motion). Blueprint reaches this through Get
 * Local Player Subsystem.
 */
UCLASS()
class RECICOMMONUI_API UReciMotionSubsystem final
	: public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Sets this player's motion scale, from 0 (off) to 1 (full). Any other value, or one that is not finite, is
	 * rejected: the scale stays as it was, the call returns false, and this is reported once to LogReciCommonUI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Motion")
	bool SetMotionScale(float Scale);

	/** This player's motion scale, or the project's Default Motion Scale while the player has set none. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Motion")
	float GetMotionScale() const;

	/**
	 * The motion scale a widget's motion follows: its owning local player's, or the project's Default Motion Scale
	 * for a widget with no owning player (one made without a player, or at design time).
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "Reci Common UI|Motion",
		meta = (DefaultToSelf = "Widget"))
	static float GetMotionScaleForWidget(const UWidget* Widget);

private:
	/**
	 * The project's Default Motion Scale, clamped to 0 to 1. A value outside them, or one that is not finite (which
	 * counts as 1), is reported once.
	 */
	static float GetDefaultMotionScale();

	TOptional<float> PlayerMotionScale;
};
