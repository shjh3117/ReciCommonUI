// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Interaction/ReciInputFocusCoordinator.h"

#include "CommonActivatableWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/ReciScreenInput.h"
#include "Input/ReciScreenInputMapping.h"
#include "Input/ReciScreenInputReports.h"
#include "InputAction.h"
#include "Interaction/ReciKeyRouting.h"
#include "Interaction/ReciRoutingEnvironment.h"
#include "Navigation/ReciEnhancedInputNavigationConfig.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "ReciReport.h"
#include "Widgets/ReciWidgetTreeUtils.h"
#include "Widgets/SViewport.h"

using UE::ReciCommonUI::Private::FReciKeyRoutingSnapshot;
using UE::ReciCommonUI::Private::GetScreenActions;
using UE::ReciCommonUI::Private::GetStickDirectionKeys;
using UE::ReciCommonUI::Private::HasKeySelectorFocus;
using UE::ReciCommonUI::Private::HasTextEditingFocus;
using UE::ReciCommonUI::Private::IsSelectingKey;
using UE::ReciCommonUI::Private::IsStickDirectionKey;
using UE::ReciCommonUI::Private::ResolveNavigationConfig;
using UE::ReciCommonUI::Private::TakesRawInput;

namespace
{
	bool IsDirection(const EUINavigation Navigation)
	{
		return Navigation == EUINavigation::Up
			|| Navigation == EUINavigation::Down
			|| Navigation == EUINavigation::Left
			|| Navigation == EUINavigation::Right;
	}

	/**
	 * The direction Slate's analog navigation reads from a stick's value, from the config's analog settings alone: unlike
	 * GetNavigationDirectionFromAnalog, it leaves the config's repeat state untouched, so Slate can still take the step.
	 * Reci's config gives a stick one direction at a time (Stick Dominant Axis).
	 */
	EUINavigation GetAnalogDirection(const FNavigationConfig& Config, const FAnalogInputEvent& Event)
	{
		if (const FReciEnhancedInputNavigationConfig* ReciConfig = FReciEnhancedInputNavigationConfig::Find(&Config))
		{
			return ReciConfig->GetStickDirection(Event);
		}
		if (!Config.bAnalogNavigation)
		{
			return EUINavigation::Invalid;
		}
		const float Value = Event.GetAnalogValue();
		if (Event.GetKey() == Config.AnalogHorizontalKey)
		{
			return Value < -Config.AnalogNavigationHorizontalThreshold ? EUINavigation::Left
				: Value > Config.AnalogNavigationHorizontalThreshold ? EUINavigation::Right
				: EUINavigation::Invalid;
		}
		if (Event.GetKey() == Config.AnalogVerticalKey)
		{
			return Value > Config.AnalogNavigationVerticalThreshold ? EUINavigation::Up
				: Value < -Config.AnalogNavigationVerticalThreshold ? EUINavigation::Down
				: EUINavigation::Invalid;
		}
		return EUINavigation::Invalid;
	}

	/** When Slate received the event, on the FPlatformTime::Seconds clock (its timestamp counts CPU cycles from another origin). */
	double GetSlateEventSeconds(const FInputEvent& Event)
	{
		const uint64 Now = FPlatformTime::Cycles64();
		const uint64 EventCycles = Event.GetEventTimestamp();
		return FPlatformTime::Seconds() - (Now > EventCycles ? FPlatformTime::ToSeconds64(Now - EventCycles) : 0.0);
	}

	/** Whether the key is one of the action's keys right now: the player's bindings, in applied mapping contexts. */
	bool IsKeyMappedToAction(
		const IReciRoutingEnvironment& Environment,
		const UCommonActivatableWidget* Screen,
		const UInputAction* Action,
		const FKey& Key)
	{
		return Environment.GetActionKeys(*Screen, *Action).Contains(Key);
	}

	/** Whether one of the screen's actions that it does not suspend maps the key. */
	bool DoesScreenUseKey(const IReciRoutingEnvironment& Environment, const UCommonActivatableWidget* Screen, const FKey& Key)
	{
		for (const UInputAction* Action : GetScreenActions(*Screen))
		{
			if (IsKeyMappedToAction(Environment, Screen, Action, Key)
				&& !IReciScreenInput::Execute_IsReciScreenActionSuspended(Screen, Action))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Offers the key to each of the screen's actions it is mapped to and the screen does not suspend, until one is
	 * handled. The key's time is resolved into TimestampSeconds when an action first needs it, and kept there for the
	 * screens offered the key after this one.
	 */
	bool RouteScreenAction(
		const IReciRoutingEnvironment& Environment,
		UCommonActivatableWidget* Screen,
		const FKey& Key,
		const bool bIsRepeat,
		TOptional<double>& TimestampSeconds,
		TFunctionRef<double()> GetTimestampSeconds)
	{
		const TWeakObjectPtr<UCommonActivatableWidget> WeakScreen = Screen;
		for (UInputAction* Action : GetScreenActions(*Screen))
		{
			// A handler may close its screen.
			if (!WeakScreen.IsValid())
			{
				return false;
			}
			if (!IsKeyMappedToAction(Environment, Screen, Action, Key)
				|| IReciScreenInput::Execute_IsReciScreenActionSuspended(Screen, Action))
			{
				continue;
			}
			// Resolved once, and only for a key a screen uses: a timing plugin may hand out each device time once.
			if (!TimestampSeconds.IsSet())
			{
				TimestampSeconds = GetTimestampSeconds();
			}
			if (IReciScreenInput::Execute_HandleReciScreenAction(Screen, Action, bIsRepeat, TimestampSeconds.GetValue()))
			{
				return true;
			}
		}
		return false;
	}
}

/**
 * Notes the widget that has each user's focus at a key-down, ahead of CommonInput's preprocessor, which moves focus when
 * the key changes the input device. Reci's own processor runs after CommonInput's and compares. Like Reci's own, it
 * passes every input once released.
 */
class FReciFocusSnapshotProcessor final : public IInputProcessor
{
public:
	explicit FReciFocusSnapshotProcessor(FReciInputFocusCoordinator& InCoordinator)
		: Coordinator(InCoordinator)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
	{
		if (!Coordinator.bProcessorsInstalled || KeyEvent.IsRepeat() || !Coordinator.HasRegistrations())
		{
			return false;
		}
		if (const TSharedPtr<const FSlateUser> User = SlateApp.GetUser(KeyEvent.GetUserIndex()))
		{
			Coordinator.FocusSnapshots.Add(KeyEvent.GetUserIndex(), {KeyEvent.GetKey(), User->GetFocusedWidget()});
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("ReciCommonUIFocusSnapshot");
	}

private:
	FReciInputFocusCoordinator& Coordinator;
};

/**
 * Routes each input to one Reci handler, or leaves it to the stock path. Once released, and until Slate removes it at its
 * next input, it passes every input.
 */
class FReciInputFocusProcessor final : public IInputProcessor
{
public:
	explicit FReciInputFocusProcessor(
		FReciInputFocusCoordinator& InCoordinator)
		: Coordinator(InCoordinator)
	{
	}

	virtual void Tick(
		const float DeltaTime,
		FSlateApplication& SlateApp,
		TSharedRef<ICursor> Cursor) override
	{
		// Between inputs: a handler may unregister its screen in the middle of a key-down whose release must still be
		// swallowed.
		Coordinator.ReleaseProcessorIfIdle();
	}

	virtual bool HandleKeyDownEvent(
		FSlateApplication& SlateApp,
		const FKeyEvent& KeyEvent) override
	{
		if (!Coordinator.bProcessorsInstalled)
		{
			return false;
		}
		// A repeat belongs to whoever took its press, even after that handler opened another screen.
		if (KeyEvent.IsRepeat())
		{
			return HandleRepeat(SlateApp, KeyEvent);
		}
		return HandlePress(SlateApp, KeyEvent);
	}

	/** Routes a key-down, or a repeat that takes over as a press (HandleRepeat). */
	bool HandlePress(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent)
	{
		using FKeyPress = FReciInputFocusCoordinator::FKeyPress;
		using EPressOwner = FReciInputFocusCoordinator::EPressOwner;
		const uint32 UserIndex = KeyEvent.GetUserIndex();
		// A new press starts clean: a release lost while the window was inactive must not leave this key's release
		// swallowed, or a game binding held on that key would never see it released.
		ForgetPress(UserIndex, KeyEvent.GetKey());
		const bool bFocusMovedBeforeKey = Coordinator.DidFocusMoveBeforeKey(KeyEvent);
		// Under a Game input config the key is the game's: Reci leaves it, and records nothing, so its repeats and release
		// pass too. A press Reci took before the mode changed keeps its record.
		if (!Coordinator.AllowsMenuInput(UserIndex)) return false;
		FKeyPress Press;
		// A key-down during which focus moved before Reci saw it (CommonUI moves focus when the key changes the input
		// device) goes nowhere, with its repeats and release: it must not act on a widget that was not visibly focused
		// before it.
		if (bFocusMovedBeforeKey && Coordinator.HasReciLayerFocus(UserIndex))
		{
			Press.Owner = EPressOwner::DeviceChange;
			Press.bBlockRepeats = true;
			RecordPress(KeyEvent, Press);
			return true;
		}
		// A text field, or a key selector that is selecting, takes the key as it is, Escape and Back included: a popup
		// stays open. Nothing is recorded, so its release reaches the widget too.
		if (TakesRawInput(UserIndex)) return false;
		// A stick-direction key whose direction is not its stick's (Stick Dominant Axis) goes nowhere in a Reci layer, with
		// its release, and its repeats until it is (HandleRepeat). Outside a Reci layer it goes on to the stock path.
		if (!IsDominantStickKey(KeyEvent))
		{
			if (!Coordinator.HasReciLayerFocus(UserIndex))
			{
				return false;
			}
			Press.Owner = EPressOwner::NonDominantStick;
			RecordPress(KeyEvent, Press);
			return true;
		}
		// The player's own navigation config decides which keys are Back and Accept: it follows their Enhanced Input
		// mappings.
		const EUINavigationAction Action = SlateApp.GetNavigationActionFromKey(KeyEvent);
		const bool bAcceptOrBack = Action == EUINavigationAction::Accept || Action == EUINavigationAction::Back;
		// A press confirms at most once: the Accept and Back repeats of a press Reci takes reach no handler.
		Press.bBlockRepeats = bAcceptOrBack;
		// Back closes the open popup first, one layer at a time, before any screen handles it.
		if (Action == EUINavigationAction::Back && Coordinator.CloseTopPopup(UserIndex))
		{
			Press.Owner = EPressOwner::PopupBack;
			RecordPress(KeyEvent, Press);
			return true;
		}
		// Read before any handler runs: a handler may move focus or close the layer.
		const bool bInReciLayer = bAcceptOrBack && Coordinator.HasReciLayerFocus(UserIndex);
		const TSharedPtr<SWidget> FocusedAtPress = bInReciLayer ? SlateApp.GetUserFocusedWidget(UserIndex) : nullptr;
		if (Coordinator.RouteScreenKey(KeyEvent, Press))
		{
			RecordPress(KeyEvent, Press);
			return true;
		}
		// A key selector that is not selecting takes Accept to start selecting, and leaves the directions to Slate
		// navigation, even inside a Reci control.
		if (const FReciInputFocusCoordinator::FTargetEntry* Entry =
			HasKeySelectorFocus(UserIndex) ? nullptr : Coordinator.FindFocusedTarget(UserIndex))
		{
			// Copied first: a handler may register or unregister targets.
			const TWeakObjectPtr<UWidget> TargetWidget = Entry->Widget;
			IReciInputFocusTarget* Target = Entry->Target;
			if (RouteFocusedKey(*Target, KeyEvent))
			{
				Press.Owner = EPressOwner::FocusedControl;
				Press.TargetWidget = TargetWidget;
				Press.Target = Target;
				RecordPress(KeyEvent, Press);
				return true;
			}
		}
		// The stock path takes it. Its Accept or Back in a Reci layer keeps its repeats blocked, since a stock button
		// clicks on every repeat, and its release is swallowed if focus moves meanwhile, since that would act on the
		// widget that has focus then.
		if (bInReciLayer)
		{
			Press.Owner = EPressOwner::Stock;
			Press.FocusedAtPress = FocusedAtPress;
			RecordPress(KeyEvent, Press);
		}
		// A stick direction no Reci handler took is noted while it is held, so that the stick's values know the screens
		// declined it and leave its deflection to the focused control and Slate (HoldsScreenAxis).
		else if (IsStickDirectionKey(KeyEvent.GetKey()) && Coordinator.HasRegistrations())
		{
			Press.Owner = EPressOwner::StockStick;
			RecordPress(KeyEvent, Press);
		}
		return false;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		if (!Coordinator.bProcessorsInstalled)
		{
			return false;
		}
		TMap<FKey, FReciInputFocusCoordinator::FKeyPress>* Presses = Coordinator.KeyPresses.Find(Event.GetUserIndex());
		FReciInputFocusCoordinator::FKeyPress Press;
		if (!Presses || !Presses->RemoveAndCopyValue(Event.GetKey(), Press))
		{
			return false;
		}
		if (Presses->IsEmpty())
		{
			Coordinator.KeyPresses.Remove(Event.GetUserIndex());
		}
		// A release follows its press: swallowed when Reci took the press, and for a stock Accept or Back in a Reci layer
		// once focus has moved since the press.
		using EPressOwner = FReciInputFocusCoordinator::EPressOwner;
		if (Press.Owner == EPressOwner::StockStick)
		{
			return false;
		}
		return Press.Owner != EPressOwner::Stock
			|| SlateApp.GetUserFocusedWidget(Event.GetUserIndex()) != Press.FocusedAtPress.Pin();
	}

	virtual bool HandleAnalogInputEvent(
		FSlateApplication& SlateApp,
		const FAnalogInputEvent& AnalogEvent) override
	{
		if (!Coordinator.bProcessorsInstalled)
		{
			return false;
		}
		const uint32 UserIndex = AnalogEvent.GetUserIndex();
		// Reci's config notes every stick value, wherever it goes: a stick's direction follows its latest values.
		if (FReciEnhancedInputNavigationConfig* ReciConfig = FindReciConfig(UserIndex))
		{
			ReciConfig->NoteStickValue(AnalogEvent);
		}
		// A widget that takes raw input, and a Game input mode, get the value as it is.
		const bool bStock = TakesRawInput(UserIndex) || !Coordinator.AllowsMenuInput(UserIndex);
		if (!bStock && !Coordinator.HoldsScreenAxis(AnalogEvent))
		{
			return RouteFocusedAxis(AnalogEvent);
		}
		// The value goes elsewhere, so a deflection a focused control or Slate had of this axis ends with it.
		ForgetDeflection(UserIndex, AnalogEvent.GetKey());
		return !bStock;
	}

	virtual bool HandleMouseWheelOrGestureEvent(
		FSlateApplication& SlateApp,
		const FPointerEvent& WheelEvent,
		const FPointerEvent* GestureEvent) override
	{
		if (!Coordinator.bProcessorsInstalled)
		{
			return false;
		}
		const FReciInputFocusCoordinator::FPointerIdentity Pointer{
			WheelEvent.GetUserIndex(),
			WheelEvent.GetPointerIndex()};
		Coordinator.RecordPointerPosition(
			Pointer,
			WheelEvent.GetScreenSpacePosition());

		FReciInputFocusCoordinator::FTargetEntry* Entry =
			Coordinator.FindPointerTarget(
				SlateApp,
				WheelEvent.GetScreenSpacePosition(),
				WheelEvent.GetUserIndex());
		if (!Entry)
		{
			Entry = Coordinator.FindFocusedTarget(
				WheelEvent.GetUserIndex());
		}
		if (!Entry)
		{
			return false;
		}

		const bool bHandled = Entry->Target->HandleReciMouseWheel(
			WheelEvent.GetWheelDelta(),
			WheelEvent.GetScreenSpacePosition());
		if (bHandled)
		{
			Coordinator.ReturnFocusToDrivenItem(*Entry);
		}
		return bHandled;
	}

	virtual bool HandleMouseButtonDownEvent(
		FSlateApplication& SlateApp,
		const FPointerEvent& PointerEvent) override
	{
		if (!Coordinator.bProcessorsInstalled)
		{
			return false;
		}
		if (!PointerEvent.IsTouchEvent()
			&& PointerEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return false;
		}
		if (Coordinator.CapturedTarget)
		{
			// A control owns one active drag gesture at a time. In particular, a
			// second touch must not replace the pointer that is being tracked.
			return false;
		}
		// A key selector that is selecting takes the button as the key.
		if (TakesRawInput(PointerEvent.GetUserIndex()))
		{
			return false;
		}

		const FReciInputFocusCoordinator::FPointerIdentity Pointer{
			PointerEvent.GetUserIndex(),
			PointerEvent.GetPointerIndex()};
		Coordinator.RecordPointerPosition(
			Pointer,
			PointerEvent.GetScreenSpacePosition());

		FReciInputFocusCoordinator::FTargetEntry* Entry =
			Coordinator.FindPointerTarget(
				SlateApp,
				PointerEvent.GetScreenSpacePosition(),
				PointerEvent.GetUserIndex());
		if (!Entry)
		{
			return false;
		}

		const UWidget* Surface =
			Entry->Target->GetReciInputSurfaceWidget();
		if (!Surface
			|| !Entry->Target->HandleReciPointerDown(
				Surface->GetCachedGeometry(),
				PointerEvent))
		{
			return false;
		}

		Coordinator.CapturedWidget = Entry->Widget;
		Coordinator.CapturedTarget = Entry->Target;
		Coordinator.CapturedPointer = Pointer;
		return true;
	}

	virtual bool HandleMouseMoveEvent(
		FSlateApplication& SlateApp,
		const FPointerEvent& PointerEvent) override
	{
		if (!Coordinator.bProcessorsInstalled)
		{
			return false;
		}
		const FReciInputFocusCoordinator::FPointerIdentity Pointer{
			PointerEvent.GetUserIndex(),
			PointerEvent.GetPointerIndex()};

		if (Coordinator.CapturedTarget
			&& Coordinator.CapturedPointer == Pointer)
		{
			FReciInputFocusCoordinator::FTargetEntry* Entry =
				Coordinator.FindTargetEntry(Coordinator.CapturedTarget);
			if (!Entry
				|| !Entry->Target->HasReciPointerCapture(
					Pointer.PointerIndex))
			{
				Coordinator.ClearCapturedTarget();
				return false;
			}

			Coordinator.RecordPointerPosition(
				Pointer,
				PointerEvent.GetScreenSpacePosition());
			if (!PointerEvent.IsTouchEvent()
				&& !PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				Entry->Target->HandleReciPointerCaptureLost();
				Coordinator.ClearCapturedTarget();
				return true;
			}

			const UWidget* Surface =
				Entry->Target->GetReciInputSurfaceWidget();
			return Surface
				&& Entry->Target->HandleReciPointerMove(
					Surface->GetCachedGeometry(),
					PointerEvent);
		}

		if (PointerEvent.IsTouchEvent()
			|| !Coordinator.HasPointerPositionChanged(
				Pointer,
				PointerEvent.GetScreenSpacePosition()))
		{
			return false;
		}

		FReciInputFocusCoordinator::FTargetEntry* Entry =
			Coordinator.FindPointerTarget(
				SlateApp,
				PointerEvent.GetScreenSpacePosition(),
				PointerEvent.GetUserIndex());
		IReciInputFocusTarget* PreviousTarget =
			Coordinator.PointerFocusTargets.FindRef(
				PointerEvent.GetUserIndex());
		if (PreviousTarget && (!Entry || PreviousTarget != Entry->Target))
		{
			Coordinator.ReturnFocusToDrivenItem(PreviousTarget);
		}

		if (!Entry)
		{
			Coordinator.PointerFocusTargets.Remove(
				PointerEvent.GetUserIndex());
			return false;
		}

		Coordinator.PointerFocusTargets.Add(
			PointerEvent.GetUserIndex(),
			Entry->Target);
		// FindPointerTarget never returns a target beneath an open popup, so hover needs no popup check.
		Entry->Target->HandleReciPointerHover(
			PointerEvent.GetScreenSpacePosition());
		return false;
	}

	virtual bool HandleMouseButtonUpEvent(
		FSlateApplication& SlateApp,
		const FPointerEvent& PointerEvent) override
	{
		if (!Coordinator.bProcessorsInstalled)
		{
			return false;
		}
		if (!PointerEvent.IsTouchEvent()
			&& PointerEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return false;
		}

		const FReciInputFocusCoordinator::FPointerIdentity Pointer{
			PointerEvent.GetUserIndex(),
			PointerEvent.GetPointerIndex()};
		if (!Coordinator.CapturedTarget
			|| !(Coordinator.CapturedPointer == Pointer))
		{
			return false;
		}

		FReciInputFocusCoordinator::FTargetEntry* Entry =
			Coordinator.FindTargetEntry(Coordinator.CapturedTarget);
		Coordinator.RecordPointerPosition(
			Pointer,
			PointerEvent.GetScreenSpacePosition());
		// The release belongs to a widget that takes raw input now: the gesture ends without a click.
		if (TakesRawInput(PointerEvent.GetUserIndex()))
		{
			if (Entry)
			{
				Entry->Target->HandleReciPointerCaptureLost();
			}
			Coordinator.ClearCapturedTarget();
			return false;
		}
		const bool bHandled = Entry
			&& Entry->Target->HandleReciPointerUp(PointerEvent);
		Coordinator.ClearCapturedTarget();
		return bHandled;
	}

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("ReciCommonUIInputFocusCoordinator");
	}

private:
	void RecordPress(const FKeyEvent& KeyEvent, const FReciInputFocusCoordinator::FKeyPress& Press)
	{
		Coordinator.KeyPresses.FindOrAdd(KeyEvent.GetUserIndex()).Add(KeyEvent.GetKey(), Press);
	}

	void ForgetPress(const uint32 UserIndex, const FKey& Key)
	{
		if (TMap<FKey, FReciInputFocusCoordinator::FKeyPress>* Presses = Coordinator.KeyPresses.Find(UserIndex))
		{
			Presses->Remove(Key);
			if (Presses->IsEmpty())
			{
				Coordinator.KeyPresses.Remove(UserIndex);
			}
		}
	}

	/** The user's navigation config when it is Reci's, which keeps their sticks' directions (Stick Dominant Axis). */
	static FReciEnhancedInputNavigationConfig* FindReciConfig(const uint32 UserIndex)
	{
		return FReciEnhancedInputNavigationConfig::Find(ResolveNavigationConfig(UserIndex).Get());
	}

	/** Whether a key may act: any key but a stick-direction key whose direction is not its stick's (Stick Dominant Axis). */
	static bool IsDominantStickKey(const FKeyEvent& Event)
	{
		const FReciEnhancedInputNavigationConfig* ReciConfig = FindReciConfig(Event.GetUserIndex());
		return !ReciConfig || ReciConfig->IsStickKeyDominant(Event);
	}

	void ForgetDeflection(const uint32 UserIndex, const FKey& Axis)
	{
		if (TMap<FKey, FReciInputFocusCoordinator::FAnalogDeflection>* Deflections = Coordinator.AnalogDeflections.Find(UserIndex))
		{
			Deflections->Remove(Axis);
			if (Deflections->IsEmpty())
			{
				Coordinator.AnalogDeflections.Remove(UserIndex);
			}
		}
	}

	/**
	 * Offers a stick's deflection to the focused Reci control, one owner per deflection. Its first step is read from the
	 * user's navigation config without touching the config's repeat state, or taken from Reci's config, which answers each
	 * value once however often it is asked. A control that takes it owns the deflection while the axis keeps its
	 * direction: every value of it is consumed, and the config's own repeat timing, primed on the first step, paces the
	 * later steps, which go only to that control. A deflection the control declines, or that starts with no Reci control
	 * focused, goes to Slate's analog navigation whole, as if Reci were not there.
	 */
	bool RouteFocusedAxis(const FAnalogInputEvent& Event)
	{
		using FAnalogDeflection = FReciInputFocusCoordinator::FAnalogDeflection;
		const uint32 UserIndex = Event.GetUserIndex();
		const TSharedPtr<FNavigationConfig> NavigationConfig = ResolveNavigationConfig(UserIndex);
		const EUINavigation Direction = NavigationConfig ? GetAnalogDirection(*NavigationConfig, Event) : EUINavigation::Invalid;
		const TMap<FKey, FAnalogDeflection>* Deflections = Coordinator.AnalogDeflections.Find(UserIndex);
		const FAnalogDeflection* Found = Deflections ? Deflections->Find(Event.GetKey()) : nullptr;
		if (Found && Found->Direction == Direction)
		{
			if (!Found->Target)
			{
				return false;
			}
			// Copied: a handler may change the records.
			const FAnalogDeflection Deflection = *Found;
			if (NavigationConfig->GetNavigationDirectionFromAnalog(Event) == Direction)
			{
				if (FReciInputFocusCoordinator::FTargetEntry* Entry =
					FindTakingTarget(Deflection.TargetWidget, Deflection.Target, UserIndex))
				{
					RouteFocusedDirection(*Entry->Target, Direction, UserIndex);
				}
			}
			return true;
		}
		ForgetDeflection(UserIndex, Event.GetKey());
		if (!IsDirection(Direction)
			|| (FindReciConfig(UserIndex) && NavigationConfig->GetNavigationDirectionFromAnalog(Event) != Direction))
		{
			return false;
		}
		FAnalogDeflection Deflection;
		Deflection.Direction = Direction;
		// A key selector that is not selecting leaves the directions to Slate navigation, even inside a Reci control.
		if (FReciInputFocusCoordinator::FTargetEntry* Entry =
			HasKeySelectorFocus(UserIndex) ? nullptr : Coordinator.FindFocusedTarget(UserIndex))
		{
			// Copied first: a handler may register or unregister targets.
			const TWeakObjectPtr<UWidget> TargetWidget = Entry->Widget;
			IReciInputFocusTarget* Target = Entry->Target;
			if (RouteFocusedDirection(*Target, Direction, UserIndex))
			{
				Deflection.TargetWidget = TargetWidget;
				Deflection.Target = Target;
				// Primes the config's repeat timing, which Slate would have started had it taken this step.
				NavigationConfig->GetNavigationDirectionFromAnalog(Event);
			}
		}
		Coordinator.AnalogDeflections.FindOrAdd(UserIndex).Add(Event.GetKey(), Deflection);
		return Deflection.Target != nullptr;
	}

	/**
	 * The registered control that took a press or a deflection, while it still takes the user's focused input: still
	 * registered for the same widget, usable, the user's and not beneath an open popup.
	 */
	FReciInputFocusCoordinator::FTargetEntry* FindTakingTarget(
		const TWeakObjectPtr<UWidget>& TargetWidget,
		IReciInputFocusTarget* Target,
		const uint32 UserIndex)
	{
		FReciInputFocusCoordinator::FTargetEntry* Entry = Coordinator.FindTargetEntry(Target);
		const bool bTakesInput = Entry
			&& Entry->Widget == TargetWidget
			&& Coordinator.IsTargetUsable(*Entry)
			&& Entry->Target->GetReciInputUserIndex() == static_cast<int32>(UserIndex)
			&& !Coordinator.IsBelowOpenPopup(*Entry)
			&& Entry->Target->CanHandleReciFocusedInput();
		return bTakesInput ? Entry : nullptr;
	}

	/** Offers a key-down to the focused Reci control: a direction to its navigation, Accept to its Accept. */
	bool RouteFocusedKey(IReciInputFocusTarget& Target, const FKeyEvent& KeyEvent)
	{
		const TSharedPtr<FNavigationConfig> NavigationConfig = ResolveNavigationConfig(KeyEvent.GetUserIndex());
		if (!NavigationConfig)
		{
			return false;
		}
		const EUINavigation Direction = NavigationConfig->GetNavigationDirectionFromKey(KeyEvent);
		if (IsDirection(Direction))
		{
			return RouteFocusedDirection(Target, Direction, KeyEvent.GetUserIndex());
		}
		if (NavigationConfig->GetNavigationActionFromKey(KeyEvent) == EUINavigationAction::Accept)
		{
			const bool bHandled = Target.HandleReciAccept();
			if (bHandled)
			{
				if (auto* FocusedNow = Coordinator.FindFocusedTarget(KeyEvent.GetUserIndex()))
					Coordinator.ReturnFocusToDrivenItem(*FocusedNow);
			}
			return bHandled;
		}
		return false;
	}

	bool RouteFocusedDirection(IReciInputFocusTarget& Target, const EUINavigation Direction, const uint32 UserIndex)
	{
		const bool bHandled = Target.HandleReciNavigation(Direction);
		if (bHandled)
		{
			// Blueprint navigation callbacks can move focus to another surface.
			// Returning focus on the former target would undo that transfer.
			if (auto* FocusedNow = Coordinator.FindFocusedTarget(UserIndex))
				Coordinator.ReturnFocusToDrivenItem(*FocusedNow);
		}
		return bHandled;
	}

	/**
	 * A repeat goes only to the handler that took its press, while that handler may still take the user's input, and is
	 * consumed whether or not the handler takes it. A press Reci left to the stock path gives its repeats to no Reci
	 * handler. A stick-direction key's repeat whose direction is not its stick's (Stick Dominant Axis) is consumed; one
	 * held back at its press is routed as the press it could not be once its direction is the stick's.
	 */
	bool HandleRepeat(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent)
	{
		using EPressOwner = FReciInputFocusCoordinator::EPressOwner;
		const uint32 UserIndex = KeyEvent.GetUserIndex();
		const TMap<FKey, FReciInputFocusCoordinator::FKeyPress>* Presses = Coordinator.KeyPresses.Find(UserIndex);
		const FReciInputFocusCoordinator::FKeyPress* Found = Presses ? Presses->Find(KeyEvent.GetKey()) : nullptr;
		if (!Found || Found->Owner == EPressOwner::StockStick)
		{
			return false;
		}
		const bool bDominant = IsDominantStickKey(KeyEvent);
		if (Found->Owner == EPressOwner::NonDominantStick && bDominant)
		{
			ForgetPress(UserIndex, KeyEvent.GetKey());
			return HandlePress(SlateApp, KeyEvent);
		}
		if (Found->bBlockRepeats || !bDominant)
		{
			return true;
		}
		// Copied: a handler may change the records.
		const FReciInputFocusCoordinator::FKeyPress Press = *Found;
		if (TakesRawInput(UserIndex) || !Coordinator.AllowsMenuInput(UserIndex))
		{
			return true;
		}
		switch (Press.Owner)
		{
		case EPressOwner::ScreenKey:
		case EPressOwner::ScreenAction:
			Coordinator.RouteScreenKeyRepeat(KeyEvent, Press);
			break;
		case EPressOwner::FocusedControl:
			RouteFocusedRepeat(KeyEvent, Press);
			break;
		default:
			break;
		}
		return true;
	}

	/** Gives a direction's repeat to the control that took its press, while that control still takes focused input. */
	void RouteFocusedRepeat(const FKeyEvent& KeyEvent, const FReciInputFocusCoordinator::FKeyPress& Press)
	{
		const uint32 UserIndex = KeyEvent.GetUserIndex();
		FReciInputFocusCoordinator::FTargetEntry* Entry = FindTakingTarget(Press.TargetWidget, Press.Target, UserIndex);
		if (!Entry)
		{
			return;
		}
		const TSharedPtr<FNavigationConfig> NavigationConfig = ResolveNavigationConfig(UserIndex);
		const EUINavigation Direction =
			NavigationConfig ? NavigationConfig->GetNavigationDirectionFromKey(KeyEvent) : EUINavigation::Invalid;
		if (IsDirection(Direction))
		{
			RouteFocusedDirection(*Entry->Target, Direction, UserIndex);
		}
	}

	FReciInputFocusCoordinator& Coordinator;
};

FReciInputFocusCoordinator& FReciInputFocusCoordinator::Get()
{
	static FReciInputFocusCoordinator Coordinator;
	return Coordinator;
}

void FReciInputFocusCoordinator::InstallProcessor()
{
	if (bProcessorsInstalled || !FSlateApplication::IsInitialized())
	{
		return;
	}

	if (!InputProcessor.IsValid())
	{
		InputProcessor = MakeShared<FReciInputFocusProcessor>(*this);
		FocusSnapshotProcessor = MakeShared<FReciFocusSnapshotProcessor>(*this);
	}
	// A processor released in Slate's Tick stays in Slate's list until its next input; registering it again then keeps it
	// where it is. Index 0 of the Game bucket puts Reci's ahead of the processors appended there.
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
	// First of all in the PreGame bucket, ahead of CommonInput's preprocessor there; Reci's own runs in the Game bucket.
	FInputPreprocessorRegistrationKey SnapshotRegistration;
	SnapshotRegistration.Type = EInputPreProcessorType::PreGame;
	SnapshotRegistration.Priority = 0;
	FSlateApplication::Get().RegisterInputPreProcessor(FocusSnapshotProcessor, SnapshotRegistration);
	bProcessorsInstalled = true;
}

void FReciInputFocusCoordinator::ReleaseProcessorIfIdle()
{
	if (!bProcessorsInstalled)
	{
		return;
	}
	CompactTargets();
	CompactInputScreens();
	OpenPopups.RemoveAll([](const FOpenPopup& Popup) { return !Popup.Owner.IsValid(); });
	if (NeedsProcessors() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	// During Slate's Tick, Slate removes them at its next input.
	FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	FSlateApplication::Get().UnregisterInputPreProcessor(FocusSnapshotProcessor);
	bProcessorsInstalled = false;
	// Routing state that outlives no input: the next install starts clean.
	AnalogDeflections.Reset();
	FocusSnapshots.Reset();
	LastPointerPositions.Reset();
	PointerFocusTargets.Reset();
	ClearCapturedTarget();
}

bool FReciInputFocusCoordinator::NeedsProcessors() const
{
	if (HasRegistrations() || CapturedWidget.IsValid() || !KeyPresses.IsEmpty())
	{
		return true;
	}
	for (const TPair<uint32, TMap<FKey, FAnalogDeflection>>& UserDeflections : AnalogDeflections)
	{
		for (const TPair<FKey, FAnalogDeflection>& Deflection : UserDeflections.Value)
		{
			if (Deflection.Value.Target)
			{
				return true;
			}
		}
	}
	return false;
}

void FReciInputFocusCoordinator::Shutdown()
{
	if (FSlateApplication::IsInitialized())
	{
		if (InputProcessor.IsValid())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		if (FocusSnapshotProcessor.IsValid())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(FocusSnapshotProcessor);
		}
	}
	InputProcessor.Reset();
	FocusSnapshotProcessor.Reset();
	bProcessorsInstalled = false;
	if (KeyRoutingTicker.IsValid())
	{
		FTSTicker::RemoveTicker(KeyRoutingTicker);
		KeyRoutingTicker.Reset();
	}
	OnKeyRoutingChanged.Clear();
	KeyRoutingSignatures.Reset();
	BroadcastSnapshot.Reset();
	BroadcastUser = INDEX_NONE;
	Targets.Reset();
	InputScreens.Reset();
	KeyPresses.Reset();
	AnalogDeflections.Reset();
	FocusSnapshots.Reset();
	LastPointerPositions.Reset();
	PointerFocusTargets.Reset();
	ClearCapturedTarget();
}

void FReciInputFocusCoordinator::RegisterTarget(
	UWidget* TargetWidget,
	IReciInputFocusTarget* Target)
{
	if (!TargetWidget || !Target)
	{
		return;
	}

	InstallProcessor();
	CompactTargets();
	if (FTargetEntry* ExistingEntry = FindTargetEntry(Target))
	{
		ExistingEntry->Widget = TargetWidget;
		return;
	}

	FTargetEntry& Entry = Targets.AddDefaulted_GetRef();
	Entry.Widget = TargetWidget;
	Entry.Target = Target;
}

void FReciInputFocusCoordinator::UnregisterTarget(
	IReciInputFocusTarget* Target)
{
	if (!Target)
	{
		return;
	}

	if (CapturedTarget == Target)
	{
		ClearCapturedTarget();
	}
	for (auto Iterator = PointerFocusTargets.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Value() == Target)
		{
			Iterator.RemoveCurrent();
		}
	}
	Targets.RemoveAll(
		[Target](const FTargetEntry& Entry)
		{
			return Entry.Target == Target;
		});
}

void FReciInputFocusCoordinator::ReturnFocusToDrivenItem(
	IReciInputFocusTarget* Target)
{
	if (FTargetEntry* Entry = FindTargetEntry(Target))
	{
		ReturnFocusToDrivenItem(*Entry);
	}
}

void FReciInputFocusCoordinator::CompactTargets()
{
	Targets.RemoveAll(
		[](const FTargetEntry& Entry)
		{
			return !Entry.Widget.IsValid() || !Entry.Target;
		});
}

FReciInputFocusCoordinator::FTargetEntry*
FReciInputFocusCoordinator::FindTargetEntry(IReciInputFocusTarget* Target)
{
	return Targets.FindByPredicate(
		[Target](const FTargetEntry& Entry)
		{
			return Entry.Target == Target && Entry.Widget.IsValid();
		});
}

void FReciInputFocusCoordinator::RegisterScreenInput(UCommonActivatableWidget* Screen)
{
	// The Blueprint entry point takes any activatable widget, so the compiler cannot catch these.
	if (!Screen)
	{
		RECI_WARN_ONCE(TEXT("RegisterScreenInput"), TEXT("None"), TEXT("Register Screen Input was called with no screen."));
		return;
	}
	if (!Screen->GetClass()->ImplementsInterface(UReciScreenInput::StaticClass()))
	{
		RECI_WARN_ONCE(TEXT("RegisterScreenInput"), Screen->GetClass()->GetPathName(),
			TEXT("%s does not implement Reci Screen Input; Register Screen Input ignores it, so it receives no screen input."),
			*Screen->GetClass()->GetName());
		return;
	}
	InstallProcessor();
	CompactInputScreens();
	InputScreens.Add(Screen);
	UE::ReciCommonUI::Private::ReportScreenInputSetup(*Screen, GetRoutingEnvironment());
}

void FReciInputFocusCoordinator::UnregisterScreenInput(UCommonActivatableWidget* Screen)
{
	InputScreens.Remove(Screen);
	CompactInputScreens();
}

bool FReciInputFocusCoordinator::IsScreenInputRegistered(const UCommonActivatableWidget& Screen) const
{
	return InputScreens.Contains(&Screen);
}

void FReciInputFocusCoordinator::CompactInputScreens()
{
	for (auto Iterator = InputScreens.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}
}

void FReciInputFocusCoordinator::RegisterOpenPopup(UWidget* Owner, const int32 UserIndex, TFunction<void()> Close)
{
	if (!Owner) return;
	InstallProcessor();
	UnregisterOpenPopup(Owner);
	OpenPopups.Add({Owner, UserIndex, MoveTemp(Close)});
}

void FReciInputFocusCoordinator::UnregisterOpenPopup(UWidget* Owner)
{
	OpenPopups.RemoveAll([Owner](const FOpenPopup& Popup) { return !Popup.Owner.IsValid() || Popup.Owner.Get() == Owner; });
}

const UWidget* FReciInputFocusCoordinator::FindTopPopupOwner(const uint32 UserIndex) const
{
	for (int32 Index = OpenPopups.Num() - 1; Index >= 0; --Index)
	{
		if (OpenPopups[Index].Owner.IsValid() && OpenPopups[Index].UserIndex == static_cast<int32>(UserIndex))
		{
			return OpenPopups[Index].Owner.Get();
		}
	}
	return nullptr;
}

bool FReciInputFocusCoordinator::IsBelowOpenPopup(const FTargetEntry& Entry) const
{
	const UWidget* Owner = Entry.Target ? FindTopPopupOwner(Entry.Target->GetReciInputUserIndex()) : nullptr;
	return Owner && Entry.Widget.IsValid() && !Entry.Widget->IsIn(Owner);
}

bool FReciInputFocusCoordinator::CloseTopPopup(const uint32 UserIndex)
{
	OpenPopups.RemoveAll([](const FOpenPopup& Popup) { return !Popup.Owner.IsValid(); });
	for (int32 Index = OpenPopups.Num() - 1; Index >= 0; --Index)
	{
		if (OpenPopups[Index].UserIndex != static_cast<int32>(UserIndex)) continue;
		// Copy before calling: closing unregisters the popup and changes the array.
		const TFunction<void()> Close = OpenPopups[Index].Close;
		OpenPopups.RemoveAt(Index);
		if (Close) Close();
		return true;
	}
	return false;
}

const IReciRoutingEnvironment& FReciInputFocusCoordinator::GetRoutingEnvironment() const
{
	return RoutingEnvironment ? *RoutingEnvironment : FReciStockRoutingEnvironment::Get();
}

const IReciRoutingEnvironment& UE::ReciCommonUI::Private::GetRoutingEnvironment()
{
	return FReciInputFocusCoordinator::Get().GetRoutingEnvironment();
}

FReciInputFocusCoordinator::FInputScreenChain FReciInputFocusCoordinator::FindInputScreens(
	const uint32 UserIndex,
	const bool bIgnorePopups)
{
	FInputScreenChain Screens;
	// With no screen registered, an input costs nothing here (principle 4).
	if (InputScreens.IsEmpty())
	{
		return Screens;
	}
	CompactInputScreens();
	// An open popup is its user's top layer for keys as for the pointer. It is hosted inside its screen, so focus in it
	// is focus in the screen, but no screen takes screen input while it is open: its Back and its own list come first.
	if (!bIgnorePopups && FindTopPopupOwner(UserIndex))
	{
		return Screens;
	}
	UCommonActivatableWidget* Leaf = UCommonUIActionRouterBase::FindActivatable(
		FSlateApplication::Get().GetUserFocusedWidget(UserIndex), nullptr);
	// CommonUI gives a player's input to one root, the active one. Focus outside it (beside or beneath an Is Modal
	// dialog, say) takes no screen input, as CommonUI's own bindings there take none.
	if (!Leaf || !GetRoutingEnvironment().IsInActiveRoot(*Leaf))
	{
		return Screens;
	}
	// Outward from the user's focus through the activatable widgets around it: the Slate-parent walk CommonUI builds its
	// activatable tree with.
	for (UCommonActivatableWidget* Screen = Leaf;
		Screen;
		Screen = UCommonUIActionRouterBase::FindOwningActivatable(Screen->GetCachedWidget(), nullptr))
	{
		const TOptional<int32> ScreenUser = GetRoutingEnvironment().GetUserIndex(*Screen);
		if (InputScreens.Contains(Screen) && ScreenUser.IsSet() && ScreenUser.GetValue() == static_cast<int32>(UserIndex)
			&& Screen->IsActivated() && Screen->IsVisible() && Screen->GetIsEnabled())
		{
			Screens.Add(Screen);
		}
		// CommonUI makes an Is Modal widget a root of its own: the screens around it get none of the input inside it.
		if (Screen->IsModal())
		{
			break;
		}
	}
	return Screens;
}

bool FReciInputFocusCoordinator::HasRegistrations() const
{
	return !InputScreens.IsEmpty() || !Targets.IsEmpty() || !OpenPopups.IsEmpty();
}

bool FReciInputFocusCoordinator::DidFocusMoveBeforeKey(const FKeyEvent& Event)
{
	FFocusSnapshot Snapshot;
	if (!FocusSnapshots.RemoveAndCopyValue(Event.GetUserIndex(), Snapshot) || Snapshot.Key != Event.GetKey())
	{
		return false;
	}
	const TSharedPtr<const FSlateUser> User = FSlateApplication::Get().GetUser(Event.GetUserIndex());
	return User && User->GetFocusedWidget() != Snapshot.Focused.Pin();
}

bool FReciInputFocusCoordinator::AllowsMenuInput(const uint32 UserIndex)
{
	// With nothing registered there is nothing to route, and an input costs nothing here (principle 4).
	if (!HasRegistrations())
	{
		return true;
	}
	// The mode is the player's, so any of the player's widgets asks the right router: the leaf-most activatable widget
	// around the focus, else the focused Reci control, else the open popup. With none of them, nothing of Reci's takes
	// the input anyway.
	const UWidget* Context = UCommonUIActionRouterBase::FindActivatable(
		FSlateApplication::Get().GetUserFocusedWidget(UserIndex), nullptr);
	if (!Context)
	{
		const FTargetEntry* Entry = FindFocusedTarget(UserIndex);
		Context = Entry ? Entry->Widget.Get() : FindTopPopupOwner(UserIndex);
	}
	if (!Context)
	{
		return true;
	}
	const ECommonInputMode Mode = GetRoutingEnvironment().GetActiveInputMode(*Context);
	return Mode == ECommonInputMode::Menu || Mode == ECommonInputMode::All;
}

bool FReciInputFocusCoordinator::RouteScreenKey(const FKeyEvent& Event, FKeyPress& Press)
{
	const FInputScreenChain Screens = FindInputScreens(Event.GetUserIndex());
	TOptional<double> TimestampSeconds;
	// Innermost first, as CommonUI offers a key to a child's bindings before its parent's: a screen's key capture, then
	// its actions, then the next screen out.
	for (const TWeakObjectPtr<UCommonActivatableWidget>& WeakScreen : Screens)
	{
		// A handler may close any screen of the chain.
		UCommonActivatableWidget* Screen = WeakScreen.Get();
		if (!Screen)
		{
			continue;
		}
		if (IReciScreenInput::Execute_HandleReciScreenKey(Screen, Event.GetKey(), false))
		{
			Press.Owner = EPressOwner::ScreenKey;
			Press.Screen = WeakScreen;
			return true;
		}
		if (WeakScreen.IsValid()
			&& RouteScreenAction(
				GetRoutingEnvironment(),
				Screen,
				Event.GetKey(),
				false,
				TimestampSeconds,
				[this, &Event]() { return ResolveEventTime(Event, Event.GetKey(), false); }))
		{
			Press.Owner = EPressOwner::ScreenAction;
			Press.Screen = WeakScreen;
			return true;
		}
	}
	return false;
}

void FReciInputFocusCoordinator::RouteScreenKeyRepeat(const FKeyEvent& Event, const FKeyPress& Press)
{
	UCommonActivatableWidget* Screen = Press.Screen.Get();
	// Only while the screen still takes the user's screen input: not once another screen has focus, a popup is open, or
	// the screen is closed.
	if (!Screen || !FindInputScreens(Event.GetUserIndex()).Contains(Press.Screen))
	{
		return;
	}
	if (Press.Owner == EPressOwner::ScreenKey)
	{
		IReciScreenInput::Execute_HandleReciScreenKey(Screen, Event.GetKey(), true);
	}
	else
	{
		TOptional<double> TimestampSeconds;
		RouteScreenAction(
			GetRoutingEnvironment(),
			Screen,
			Event.GetKey(),
			true,
			TimestampSeconds,
			[this, &Event]() { return ResolveEventTime(Event, Event.GetKey(), true); });
	}
}

bool FReciInputFocusCoordinator::HasReciLayerFocus(const uint32 UserIndex)
{
	if (!FindInputScreens(UserIndex).IsEmpty() || FindTopPopupOwner(UserIndex)) return true;
	CompactTargets();
	for (const auto& Entry : Targets)
	{
		if (!IsTargetUsable(Entry) || Entry.Target->GetReciInputUserIndex() != static_cast<int32>(UserIndex)) continue;
		const auto* Screen = Entry.Widget->GetTypedOuter<UCommonActivatableWidget>();
		if (Screen && Screen->IsActivated() && (Screen->HasAnyUserFocus() || Screen->HasFocusedDescendants())) return true;
	}
	return false;
}

bool FReciInputFocusCoordinator::HoldsScreenAxis(const FAnalogInputEvent& Event)
{
	// A screen's stick actions come from the platform's stick-direction keys, as any key's do (RouteScreenKey). The
	// stick's values only have to stay out of focused navigation while a screen owns the deflection or may still take it.
	const uint32 UserIndex = Event.GetUserIndex();
	const TMap<FKey, FKeyPress>* Presses = KeyPresses.Find(UserIndex);
	const auto FindPress = [Presses](const FKey& Key) { return Presses ? Presses->Find(Key) : nullptr; };
	// A key held back as not the stick's direction (Stick Dominant Axis) has not arrived yet, as far as the stick goes.
	const auto IsHeldBack = [](const FKeyPress* Press) { return Press && Press->Owner == EPressOwner::NonDominantStick; };
	const auto IsTakenByReci = [&FindPress, &IsHeldBack](const FKey& Key)
	{
		const FKeyPress* Press = FindPress(Key);
		return Press && Press->Owner != EPressOwner::Stock && Press->Owner != EPressOwner::StockStick && !IsHeldBack(Press);
	};
	FKey NegativeKey;
	FKey PositiveKey;
	const bool bStick = GetStickDirectionKeys(Event.GetKey(), NegativeKey, PositiveKey);
	// A press of one of the axis's direction keys that Reci took, or swallowed at a device change, owns the stick until its
	// release, even once its screen has closed: the stick must not also move focus.
	if (bStick && (IsTakenByReci(NegativeKey) || IsTakenByReci(PositiveKey)))
	{
		return true;
	}
	const FInputScreenChain Screens = FindInputScreens(UserIndex);
	if (Screens.IsEmpty())
	{
		return false;
	}
	// A capturing screen takes the stick's direction keys as keys, so the stick itself must not move focus meanwhile.
	if (Screens.ContainsByPredicate([](const TWeakObjectPtr<UCommonActivatableWidget>& Screen)
		{
			return Screen.IsValid() && IReciScreenInput::Execute_IsCapturingReciScreenKeys(Screen.Get());
		}))
	{
		return true;
	}
	if (!bStick)
	{
		return false;
	}
	// Undecided: the stick is past the navigation threshold toward a direction a screen maps, and that direction's key
	// has not arrived, or was held back and arrives at its next repeat. Devices send a stick's value ahead of its
	// direction key in the same poll, so focused navigation would otherwise step on a deflection the screen then takes.
	// Once the key has arrived and no screen took it, the deflection is the focused control's and Slate's.
	const TSharedPtr<FNavigationConfig> NavigationConfig = ResolveNavigationConfig(UserIndex);
	if (!NavigationConfig || !IsDirection(GetAnalogDirection(*NavigationConfig, Event)))
	{
		return false;
	}
	const FKey& Key = Event.GetAnalogValue() > 0 ? PositiveKey : NegativeKey;
	const FKeyPress* KeyPress = FindPress(Key);
	return (!KeyPress || IsHeldBack(KeyPress))
		&& Screens.ContainsByPredicate([this, &Key](const TWeakObjectPtr<UCommonActivatableWidget>& Screen)
			{
				return Screen.IsValid() && DoesScreenUseKey(GetRoutingEnvironment(), Screen.Get(), Key);
			});
}

void FReciInputFocusCoordinator::SetEventTimeResolver(FEventTimeResolver Resolver)
{
	EventTimeResolver = MoveTemp(Resolver);
}

FReciKeyRoutingSnapshot FReciInputFocusCoordinator::BuildKeyRoutingSnapshot(const uint32 UserIndex, const bool bWithKeys)
{
	FReciKeyRoutingSnapshot Snapshot;
	if (!FSlateApplication::IsInitialized())
	{
		return Snapshot;
	}
	FSlateApplication& SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWidget> Focused = SlateApp.GetUserFocusedWidget(UserIndex);
	// With focus on the game viewport, a key goes to the viewport client, and so to CommonUI's bindings.
	Snapshot.bWidgetFocused = Focused.IsValid() && Focused != SlateApp.GetGameViewport();
	Snapshot.bTextEditing = HasTextEditingFocus(UserIndex);
	Snapshot.bSelectingKey = IsSelectingKey(UserIndex);
	Snapshot.bMenuInput = AllowsMenuInput(UserIndex);
	Snapshot.bPopupOpen = FindTopPopupOwner(UserIndex) != nullptr;
	// The screens are read beneath an open popup too: a hint for a screen's action needs to know the screen gets no key.
	for (const TWeakObjectPtr<UCommonActivatableWidget>& WeakScreen : FindInputScreens(UserIndex, true))
	{
		const UCommonActivatableWidget* Screen = WeakScreen.Get();
		if (!Screen)
		{
			continue;
		}
		FReciKeyRoutingSnapshot::FScreen& Entry = Snapshot.Screens.AddDefaulted_GetRef();
		Entry.Screen = Screen;
		Entry.bCapturing = IReciScreenInput::Execute_IsCapturingReciScreenKeys(Screen);
		for (const UInputAction* Action : GetScreenActions(*Screen))
		{
			FReciKeyRoutingSnapshot::FAction& ActionEntry = Entry.Actions.AddDefaulted_GetRef();
			ActionEntry.Action = Action;
			ActionEntry.bSuspended = IReciScreenInput::Execute_IsReciScreenActionSuspended(Screen, Action);
			if (bWithKeys)
			{
				ActionEntry.Keys = GetRoutingEnvironment().GetActionKeys(*Screen, *Action);
			}
		}
	}
	return Snapshot;
}

bool FReciInputFocusCoordinator::IsKeyTakenBefore(
	const int32 UserIndex,
	const FKey& Key,
	const EReciKeyHintStage Stage,
	const UInputAction* OwnAction)
{
	if (!FSlateApplication::IsInitialized() || UserIndex < 0 || !Key.IsValid())
	{
		return false;
	}
	UE::ReciCommonUI::Private::FReciKeyRoutingQuery Query;
	Query.Key = Key;
	Query.Stage = Stage;
	Query.OwnAction = OwnAction;
	// The player's own navigation config decides which keys are Back, Accept and the directions, as at a key-down.
	if (const TSharedPtr<FNavigationConfig> NavigationConfig = ResolveNavigationConfig(UserIndex))
	{
		const FKeyEvent Event(Key, FModifierKeysState(), UserIndex, false, 0, 0);
		Query.NavigationAction = NavigationConfig->GetNavigationActionFromKey(Event);
		Query.bNavigationDirection = IsDirection(NavigationConfig->GetNavigationDirectionFromKey(Event));
	}
	if (BroadcastSnapshot && BroadcastUser == UserIndex)
	{
		return UE::ReciCommonUI::Private::IsKeyTakenBefore(*BroadcastSnapshot, Query);
	}
	return UE::ReciCommonUI::Private::IsKeyTakenBefore(BuildKeyRoutingSnapshot(UserIndex, true), Query);
}

TOptional<int32> FReciInputFocusCoordinator::GetUserIndex(const UWidget& Widget) const
{
	return GetRoutingEnvironment().GetUserIndex(Widget);
}

FDelegateHandle FReciInputFocusCoordinator::AddKeyRoutingListener(UObject* Owner, TFunction<void(int32 UserIndex)> Listener)
{
	if (!Owner || !Listener)
	{
		return FDelegateHandle();
	}
	// A listener's own answers are from when it was added: every user's routing counts as changed at the next frame.
	KeyRoutingSignatures.Reset();
	if (!KeyRoutingTicker.IsValid())
	{
		KeyRoutingTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FReciInputFocusCoordinator::TickKeyRouting));
	}
	return OnKeyRoutingChanged.AddWeakLambda(Owner, [Listener = MoveTemp(Listener)](const int32 UserIndex)
	{
		Listener(UserIndex);
	});
}

void FReciInputFocusCoordinator::RemoveKeyRoutingListener(const FDelegateHandle Handle)
{
	OnKeyRoutingChanged.Remove(Handle);
}

bool FReciInputFocusCoordinator::FKeyRoutingSignature::operator==(const FKeyRoutingSignature& Other) const
{
	return Focused.Pin() == Other.Focused.Pin()
		&& TopPopupOwner == Other.TopPopupOwner
		&& FocusLayer == Other.FocusLayer
		&& bFocusLayerInActiveRoot == Other.bFocusLayerInActiveRoot
		&& bMenuInput == Other.bMenuInput
		&& bTextEditing == Other.bTextEditing
		&& bSelectingKey == Other.bSelectingKey
		&& Screens == Other.Screens
		&& Answers == Other.Answers;
}

bool FReciInputFocusCoordinator::TickKeyRouting(const float DeltaSeconds)
{
	// Only while something listens (principle 4); the screens' Blueprints are asked only while one is on a focus path.
	if (!OnKeyRoutingChanged.IsBound() || !FSlateApplication::IsInitialized())
	{
		KeyRoutingTicker.Reset();
		KeyRoutingSignatures.Reset();
		return false;
	}
	TArray<int32> ChangedUsers;
	TSet<int32> Users;
	FSlateApplication::Get().ForEachUser([this, &ChangedUsers, &Users](FSlateUser& User)
	{
		const int32 UserIndex = User.GetUserIndex();
		Users.Add(UserIndex);
		const FReciKeyRoutingSnapshot Snapshot = BuildKeyRoutingSnapshot(UserIndex, false);
		FKeyRoutingSignature Signature;
		Signature.Focused = User.GetFocusedWidget();
		Signature.TopPopupOwner = FindTopPopupOwner(UserIndex);
		// A dialog that becomes the active root can leave focus where it was; the hints of the layers beneath it change.
		const UCommonActivatableWidget* FocusLayer = UCommonUIActionRouterBase::FindActivatable(Signature.Focused.Pin(), nullptr);
		Signature.FocusLayer = FocusLayer;
		Signature.bFocusLayerInActiveRoot = !FocusLayer || GetRoutingEnvironment().IsInActiveRoot(*FocusLayer);
		Signature.bMenuInput = Snapshot.bMenuInput;
		Signature.bTextEditing = Snapshot.bTextEditing;
		Signature.bSelectingKey = Snapshot.bSelectingKey;
		for (const FReciKeyRoutingSnapshot::FScreen& Screen : Snapshot.Screens)
		{
			Signature.Screens.Add(Screen.Screen);
			Signature.Answers.Add(Screen.bCapturing);
			for (const FReciKeyRoutingSnapshot::FAction& Action : Screen.Actions)
			{
				Signature.Answers.Add(Action.bSuspended);
			}
		}
		const FKeyRoutingSignature* Previous = KeyRoutingSignatures.Find(UserIndex);
		if (!Previous || !(*Previous == Signature))
		{
			KeyRoutingSignatures.Add(UserIndex, MoveTemp(Signature));
			ChangedUsers.Add(UserIndex);
		}
	});
	for (auto It = KeyRoutingSignatures.CreateIterator(); It; ++It)
	{
		if (!Users.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	// After every user is read. Every listener refreshes from the same routing, so it is read once; a listener only
	// refreshes a hint, which changes no routing.
	for (const int32 UserIndex : ChangedUsers)
	{
		BroadcastSnapshot = MakeShared<FReciKeyRoutingSnapshot>(BuildKeyRoutingSnapshot(UserIndex, true));
		BroadcastUser = UserIndex;
		OnKeyRoutingChanged.Broadcast(UserIndex);
		BroadcastSnapshot.Reset();
		BroadcastUser = INDEX_NONE;
	}
	return true;
}

double FReciInputFocusCoordinator::ResolveEventTime(const FInputEvent& Event, const FKey& Key, const bool bIsRepeat) const
{
	return EventTimeResolver ? EventTimeResolver(Event, Key, bIsRepeat) : GetSlateEventSeconds(Event);
}

FReciInputFocusCoordinator::FTargetEntry*
FReciInputFocusCoordinator::FindFocusedTarget(const uint32 UserIndex)
{
	CompactTargets();
	for (int32 EntryIndex = Targets.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		FTargetEntry& Entry = Targets[EntryIndex];
		if (IsTargetUsable(Entry)
			&& Entry.Target->GetReciInputUserIndex()
				== static_cast<int32>(UserIndex)
			// While a popup is open, only the controls inside it take focused input: a list holding the popup's
			// dropdown also holds its focus, and must not take the popup's keys whatever order they registered in.
			&& !IsBelowOpenPopup(Entry)
			&& Entry.Target->CanHandleReciFocusedInput())
		{
			return &Entry;
		}
	}
	return nullptr;
}

FReciInputFocusCoordinator::FTargetEntry*
FReciInputFocusCoordinator::FindPointerTarget(
	FSlateApplication& SlateApp,
	const FVector2D& ScreenPosition,
	const uint32 UserIndex)
{
	CompactTargets();
	// No hit test while none of the user's controls could take the pointer: none is registered, or each is hidden or
	// disabled (principle 4).
	if (!Targets.ContainsByPredicate([this, UserIndex](const FTargetEntry& Entry)
		{
			return IsTargetUsable(Entry) && Entry.Target->GetReciInputUserIndex() == static_cast<int32>(UserIndex);
		}))
	{
		return nullptr;
	}
	const FWidgetPath HitPath = SlateApp.LocateWindowUnderMouse(
		ScreenPosition,
		SlateApp.GetInteractiveTopLevelWindows(),
		false,
		static_cast<int32>(UserIndex));
	if (!HitPath.IsValid())
	{
		return nullptr;
	}

	// An open popup is the top layer: the lists beneath it never take its pointer input.
	const UWidget* PopupOwner = FindTopPopupOwner(UserIndex);

	for (int32 EntryIndex = Targets.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		FTargetEntry& Entry = Targets[EntryIndex];
		const bool bTargetUsable = IsTargetUsable(Entry);
		const bool bUserMatches =
			Entry.Target->GetReciInputUserIndex()
				== static_cast<int32>(UserIndex);
		if (!bTargetUsable || !bUserMatches)
		{
			continue;
		}
		if (PopupOwner && !Entry.Widget->IsIn(PopupOwner))
		{
			continue;
		}

		UWidget* Surface = const_cast<UWidget*>(
			Entry.Target->GetReciInputSurfaceWidget());
		TSharedPtr<SWidget> SurfaceSlate;
		if (Surface)
		{
			SurfaceSlate = Surface->TakeWidget();
		}
		const bool bGeometryContains = Surface
			&& Surface->GetCachedGeometry().IsUnderLocation(ScreenPosition);
		const bool bPathContains = SurfaceSlate.IsValid()
			&& HitPath.ContainsWidget(SurfaceSlate.Get());
		const bool bFallsThroughToViewport =
			HitPath.GetLastWidget()->GetType() == FName(TEXT("SViewport"));
		if (Surface && SurfaceSlate.IsValid()
			&& bGeometryContains
			&& (bPathContains || bFallsThroughToViewport))
		{
			return &Entry;
		}
	}
	return nullptr;
}

bool FReciInputFocusCoordinator::IsTargetUsable(
	const FTargetEntry& Entry) const
{
	const UWidget* Widget = Entry.Widget.Get();
	const UWidget* Surface = Entry.Target
		? Entry.Target->GetReciInputSurfaceWidget()
		: nullptr;
	return Widget
		&& Surface
		&& Widget->GetIsEnabled()
		&& Widget->IsVisible()
		&& Surface->GetIsEnabled()
		&& Surface->IsVisible();
}

void FReciInputFocusCoordinator::ReturnFocusToDrivenItem(FTargetEntry& Entry)
{
	if (Entry.Target && Entry.Widget.IsValid()
		// A list beneath an open popup must not pull focus back out of the popup (Accept, pointer leaving it).
		&& !IsBelowOpenPopup(Entry))
	{
		Entry.Target->ReturnReciFocusToDrivenItem();
	}
}

bool FReciInputFocusCoordinator::HasPointerPositionChanged(
	const FPointerIdentity& Pointer,
	const FVector2D& ScreenPosition)
{
	if (const FVector2D* PreviousPosition =
		LastPointerPositions.Find(Pointer))
	{
		const bool bChanged =
			!PreviousPosition->Equals(ScreenPosition, 0.1f);
		LastPointerPositions.Add(Pointer, ScreenPosition);
		return bChanged;
	}

	LastPointerPositions.Add(Pointer, ScreenPosition);
	return true;
}

void FReciInputFocusCoordinator::RecordPointerPosition(
	const FPointerIdentity& Pointer,
	const FVector2D& ScreenPosition)
{
	LastPointerPositions.Add(Pointer, ScreenPosition);
}

void FReciInputFocusCoordinator::ClearCapturedTarget()
{
	CapturedWidget.Reset();
	CapturedTarget = nullptr;
	CapturedPointer = FPointerIdentity();
}
