// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Input/ReciScreenInputReports.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActionWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Containers/Ticker.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Input/ReciScreenInputMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Interaction/ReciRoutingEnvironment.h"
#include "ReciReport.h"
#include "UObject/ObjectKey.h"
#include "Widgets/ReciFocusStyleButtonBase.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciMappedKeyText.h"

namespace UE::ReciCommonUI::Private
{
	namespace
	{
		bool IsDirectionalKey(const FKey& Key, const FNavigationConfig* Config, const uint32 UserIndex)
		{
			if (IsStickDirectionKey(Key))
			{
				return true;
			}
			if (!Config)
			{
				return false;
			}
			const EUINavigation Direction =
				Config->GetNavigationDirectionFromKey(FKeyEvent(Key, FModifierKeysState(), UserIndex, false, 0, 0));
			return Direction == EUINavigation::Up
				|| Direction == EUINavigation::Down
				|| Direction == EUINavigation::Left
				|| Direction == EUINavigation::Right;
		}

		bool IsKeyboardKey(const FKey& Key)
		{
			return Key.IsValid() && !Key.IsGamepadKey() && !Key.IsMouseButton() && !Key.IsTouch()
				&& !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D();
		}

		/** The action's keys: the player's bindings, and the keys the screen's Input Mapping authors for it. */
		TSet<FKey> GetReachingKeys(
			const UCommonActivatableWidget& Screen,
			const UInputMappingContext* Mapping,
			const UInputAction& Action,
			const IReciRoutingEnvironment& Environment)
		{
			TSet<FKey> Keys(Environment.GetActionKeys(Screen, Action));
			if (Mapping)
			{
				for (const FEnhancedActionKeyMapping& KeyMapping : Mapping->GetMappings())
				{
					if (KeyMapping.Action == &Action)
					{
						Keys.Add(KeyMapping.Key);
					}
				}
			}
			return Keys;
		}

		bool HasKeyOnBothDevices(const TSet<FKey>& Keys)
		{
			bool bKeyboard = false;
			bool bGamepad = false;
			for (const FKey& Key : Keys)
			{
				bKeyboard |= IsKeyboardKey(Key);
				bGamepad |= Key.IsGamepadKey();
			}
			return bKeyboard && bGamepad;
		}

		/** The bound widget a button's Blueprint names, found the way the Blueprint compiler binds it: by name. */
		template <typename TWidget>
		const TWidget* FindNamedWidget(const UUserWidget& Widget, const TCHAR* Name)
		{
			return Widget.WidgetTree ? Cast<TWidget>(Widget.WidgetTree->FindWidget(FName(Name))) : nullptr;
		}

		struct FScreenWidgets
		{
			/** CommonUI buttons outside the screen's Reci lists. */
			TArray<UCommonButtonBase*> Controls;
			/** The actions some widget of the screen shows the keys of. */
			TSet<const UInputAction*> HintedActions;
		};

		/**
		 * Collects the screen's buttons and hinted actions, through the trees of the user widgets in it. An activatable
		 * widget nested in the screen is left to its own registration. Buttons in a Reci list are reached through the
		 * list or the screen's actions, which this check cannot see.
		 */
		void CollectScreenWidgets(UWidget* Widget, const bool bInsideList, FScreenWidgets& Out)
		{
			if (!Widget || Widget->IsA<UCommonActivatableWidget>())
			{
				return;
			}
			const bool bInList = bInsideList || Widget->IsA<UReciListBase>();
			if (const UReciMappedKeyText* KeyText = Cast<UReciMappedKeyText>(Widget))
			{
				Out.HintedActions.Add(KeyText->GetEnhancedInputAction());
				for (const UInputAction* Action : KeyText->AdditionalInputActions)
				{
					Out.HintedActions.Add(Action);
				}
			}
			if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget))
			{
				if (!bInList)
				{
					Out.Controls.Add(Button);
				}
				// A Reci button's Input Action Widget shows its Triggering Input Action, else its Hint Input Action.
				const UReciFocusStyleButtonBase* ReciButton = Cast<UReciFocusStyleButtonBase>(Button);
				if (ReciButton && FindNamedWidget<UCommonActionWidget>(*ReciButton, TEXT("InputActionWidget")))
				{
					Out.HintedActions.Add(ReciButton->GetEnhancedInputAction());
					Out.HintedActions.Add(ReciButton->HintInputAction);
				}
			}
			if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
			{
				CollectScreenWidgets(UserWidget->GetRootWidget(), bInList, Out);
			}
			if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
				{
					CollectScreenWidgets(Panel->GetChildAt(Index), bInList, Out);
				}
			}
		}

		/** Whether the button has its own key on keyboard and gamepad and a widget that shows it (principle 13). */
		bool HasHintedOwnKey(
			const UCommonButtonBase& Button,
			const UCommonActivatableWidget& Screen,
			const UInputMappingContext* Mapping,
			const IReciRoutingEnvironment& Environment)
		{
			const UReciFocusStyleButtonBase* ReciButton = Cast<UReciFocusStyleButtonBase>(&Button);
			const bool bShowsKey = FindNamedWidget<UCommonActionWidget>(Button, TEXT("InputActionWidget")) != nullptr;
			if (!bShowsKey)
			{
				return false;
			}
			const UInputAction* OwnAction = Button.GetEnhancedInputAction();
			if (!OwnAction && ReciButton)
			{
				OwnAction = ReciButton->HintInputAction;
			}
			if (!OwnAction)
			{
				// A data-table Triggering Input Action: its keys come from CommonUI's input data, which this check does not read.
				FDataTableRowHandle InputActionRow;
				return Button.GetInputAction(InputActionRow) && !InputActionRow.IsNull();
			}
			return HasKeyOnBothDevices(GetReachingKeys(Screen, Mapping, *OwnAction, Environment));
		}

		FString JoinNames(const TArray<TObjectPtr<UWidget>>& Widgets)
		{
			return FString::JoinBy(Widgets, TEXT(", "), [](const TObjectPtr<UWidget>& Widget) { return GetNameSafe(Widget); });
		}

		FString JoinNames(const TArray<TObjectPtr<UInputAction>>& Actions)
		{
			return FString::JoinBy(Actions, TEXT(", "), [](const TObjectPtr<UInputAction>& Action) { return GetNameSafe(Action); });
		}

		void ReportIgnoredTriggersAndModifiers(const UCommonActivatableWidget& Screen)
		{
			const UInputMappingContext* Mapping = GetScreenInputMapping(Screen);
			if (!Mapping)
			{
				return;
			}
			for (const FEnhancedActionKeyMapping& KeyMapping : Mapping->GetMappings())
			{
				const UInputAction* Action = KeyMapping.Action;
				if (!Action)
				{
					continue;
				}
				const bool bHasSetup = !KeyMapping.Triggers.IsEmpty() || !KeyMapping.Modifiers.IsEmpty()
					|| !Action->Triggers.IsEmpty() || !Action->Modifiers.IsEmpty();
				if (bHasSetup)
				{
					RECI_WARN_ONCE(TEXT("IgnoredScreenInputSetup"), Mapping->GetPathName() + TEXT(" ") + Action->GetPathName(),
						TEXT("%s: Reci screen input ignores the triggers and modifiers of %s in %s; HandleReciScreenAction "
							"receives it on every key-down and repeat (check bIsRepeat)."),
						*Screen.GetClass()->GetName(), *Action->GetName(), *Mapping->GetName());
				}
			}
		}

		void ReportReachability(const UCommonActivatableWidget& Screen, const IReciRoutingEnvironment& Environment)
		{
			// Once per screen class: the check walks the screen's tree, which a screen class builds the same way each time.
			if (!ShouldReportOnce(TEXT("ScreenReachabilityChecked"), Screen.GetClass()->GetPathName()))
			{
				return;
			}
			const FReciScreenReachability Reachability = CheckScreenReachability(Screen, Environment);
			if (Reachability.ControlsWithoutKey.IsEmpty() && Reachability.ActionsMissingDevice.IsEmpty()
				&& Reachability.ActionsWithoutHint.IsEmpty())
			{
				return;
			}
			FString Problems;
			if (!Reachability.ControlsWithoutKey.IsEmpty())
			{
				Problems += TEXT(" Controls without their own hinted key on keyboard and gamepad: ")
					+ JoinNames(Reachability.ControlsWithoutKey) + TEXT(".");
			}
			if (!Reachability.ActionsMissingDevice.IsEmpty())
			{
				Problems += TEXT(" Actions without a keyboard key or without a gamepad key: ")
					+ JoinNames(Reachability.ActionsMissingDevice) + TEXT(".");
			}
			if (!Reachability.ActionsWithoutHint.IsEmpty())
			{
				Problems += TEXT(" Actions no key hint shows: ") + JoinNames(Reachability.ActionsWithoutHint) + TEXT(".");
			}
			UE_LOG(LogReciCommonUI, Warning,
				TEXT("%s takes directional keys, so each of its controls and actions needs its own key on keyboard and "
					"gamepad with a visible hint (Reci Common UI principle 13; Check Screen Reachability).%s"),
				*Screen.GetClass()->GetName(), *Problems);
		}
	}

	FReciScreenReachability CheckScreenReachability(
		const UCommonActivatableWidget& Screen,
		const IReciRoutingEnvironment& Environment)
	{
		FReciScreenReachability Result;
		const UInputMappingContext* Mapping = GetScreenInputMapping(Screen);
		if (!Mapping)
		{
			return Result;
		}
		// Without a player, Slate's own navigation config, as Slate navigates a user that has none.
		const TOptional<int32> UserIndex = Environment.GetUserIndex(Screen);
		TSharedPtr<FNavigationConfig> Config;
		if (UserIndex.IsSet())
		{
			Config = ResolveNavigationConfig(UserIndex.GetValue());
		}
		else if (FSlateApplication::IsInitialized())
		{
			Config = FSlateApplication::Get().GetNavigationConfig();
		}
		// The authored keys, known at registration although Enhanced Input may apply the mapping context a tick later.
		for (const FEnhancedActionKeyMapping& KeyMapping : Mapping->GetMappings())
		{
			if (KeyMapping.Action && IsDirectionalKey(KeyMapping.Key, Config.Get(), UserIndex.Get(0)))
			{
				Result.bTakesDirectionalKeys = true;
				break;
			}
		}
		if (!Result.bTakesDirectionalKeys)
		{
			return Result;
		}

		FScreenWidgets Widgets;
		CollectScreenWidgets(Screen.GetRootWidget(), false, Widgets);
		for (UCommonButtonBase* Button : Widgets.Controls)
		{
			if (!HasHintedOwnKey(*Button, Screen, Mapping, Environment))
			{
				Result.ControlsWithoutKey.Add(Button);
			}
		}
		for (UInputAction* Action : GetScreenActions(Screen))
		{
			if (!HasKeyOnBothDevices(GetReachingKeys(Screen, Mapping, *Action, Environment)))
			{
				Result.ActionsMissingDevice.Add(Action);
			}
			if (!Widgets.HintedActions.Contains(Action))
			{
				Result.ActionsWithoutHint.Add(Action);
			}
		}
		return Result;
	}

	void ReportScreenInputSetup(const UCommonActivatableWidget& Screen, const IReciRoutingEnvironment& Environment)
	{
		ReportIgnoredTriggersAndModifiers(Screen);
		ReportReachability(Screen, Environment);
	}

#if !UE_BUILD_SHIPPING
	namespace
	{
		/**
		 * Binds once to the activation of each rebuilt activatable widget that implements screen input, and on the tick
		 * after it activates reports it if it is still activated and not registered. The tick's delay lets a handler of
		 * On Widget Activated, which CommonUI broadcasts after On Activated, register it too.
		 */
		class FUnregisteredScreenReports
		{
		public:
			static FUnregisteredScreenReports& Get()
			{
				static FUnregisteredScreenReports Reports;
				return Reports;
			}

			void Start()
			{
				if (!RebuildingHandle.IsValid())
				{
					RebuildingHandle = UCommonActivatableWidget::OnRebuilding.AddRaw(this, &FUnregisteredScreenReports::HandleRebuilding);
				}
			}

			void Stop()
			{
				UCommonActivatableWidget::OnRebuilding.Remove(RebuildingHandle);
				RebuildingHandle.Reset();
				// Only this report's own bindings: other listeners of the widget keep theirs.
				for (const TPair<TObjectKey<UCommonActivatableWidget>, FDelegateHandle>& Binding : Bindings)
				{
					if (UCommonActivatableWidget* Widget = Binding.Key.ResolveObjectPtr())
					{
						Widget->OnActivated().Remove(Binding.Value);
					}
				}
				Bindings.Reset();
				CancelTicker();
				Pending.Reset();
			}

			void RunChecks()
			{
				CancelTicker();
				const TArray<TWeakObjectPtr<UCommonActivatableWidget>> Screens = MoveTemp(Pending);
				Pending.Reset();
				for (const TWeakObjectPtr<UCommonActivatableWidget>& WeakScreen : Screens)
				{
					const UCommonActivatableWidget* Screen = WeakScreen.Get();
					if (Screen && Screen->IsActivated() && !FReciInputFocusCoordinator::Get().IsScreenInputRegistered(*Screen))
					{
						RECI_WARN_ONCE(TEXT("UnregisteredScreen"), Screen->GetClass()->GetPathName(),
							TEXT("%s implements Reci Screen Input but did not call Register Screen Input when it activated; "
								"it receives no screen input."),
							*Screen->GetClass()->GetName());
					}
				}
			}

			TArray<TWeakObjectPtr<UCommonActivatableWidget>> ExchangePending(TArray<TWeakObjectPtr<UCommonActivatableWidget>> InPending)
			{
				Swap(Pending, InPending);
				if (Pending.IsEmpty())
				{
					CancelTicker();
				}
				else
				{
					ScheduleTicker();
				}
				return InPending;
			}

		private:
			void HandleRebuilding(UCommonActivatableWidget& Widget)
			{
				if (!Widget.GetClass()->ImplementsInterface(UReciScreenInput::StaticClass()))
				{
					return;
				}
				const TObjectKey<UCommonActivatableWidget> Key(&Widget);
				if (Bindings.Contains(Key))
				{
					return;
				}
				// Forget the widgets that are gone, so the map holds only live screens.
				for (auto Iterator = Bindings.CreateIterator(); Iterator; ++Iterator)
				{
					if (!Iterator.Key().ResolveObjectPtr())
					{
						Iterator.RemoveCurrent();
					}
				}
				Bindings.Add(Key, Widget.OnActivated().AddRaw(this, &FUnregisteredScreenReports::HandleActivated,
					TWeakObjectPtr<UCommonActivatableWidget>(&Widget)));
			}

			void HandleActivated(const TWeakObjectPtr<UCommonActivatableWidget> Widget)
			{
				Pending.AddUnique(Widget);
				ScheduleTicker();
			}

			void ScheduleTicker()
			{
				if (!TickerHandle.IsValid())
				{
					TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
					{
						TickerHandle.Reset();
						RunChecks();
						return false;
					}));
				}
			}

			void CancelTicker()
			{
				if (TickerHandle.IsValid())
				{
					FTSTicker::RemoveTicker(TickerHandle);
					TickerHandle.Reset();
				}
			}

			FDelegateHandle RebuildingHandle;
			TMap<TObjectKey<UCommonActivatableWidget>, FDelegateHandle> Bindings;
			TArray<TWeakObjectPtr<UCommonActivatableWidget>> Pending;
			FTSTicker::FDelegateHandle TickerHandle;
		};
	}

	void StartUnregisteredScreenReports()
	{
		FUnregisteredScreenReports::Get().Start();
	}

	void StopUnregisteredScreenReports()
	{
		FUnregisteredScreenReports::Get().Stop();
	}

#if WITH_DEV_AUTOMATION_TESTS
	void RunUnregisteredScreenChecksForTests()
	{
		FUnregisteredScreenReports::Get().RunChecks();
	}

	TArray<TWeakObjectPtr<UCommonActivatableWidget>> ExchangePendingScreenChecksForTests(
		TArray<TWeakObjectPtr<UCommonActivatableWidget>> Pending)
	{
		return FUnregisteredScreenReports::Get().ExchangePending(MoveTemp(Pending));
	}
#endif
#endif
}
