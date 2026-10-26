// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ReciCommonUISettings.generated.h"

/**
 * Project-wide settings for ReciCommonUI.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Reci Common UI"))
class RECICOMMONUI_API UReciCommonUISettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	/**
	 * Tab and Shift+Tab move focus to the next and previous widget, as in Slate. Turning it off applies to every
	 * player and every screen, and departs from the W3C APG keyboard practices. Read when a local player is created.
	 */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Navigation")
	bool bTabNavigation = true;

	/**
	 * A stick moves one direction at a time: along the axis it is pushed further on, once that axis is past its
	 * navigation threshold. Held, it repeats along the axis that is further at each repeat; the first repeat waits the
	 * repeat delay only while the stick stays within 90 degrees of its last move, and at rest it starts over. A
	 * stick-direction key reaches a screen or control only while its direction is the stick's. A device sends each
	 * poll's axes in order (X first on XInput), so a stick that reaches a diagonal within one poll may first move along
	 * X. Applies while Reci replaces the player's navigation config (a Reci UI Input Data, or Tab Navigation off). Off,
	 * each axis navigates on its own, as in Slate. Read when a local player is created.
	 */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Navigation")
	bool bStickDominantAxis = true;

	/**
	 * Motion scale for widgets whose player set none: 1 full motion, 0 turns Reci motion off. Between them, Reci's
	 * durations, time constants and glides shorten by the scale; a drag itself stays 1:1. A player's own value, set
	 * on its Reci Motion Subsystem, takes its place.
	 */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Motion",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultMotionScale = 1.0f;
};
