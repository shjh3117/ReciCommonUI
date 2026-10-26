// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

struct FAnalogInputEvent;
struct FInputEvent;
struct FKeyEvent;
struct FPointerEvent;
class FReciFocusSnapshotProcessor;
class FReciInputFocusProcessor;
class IInputProcessor;
class IReciRoutingEnvironment;
class SWidget;
class UInputAction;
class UWidget;
class UCommonActivatableWidget;

namespace UE::ReciCommonUI::Private
{
	struct FReciKeyRoutingSnapshot;
	const IReciRoutingEnvironment& GetRoutingEnvironment();
}

/** Internal: where a key hint's key goes, for FReciInputFocusCoordinator::IsKeyTakenBefore. */
enum class EReciKeyHintStage : uint8
{
	/**
	 * One of a screen's actions, when a registered screen on the user's focus path maps the action (a button's Hint Input
	 * Action); otherwise as Stock.
	 */
	ScreenAction,
	/** The focused control: a focused button's click action. */
	FocusedControl,
	/**
	 * A capturing screen's key capture (Handle Reci Screen Key), for a key the capture handles itself, such as the key
	 * that ends it. Without a capture on the focus path nothing takes the key there.
	 */
	ScreenKey,
	/** CommonUI's bindings, after Slate's focused widget: a button's Triggering Input Action. */
	Stock,
};

/**
 * Input contract implemented by controls that use ReciCommonUI routing.
 *
 * Implementations own only their control-specific behavior, including where
 * their focus goes. The coordinator owns Slate preprocessing, active-layer hit
 * testing, pointer movement filtering and pointer capture, and tells a control
 * when the pointer hovers it and when its focus should return to its driven item.
 */
class RECICOMMONUI_API IReciInputFocusTarget
{
public:
	virtual ~IReciInputFocusTarget() = default;

	/** Widget surface that must be in the active Slate hit path. */
	virtual const UWidget* GetReciInputSurfaceWidget() const = 0;

	/** Slate user that owns this target. */
	virtual int32 GetReciInputUserIndex() const = 0;

	/** Whether focused keyboard/gamepad input currently belongs here. */
	virtual bool CanHandleReciFocusedInput() const = 0;

	virtual bool HandleReciNavigation(EUINavigation Direction) = 0;
	virtual bool HandleReciAccept() = 0;
	virtual bool HandleReciMouseWheel(
		float WheelDelta,
		const FVector2D& ScreenPosition) = 0;

	virtual bool HandleReciPointerDown(
		const FGeometry& SurfaceGeometry,
		const FPointerEvent& PointerEvent) = 0;
	virtual bool HandleReciPointerMove(
		const FGeometry& SurfaceGeometry,
		const FPointerEvent& PointerEvent) = 0;
	virtual bool HandleReciPointerUp(
		const FPointerEvent& PointerEvent) = 0;
	virtual bool HasReciPointerCapture(int32 PointerIndex) const = 0;
	virtual void HandleReciPointerCaptureLost() = 0;

	/**
	 * Called only for a real, non-dragging mouse-position change over this
	 * control. The control applies its own hover policy.
	 */
	virtual void HandleReciPointerHover(
		const FVector2D& ScreenPosition) = 0;

	/**
	 * Called on the control that holds focus after a Reci control handled a
	 * navigation key, stick step or Accept, on a control that handled a wheel
	 * turn or started its own drag, and when the pointer leaves a control. A
	 * control that holds focus puts it back on its driven item.
	 */
	virtual void ReturnReciFocusToDrivenItem() = 0;
};

/**
 * Process-wide ReciCommonUI input and focus coordinator.
 *
 * Exactly one interaction preprocessor is shared by all registered controls,
 * with a second one that only notes each user's focus at a key-down, ahead of
 * CommonInput's. Controls register as targets during construction and never
 * install their own interaction preprocessors. Both preprocessors are installed
 * on the first registration and removed once nothing needs them.
 */
class RECICOMMONUI_API FReciInputFocusCoordinator final
{
public:
	static FReciInputFocusCoordinator& Get();

	/** Internal: at module shutdown, removes the input preprocessors and forgets every registration. */
	void Shutdown();

	/**
	 * A Reci control (an IReciInputFocusTarget, such as a list's viewport or a composite of lists) registers here while
	 * it is constructed, with the widget whose geometry is its input surface, and unregisters when it is destructed or
	 * releases its Slate resources. While registered it takes its user's routed keys, wheel and pointer in the order
	 * DESIGN.md declares (principle 2), and the input preprocessors stay installed.
	 */
	void RegisterTarget(UWidget* TargetWidget, IReciInputFocusTarget* Target);
	void UnregisterTarget(IReciInputFocusTarget* Target);
	void RegisterScreenInput(UCommonActivatableWidget* Screen);
	void UnregisterScreenInput(UCommonActivatableWidget* Screen);
	/** Internal: whether the screen is registered for screen input, for the report on a screen that is not. */
	bool IsScreenInputRegistered(const UCommonActivatableWidget& Screen) const;

	/**
	 * Internal: a popup layer (a dropdown's list), registered while it is open. While open it is its user's top input
	 * layer: Back closes it, screen input is suspended, and only Reci controls inside it (whose widgets are in the
	 * owner, as a popup created with the owner as its outer is) take focused input.
	 */
	void RegisterOpenPopup(UWidget* Owner, int32 UserIndex, TFunction<void()> Close);
	void UnregisterOpenPopup(UWidget* Owner);

	/** When a key went down, in seconds on the FPlatformTime::Seconds clock. */
	using FEventTimeResolver = TFunction<double(const FInputEvent& Event, const FKey& Key, bool bIsRepeat)>;

	/**
	 * The seam for device key times, such as an input-timing plugin's: HandleReciScreenAction receives the resolver's
	 * result as TimestampSeconds. One resolver serves every user; setting one replaces the last, and nullptr detaches it.
	 * With none, TimestampSeconds is when Slate received the key.
	 *
	 * It runs on the game thread in Reci's input preprocessor, after CommonInput's and before the focused widget,
	 * CommonUI's bindings and Enhanced Input see the key, and gets the key event, its key and whether it is a repeat. It
	 * is called at most once per key-down or repeat, just before the first HandleReciScreenAction for it, and every
	 * action offered that event gets the same time:
	 * - A key-down Reci offers the screens (DESIGN.md, Keys a screen needs), when an action of theirs that is not
	 *   suspended maps the key: also when several actions or screens use the key, and when none of them takes it and the
	 *   key goes on to focused navigation and the stock path.
	 * - A repeat of a press a screen action took, other than an Accept or Back, while that screen still takes the user's
	 *   screen input and an action of its that is not suspended maps the key.
	 * A stick's direction keys (Gamepad Left Stick Up and so on) are keys like any other here, except that with Stick
	 * Dominant Axis Reci offers one only while its direction is the stick's. One held back at its key-down for that
	 * reason is routed as a key-down at its first repeat once its direction is the stick's, so when that repeat is
	 * offered, the resolver gets it with bIsRepeat false.
	 *
	 * Reci consumes these without calling it: a key-down during which focus moved before Reci saw it (a device change),
	 * a Back that closes a popup, a key a screen's HandleReciScreenKey takes before any action is offered it, a focused
	 * Reci control's navigation or Accept by a key no action is offered, and, in a Reci layer, a stick-direction
	 * key-down whose direction is not the stick's (Stick Dominant Axis); the repeats of a held Accept or Back, of a
	 * press that HandleReciScreenKey, a focused Reci control or a device change took, of a screen action's press once
	 * the condition above no longer holds, and of a stick-direction key whose press Reci took or held back, while its
	 * direction is not the stick's; and the key-ups of presses Reci took or held back, the stick values that step a
	 * focused Reci control or that Reci holds back while a screen owns the stick's direction, and the wheel turns and
	 * pointer input a Reci control takes. Nor is it called for a key-down or repeat no action is offered (as while no
	 * registered screen takes the user's screen input, a popup is open, a text field or a selecting key selector has
	 * focus, or CommonUI's input mode is Game), for any key-up or stick value, or for wheel or pointer input. A timing
	 * plugin that hands out each device time once must therefore expire the times nobody claims.
	 */
	void SetEventTimeResolver(FEventTimeResolver Resolver);

	/**
	 * Internal, for key hints: whether a handler that comes before Stage takes the user's key now, so a hint must leave
	 * the key out (DESIGN.md principle 6). Earlier handlers are, in routing order: a text field or a selecting key
	 * selector; an open popup's Back; a capturing screen (every key); the actions of the registered screens on the focus
	 * path that are not suspended (Is Reci Screen Action Suspended), other than OwnAction and, at ScreenAction, only those
	 * before OwnAction; and, at Stock, Accept and the directions while a widget holds focus. At ScreenAction a key also
	 * counts as taken while OwnAction's screen gets no key or suspends OwnAction.
	 */
	bool IsKeyTakenBefore(int32 UserIndex, const FKey& Key, EReciKeyHintStage Stage, const UInputAction* OwnAction);

	/** Internal: the Slate user of the widget's player, as routing reads it; unset when the widget has none. */
	TOptional<int32> GetUserIndex(const UWidget& Widget) const;

	/** Internal: a user's routing changed, so keys IsKeyTakenBefore answered for may be taken by another handler now. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnKeyRoutingChanged, int32 /*UserIndex*/);

	/**
	 * Internal: listens to OnKeyRoutingChanged while Owner lives. It is broadcast, at most once a frame per user, after
	 * the user's focus, open popups, raw input, input mode or focus-path screens change, a screen's capture or suspended
	 * actions do, or the activatable widget around the user's focus enters or leaves CommonUI's active root. It is not
	 * broadcast for a mapping or input-method change, which refresh a key text already. While nothing listens, nothing is
	 * checked.
	 */
	FDelegateHandle AddKeyRoutingListener(UObject* Owner, TFunction<void(int32 UserIndex)> Listener);
	void RemoveKeyRoutingListener(FDelegateHandle Handle);

private:
	friend class FReciFocusSnapshotProcessor;
	friend class FReciInputFocusProcessor;
	friend class UReciScrollBox;
	friend const IReciRoutingEnvironment& UE::ReciCommonUI::Private::GetRoutingEnvironment();
#if WITH_DEV_AUTOMATION_TESTS
	/** Lets the test harness swap the routing environment and the resolver, and set registrations aside. */
	friend struct FReciInputFocusCoordinatorTestAccess;
#endif

	/**
	 * Installs both input preprocessors unless they are installed: on each registration, so that while no Reci control,
	 * screen or popup is registered, Reci does no work on any input (principle 4).
	 */
	void InstallProcessor();
	/**
	 * Removes both input preprocessors once nothing needs them (NeedsProcessors). Only from the processor's Tick, between
	 * inputs, never from an Unregister function: a handler may unregister its screen in the middle of a key-down whose
	 * release must still be swallowed.
	 */
	void ReleaseProcessorIfIdle();
	/**
	 * Whether a Reci control, screen or popup is registered, a pointer is captured, a key Reci recorded a press of is held,
	 * or a stick deflection a Reci control took lasts: their repeats, releases and later values must still go where the
	 * press or deflection went.
	 */
	bool NeedsProcessors() const;

	/**
	 * For a gesture a control routes itself, such as a drag: tells the
	 * registered target to return its focus to its driven item, as after a key
	 * it handled. An unregistered target, or one beneath an open popup, is left
	 * alone.
	 */
	void ReturnFocusToDrivenItem(IReciInputFocusTarget* Target);

	struct FTargetEntry
	{
		TWeakObjectPtr<UWidget> Widget;
		IReciInputFocusTarget* Target = nullptr;
	};

	struct FPointerIdentity
	{
		uint32 UserIndex = 0;
		int32 PointerIndex = 0;

		bool operator==(const FPointerIdentity& Other) const
		{
			return UserIndex == Other.UserIndex
				&& PointerIndex == Other.PointerIndex;
		}

		friend uint32 GetTypeHash(const FPointerIdentity& Identity)
		{
			return HashCombine(
				GetTypeHash(Identity.UserIndex),
				GetTypeHash(Identity.PointerIndex));
		}
	};

	void CompactTargets();
	void CompactInputScreens();
	FTargetEntry* FindTargetEntry(IReciInputFocusTarget* Target);
	FTargetEntry* FindFocusedTarget(uint32 UserIndex);
	FTargetEntry* FindPointerTarget(
		class FSlateApplication& SlateApp,
		const FVector2D& ScreenPosition,
		uint32 UserIndex);
	bool IsTargetUsable(const FTargetEntry& Entry) const;
	void ReturnFocusToDrivenItem(FTargetEntry& Entry);
	bool HasPointerPositionChanged(
		const FPointerIdentity& Pointer,
		const FVector2D& ScreenPosition);
	void RecordPointerPosition(
		const FPointerIdentity& Pointer,
		const FVector2D& ScreenPosition);
	void ClearCapturedTarget();
	/** What routing reads from the stock systems: theirs, unless a test installed its own answers. */
	const IReciRoutingEnvironment& GetRoutingEnvironment() const;
	/** The registered screens that take a user's screen input, innermost first. */
	using FInputScreenChain = TArray<TWeakObjectPtr<UCommonActivatableWidget>, TInlineAllocator<4>>;
	/**
	 * The registered screens on the user's focus path, innermost first, up to the first Is Modal activatable widget
	 * outward from focus, that one included: each activated, visible, enabled and the user's. None while one of the
	 * user's popups is open (unless bIgnorePopups), or while the leaf-most activatable widget around the focus is outside
	 * CommonUI's active root.
	 */
	FInputScreenChain FindInputScreens(uint32 UserIndex, bool bIgnorePopups = false);
	/** Whether any Reci control, screen or popup is registered. With none, an input costs nothing (principle 4). */
	bool HasRegistrations() const;
	/**
	 * Whether the user's focus moved between this key-down reaching the snapshot processor, ahead of CommonInput, and
	 * reaching Reci: CommonUI moves it there when the key changes the input device. Uses up the snapshot.
	 */
	bool DidFocusMoveBeforeKey(const FKeyEvent& Event);
	/**
	 * Whether CommonUI's active input mode for the user's player is Menu or All, the modes in which CommonUI fires a Menu
	 * binding. Reci routes the user's keys and sticks only then; under Game they go to the stock path.
	 */
	bool AllowsMenuInput(uint32 UserIndex);
	/**
	 * Whether the user's focus is in a Reci layer: a registered screen takes its input, one of its popups is open, or the
	 * activated screen of a registered Reci control holds focus. The stock path's Accept and Back there keep their repeats
	 * blocked, since a stock button clicks on every repeat.
	 */
	bool HasReciLayerFocus(uint32 UserIndex);

	/** Who took a key's press: where its repeats go, and whether its release is swallowed. */
	enum class EPressOwner : uint8
	{
		/** The stock path, recorded only for an Accept or Back pressed in a Reci layer. */
		Stock,
		/**
		 * The stock path, for a stick-direction key no Reci handler took: recorded only so that the stick's values know
		 * the screens declined that direction. Its repeats and release pass.
		 */
		StockStick,
		/** A Back that closed one of the user's popups. */
		PopupBack,
		/** A screen's HandleReciScreenKey. */
		ScreenKey,
		/** One of a screen's actions. */
		ScreenAction,
		/** The focused Reci control. */
		FocusedControl,
		/**
		 * Nobody: focus moved before Reci saw the key-down, as CommonUI moves it when the key changes the input device, and
		 * the press must not act on the new focus.
		 */
		DeviceChange,
		/**
		 * Nobody: a stick-direction key, pressed in a Reci layer, whose direction is not its stick's (Stick Dominant Axis).
		 * Its repeats are consumed until its direction is the stick's; its next repeat then is routed as a press.
		 */
		NonDominantStick,
	};

	/** A key a user holds, from its key-down to its key-up. A press Reci leaves to the stock path is mostly not recorded. */
	struct FKeyPress
	{
		EPressOwner Owner = EPressOwner::Stock;
		/** The screen that took the press (ScreenKey, ScreenAction). */
		TWeakObjectPtr<UCommonActivatableWidget> Screen;
		/** The control that took the press (FocusedControl): its widget, and the target registered for it. */
		TWeakObjectPtr<UWidget> TargetWidget;
		IReciInputFocusTarget* Target = nullptr;
		/** No handler gets its repeats: an Accept or Back that Reci took, or that the stock path took in a Reci layer. */
		bool bBlockRepeats = false;
		/** For an Accept or Back the stock path took in a Reci layer, the widget that had focus at the press. */
		TWeakPtr<SWidget> FocusedAtPress;
	};

	/** Offers a key-down to the user's screens, innermost first. When one takes it, Press names that screen and how. */
	bool RouteScreenKey(const FKeyEvent& Event, FKeyPress& Press);
	/** Gives a repeat to the screen that took its press, if that screen still takes the user's screen input. */
	void RouteScreenKeyRepeat(const FKeyEvent& Event, const FKeyPress& Press);
	/**
	 * Whether the user's screens hold a stick's value back from focused navigation: while one of them captures keys,
	 * while a press of one of the axis's direction keys that Reci took, or swallowed at a device change, is held, and
	 * while the stick is past the navigation threshold toward a direction a screen maps whose key has not arrived yet or
	 * was held back as not the stick's direction (NonDominantStick).
	 */
	bool HoldsScreenAxis(const FAnalogInputEvent& Event);
	/** When the event happened, by the resolver if one is set. */
	double ResolveEventTime(const FInputEvent& Event, const FKey& Key, bool bIsRepeat) const;
	bool CloseTopPopup(uint32 UserIndex);
	const UWidget* FindTopPopupOwner(uint32 UserIndex) const;
	bool IsBelowOpenPopup(const FTargetEntry& Entry) const;
	struct FOpenPopup
	{
		TWeakObjectPtr<UWidget> Owner;
		int32 UserIndex = 0;
		TFunction<void()> Close;
	};
	TArray<FOpenPopup> OpenPopups;
	FEventTimeResolver EventTimeResolver;
	/** Null outside tests: the stock systems answer. */
	const IReciRoutingEnvironment* RoutingEnvironment = nullptr;
	/** In no order: CommonUI's activatable tree, read at each input, decides which of them take it. */
	TSet<TWeakObjectPtr<UCommonActivatableWidget>> InputScreens;
	/** Each user's held keys, by who took their press. */
	TMap<uint32, TMap<FKey, FKeyPress>> KeyPresses;
	/**
	 * Who takes the steps of a stick held past its user's analog navigation threshold, from its first step while the
	 * axis keeps its direction, until it returns inside the threshold or turns the other way or, with Stick Dominant
	 * Axis, stops being the stick's direction: the focused Reci control that took the first step, or else the stock
	 * path.
	 */
	struct FAnalogDeflection
	{
		EUINavigation Direction = EUINavigation::Invalid;
		/** The control that took the first step: its widget, and the target registered for it. Null for the stock path. */
		TWeakObjectPtr<UWidget> TargetWidget;
		IReciInputFocusTarget* Target = nullptr;
	};
	/** Each user's deflected sticks, by axis. */
	TMap<uint32, TMap<FKey, FAnalogDeflection>> AnalogDeflections;
	/** The widget that had a user's focus when their last key-down reached the snapshot processor. */
	struct FFocusSnapshot
	{
		FKey Key;
		TWeakPtr<SWidget> Focused;
	};
	TMap<uint32, FFocusSnapshot> FocusSnapshots;

	TArray<FTargetEntry> Targets;
	TMap<FPointerIdentity, FVector2D> LastPointerPositions;
	TWeakObjectPtr<UWidget> CapturedWidget;
	IReciInputFocusTarget* CapturedTarget = nullptr;
	FPointerIdentity CapturedPointer;
	TMap<uint32, IReciInputFocusTarget*> PointerFocusTargets;
	/** The user's routing now, for IsKeyTakenBefore; asks the screens' Blueprints. Keys only when bWithKeys. */
	UE::ReciCommonUI::Private::FReciKeyRoutingSnapshot BuildKeyRoutingSnapshot(uint32 UserIndex, bool bWithKeys);
	/** Each frame while OnKeyRoutingChanged has listeners: broadcasts for each user whose routing changed. */
	bool TickKeyRouting(float DeltaSeconds);

	/** What OnKeyRoutingChanged compares from frame to frame for a user. */
	struct FKeyRoutingSignature
	{
		TWeakPtr<SWidget> Focused;
		TWeakObjectPtr<const UWidget> TopPopupOwner;
		/** The leaf-most activatable widget around focus, and whether it is in CommonUI's active root. */
		TWeakObjectPtr<const UCommonActivatableWidget> FocusLayer;
		bool bFocusLayerInActiveRoot = true;
		bool bMenuInput = true;
		bool bTextEditing = false;
		bool bSelectingKey = false;
		TArray<TWeakObjectPtr<const UCommonActivatableWidget>> Screens;
		/** Each screen's capture, then its actions' suspended answers, screen by screen. */
		TArray<bool> Answers;

		bool operator==(const FKeyRoutingSignature& Other) const;
	};
	FOnKeyRoutingChanged OnKeyRoutingChanged;
	FTSTicker::FDelegateHandle KeyRoutingTicker;
	/** The last signature of each user, from the ticker; emptied when a listener is added, so it hears the next frame. */
	TMap<int32, FKeyRoutingSignature> KeyRoutingSignatures;
	/** While OnKeyRoutingChanged is broadcast for a user: that user's routing, read once for every listener's hint. */
	TSharedPtr<UE::ReciCommonUI::Private::FReciKeyRoutingSnapshot> BroadcastSnapshot;
	int32 BroadcastUser = INDEX_NONE;

	/** Made on the first install and kept until Shutdown, so that installing again finds them still in Slate's lists. */
	TSharedPtr<IInputProcessor> InputProcessor;
	TSharedPtr<IInputProcessor> FocusSnapshotProcessor;
	/**
	 * Whether the processors take input. Slate removes a processor released during its Tick only at its next input, so a
	 * released processor passes every input until then.
	 */
	bool bProcessorsInstalled = false;
};
