// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Tests/ReciTestHarness.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputAction.h"

namespace
{
	UOverlay* GetOrCreateOverlayRoot(UWidgetTree& Tree, const FName Name)
	{
		if (!Tree.RootWidget)
		{
			Tree.RootWidget = Tree.ConstructWidget<UOverlay>(UOverlay::StaticClass(), Name);
		}
		return Cast<UOverlay>(Tree.RootWidget);
	}

	template <typename TProperty>
	TProperty* FindParameter(const UFunction& Function, const TCHAR* Name)
	{
		TProperty* Property = CastField<TProperty>(Function.FindPropertyByName(Name));
		ensureMsgf(Property, TEXT("%s has no parameter %s."), *Function.GetName(), Name);
		return Property;
	}

	void SetReturnValue(const UFunction& Function, void* Parameters, const bool bValue)
	{
		if (const FBoolProperty* ReturnValue = FindParameter<FBoolProperty>(Function, TEXT("ReturnValue")))
		{
			ReturnValue->SetPropertyValue_InContainer(Parameters, bValue);
		}
	}

	FString DescribeRepeat(const bool bIsRepeat)
	{
		return bIsRepeat ? TEXT(" repeat") : TEXT("");
	}
}

UOverlay* UReciTestScreen::GetContent()
{
	return GetOrCreateOverlayRoot(*WidgetTree, TEXT("Content"));
}

void UReciTestScreen::ProcessEvent(UFunction* Function, void* Parameters)
{
	const FName Name = Function ? Function->GetFName() : NAME_None;
	if (Name == GET_FUNCTION_NAME_CHECKED(IReciScreenInput, HandleReciScreenKey))
	{
		const FStructProperty* KeyParameter = FindParameter<FStructProperty>(*Function, TEXT("Key"));
		const FBoolProperty* RepeatParameter = FindParameter<FBoolProperty>(*Function, TEXT("bIsRepeat"));
		if (!KeyParameter || !RepeatParameter)
		{
			return;
		}
		const FKey Key = *KeyParameter->ContainerPtrToValuePtr<FKey>(Parameters);
		const bool bIsRepeat = RepeatParameter->GetPropertyValue_InContainer(Parameters);
		if (Log)
		{
			Log->Add(*this, TEXT("Key ") + Key.ToString() + DescribeRepeat(bIsRepeat));
		}
		if (OnCall)
		{
			OnCall(Name);
		}
		SetReturnValue(*Function, Parameters, HandledKeys.Contains(Key));
		return;
	}
	if (Name == GET_FUNCTION_NAME_CHECKED(IReciScreenInput, HandleReciScreenAction))
	{
		const FObjectProperty* ActionParameter = FindParameter<FObjectProperty>(*Function, TEXT("Action"));
		const FBoolProperty* RepeatParameter = FindParameter<FBoolProperty>(*Function, TEXT("bIsRepeat"));
		const FDoubleProperty* TimeParameter = FindParameter<FDoubleProperty>(*Function, TEXT("TimestampSeconds"));
		if (!ActionParameter || !RepeatParameter || !TimeParameter)
		{
			return;
		}
		UInputAction* Action = Cast<UInputAction>(ActionParameter->GetObjectPropertyValue_InContainer(Parameters));
		const bool bIsRepeat = RepeatParameter->GetPropertyValue_InContainer(Parameters);
		ActionTimestamps.Add(TimeParameter->GetPropertyValue_InContainer(Parameters));
		if (Log)
		{
			Log->Add(*this, TEXT("Action ") + GetNameSafe(Action) + DescribeRepeat(bIsRepeat));
		}
		if (OnCall)
		{
			OnCall(Name);
		}
		SetReturnValue(*Function, Parameters, HandledActions.Contains(Action));
		return;
	}
	if (Name == GET_FUNCTION_NAME_CHECKED(IReciScreenInput, IsReciScreenActionSuspended))
	{
		const FObjectProperty* ActionParameter = FindParameter<FObjectProperty>(*Function, TEXT("Action"));
		if (!ActionParameter)
		{
			return;
		}
		UInputAction* Action = Cast<UInputAction>(ActionParameter->GetObjectPropertyValue_InContainer(Parameters));
		SetReturnValue(*Function, Parameters, SuspendedActions.Contains(Action));
		return;
	}
	if (Name == GET_FUNCTION_NAME_CHECKED(IReciScreenInput, IsCapturingReciScreenKeys))
	{
		if (Log)
		{
			Log->Add(*this, TEXT("Capturing"));
		}
		if (OnCall)
		{
			OnCall(Name);
		}
		SetReturnValue(*Function, Parameters, bCapturingKeys);
		return;
	}
	Super::ProcessEvent(Function, Parameters);
}

void UReciTestFocusTarget::Register()
{
	FReciInputFocusCoordinator::Get().RegisterTarget(this, this);
}

void UReciTestFocusTarget::Unregister()
{
	FReciInputFocusCoordinator::Get().UnregisterTarget(this);
}

void UReciTestFocusTarget::SetContent(UWidget* Content)
{
	GetBounds()->SetContent(Content);
}

int32 UReciTestFocusTarget::GetReciInputUserIndex() const
{
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	const TSharedPtr<const FSlateUser> User = Player ? Player->GetSlateUser() : nullptr;
	return User ? User->GetUserIndex() : 0;
}

bool UReciTestFocusTarget::CanHandleReciFocusedInput() const
{
	return HasAnyUserFocus() || HasFocusedDescendants();
}

bool UReciTestFocusTarget::HandleReciNavigation(const EUINavigation Direction)
{
	if (Log)
	{
		Log->Add(*this, TEXT("Navigate ") + StaticEnum<EUINavigation>()->GetNameStringByValue(static_cast<int64>(Direction)));
	}
	return HandledDirections.Contains(Direction);
}

bool UReciTestFocusTarget::HandleReciAccept()
{
	if (Log)
	{
		Log->Add(*this, TEXT("Accept"));
	}
	return bHandlesAccept;
}

bool UReciTestFocusTarget::HandleReciMouseWheel(const float WheelDelta, const FVector2D& ScreenPosition)
{
	if (Log)
	{
		Log->Add(*this, TEXT("Wheel"));
	}
	return bHandlesPointer;
}

bool UReciTestFocusTarget::HandleReciPointerDown(const FGeometry& SurfaceGeometry, const FPointerEvent& PointerEvent)
{
	if (Log)
	{
		Log->Add(*this, TEXT("PointerDown"));
	}
	bHasPointerCapture = bHandlesPointer;
	return bHandlesPointer;
}

bool UReciTestFocusTarget::HandleReciPointerMove(const FGeometry& SurfaceGeometry, const FPointerEvent& PointerEvent)
{
	if (Log)
	{
		Log->Add(*this, TEXT("PointerMove"));
	}
	return bHasPointerCapture;
}

bool UReciTestFocusTarget::HandleReciPointerUp(const FPointerEvent& PointerEvent)
{
	if (Log)
	{
		Log->Add(*this, TEXT("PointerUp"));
	}
	const bool bHadPointerCapture = bHasPointerCapture;
	bHasPointerCapture = false;
	return bHadPointerCapture;
}

void UReciTestFocusTarget::HandleReciPointerCaptureLost()
{
	if (Log)
	{
		Log->Add(*this, TEXT("CaptureLost"));
	}
	bHasPointerCapture = false;
}

void UReciTestFocusTarget::HandleReciPointerHover(const FVector2D& ScreenPosition)
{
	if (Log)
	{
		Log->Add(*this, TEXT("Hover"));
	}
}

void UReciTestFocusTarget::ReturnReciFocusToDrivenItem()
{
	if (Log)
	{
		Log->Add(*this, TEXT("ReturnFocus"));
	}
}

void UReciTestFocusTarget::LogKeySelected(const FInputChord SelectedKey)
{
	if (Log)
	{
		Log->Add(*this, TEXT("KeySelected ") + SelectedKey.Key.ToString());
	}
}

USizeBox* UReciTestFocusTarget::GetBounds()
{
	if (!WidgetTree->RootWidget)
	{
		auto* Bounds = WidgetTree->ConstructWidget<USizeBox>();
		Bounds->SetWidthOverride(60);
		Bounds->SetHeightOverride(40);
		WidgetTree->RootWidget = Bounds;
	}
	return CastChecked<USizeBox>(WidgetTree->RootWidget);
}

TSharedRef<SWidget> UReciTestFocusTarget::RebuildWidget()
{
	SetIsFocusable(true);
	GetBounds();
	return Super::RebuildWidget();
}

void UReciTestFocusTarget::BeginDestroy()
{
	Unregister();
	Super::BeginDestroy();
}

UReciTestFocusTarget* UReciTestPopupOwner::Open(const int32 UserIndex)
{
	if (Popup)
	{
		return Popup;
	}
	Popup = CreateWidget<UReciTestFocusTarget>(this, UReciTestFocusTarget::StaticClass(), *(GetName() + TEXT("Popup")));
	Popup->Log = Log;
	UOverlaySlot* OverlaySlot = GetOrCreateOverlayRoot(*WidgetTree, TEXT("PopupLayer"))->AddChildToOverlay(Popup);
	OverlaySlot->SetHorizontalAlignment(HAlign_Left);
	OverlaySlot->SetVerticalAlignment(VAlign_Top);
	Popup->Register();
	const TWeakObjectPtr<UReciTestPopupOwner> WeakThis = this;
	FReciInputFocusCoordinator::Get().RegisterOpenPopup(this, UserIndex, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Close();
		}
	});
	return Popup;
}

void UReciTestPopupOwner::Close()
{
	if (!Popup)
	{
		return;
	}
	if (Log)
	{
		Log->Add(*this, TEXT("Close"));
	}
	FReciInputFocusCoordinator::Get().UnregisterOpenPopup(this);
	Popup->Unregister();
	Popup->RemoveFromParent();
	Popup = nullptr;
}

TSharedRef<SWidget> UReciTestPopupOwner::RebuildWidget()
{
	GetOrCreateOverlayRoot(*WidgetTree, TEXT("PopupLayer"));
	return Super::RebuildWidget();
}

void UReciTestPopupOwner::BeginDestroy()
{
	FReciInputFocusCoordinator::Get().UnregisterOpenPopup(this);
	Super::BeginDestroy();
}

#if WITH_DEV_AUTOMATION_TESTS

#include "CommonInputBaseTypes.h"
#include "CommonInputSettings.h"
#include "Engine/Engine.h"
#include "Engine/PlatformSettingsManager.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/NavigationConfig.h"
#include "GameFramework/PlayerController.h"
#include "ICommonInputModule.h"
#include "InputMappingContext.h"
#include "Input/ReciScreenInputReports.h"
#include "Misc/AutomationTest.h"
#include "Navigation/ReciEnhancedInputNavigationConfig.h"
#include "ReciReport.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

namespace
{
	/** Slate creates a user when input first arrives from it; the harness registers its own before any input. */
	struct FReciSlateUserAccess : FSlateApplication
	{
		static TSharedRef<FSlateUser> RegisterUser(FSlateApplication& Slate, const int32 UserIndex)
		{
			// RegisterNewUser is protected. A pointer to it formed through this derived class can be called on Slate.
			TSharedRef<FSlateUser> (FSlateApplication::*Register)(int32, bool) = &FReciSlateUserAccess::RegisterNewUser;
			return (Slate.*Register)(UserIndex, false);
		}

		/** Marks every active timer that is due as pending, as Slate's tick does before it paints. */
		static void MarkDueTimersPending(FSlateApplication& Slate)
		{
			void (FSlateApplicationBase::*Update)() = &FReciSlateUserAccess::UpdateAnyActiveTimersArePending;
			(Slate.*Update)();
		}
	};
}

FReciInputFocusCoordinatorTestAccess::FState FReciInputFocusCoordinatorTestAccess::TakeState()
{
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	FState State;
	State.Targets = MoveTemp(Coordinator.Targets);
	State.InputScreens = MoveTemp(Coordinator.InputScreens);
	State.OpenPopups = MoveTemp(Coordinator.OpenPopups);
	State.EventTimeResolver = MoveTemp(Coordinator.EventTimeResolver);
	State.RoutingEnvironment = Coordinator.RoutingEnvironment;
	State.KeyPresses = MoveTemp(Coordinator.KeyPresses);
	State.AnalogDeflections = MoveTemp(Coordinator.AnalogDeflections);
	State.FocusSnapshots = MoveTemp(Coordinator.FocusSnapshots);
	State.LastPointerPositions = MoveTemp(Coordinator.LastPointerPositions);
	State.CapturedWidget = Coordinator.CapturedWidget;
	State.CapturedTarget = Coordinator.CapturedTarget;
	State.CapturedPointer = Coordinator.CapturedPointer;
	State.PointerFocusTargets = MoveTemp(Coordinator.PointerFocusTargets);

	Coordinator.Targets.Reset();
	Coordinator.InputScreens.Reset();
	Coordinator.OpenPopups.Reset();
	Coordinator.EventTimeResolver = nullptr;
	Coordinator.RoutingEnvironment = nullptr;
	Coordinator.KeyPresses.Reset();
	Coordinator.AnalogDeflections.Reset();
	Coordinator.FocusSnapshots.Reset();
	Coordinator.LastPointerPositions.Reset();
	Coordinator.ClearCapturedTarget();
	Coordinator.PointerFocusTargets.Reset();
	return State;
}

void FReciInputFocusCoordinatorTestAccess::RestoreState(FState&& State)
{
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	Coordinator.Targets = MoveTemp(State.Targets);
	Coordinator.InputScreens = MoveTemp(State.InputScreens);
	Coordinator.OpenPopups = MoveTemp(State.OpenPopups);
	Coordinator.EventTimeResolver = MoveTemp(State.EventTimeResolver);
	Coordinator.RoutingEnvironment = State.RoutingEnvironment;
	Coordinator.KeyPresses = MoveTemp(State.KeyPresses);
	Coordinator.AnalogDeflections = MoveTemp(State.AnalogDeflections);
	Coordinator.FocusSnapshots = MoveTemp(State.FocusSnapshots);
	Coordinator.LastPointerPositions = MoveTemp(State.LastPointerPositions);
	Coordinator.CapturedWidget = State.CapturedWidget;
	Coordinator.CapturedTarget = State.CapturedTarget;
	Coordinator.CapturedPointer = State.CapturedPointer;
	Coordinator.PointerFocusTargets = MoveTemp(State.PointerFocusTargets);
	// A test may have released the processors that the state put back still needs.
	if (Coordinator.NeedsProcessors())
	{
		Coordinator.InstallProcessor();
	}
}

void FReciInputFocusCoordinatorTestAccess::SetRoutingEnvironment(const IReciRoutingEnvironment* Environment)
{
	FReciInputFocusCoordinator::Get().RoutingEnvironment = Environment;
}

double FReciInputFocusCoordinatorTestAccess::ResolveEventTime(const FInputEvent& Event, const FKey& Key, const bool bIsRepeat)
{
	return FReciInputFocusCoordinator::Get().ResolveEventTime(Event, Key, bIsRepeat);
}

int32 FReciInputFocusCoordinatorTestAccess::GetTargetCount()
{
	return FReciInputFocusCoordinator::Get().Targets.Num();
}

int32 FReciInputFocusCoordinatorTestAccess::GetScreenCount()
{
	return FReciInputFocusCoordinator::Get().InputScreens.Num();
}

int32 FReciInputFocusCoordinatorTestAccess::GetPopupCount()
{
	return FReciInputFocusCoordinator::Get().OpenPopups.Num();
}

int32 FReciInputFocusCoordinatorTestAccess::GetPressCount(const int32 UserIndex)
{
	const TMap<FKey, FReciInputFocusCoordinator::FKeyPress>* Presses = FReciInputFocusCoordinator::Get().KeyPresses.Find(UserIndex);
	return Presses ? Presses->Num() : 0;
}

bool FReciInputFocusCoordinatorTestAccess::HasFocusSnapshot(const int32 UserIndex)
{
	return FReciInputFocusCoordinator::Get().FocusSnapshots.Contains(UserIndex);
}

int32 FReciInputFocusCoordinatorTestAccess::CountInstalledProcessors()
{
	const FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	const FSlateApplication& Slate = FSlateApplication::Get();
	const bool bInput = Coordinator.InputProcessor
		&& Slate.FindInputPreProcessor(Coordinator.InputProcessor, EInputPreProcessorType::Game) != INDEX_NONE;
	const bool bSnapshot = Coordinator.FocusSnapshotProcessor
		&& Slate.FindInputPreProcessor(Coordinator.FocusSnapshotProcessor, EInputPreProcessorType::PreGame) != INDEX_NONE;
	return (bInput ? 1 : 0) + (bSnapshot ? 1 : 0);
}

void FReciInputFocusCoordinatorTestAccess::Tick()
{
	const FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSharedPtr<ICursor> Cursor = Slate.GetPlatformCursor();
	if (Coordinator.InputProcessor && Cursor)
	{
		Coordinator.InputProcessor->Tick(0.0f, Slate, Cursor.ToSharedRef());
	}
}

bool FReciInputFocusCoordinatorTestAccess::KeyDown(const FKeyEvent& Event)
{
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	Coordinator.InstallProcessor();
	return Coordinator.InputProcessor && Coordinator.InputProcessor->HandleKeyDownEvent(FSlateApplication::Get(), Event);
}

bool FReciInputFocusCoordinatorTestAccess::KeyUp(const FKeyEvent& Event)
{
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	Coordinator.InstallProcessor();
	return Coordinator.InputProcessor && Coordinator.InputProcessor->HandleKeyUpEvent(FSlateApplication::Get(), Event);
}

bool FReciInputFocusCoordinatorTestAccess::AnalogInput(const FAnalogInputEvent& Event)
{
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	Coordinator.InstallProcessor();
	return Coordinator.InputProcessor && Coordinator.InputProcessor->HandleAnalogInputEvent(FSlateApplication::Get(), Event);
}

bool FReciInputFocusCoordinatorTestAccess::MouseWheel(const FPointerEvent& Event)
{
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	Coordinator.InstallProcessor();
	return Coordinator.InputProcessor
		&& Coordinator.InputProcessor->HandleMouseWheelOrGestureEvent(FSlateApplication::Get(), Event, nullptr);
}

bool FReciTestFocusMover::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event)
{
	if (Event.IsRepeat() || Event.GetKey() != Key || Event.GetUserIndex() != FReciTestHarness::UserIndex)
	{
		return false;
	}
	bSawSnapshot = FReciInputFocusCoordinatorTestAccess::HasFocusSnapshot(FReciTestHarness::UserIndex);
	if (bArmed && MoveTo.IsValid())
	{
		SlateApp.SetUserFocus(Event.GetUserIndex(), MoveTo->TakeWidget());
	}
	return false;
}

FReciScopedCoordinatorState::FReciScopedCoordinatorState()
	: Saved(FReciInputFocusCoordinatorTestAccess::TakeState())
{
}

FReciScopedCoordinatorState::~FReciScopedCoordinatorState()
{
	FReciInputFocusCoordinatorTestAccess::RestoreState(MoveTemp(Saved));
}

FReciScopedReports::FReciScopedReports()
	: Saved(UE::ReciCommonUI::Private::ExchangeReportsForTests({}))
{
#if !UE_BUILD_SHIPPING
	SavedPendingScreens = UE::ReciCommonUI::Private::ExchangePendingScreenChecksForTests({});
#endif
}

FReciScopedReports::~FReciScopedReports()
{
#if !UE_BUILD_SHIPPING
	UE::ReciCommonUI::Private::ExchangePendingScreenChecksForTests(MoveTemp(SavedPendingScreens));
#endif
	UE::ReciCommonUI::Private::ExchangeReportsForTests(MoveTemp(Saved));
}

FReciScopedObjectState::FReciScopedObjectState(UObject& InObject)
	: Object(&InObject)
	, Snapshot(NewObject<UObject>(GetTransientPackage(), InObject.GetClass(), NAME_None, RF_Transient, &InObject))
{
}

FReciScopedObjectState::~FReciScopedObjectState()
{
	UObject* Target = Object.Get();
	if (!Target || !Snapshot)
	{
		return;
	}
	for (TFieldIterator<FProperty> Property(Target->GetClass()); Property; ++Property)
	{
		Property->CopyCompleteValue_InContainer(Target, Snapshot.Get());
	}
}

namespace
{
	/** The Input Data class CommonUI loaded, which it keeps private; Reci reads it the same way. */
	FObjectPropertyBase* FindInputDataClassProperty()
	{
		return FindFProperty<FObjectPropertyBase>(UCommonInputSettings::StaticClass(), TEXT("InputDataClass"));
	}
}

FReciScopedInputData::FReciScopedInputData(FAutomationTestBase& Test)
	: DefaultsState(*GetMutableDefault<UReciTestInputData>())
{
	UCommonInputSettings& Settings = ICommonInputModule::GetSettings();
	// Loaded now, a later load cannot put the project's class back while the scope lasts.
	Settings.LoadData();
	const FObjectPropertyBase* Property = FindInputDataClassProperty();
	if (!Property)
	{
		Test.AddError(TEXT("Common Input Settings has no InputDataClass property, so the test Input Data is not set."));
		return;
	}
	UClass* InputDataClass = Cast<UClass>(Property->GetObjectPropertyValue_InContainer(&Settings));
	SavedInputDataClass.Reset(InputDataClass);
	if (InputDataClass)
	{
		UReciTestInputData& Defaults = GetDefaults();
		const UObject* ReplacedDefaults = InputDataClass->GetDefaultObject();
		for (TFieldIterator<FProperty> CommonUIProperty(UCommonUIInputData::StaticClass()); CommonUIProperty; ++CommonUIProperty)
		{
			CommonUIProperty->CopyCompleteValue_InContainer(&Defaults, ReplacedDefaults);
		}
	}
	SetInputDataClass(UReciTestInputData::StaticClass());
}

FReciScopedInputData::~FReciScopedInputData()
{
	if (const FObjectPropertyBase* Property = FindInputDataClassProperty())
	{
		Property->SetObjectPropertyValue_InContainer(&ICommonInputModule::GetSettings(), SavedInputDataClass.Get());
	}
	// The test Input Data's defaults come back as DefaultsState is destroyed.
}

UReciTestInputData& FReciScopedInputData::GetDefaults() const
{
	return *GetMutableDefault<UReciTestInputData>();
}

void FReciScopedInputData::SetInputDataClass(const TSubclassOf<UCommonUIInputData> InputDataClass) const
{
	if (const FObjectPropertyBase* Property = FindInputDataClassProperty())
	{
		Property->SetObjectPropertyValue_InContainer(&ICommonInputModule::GetSettings(), InputDataClass.Get());
	}
}

void FReciTestRoutingEnvironment::ApplyMapping(const UInputMappingContext* Mapping)
{
	if (Mapping)
	{
		AppliedMappings.AddUnique(Mapping);
	}
}

void FReciTestRoutingEnvironment::RemoveMapping(const UInputMappingContext* Mapping)
{
	AppliedMappings.Remove(Mapping);
}

void FReciTestRoutingEnvironment::SetActiveRoot(const UCommonActivatableWidget& Root)
{
	const TWeakObjectPtr<const UCommonActivatableWidget> WeakRoot = &Root;
	ActiveRoot = [WeakRoot](const UCommonActivatableWidget& Widget)
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

bool FReciTestRoutingEnvironment::IsInActiveRoot(const UCommonActivatableWidget& Widget) const
{
	return !ActiveRoot || ActiveRoot(Widget);
}

void FReciTestRoutingEnvironment::SetInputType(const ECommonInputType InputType)
{
	FReciInputMethod Method;
	Method.InputType = InputType;
	const UCommonInputPlatformSettings* PlatformSettings = UCommonInputPlatformSettings::Get();
	Method.GamepadName = PlatformSettings ? PlatformSettings->GetDefaultGamepadName() : NAME_None;
	InputMethod = Method;
}

TArray<FKey> FReciTestRoutingEnvironment::GetActionKeys(const UWidget& Widget, const UInputAction& Action) const
{
	const ULocalPlayer* Player = Widget.GetOwningLocalPlayer();
	return Player ? GetPlayerActionKeys(*Player, Action) : TArray<FKey>();
}

TOptional<FReciInputMethod> FReciTestRoutingEnvironment::GetInputMethod(const ULocalPlayer& Player) const
{
	return FSlateApplication::Get().GetUserIndexForController(Player.GetControllerId()) == UserIndex
		? InputMethod
		: TOptional<FReciInputMethod>();
}

TArray<FKey> FReciTestRoutingEnvironment::GetPlayerActionKeys(
	const ULocalPlayer& Player,
	const UInputAction& Action) const
{
	TArray<FKey> Keys;
	if (FSlateApplication::Get().GetUserIndexForController(Player.GetControllerId()) != UserIndex)
	{
		return Keys;
	}
	for (const TWeakObjectPtr<const UInputMappingContext>& Mapping : AppliedMappings)
	{
		if (!Mapping.IsValid())
		{
			continue;
		}
		for (const FEnhancedActionKeyMapping& KeyMapping : Mapping->GetMappings())
		{
			if (KeyMapping.Action == &Action)
			{
				Keys.AddUnique(KeyMapping.Key);
			}
		}
	}
	return Keys;
}

FReciTestHarness::FReciTestHarness(FAutomationTestBase& InTest)
	: Test(InTest)
	, Environment(UserIndex)
	, Log(MakeShared<FReciTestCallLog>())
{
	if (!FSlateApplication::IsInitialized())
	{
		Test.AddError(TEXT("The test harness needs Slate."));
		return;
	}
	FSlateApplication& Slate = FSlateApplication::Get();
	if (Slate.GetUser(UserIndex))
	{
		Test.AddError(FString::Printf(TEXT("Slate user %d already exists, and the test harness plays as that user."), UserIndex));
		return;
	}
	SlateUser = FReciSlateUserAccess::RegisterUser(Slate, UserIndex);
	// Slate's default keys, so routing tests do not depend on the navigation config of the project or the editor.
	SlateUser->SetUserNavigationConfig(MakeShared<FNavigationConfig>());

	World = UWorld::CreateWorld(EWorldType::Game, false);
	PlayerController = World->SpawnActor<APlayerController>();
	if (!PlayerController)
	{
		Test.AddError(TEXT("The test harness could not spawn its player controller."));
		return;
	}
	// A local player belongs to the engine. PlayerAdded is not called: it would start every local player subsystem in
	// the process, the game's and other plugins' included, so the player has no subsystem (no Enhanced Input, no
	// CommonUI action router), and FReciTestRoutingEnvironment answers in their place.
	LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	LocalPlayer->SetControllerId(UserIndex);
	LocalPlayer->PlayerController = PlayerController;
	PlayerController->Player = LocalPlayer;
	// A world that is not playing does not list the controllers spawned into it, and a widget's player context needs it.
	World->AddController(PlayerController);

	PaintTime = FApp::GetCurrentTime();
	Layers = SNew(SOverlay);
	Window = SNew(SVirtualWindow).Size(FVector2D(1280, 720));
	Window->SetContent(Layers.ToSharedRef());
	Slate.RegisterVirtualWindow(Window.ToSharedRef());

	FReciInputFocusCoordinatorTestAccess::SetRoutingEnvironment(&Environment);
	bValid = true;
}

FReciTestHarness::~FReciTestHarness()
{
	FReciInputFocusCoordinatorTestAccess::SetRoutingEnvironment(nullptr);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		if (SlateUser)
		{
			// Clears the user's focus first.
			Slate.UnregisterUser(UserIndex);
		}
		if (Window)
		{
			Slate.UnregisterVirtualWindow(Window.ToSharedRef());
		}
	}
	SlateUser.Reset();
	for (UUserWidget* Widget : RootWidgets)
	{
		static_cast<UWidget*>(Widget)->ReleaseSlateResources(true);
	}
	Layers.Reset();
	Window.Reset();
	if (World)
	{
		if (PlayerController)
		{
			World->RemoveController(PlayerController);
			PlayerController->Player = nullptr;
		}
		World->DestroyWorld(false);
	}
	if (LocalPlayer)
	{
		LocalPlayer->PlayerController = nullptr;
		LocalPlayer->MarkAsGarbage();
	}
	PreservedObjects.Reset();
	// The coordinator's state and the reports come back as their members are destroyed.
}

UReciTestScreen* FReciTestHarness::CreateScreen(const FName Name, UReciTestScreen* Parent, TSubclassOf<UReciTestScreen> ScreenClass)
{
	if (!ScreenClass)
	{
		ScreenClass = UReciTestScreen::StaticClass();
	}
	UReciTestScreen* Screen = Parent
		? Parent->WidgetTree->ConstructWidget<UReciTestScreen>(ScreenClass, Name)
		: CreateWidget<UReciTestScreen>(PlayerController.Get(), ScreenClass, Name);
	Screen->Log = Log;
	Screen->SetIsFocusable(true);
	Screen->GetContent();
	if (Parent)
	{
		Parent->GetContent()->AddChild(Screen);
	}
	else
	{
		AddLayer(*Screen);
	}
	return Keep(Screen);
}

UReciTestFocusTarget* FReciTestHarness::CreateTarget(const FName Name, UReciTestScreen* Screen)
{
	UReciTestFocusTarget* Target = nullptr;
	if (Screen)
	{
		Target = Screen->WidgetTree->ConstructWidget<UReciTestFocusTarget>(UReciTestFocusTarget::StaticClass(), Name);
		UOverlaySlot* OverlaySlot = Screen->GetContent()->AddChildToOverlay(Target);
		OverlaySlot->SetHorizontalAlignment(HAlign_Left);
		OverlaySlot->SetVerticalAlignment(VAlign_Top);
	}
	else
	{
		Target = CreateWidget<UReciTestFocusTarget>(PlayerController.Get(), UReciTestFocusTarget::StaticClass(), Name);
		AddLayer(*Target);
	}
	Target->Log = Log;
	Target->Register();
	return Keep(Target);
}

UReciTestPopupOwner* FReciTestHarness::CreatePopupOwner(const FName Name, UReciTestScreen* Screen)
{
	auto* Owner = Screen->WidgetTree->ConstructWidget<UReciTestPopupOwner>(UReciTestPopupOwner::StaticClass(), Name);
	Owner->Log = Log;
	UOverlaySlot* OverlaySlot = Screen->GetContent()->AddChildToOverlay(Owner);
	OverlaySlot->SetHorizontalAlignment(HAlign_Left);
	OverlaySlot->SetVerticalAlignment(VAlign_Top);
	return Keep(Owner);
}

UInputAction* FReciTestHarness::CreateAction(const FName Name)
{
	return Keep(NewObject<UInputAction>(World, Name));
}

UInputMappingContext* FReciTestHarness::CreateMapping(const TConstArrayView<TPair<UInputAction*, FKey>> Mappings)
{
	auto* Mapping = NewObject<UInputMappingContext>(World);
	for (const TPair<UInputAction*, FKey>& Entry : Mappings)
	{
		Mapping->MapKey(Entry.Key, Entry.Value);
	}
	return Keep(Mapping);
}

void FReciTestHarness::ActivateScreen(UReciTestScreen& Screen)
{
	Screen.ActivateWidget();
	Environment.ApplyMapping(Screen.GetInputMapping());
}

void FReciTestHarness::DeactivateScreen(UReciTestScreen& Screen)
{
	Screen.DeactivateWidget();
	Environment.RemoveMapping(Screen.GetInputMapping());
}

bool FReciTestHarness::Focus(UWidget& Widget)
{
	return FSlateApplication::Get().SetUserFocus(UserIndex, Widget.TakeWidget());
}

TSharedPtr<SWidget> FReciTestHarness::GetFocusedWidget() const
{
	return FSlateApplication::Get().GetUserFocusedWidget(UserIndex);
}

void FReciTestHarness::Paint(const float DeltaSeconds)
{
	if (!Window)
	{
		return;
	}
	Window->SlatePrepass(1.0f);
	FSlateWindowElementList Elements(Window);
	Window->PaintWindow(PaintTime, DeltaSeconds, Elements, FWidgetStyle(), true);
}

void FReciTestHarness::Advance(const float Seconds)
{
	constexpr float Step = 1.0f / 60.0f;
	for (float Elapsed = 0.0f; Elapsed < Seconds - KINDA_SMALL_NUMBER; Elapsed += Step)
	{
		PaintTime += Step;
		FReciSlateUserAccess::MarkDueTimersPending(FSlateApplication::Get());
		Paint(Step);
	}
}

bool FReciTestHarness::KeyDown(const FKey& Key, const bool bIsRepeat)
{
	return FReciInputFocusCoordinatorTestAccess::KeyDown(FKeyEvent(Key, FModifierKeysState(), UserIndex, bIsRepeat, 0, 0));
}

bool FReciTestHarness::KeyUp(const FKey& Key)
{
	return FReciInputFocusCoordinatorTestAccess::KeyUp(FKeyEvent(Key, FModifierKeysState(), UserIndex, false, 0, 0));
}

bool FReciTestHarness::AnalogInput(const FKey& Axis, const float Value)
{
	return FReciInputFocusCoordinatorTestAccess::AnalogInput(
		FAnalogInputEvent(Axis, FModifierKeysState(), UserIndex, false, 0, 0, Value));
}

bool FReciTestHarness::MouseWheel(const float WheelDelta)
{
	// Far from any window the editor has open. The event keeps a pointer to the pressed buttons.
	const FVector2D Position(-100000.0, -100000.0);
	const TSet<FKey> PressedButtons;
	return FReciInputFocusCoordinatorTestAccess::MouseWheel(
		FPointerEvent(UserIndex, 0, Position, Position, PressedButtons, EKeys::MouseWheelAxis, WheelDelta, FModifierKeysState()));
}

bool FReciTestHarness::SlateKeyDown(const FKey& Key, const bool bIsRepeat)
{
	return FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), UserIndex, bIsRepeat, 0, 0));
}

bool FReciTestHarness::SlateKeyUp(const FKey& Key)
{
	return FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), UserIndex, false, 0, 0));
}

void FReciTestHarness::AddLayer(UUserWidget& Widget)
{
	RootWidgets.Add(&Widget);
	Layers->AddSlot()[Widget.TakeWidget()];
}

void FReciTestHarness::Preserve(UObject& Object)
{
	PreservedObjects.Add(MakeUnique<FReciScopedObjectState>(Object));
}

void FReciTestHarness::UseReciNavigationConfig(const bool bStickDominantAxis)
{
	if (SlateUser)
	{
		SlateUser->SetUserNavigationConfig(MakeShared<FReciEnhancedInputNavigationConfig>(
			MakeShared<FNavigationConfig>(), false, false, true, bStickDominantAxis));
	}
}

void FReciTestHarness::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(World);
	Collector.AddReferencedObject(PlayerController);
	Collector.AddReferencedObject(LocalPlayer);
	Collector.AddReferencedObjects(Objects);
	Collector.AddReferencedObjects(RootWidgets);
}

#endif
