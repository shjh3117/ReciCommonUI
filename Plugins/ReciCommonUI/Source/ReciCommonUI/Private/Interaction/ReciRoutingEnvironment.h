// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "CommonInputModeTypes.h"
#include "CommonInputTypeEnum.h"
#include "InputCoreTypes.h"

class IReciRoutingEnvironment;
class UCommonActivatableWidget;
class UInputAction;
class ULocalPlayer;
class UWidget;

namespace UE::ReciCommonUI::Private
{
	/**
	 * The environment Reci reads now: the stock systems' answers, unless a test installed its own through
	 * FReciInputFocusCoordinatorTestAccess.
	 */
	const IReciRoutingEnvironment& GetRoutingEnvironment();

	/**
	 * Whether CommonUI gives the player's input to the widget's layer, the leaf-most activatable widget around it: false
	 * while that layer is activated but outside its player's active root, as beneath an Is Modal dialog nested in it, where
	 * neither CommonUI's bindings nor screen input get a key. A widget with no layer, or not in Slate's tree yet, counts as
	 * getting input, and so does a deactivated layer, whose hints keep what they showed while it fades out.
	 */
	bool IsLayerInActiveRoot(const UWidget& Widget);
}

/** A player's input method, as CommonInput detects it. */
struct FReciInputMethod
{
	ECommonInputType InputType = ECommonInputType::MouseAndKeyboard;
	/** The current gamepad type, for controller data glyphs. */
	FName GamepadName;
	/** Whether the platform shows input keys at all (CommonInput's Should Show Input Keys). */
	bool bShowsInputKeys = true;
};

/**
 * What Reci reads from CommonUI, CommonInput, Slate and Enhanced Input instead of keeping its own copy (principle 1):
 * the input coordinator while it routes an input, and a key text, Is Key Mapped To Action and the navigation subsystem
 * for a player's keys and input method. Automation tests answer some of it themselves, through
 * FReciInputFocusCoordinatorTestAccess, where the real answer needs a game viewport or a player's subsystems.
 */
class IReciRoutingEnvironment
{
public:
	virtual ~IReciRoutingEnvironment() = default;

	/**
	 * Whether the activatable widget is in CommonUI's active root for its player, the root that receives the player's
	 * input. Screen input goes to no screen while the leaf-most activatable widget around the user's focus is not.
	 */
	virtual bool IsInActiveRoot(const UCommonActivatableWidget& Widget) const = 0;

	/**
	 * CommonUI's active input mode for the widget's player. Reci routes keys and sticks only while it is Menu or All, the
	 * modes in which CommonUI fires a Menu binding.
	 */
	virtual ECommonInputMode GetActiveInputMode(const UWidget& Widget) const = 0;

	/** The Slate user of the widget's owning local player; unset when the widget has none. */
	virtual TOptional<int32> GetUserIndex(const UWidget& Widget) const = 0;

	/** The keys the widget's player maps to the action, in the mapping contexts applied to that player. */
	virtual TArray<FKey> GetActionKeys(const UWidget& Widget, const UInputAction& Action) const = 0;

	/** The keys the player maps to the action, in the mapping contexts applied to that player. */
	virtual TArray<FKey> GetPlayerActionKeys(const ULocalPlayer& Player, const UInputAction& Action) const = 0;

	/** The player's input method; unset when the player has no game instance or no CommonInput subsystem. */
	virtual TOptional<FReciInputMethod> GetInputMethod(const ULocalPlayer& Player) const = 0;
};

/** The stock systems' answers, which the coordinator uses outside tests. */
class FReciStockRoutingEnvironment : public IReciRoutingEnvironment
{
public:
	static const FReciStockRoutingEnvironment& Get();

	/** CommonUI's action router answers; a widget whose player has no router counts as in the active root. */
	virtual bool IsInActiveRoot(const UCommonActivatableWidget& Widget) const override;
	/** CommonUI's action router answers; All when the widget's player has no router or no input config is applied yet. */
	virtual ECommonInputMode GetActiveInputMode(const UWidget& Widget) const override;
	virtual TOptional<int32> GetUserIndex(const UWidget& Widget) const override;
	/** The keys of the widget's owning player; none without one. */
	virtual TArray<FKey> GetActionKeys(const UWidget& Widget, const UInputAction& Action) const override;
	/** Enhanced Input's local player subsystem answers; none without one. */
	virtual TArray<FKey> GetPlayerActionKeys(const ULocalPlayer& Player, const UInputAction& Action) const override;
	/** CommonInput's local player subsystem answers. */
	virtual TOptional<FReciInputMethod> GetInputMethod(const ULocalPlayer& Player) const override;
};
