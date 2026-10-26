// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSettings.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "ICommonInputModule.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "ReciDemoTestComposer.h"
#include "Tests/ReciTestHarness.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciMappedKeyText.h"

namespace
{
	UObject* GetDemoVariable(UObject& Object, const TCHAR* Name)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object.GetClass(), Name);
		return Property ? Property->GetObjectPropertyValue_InContainer(&Object) : nullptr;
	}

	bool HoldsFocus(const UWidget* Widget)
	{
		return Widget && (Widget->HasAnyUserFocus() || Widget->HasFocusedDescendants());
	}

	/** Makes Root CommonUI's active root for the harness, as CommonUI answers it: a widget is in it when Root is the widget or a Slate ancestor of it. */
	void SetActiveRoot(FReciTestRoutingEnvironment& Environment, const UCommonActivatableWidget& Root)
	{
		const TWeakObjectPtr<const UCommonActivatableWidget> WeakRoot = &Root;
		Environment.ActiveRoot = [WeakRoot](const UCommonActivatableWidget& Widget)
		{
			const TSharedPtr<SWidget> RootWidget = WeakRoot.IsValid() ? WeakRoot->GetCachedWidget() : nullptr;
			for (TSharedPtr<SWidget> Walker = Widget.GetCachedWidget(); RootWidget && Walker; Walker = Walker->GetParentWidget())
			{
				if (Walker == RootWidget)
				{
					return true;
				}
			}
			return false;
		};
	}

	/** Binds a Blueprint event dispatcher of Object to a handler of the stand-in composer, as Bind Event does. */
	bool BindDispatcher(UObject& Object, const TCHAR* Dispatcher, UReciDemoTestComposer& Composer, const FName Handler)
	{
		const FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(Object.GetClass(), Dispatcher);
		if (!Property)
		{
			return false;
		}
		FScriptDelegate Delegate;
		Delegate.BindUFunction(&Composer, Handler);
		Property->AddDelegate(MoveTemp(Delegate), &Object);
		return true;
	}

	/** Calls the screen's Show Dialog Choice, as the composer's Handle Dialog Closed does. */
	bool ShowDialogChoice(UObject& Screen, const bool bConfirmed)
	{
		UFunction* Function = Screen.FindFunction(TEXT("ShowDialogChoice"));
		if (!Function)
		{
			return false;
		}
		struct
		{
			bool bConfirmed = false;
		} ShowParameters;
		ShowParameters.bConfirmed = bConfirmed;
		Screen.ProcessEvent(Function, &ShowParameters);
		return true;
	}
}

// The demo's Modal page (Content/Demo/Pages/WBP_DemoModalPage) in the demo's screen (WBP_DemoScreen), laid out and painted
// in a stand-in for the demo's composer (WBP_DemoComposer): the screen on a Common Activatable Widget Stack, a Modal Stack
// over it, and handlers that do what the composer's graph does. Open dialog's On Dialog Requested reaches the screen's own
// On Dialog Requested, on which the composer pushes WBP_DemoConfirmModal onto the Modal Stack: an Is Modal Back handler
// whose Get Desired Focus Target is Cancel, its first action (M3 Dialogs: initial focus on the first interactive element).
// Back closes it as Cancel and OK closes it as OK; each time its On Closed reaches the screen's Show Dialog Choice, so the
// status line shows the choice and focus returns to Open dialog. While it is open, the screen stays on its stack, its hint
// line shows no key, and its hint rows hide but keep their room, so nothing beneath the dialog moves.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDemoModalPageTest, "ReciCommonUI.Demo.ModalPage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDemoModalPageTest::RunTest(const FString& Parameters)
{
	UClass* ScreenClass = LoadClass<UCommonActivatableWidget>(nullptr, TEXT("/ReciCommonUI/Demo/WBP_DemoScreen.WBP_DemoScreen_C"));
	UClass* DialogClass = LoadClass<UCommonActivatableWidget>(nullptr, TEXT("/ReciCommonUI/Demo/WBP_DemoConfirmModal.WBP_DemoConfirmModal_C"));
	// The composer adds the demo's input mapping, which the harness applies to its player as CommonUI applies a screen's.
	UInputMappingContext* Mapping = static_cast<UInputMappingContext*>(LoadObject<UObject>(nullptr, TEXT("/ReciCommonUI/Demo/Input/IMC_ReciDemo.IMC_ReciDemo")));
	if (!TestNotNull(TEXT("The screen's Blueprint class loads"), ScreenClass) || !TestNotNull(TEXT("and the dialog's"), DialogClass)
		|| !TestNotNull(TEXT("and the demo's input mapping"), Mapping))
	{
		return false;
	}
	// The buttons show the click action's key, which CommonUI's input subsystem loads when a local player starts; the
	// test has none.
	ICommonInputModule::GetSettings().LoadData();
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
	FReciInputMethod Keyboard;
	Keyboard.InputType = ECommonInputType::MouseAndKeyboard;
	Environment.InputMethod = Keyboard;
	UReciTestScreen* Composer = Harness.CreateScreen(TEXT("Composer"));
	Composer->SetInputMapping(Mapping);
	Harness.ActivateScreen(*Composer);
	SetActiveRoot(Environment, *Composer);
	UCommonActivatableWidgetStack* Stack =
		Composer->WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("Stack"));
	UCommonActivatableWidgetStack* ModalStack =
		Composer->WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("ModalStack"));
	Stack->SetTransitionDuration(0.0f);
	ModalStack->SetTransitionDuration(0.0f);
	Composer->GetContent()->AddChild(Stack);
	Composer->GetContent()->AddChild(ModalStack);
	UCommonActivatableWidget* Screen = Stack->AddWidget<UCommonActivatableWidget>(ScreenClass);
	if (!TestNotNull(TEXT("The Stack takes the screen"), Screen))
	{
		return false;
	}
	TestTrue(TEXT("and activates it"), Screen->IsActivated());

	// The composer's graph: On Dialog Requested pushes the dialog onto the Modal Stack and binds its On Closed, which
	// calls the screen's Show Dialog Choice.
	TStrongObjectPtr<UReciDemoTestComposer> Handlers(NewObject<UReciDemoTestComposer>());
	int32 Requests = 0;
	Handlers->OnDialogRequested = [this, &Requests, ModalStack, DialogClass, &Handlers]()
	{
		++Requests;
		UCommonActivatableWidget* Pushed = ModalStack->AddWidget<UCommonActivatableWidget>(DialogClass);
		TestTrue(TEXT("The dialog tells its choice through On Closed"),
			Pushed && BindDispatcher(*Pushed, TEXT("OnClosed"), *Handlers, GET_FUNCTION_NAME_CHECKED(UReciDemoTestComposer, HandleDialogClosed)));
	};
	Handlers->OnDialogClosed = [this, Screen](const bool bConfirmed)
	{
		TestTrue(TEXT("The screen has Show Dialog Choice"), ShowDialogChoice(*Screen, bConfirmed));
	};
	if (!TestTrue(TEXT("The screen asks for the dialog through On Dialog Requested"),
		BindDispatcher(*Screen, TEXT("OnDialogRequested"), *Handlers, GET_FUNCTION_NAME_CHECKED(UReciDemoTestComposer, HandleDialogRequested))))
	{
		return false;
	}

	// The Modal tab: the screen's tab list drives its switcher.
	UReciListBase* Pages = Cast<UReciListBase>(GetDemoVariable(*Screen, TEXT("Pages")));
	const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(GetDemoVariable(*Screen, TEXT("Switcher")));
	UUserWidget* Page = Cast<UUserWidget>(GetDemoVariable(*Screen, TEXT("ModalPage")));
	if (!TestNotNull(TEXT("The screen holds its tab list"), Pages) || !TestNotNull(TEXT("its switcher"), Switcher)
		|| !TestNotNull(TEXT("and the Modal page"), Page))
	{
		return false;
	}
	Harness.Advance(0.1f);
	TestTrue(TEXT("The tab list drives the Modal tab"), Pages->SetDrivenItemIndex(4, false));
	Harness.Advance(1.0f);
	TestTrue(TEXT("which shows the Modal page"), Switcher->GetActiveWidget() == Page);

	UWidget* OpenButton = Cast<UWidget>(GetDemoVariable(*Page, TEXT("OpenButton")));
	const UTextBlock* Status = Cast<UTextBlock>(GetDemoVariable(*Page, TEXT("StatusText")));
	const UUserWidget* BackHint = Cast<UUserWidget>(Page->GetWidgetFromName(TEXT("BackHint")));
	UReciMappedKeyText* BackKeys = BackHint ? Cast<UReciMappedKeyText>(BackHint->GetWidgetFromName(TEXT("Keys"))) : nullptr;
	if (!TestNotNull(TEXT("The page holds Open dialog"), OpenButton) || !TestNotNull(TEXT("its status line"), Status)
		|| !TestNotNull(TEXT("and its Back hint row"), BackKeys))
	{
		return false;
	}
	const auto Hint = [BackKeys]()
	{
		BackKeys->RefreshMappedKeyText();
		return BackKeys->ShowsKeys() ? BackKeys->GetMappedKeyText().ToString() : FString(TEXT("-"));
	};
	const auto Press = [&Harness](const FKey& Key)
	{
		Harness.SlateKeyDown(Key);
		Harness.SlateKeyUp(Key);
		Harness.Advance(0.1f);
	};
	const FString ScreenHint = Hint();
	// How far down the status line sits: below the hint rows and the action bar, it moves up if any of them gives up its room.
	const auto StatusTop = [Status]() { return Status->GetCachedGeometry().GetAbsolutePosition().Y; };
	const float ScreenStatusTop = StatusTop();
	TestEqual(TEXT("The status line starts with no choice"), Status->GetText().ToString(), FString(TEXT("No choice yet.")));
	TestNotEqual(TEXT("The screen's hint line shows the Back key"), ScreenHint, FString(TEXT("-")));

	// Open dialog: the composer pushes the dialog onto its Modal Stack, Is Modal, and focus lands on Cancel, its first action.
	const auto Open = [this, &Harness, &Environment, &Press, &Requests, Stack, ModalStack, Screen, OpenButton, DialogClass]() -> UCommonActivatableWidget*
	{
		TestTrue(TEXT("Focus reaches Open dialog"), Harness.Focus(*OpenButton));
		const int32 RequestsBefore = Requests;
		Press(EKeys::Enter);
		TestEqual(TEXT("Accept on Open dialog asks for the dialog once"), Requests, RequestsBefore + 1);
		UCommonActivatableWidget* Dialog = ModalStack->GetActiveWidget();
		if (!TestNotNull(TEXT("and the composer pushes it onto its Modal Stack"), Dialog))
		{
			return nullptr;
		}
		TestTrue(TEXT("The pushed widget is the demo's dialog"), Dialog->IsA(DialogClass));
		TestTrue(TEXT("The dialog is activated"), Dialog->IsActivated());
		TestTrue(TEXT("The dialog is Is Modal"), Dialog->IsModal());
		TestTrue(TEXT("The screen stays on its Stack, activated"), Stack->GetNumWidgets() == 1 && Stack->GetActiveWidget() == Screen && Screen->IsActivated());
		// CommonUI focuses the leaf-most active widget's desired focus target; the harness answers CommonUI's active root
		// itself, so the test does both.
		SetActiveRoot(Environment, *Dialog);
		UWidget* Target = Dialog->GetDesiredFocusTarget();
		TestTrue(TEXT("The dialog's desired focus target is Cancel"), Target && Target == Dialog->GetWidgetFromName(TEXT("Cancel")));
		if (Target)
		{
			Harness.Focus(*Target);
		}
		Harness.Advance(0.1f);
		return Dialog;
	};

	UCommonActivatableWidget* Dialog = Open();
	if (!Dialog)
	{
		return false;
	}
	UWidget* OK = Dialog->GetWidgetFromName(TEXT("OK"));
	UWidget* Cancel = Dialog->GetWidgetFromName(TEXT("Cancel"));
	TestTrue(TEXT("Opening the dialog focuses Cancel"), HoldsFocus(Cancel));
	TestEqual(TEXT("Beneath the dialog the screen's hint line shows no key"), Hint(), FString(TEXT("-")));
	Harness.Advance(0.1f);
	TestEqual(TEXT("and its hint row hides"), BackHint->GetVisibility(), ESlateVisibility::Hidden);
	TestEqual(TEXT("but keeps its room: the status line stays put"), StatusTop(), ScreenStatusTop);
	Press(EKeys::Right);
	TestTrue(TEXT("Right moves to OK"), HoldsFocus(OK));
	Press(EKeys::Tab);
	TestTrue(TEXT("Tab comes round to Cancel"), HoldsFocus(Cancel));

	// Back closes it as Cancel, as CommonUI's Back binding asks the Back handler.
	UFunction* HandleBack = Dialog->FindFunction(TEXT("BP_OnHandleBackAction"));
	if (!TestNotNull(TEXT("The dialog handles Back"), HandleBack))
	{
		return false;
	}
	struct
	{
		bool ReturnValue = false;
	} BackParameters;
	Dialog->ProcessEvent(HandleBack, &BackParameters);
	SetActiveRoot(Environment, *Composer);
	Harness.Advance(0.1f);
	TestTrue(TEXT("The dialog takes Back"), BackParameters.ReturnValue);
	TestFalse(TEXT("Back deactivates the dialog"), Dialog->IsActivated());
	TestEqual(TEXT("and the Modal Stack lets it go"), ModalStack->GetNumWidgets(), 0);
	TestEqual(TEXT("The status line shows Cancel"), Status->GetText().ToString(), FString(TEXT("Last choice: Cancel")));
	TestTrue(TEXT("Focus returns to Open dialog"), HoldsFocus(OpenButton));
	TestEqual(TEXT("The screen's hint line shows its key again"), Hint(), ScreenHint);
	Harness.Advance(0.1f);
	TestEqual(TEXT("and its hint row shows again"), BackHint->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("The status line is where it was"), StatusTop(), ScreenStatusTop);

	// OK closes it as OK.
	Dialog = Open();
	if (!Dialog)
	{
		return false;
	}
	OK = Dialog->GetWidgetFromName(TEXT("OK"));
	Cancel = Dialog->GetWidgetFromName(TEXT("Cancel"));
	TestTrue(TEXT("Focus is on Cancel again"), HoldsFocus(Cancel));
	Press(EKeys::Right);
	TestTrue(TEXT("Right moves to OK again"), HoldsFocus(OK));
	Press(EKeys::Enter);
	SetActiveRoot(Environment, *Composer);
	Harness.Advance(0.1f);
	TestFalse(TEXT("OK deactivates the dialog"), Dialog->IsActivated());
	TestEqual(TEXT("The status line shows OK"), Status->GetText().ToString(), FString(TEXT("Last choice: OK")));
	TestTrue(TEXT("Focus returns to Open dialog after OK"), HoldsFocus(OpenButton));
	TestEqual(TEXT("The composer was asked for the dialog twice in all"), Requests, 2);
	return true;
}

#endif
