// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "CommonInputBaseTypes.h"

#include "ReciUIInputData.generated.h"

class UInputAction;

/**
 * CommonUI Input Data that also holds the four navigation actions, so every UI input action is set in one asset. Create
 * a Blueprint from it, set it as Input Data in Project Settings > Game > Common Input Settings, and turn on Enable
 * Enhanced Input Support there. Reci then follows it: keys that the player's applied mapping contexts map to its
 * navigation actions move focus in that direction, and keys mapped to CommonUI's click and Back actions act as Accept
 * and Back. This replaces each local player's Slate navigation config, so it reaches every screen, including screens
 * with no Reci control. With other Input Data, Reci follows nothing, click and Back included. Read when a local player
 * is created.
 */
UCLASS(Abstract, Blueprintable, ClassGroup = Input, meta = (Category = "Common Input"))
class RECICOMMONUI_API UReciUIInputData : public UCommonUIInputData
{
	GENERATED_BODY()

public:
	/**
	 * Enhanced Input action whose active digital keys navigate upward in Slate. Slate continues to handle its standard
	 * analog stick input.
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Properties",
		meta = (EditCondition = "CommonInput.CommonInputSettings.IsEnhancedInputSupportEnabled", EditConditionHides))
	TObjectPtr<UInputAction> NavigationUpAction;

	/**
	 * Enhanced Input action whose active digital keys navigate downward in Slate. Slate continues to handle its standard
	 * analog stick input.
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Properties",
		meta = (EditCondition = "CommonInput.CommonInputSettings.IsEnhancedInputSupportEnabled", EditConditionHides))
	TObjectPtr<UInputAction> NavigationDownAction;

	/**
	 * Enhanced Input action whose active digital keys navigate left in Slate. Slate continues to handle its standard
	 * analog stick input.
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Properties",
		meta = (EditCondition = "CommonInput.CommonInputSettings.IsEnhancedInputSupportEnabled", EditConditionHides))
	TObjectPtr<UInputAction> NavigationLeftAction;

	/**
	 * Enhanced Input action whose active digital keys navigate right in Slate. Slate continues to handle its standard
	 * analog stick input.
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Properties",
		meta = (EditCondition = "CommonInput.CommonInputSettings.IsEnhancedInputSupportEnabled", EditConditionHides))
	TObjectPtr<UInputAction> NavigationRightAction;

	/**
	 * Once an action of this Input Data (a navigation action, or CommonUI's click or Back action) maps a key of a
	 * device (a gamepad, or the keyboard), Slate's default keys for that direction, or for Accept or Back, stop working
	 * on that device. For example, with Navigation Up Action mapped only to W, the Up arrow no longer moves focus up.
	 * Mouse buttons and touch are never navigation keys, so they never count. Read when a local player is created.
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Properties",
		meta = (EditCondition = "CommonInput.CommonInputSettings.IsEnhancedInputSupportEnabled", EditConditionHides))
	bool bMappedKeysReplaceDefaultKeys = true;
};
