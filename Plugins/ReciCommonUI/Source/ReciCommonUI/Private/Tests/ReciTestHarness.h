// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Commands/InputChord.h"
#include "Input/ReciScreenInput.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Interaction/ReciRoutingEnvironment.h"
#include "Settings/ReciUIInputData.h"
#include "UObject/GCObject.h"
#include "UObject/StrongObjectPtr.h"
#include "ReciTestHarness.generated.h"

class UInputMappingContext;
class UOverlay;
class USizeBox;

/** The calls the harness's test widgets received, in order, one line each: "<widget name> <call>". */
struct FReciTestCallLog
{
	TArray<FString> Calls;

	void Add(const UObject& Widget, const FString& Call) { Calls.Add(Widget.GetName() + TEXT(" ") + Call); }
	/** The calls joined with ", ", for one readable comparison per test step. */
	FString ToString() const { return FString::Join(Calls, TEXT(", ")); }
	void Reset() { Calls.Reset(); }
};

/**
 * A screen that takes screen input. IReciScreenInput's functions are Blueprint events, which a native class cannot
 * override: Execute_ finds them on the interface and calls ProcessEvent, which this screen intercepts. It logs each
 * call and answers from the values a test sets.
 */
UCLASS(Hidden, NotBlueprintable, Transient)
class RECICOMMONUI_API UReciTestScreen : public UCommonActivatableWidget, public IReciScreenInput
{
	GENERATED_BODY()
public:
	/** The keys HandleReciScreenKey takes. */
	TSet<FKey> HandledKeys;
	/** The actions HandleReciScreenAction takes. */
	UPROPERTY() TSet<TObjectPtr<UInputAction>> HandledActions;
	/** The actions IsReciScreenActionSuspended answers true for. Its calls are not logged: routing asks it for each action. */
	UPROPERTY() TSet<TObjectPtr<UInputAction>> SuspendedActions;
	/** What IsCapturingReciScreenKeys returns. */
	bool bCapturingKeys = false;
	/** Runs after a call is logged and before its answer is returned, for a handler that changes screens. */
	TFunction<void(FName Function)> OnCall;
	/** The time each HandleReciScreenAction call received, in order. */
	TArray<double> ActionTimestamps;
	TSharedPtr<FReciTestCallLog> Log;

	void SetIsModal(bool bModal) { bIsModal = bModal; }
	void SetInputMapping(UInputMappingContext* Mapping) { InputMapping = Mapping; }
	UInputMappingContext* GetInputMapping() const { return InputMapping; }
	/** The overlay that holds the screen's content. */
	UOverlay* GetContent();

	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
};

/** A Reci control stand-in: a focusable 60 x 40 widget that logs every call and answers from the values a test sets. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestFocusTarget : public UUserWidget, public IReciInputFocusTarget
{
	GENERATED_BODY()
public:
	/** The directions HandleReciNavigation takes. */
	TSet<EUINavigation> HandledDirections;
	bool bHandlesAccept = false;
	/** What the wheel and pointer-down handlers return; a handled pointer-down holds the pointer until it goes up. */
	bool bHandlesPointer = false;
	TSharedPtr<FReciTestCallLog> Log;

	void Register();
	void Unregister();
	/** Puts a widget of this target's tree inside it, as an entry holds a control: focus on it is focus inside the target. */
	void SetContent(UWidget* Content);

	virtual const UWidget* GetReciInputSurfaceWidget() const override { return this; }
	/** As Reci's own controls: the Slate user of the owning player, or 0 without one. */
	virtual int32 GetReciInputUserIndex() const override;
	/** As Reci's own controls: while focus is on it or inside it. */
	virtual bool CanHandleReciFocusedInput() const override;
	virtual bool HandleReciNavigation(EUINavigation Direction) override;
	virtual bool HandleReciAccept() override;
	virtual bool HandleReciMouseWheel(float WheelDelta, const FVector2D& ScreenPosition) override;
	virtual bool HandleReciPointerDown(const FGeometry& SurfaceGeometry, const FPointerEvent& PointerEvent) override;
	virtual bool HandleReciPointerMove(const FGeometry& SurfaceGeometry, const FPointerEvent& PointerEvent) override;
	virtual bool HandleReciPointerUp(const FPointerEvent& PointerEvent) override;
	virtual bool HasReciPointerCapture(int32 PointerIndex) const override { return bHasPointerCapture; }
	virtual void HandleReciPointerCaptureLost() override;
	virtual void HandleReciPointerHover(const FVector2D& ScreenPosition) override;
	virtual void ReturnReciFocusToDrivenItem() override;

	/** Logs "KeySelected <key>", for a key selector's On Key Selected. */
	UFUNCTION()
	void LogKeySelected(FInputChord SelectedKey);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void BeginDestroy() override;

private:
	/** The 60 x 40 root, made on first use. */
	USizeBox* GetBounds();

	bool bHasPointerCapture = false;
};

/**
 * A dropdown stand-in. Its popup is a test target created with this widget as its outer, as a dropdown creates its
 * list, and hosted under this widget, as a menu anchor hosts it.
 */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestPopupOwner : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Opens the popup for the user and registers it with the coordinator. */
	UReciTestFocusTarget* Open(int32 UserIndex);
	/** Closes the popup as Back does through the coordinator: logs "Close", unregisters it and removes it. */
	void Close();
	bool IsOpen() const { return Popup != nullptr; }

	UPROPERTY() TObjectPtr<UReciTestFocusTarget> Popup;
	TSharedPtr<FReciTestCallLog> Log;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void BeginDestroy() override;
};

/** Reci UI Input Data that FReciScopedInputData makes the project's Input Data while a test runs. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestInputData : public UReciUIInputData
{
	GENERATED_BODY()
};

#if WITH_DEV_AUTOMATION_TESTS

class APlayerController;
class FAutomationTestBase;
class FSlateUser;
class SOverlay;
class SVirtualWindow;
class ULocalPlayer;

/** The coordinator's private state and input processor, for the harness. */
struct FReciInputFocusCoordinatorTestAccess
{
	/** Everything the coordinator keeps between inputs, except its input processor. */
	struct FState
	{
		TArray<FReciInputFocusCoordinator::FTargetEntry> Targets;
		TSet<TWeakObjectPtr<UCommonActivatableWidget>> InputScreens;
		TArray<FReciInputFocusCoordinator::FOpenPopup> OpenPopups;
		FReciInputFocusCoordinator::FEventTimeResolver EventTimeResolver;
		const IReciRoutingEnvironment* RoutingEnvironment = nullptr;
		TMap<uint32, TMap<FKey, FReciInputFocusCoordinator::FKeyPress>> KeyPresses;
		TMap<uint32, TMap<FKey, FReciInputFocusCoordinator::FAnalogDeflection>> AnalogDeflections;
		TMap<uint32, FReciInputFocusCoordinator::FFocusSnapshot> FocusSnapshots;
		TMap<FReciInputFocusCoordinator::FPointerIdentity, FVector2D> LastPointerPositions;
		TWeakObjectPtr<UWidget> CapturedWidget;
		IReciInputFocusTarget* CapturedTarget = nullptr;
		FReciInputFocusCoordinator::FPointerIdentity CapturedPointer;
		TMap<uint32, IReciInputFocusTarget*> PointerFocusTargets;
	};

	/** Moves the coordinator's state out, leaving it as at startup: nothing registered, no resolver, stock answers. */
	static FState TakeState();
	/**
	 * Puts back a state taken earlier, dropping whatever was registered since, and installs the input processors if
	 * what it puts back needs them.
	 */
	static void RestoreState(FState&& State);

	/** Null puts the stock answers back. */
	static void SetRoutingEnvironment(const IReciRoutingEnvironment* Environment);
	static double ResolveEventTime(const FInputEvent& Event, const FKey& Key, bool bIsRepeat);

	static int32 GetTargetCount();
	static int32 GetScreenCount();
	static int32 GetPopupCount();
	/** The keys the user holds that the coordinator has a press record for. */
	static int32 GetPressCount(int32 UserIndex);
	/** Whether the coordinator noted the user's focus at a key-down it has not routed yet. */
	static bool HasFocusSnapshot(int32 UserIndex);
	/** How many of the coordinator's two input processors are in Slate's lists. */
	static int32 CountInstalledProcessors();
	/** Ticks the coordinator's input processor as Slate does each frame, outside Slate's own Tick. */
	static void Tick();

	/**
	 * Drive the coordinator's input processor directly, as Slate does before any widget sees the event. Each installs the
	 * processors first, as a registration would.
	 */
	static bool KeyDown(const FKeyEvent& Event);
	static bool KeyUp(const FKeyEvent& Event);
	static bool AnalogInput(const FAnalogInputEvent& Event);
	static bool MouseWheel(const FPointerEvent& Event);
};

/** Sets the coordinator's state aside for a scope, and puts it back when the scope ends. */
class FReciScopedCoordinatorState
{
public:
	FReciScopedCoordinatorState();
	~FReciScopedCoordinatorState();

private:
	FReciInputFocusCoordinatorTestAccess::FState Saved;
};

/**
 * Gives a scope an empty set of warn-once reports and no activated screen waiting for the unregistered-screen check,
 * and puts the process's back when the scope ends: a check the scope left waiting is dropped.
 */
class RECICOMMONUI_API FReciScopedReports
{
public:
	FReciScopedReports();
	~FReciScopedReports();

private:
	TSet<FString> Saved;
	TArray<TWeakObjectPtr<UCommonActivatableWidget>> SavedPendingScreens;
};

/** Copies an object's properties, such as a settings CDO's, and copies them back when the scope ends. */
class FReciScopedObjectState
{
public:
	explicit FReciScopedObjectState(UObject& Object);
	~FReciScopedObjectState();

private:
	TWeakObjectPtr<UObject> Object;
	TStrongObjectPtr<UObject> Snapshot;
};

/**
 * Makes UReciTestInputData the project's Input Data for the scope. Its defaults take CommonUI's properties, such as the
 * click and Back actions, from the Input Data it replaces, no navigation action until a test sets one, and Mapped Keys
 * Replace Default Keys at its default, on. The scope's end puts back the Input Data class of CommonUI's input settings
 * and the test Input Data's defaults.
 */
class FReciScopedInputData
{
public:
	explicit FReciScopedInputData(FAutomationTestBase& Test);
	~FReciScopedInputData();

	/** The test Input Data's defaults, whose navigation actions a test sets. */
	UReciTestInputData& GetDefaults() const;
	/** Makes another class the project's Input Data instead, such as a stock UCommonUIInputData. */
	void SetInputDataClass(TSubclassOf<UCommonUIInputData> InputDataClass) const;

private:
	FReciScopedObjectState DefaultsState;
	TStrongObjectPtr<UClass> SavedInputDataClass;
};

/**
 * Answers that need a game viewport or a player's subsystems: CommonUI's active root and input mode, the keys the
 * player maps to an action, and the player's input method. A widget's user index is the stock answer, from the
 * harness's local player.
 */
class FReciTestRoutingEnvironment final : public FReciStockRoutingEnvironment
{
public:
	explicit FReciTestRoutingEnvironment(int32 InUserIndex) : UserIndex(InUserIndex) {}

	/** Whether a widget is in CommonUI's active root; unset, every widget is. */
	TFunction<bool(const UCommonActivatableWidget& Widget)> ActiveRoot;
	ECommonInputMode InputMode = ECommonInputMode::All;
	/** The harness player's input method, as CommonInput would detect it; unset, key texts show nothing, as without it. */
	TOptional<FReciInputMethod> InputMethod;

	/** Gives the harness player an input method of this type, with the platform's default gamepad. */
	void SetInputType(ECommonInputType InputType);

	/** Makes the root CommonUI's active root, answered as CommonUI does: a widget is in it when the root is the widget or one of its Slate ancestors. */
	void SetActiveRoot(const UCommonActivatableWidget& Root);

	/** Applies a mapping context to the harness player, as CommonUI does with an activated screen's Input Mapping. */
	void ApplyMapping(const UInputMappingContext* Mapping);
	void RemoveMapping(const UInputMappingContext* Mapping);

	virtual bool IsInActiveRoot(const UCommonActivatableWidget& Widget) const override;
	virtual ECommonInputMode GetActiveInputMode(const UWidget& Widget) const override { return InputMode; }
	/** The keys the applied mapping contexts map to the action, for a widget of the harness player. */
	virtual TArray<FKey> GetActionKeys(const UWidget& Widget, const UInputAction& Action) const override;
	/** The keys the applied mapping contexts map to the action, for the harness player. */
	virtual TArray<FKey> GetPlayerActionKeys(const ULocalPlayer& Player, const UInputAction& Action) const override;
	/** InputMethod, for the harness player. */
	virtual TOptional<FReciInputMethod> GetInputMethod(const ULocalPlayer& Player) const override;

private:
	int32 UserIndex;
	TArray<TWeakObjectPtr<const UInputMappingContext>> AppliedMappings;
};

/**
 * A player, a window and the coordinator for one test, in which a test builds screens, targets and popups and drives
 * input. Real: a Slate user, a virtual window that is painted for geometry, focus, a local player and its player
 * controller in a test world, and CommonUI's activatable widgets. Answered by FReciTestRoutingEnvironment: CommonUI's
 * active root and input mode, the player's action keys, and the player's input method. Construction sets aside the
 * coordinator's registrations, popups, press records and resolver and the warn-once reports; destruction puts them back
 * and removes the user, the window and the world, whatever the test checked.
 */
class RECICOMMONUI_API FReciTestHarness final : public FGCObject
{
public:
	UE_NONCOPYABLE(FReciTestHarness);

	/** The Slate user and controller id the harness plays as: a non-virtual user no device uses, so the editor's user 0 is never touched. */
	static constexpr int32 UserIndex = 7;

	explicit FReciTestHarness(FAutomationTestBase& InTest);
	virtual ~FReciTestHarness() override;

	/** False when the harness could not be built, which it has reported as a test error. */
	bool IsValid() const { return bValid; }

	UWorld* GetWorld() const { return World; }
	APlayerController* GetPlayerController() const { return PlayerController; }
	ULocalPlayer* GetLocalPlayer() const { return LocalPlayer; }
	FReciTestRoutingEnvironment& GetEnvironment() { return Environment; }
	FReciTestCallLog& GetLog() const { return *Log; }

	/**
	 * A screen of the harness player: with no parent it is a layer of the window, otherwise part of the parent's content.
	 * ScreenClass, when set, is a subclass of the test screen.
	 */
	UReciTestScreen* CreateScreen(FName Name, UReciTestScreen* Parent = nullptr, TSubclassOf<UReciTestScreen> ScreenClass = nullptr);
	/** A registered test target in the screen's content; with no screen, a layer of the window that no activatable widget holds. */
	UReciTestFocusTarget* CreateTarget(FName Name, UReciTestScreen* Screen);
	UReciTestPopupOwner* CreatePopupOwner(FName Name, UReciTestScreen* Screen);
	UInputAction* CreateAction(FName Name);
	UInputMappingContext* CreateMapping(TConstArrayView<TPair<UInputAction*, FKey>> Mappings);

	/** Activates the screen and applies its Input Mapping to the player, as CommonUI does with Enhanced Input support. */
	void ActivateScreen(UReciTestScreen& Screen);
	void DeactivateScreen(UReciTestScreen& Screen);

	/** Sets the harness user's focus, as Slate does. */
	bool Focus(UWidget& Widget);
	TSharedPtr<SWidget> GetFocusedWidget() const;
	/** Lays out and paints the window: widgets get their cached geometry, and active timers that are due run. */
	void Paint(float DeltaSeconds = 0.0f);
	/**
	 * Lets time pass in steps of 1/60 s: each step makes every active timer due, as Slate's tick does, and paints the
	 * window with that step, so motion advances by the step whatever the real clock does.
	 */
	void Advance(float Seconds);

	/** Input from the harness user, through the coordinator's input processor. True when the processor consumed it. */
	bool KeyDown(const FKey& Key, bool bIsRepeat = false);
	bool KeyUp(const FKey& Key);
	bool AnalogInput(const FKey& Axis, float Value);
	/**
	 * A wheel turn away from every window, through the coordinator's input processor. Slate does not hit-test the
	 * harness's virtual window, so Reci gives the turn to the focused Reci control. True when the processor consumed it.
	 */
	bool MouseWheel(float WheelDelta);
	/**
	 * Input from the harness user through Slate itself: every input preprocessor in its bucket order, then the widgets on
	 * the user's focus path. True when any of them handled it.
	 */
	bool SlateKeyDown(const FKey& Key, bool bIsRepeat = false);
	bool SlateKeyUp(const FKey& Key);

	/** Copies an object's properties now, such as a settings CDO's, and copies them back when the harness ends. */
	void Preserve(UObject& Object);

	/**
	 * Gives the harness user the navigation config Reci installs, over Slate's default keys, in place of Slate's own: no
	 * mapped key, Tab Navigation on, and Stick Dominant Axis as given. It is made here, in the core module, so that Reci's
	 * routing finds it from a test in any module.
	 */
	void UseReciNavigationConfig(bool bStickDominantAxis);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FReciTestHarness"); }

private:
	template <typename TObject>
	TObject* Keep(TObject* Object)
	{
		Objects.Add(Object);
		return Object;
	}

	/** Puts a widget of the harness player on top of the window's layers. */
	void AddLayer(UUserWidget& Widget);

	FAutomationTestBase& Test;
	bool bValid = false;
	FReciScopedCoordinatorState CoordinatorState;
	FReciScopedReports Reports;
	TArray<TUniquePtr<FReciScopedObjectState>> PreservedObjects;
	FReciTestRoutingEnvironment Environment;
	TSharedRef<FReciTestCallLog> Log;
	TSharedPtr<FSlateUser> SlateUser;
	TSharedPtr<SVirtualWindow> Window;
	TSharedPtr<SOverlay> Layers;
	TObjectPtr<UWorld> World;
	TObjectPtr<APlayerController> PlayerController;
	TObjectPtr<ULocalPlayer> LocalPlayer;
	TArray<TObjectPtr<UObject>> Objects;
	/** The window's layers. */
	TArray<TObjectPtr<UUserWidget>> RootWidgets;
	/** The time the window is painted at; only Advance moves it. */
	double PaintTime = 0.0;
};

/**
 * Stands in for CommonInput's preprocessor, which runs ahead of Reci's and moves focus when a key changes the input
 * device: while armed, it moves the harness user's focus on a key-down of its key. It notes whether Reci had taken a
 * focus snapshot for that key-down by then. A test registers it in Slate's PreGame bucket, after Reci's snapshot.
 */
class FReciTestFocusMover final : public IInputProcessor
{
public:
	FKey Key;
	TWeakObjectPtr<UWidget> MoveTo;
	bool bArmed = false;
	bool bSawSnapshot = false;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override;

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("ReciTestFocusMover");
	}
};

#endif
