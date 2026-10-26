// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciMappedKeyText.h"

#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSettings.h"
#include "CommonTextBlock.h"
#include "CommonUITypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/PlatformSettingsManager.h"
#include "ICommonInputModule.h"
#include "Input/CommonBoundActionButton.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Interaction/ReciRoutingEnvironment.h"
#include "Settings/ReciUIInputData.h"
#include "Settings/ReciUIInputDataAccess.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/ReciKeyTextSegments.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciMappedKeyText)

#define LOCTEXT_NAMESPACE "ReciMappedKeyText"

namespace UE::ReciCommonUI::Private
{
	namespace
	{
		/**
		 * Short names for keys whose engine names would not fit a hint, so a hint line stays short on every device. The
		 * gamepad names are the XInput (Xbox) names: without controller data CommonUI cannot tell gamepad types apart.
		 */
		const FText* FindBuiltInKeyName(const FKey& Key)
		{
			static const TMap<FName, FText> BuiltInNames = {
				{EKeys::Escape.GetFName(), LOCTEXT("EscapeKeyShortName", "ESC")},
				{EKeys::BackSpace.GetFName(), LOCTEXT("BackSpaceKeyShortName", "BS")},
				{EKeys::Enter.GetFName(), LOCTEXT("EnterKeyShortName", "ENTER")},
				{EKeys::SpaceBar.GetFName(), LOCTEXT("SpaceBarKeyShortName", "SPACE")},
				{EKeys::PageUp.GetFName(), LOCTEXT("PageUpKeyShortName", "PGUP")},
				{EKeys::PageDown.GetFName(), LOCTEXT("PageDownKeyShortName", "PGDN")},
				{EKeys::LeftMouseButton.GetFName(), LOCTEXT("LeftMouseButtonShortName", "LMB")},
				{EKeys::RightMouseButton.GetFName(), LOCTEXT("RightMouseButtonShortName", "RMB")},
				{EKeys::MiddleMouseButton.GetFName(), LOCTEXT("MiddleMouseButtonShortName", "MMB")},
				{EKeys::Up.GetFName(), FText::FromString(TEXT("↑"))},
				{EKeys::Down.GetFName(), FText::FromString(TEXT("↓"))},
				{EKeys::Left.GetFName(), FText::FromString(TEXT("←"))},
				{EKeys::Right.GetFName(), FText::FromString(TEXT("→"))},
				{EKeys::Gamepad_FaceButton_Bottom.GetFName(), LOCTEXT("GamepadAShortName", "A")},
				{EKeys::Gamepad_FaceButton_Right.GetFName(), LOCTEXT("GamepadBShortName", "B")},
				{EKeys::Gamepad_FaceButton_Left.GetFName(), LOCTEXT("GamepadXShortName", "X")},
				{EKeys::Gamepad_FaceButton_Top.GetFName(), LOCTEXT("GamepadYShortName", "Y")},
				{EKeys::Gamepad_DPad_Up.GetFName(), FText::FromString(TEXT("D↑"))},
				{EKeys::Gamepad_DPad_Down.GetFName(), FText::FromString(TEXT("D↓"))},
				{EKeys::Gamepad_DPad_Left.GetFName(), FText::FromString(TEXT("D←"))},
				{EKeys::Gamepad_DPad_Right.GetFName(), FText::FromString(TEXT("D→"))},
				{EKeys::Gamepad_LeftY.GetFName(), LOCTEXT("GamepadLeftStickShortName", "L-STICK")},
				{EKeys::Gamepad_LeftX.GetFName(), LOCTEXT("GamepadLeftStickXShortName", "L-STICK")},
				{EKeys::Gamepad_Special_Right.GetFName(), LOCTEXT("GamepadMenuShortName", "MENU")},
				{EKeys::Gamepad_Special_Left.GetFName(), FText::FromString(TEXT("VIEW"))},
				{EKeys::Gamepad_LeftStick_Up.GetFName(), FText::FromString(TEXT("L↑"))},
				{EKeys::Gamepad_LeftStick_Down.GetFName(), FText::FromString(TEXT("L↓"))},
				{EKeys::Gamepad_LeftStick_Left.GetFName(), FText::FromString(TEXT("L←"))},
				{EKeys::Gamepad_LeftStick_Right.GetFName(), FText::FromString(TEXT("L→"))},
				{EKeys::Gamepad_RightStick_Up.GetFName(), FText::FromString(TEXT("R↑"))},
				{EKeys::Gamepad_RightStick_Down.GetFName(), FText::FromString(TEXT("R↓"))},
				{EKeys::Gamepad_RightStick_Left.GetFName(), FText::FromString(TEXT("R←"))},
				{EKeys::Gamepad_RightStick_Right.GetFName(), FText::FromString(TEXT("R→"))},
				{EKeys::Gamepad_LeftShoulder.GetFName(), FText::FromString(TEXT("LB"))},
				{EKeys::Gamepad_RightShoulder.GetFName(), FText::FromString(TEXT("RB"))},
				{EKeys::Gamepad_LeftTrigger.GetFName(), FText::FromString(TEXT("LT"))},
				{EKeys::Gamepad_RightTrigger.GetFName(), FText::FromString(TEXT("RT"))},
				{EKeys::Gamepad_LeftThumbstick.GetFName(), FText::FromString(TEXT("L3"))},
				{EKeys::Gamepad_RightThumbstick.GetFName(), FText::FromString(TEXT("R3"))}};
			return BuiltInNames.Find(Key.GetFName());
		}
	}

	TArray<FReciKeySegment> BuildKeySegments(TArray<TArray<FKey>> KeyGroups, const FReciKeySegmentRules& Rules)
	{
		// CommonUI's lookup for a key's icon (GetIconForEnhancedInputAction), over every key shown.
		const UCommonInputPlatformSettings* PlatformSettings = UCommonInputPlatformSettings::Get();
		struct FShownKey
		{
			FKey Key;
			FText Name;
			TOptional<FSlateBrush> Glyph;
			bool bAuthorName = false;
		};
		TArray<FKey> ShownKeys;
		TArray<FText> ShownNames;
		TArray<FReciKeySegment> Segments;
		for (const TArray<FKey>& Keys : KeyGroups)
		{
			TArray<FShownKey> Group;
			for (const FKey& Key : Keys)
			{
				// The filter first, so a key it leaves out neither hides a name nor counts toward the maximum. A key an earlier
				// action shows is not repeated.
				if (!Key.IsValid() || !CommonUI::IsKeyValidForInputType(Key, Rules.InputType)
					|| (Rules.KeyFilter && *Rules.KeyFilter && !(*Rules.KeyFilter)(Key))
					|| ShownKeys.Contains(Key))
				{
					continue;
				}
				FShownKey Shown;
				Shown.Key = Key;
				const FText* AuthorName = Rules.KeyNameOverrides ? Rules.KeyNameOverrides->Find(Key.GetFName()) : nullptr;
				Shown.bAuthorName = AuthorName != nullptr;
				FSlateBrush Brush;
				if (!AuthorName && PlatformSettings
					&& PlatformSettings->TryGetInputBrush(Brush, Key, Rules.InputType, Rules.GamepadName))
				{
					Shown.Glyph = Brush;
				}
				const FText* BuiltInName = AuthorName ? nullptr : FindBuiltInKeyName(Key);
				Shown.Name = AuthorName ? *AuthorName
					: BuiltInName ? *BuiltInName
					: Key.GetDisplayName(Rules.bUseLongDisplayNames);
				if (Rules.bUppercaseKeyNames)
				{
					Shown.Name = Shown.Name.ToUpper();
				}
				// A name already shown is not repeated; a glyph draws its own key.
				if (!Shown.Glyph.IsSet())
				{
					if (ShownNames.ContainsByPredicate([&Shown](const FText& Name) { return Name.EqualTo(Shown.Name); }))
					{
						continue;
					}
					ShownNames.Add(Shown.Name);
				}
				ShownKeys.Add(Key);
				Group.Add(MoveTemp(Shown));
				if (Rules.MaximumDisplayedKeys > 0 && Group.Num() >= Rules.MaximumDisplayedKeys)
				{
					break;
				}
			}
			if (Group.IsEmpty())
			{
				continue;
			}
			if (!Segments.IsEmpty())
			{
				Segments.Add({Rules.ActionSeparator});
			}
			// Two or more keys without a name of the author's own are one glyph where the controller data has one for the set.
			TArray<FKey> GroupKeys;
			TArray<FText> GroupNames;
			bool bAuthorNamed = false;
			for (const FShownKey& Shown : Group)
			{
				GroupKeys.Add(Shown.Key);
				GroupNames.Add(Shown.Name);
				bAuthorNamed |= Shown.bAuthorName;
			}
			FSlateBrush GroupBrush;
			if (Group.Num() >= 2 && !bAuthorNamed && PlatformSettings
				&& PlatformSettings->TryGetInputBrush(GroupBrush, GroupKeys, Rules.InputType, Rules.GamepadName))
			{
				Segments.Add({FText::Join(Rules.KeySeparator, GroupNames), GroupBrush});
				continue;
			}
			for (int32 Index = 0; Index < Group.Num(); ++Index)
			{
				if (Index > 0)
				{
					Segments.Add({Rules.KeySeparator});
				}
				Segments.Add({Group[Index].Name, Group[Index].Glyph});
			}
		}
		return Segments;
	}

	FText JoinKeySegmentText(const TConstArrayView<FReciKeySegment> Segments)
	{
		TArray<FText> Texts;
		for (const FReciKeySegment& Segment : Segments)
		{
			Texts.Add(Segment.Text);
		}
		return Texts.IsEmpty() ? FText::GetEmpty() : FText::Join(FText::GetEmpty(), Texts);
	}
}

namespace
{
	/** The name CommonUI binds a button's Input Action Widget by. */
	const FName InputActionWidgetName(TEXT("InputActionWidget"));

	/**
	 * Whether a hint line's action has its keys go to the focused widget: CommonUI's click action, which confirms it, and,
	 * while Reci's navigation follows Enhanced Input, the navigation actions, which move its focus.
	 */
	bool IsFocusedControlAction(const UInputAction* Action)
	{
		if (!Action)
		{
			return false;
		}
		if (Action == ICommonInputModule::GetSettings().GetEnhancedInputClickAction())
		{
			return true;
		}
		const UReciUIInputData* InputData = UE::ReciCommonUI::Private::GetFollowedInputData();
		return InputData
			&& (Action == InputData->NavigationUpAction || Action == InputData->NavigationDownAction
				|| Action == InputData->NavigationLeftAction || Action == InputData->NavigationRightAction);
	}

	EHorizontalAlignment GetRowAlignment(const ETextJustify::Type Justification)
	{
		switch (Justification)
		{
		case ETextJustify::Left:
		case ETextJustify::InvariantLeft:
			return HAlign_Left;
		case ETextJustify::Right:
		case ETextJustify::InvariantRight:
			return HAlign_Right;
		default:
			return HAlign_Center;
		}
	}
}

UReciMappedKeyText::UReciMappedKeyText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16))
	, KeySeparator(LOCTEXT("DefaultKeySeparator", " / "))
	, ActionSeparator(LOCTEXT("DefaultActionSeparator", " / "))
	, UnmappedText(LOCTEXT("DefaultUnmappedText", "UNBOUND"))
	, DesignTimePreviewText(LOCTEXT("DefaultDesignTimePreviewText", "ESC"))
{
}

TSharedRef<SWidget> UReciMappedKeyText::RebuildWidget()
{
	MyKeyRow = FKeyRow();
	MyReservationRow = FKeyRow();
	MyGamepadReservationRow = FKeyRow();
	SAssignNew(MyKeyBox, SBox)
		.Padding(Padding)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(MyReservationRow.Box, SHorizontalBox).Visibility(EVisibility::Hidden)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(MyGamepadReservationRow.Box, SHorizontalBox).Visibility(EVisibility::Hidden)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(MyFallbackReservationText, STextBlock).Visibility(EVisibility::Hidden)
			]
			+ SOverlay::Slot()
			.Expose(MyKeyRowSlot)
			.HAlign(GetRowAlignment(Justification.GetValue()))
			[
				// Keys never break: the reserved space is as wide as the widest row.
				SAssignNew(MyKeyRow.Box, SHorizontalBox)
			]
		];
	ApplyTextAppearance(*MyFallbackReservationText, false);
	const FKeySegment Preview{DesignTimePreviewText};
	FillRow(MyReservationRow, MakeArrayView(&Preview, 1), false);
	FillRow(MyGamepadReservationRow, TConstArrayView<FKeySegment>(), false);
	FillRow(MyKeyRow, MakeArrayView(&Preview, 1), true);

	return MyKeyBox.ToSharedRef();
}

void UReciMappedKeyText::ReleaseSlateResources(const bool bReleaseChildren)
{
	if (KeyRoutingListener.IsValid())
	{
		FReciInputFocusCoordinator::Get().RemoveKeyRoutingListener(KeyRoutingListener);
		KeyRoutingListener.Reset();
	}
	// The images go before the brushes they draw.
	for (FKeyRow* Row : {&MyKeyRow, &MyReservationRow, &MyGamepadReservationRow})
	{
		if (Row->Box.IsValid())
		{
			Row->Box->ClearChildren();
		}
		*Row = FKeyRow();
	}
	MyKeyRowSlot = nullptr;
	MyFallbackReservationText.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UReciMappedKeyText::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyAppearance();
	UpdateKeyRoutingListener();
	UpdateActionWidget();
}

void UReciMappedKeyText::RefreshMappedKeyText()
{
	UpdateActionWidget();
}

FText UReciMappedKeyText::GetMappedKeyText() const
{
	if (IsDesignTime())
	{
		return DesignTimePreviewText;
	}

	return UE::ReciCommonUI::Private::JoinKeySegmentText(BuildMappedKeySegments(GetDisplayedInputType(), true));
}

void UReciMappedKeyText::SetEnhancedInputActions(const TArray<UInputAction*>& InActions)
{
	AdditionalInputActions.Reset();
	for (int32 Index = 1; Index < InActions.Num(); ++Index)
	{
		if (InActions[Index])
		{
			AdditionalInputActions.Add(InActions[Index]);
		}
	}
	// Updates the widget, with the further actions already in place.
	SetEnhancedInputAction(InActions.IsEmpty() ? nullptr : InActions[0]);
}

void UReciMappedKeyText::SetKeyDevice(const EReciKeyTextDevice InKeyDevice)
{
	if (KeyDevice != InKeyDevice)
	{
		KeyDevice = InKeyDevice;
		UpdateKeyRoutingListener();
		UpdateActionWidget();
	}
}

void UReciMappedKeyText::SetKeyFilter(TFunction<bool(const FKey&)> InKeyFilter)
{
	KeyFilter = MoveTemp(InKeyFilter);
	UpdateKeyRoutingListener();
	UpdateActionWidget();
}

bool UReciMappedKeyText::IsActionBarText() const
{
	return !IsDesignTime() && !KeyFilter && GetFName() == InputActionWidgetName
		&& Cast<UCommonBoundActionButton>(GetTypedOuter<UUserWidget>()) != nullptr;
}

bool UReciMappedKeyText::IsHintLineText() const
{
	if (IsDesignTime() || KeyFilter || KeyDevice != EReciKeyTextDevice::Current)
	{
		return false;
	}
	// Another CommonUI button's Input Action Widget shows that button's hint: a Reci button filters it itself, and a stock
	// button's hint is CommonUI's choice.
	return GetFName() != InputActionWidgetName || !Cast<UCommonButtonBase>(GetTypedOuter<UUserWidget>());
}

bool UReciMappedKeyText::FiltersKeys() const
{
	return static_cast<bool>(KeyFilter) || IsActionBarText() || IsHintLineText();
}

void UReciMappedKeyText::UpdateKeyRoutingListener()
{
	// Which keys an earlier handler takes changes with focus, popups and screens, not only with the mappings.
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	const bool bListen = MyKeyRow.Box.IsValid() && (IsActionBarText() || IsHintLineText());
	if (bListen && !KeyRoutingListener.IsValid())
	{
		KeyRoutingListener = Coordinator.AddKeyRoutingListener(this, [this](const int32 UserIndex)
		{
			const TOptional<int32> KeyUser = GetKeyUserIndex();
			if (KeyUser.IsSet() && KeyUser.GetValue() == UserIndex)
			{
				UpdateActionWidget();
			}
		});
	}
	else if (!bListen && KeyRoutingListener.IsValid())
	{
		Coordinator.RemoveKeyRoutingListener(KeyRoutingListener);
		KeyRoutingListener.Reset();
	}
}

void UReciMappedKeyText::LeaveOutTakenKeys(
	TArray<TArray<FKey>>& KeyGroups,
	const TConstArrayView<const UInputAction*> GroupActions,
	const ECommonInputType InputType) const
{
	const bool bActionBar = IsActionBarText();
	const TOptional<int32> User = bActionBar || IsHintLineText() ? GetKeyUserIndex() : TOptional<int32>();
	if (!User.IsSet())
	{
		return;
	}
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	// A hint line on a layer outside CommonUI's active root shows no key: neither its screen's actions nor CommonUI's
	// bindings there get one. An action bar shows only the active root's bindings.
	const bool bLayerGetsInput = bActionBar || UE::ReciCommonUI::Private::IsLayerInActiveRoot(*this);
	for (int32 Index = 0; Index < KeyGroups.Num(); ++Index)
	{
		const UInputAction* Action = GroupActions.IsValidIndex(Index) ? GroupActions[Index] : nullptr;
		const EReciKeyHintStage Stage = bActionBar ? EReciKeyHintStage::Stock
			: bKeyCaptureHint ? EReciKeyHintStage::ScreenKey
			: IsFocusedControlAction(Action) ? EReciKeyHintStage::FocusedControl
			: EReciKeyHintStage::ScreenAction;
		KeyGroups[Index].RemoveAll([&](const FKey& Key)
		{
			// A key the device does not show needs no answer.
			return !bLayerGetsInput
				|| (CommonUI::IsKeyValidForInputType(Key, InputType)
					&& Coordinator.IsKeyTakenBefore(User.GetValue(), Key, Stage, Action));
		});
	}
}

#if WITH_EDITOR
const FText UReciMappedKeyText::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Reci Common UI");
}

bool UReciMappedKeyText::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}
	static const FName StyledProperties[] = {
		GET_MEMBER_NAME_CHECKED(UReciMappedKeyText, Font),
		GET_MEMBER_NAME_CHECKED(UReciMappedKeyText, ColorAndOpacity),
		GET_MEMBER_NAME_CHECKED(UReciMappedKeyText, ShadowOffset),
		GET_MEMBER_NAME_CHECKED(UReciMappedKeyText, ShadowColorAndOpacity)};
	return !GetStyleCDO() || !MakeArrayView(StyledProperties).Contains(InProperty->GetFName());
}
#endif

void UReciMappedKeyText::UpdateActionWidget()
{
	if (!MyKeyRow.Box.IsValid())
	{
		return;
	}

	if (IsDesignTime())
	{
		ReserveHintLayout();
		const FKeySegment Preview{DesignTimePreviewText};
		FillRow(MyKeyRow, MakeArrayView(&Preview, 1), true);
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	// Current hides on touch, which has no keys to show. A fixed device shows its keys even on touch: a key-binding
	// row is about that device, not the current one.
	const TOptional<FReciInputMethod> InputMethod = GetInputMethod();
	if (bAlwaysHideOverride
		|| !HasRepresentedAction()
		|| !InputMethod.IsSet()
		|| (KeyDevice == EReciKeyTextDevice::Current
			&& (!InputMethod->bShowsInputKeys || InputMethod->InputType == ECommonInputType::Touch)))
	{
		ReserveHintLayout();
		bShowsMappedKeys = false;
		SetVisibility(ESlateVisibility::Hidden);
		SetShowsKeys(false);
		return;
	}

	const TArray<FKeySegment> Segments = BuildMappedKeySegments(GetDisplayedInputType(), true);
	const bool bHasMappedKey = !Segments.IsEmpty();
	// Keys the filter all leaves out are mapped, but none reaches its action now: nothing shows, not Unmapped Text.
	const bool bFilteredOut = !bHasMappedKey && FiltersKeys() && !BuildMappedKeySegments(GetDisplayedInputType(), false).IsEmpty();
	// A screen's own mapping context leaves as soon as the screen deactivates, which can be while it still fades
	// out. Its keys and their reserved room stay as they were rather than turn unmapped; the screen's next
	// activation refreshes them.
	const UCommonActivatableWidget* OwningScreen = GetTypedOuter<UCommonActivatableWidget>();
	if (!bHasMappedKey && !bFilteredOut && bShowsMappedKeys && OwningScreen && !OwningScreen->IsActivated())
	{
		return;
	}
	ReserveHintLayout();
	bShowsMappedKeys = bHasMappedKey || bFilteredOut;
	if (bFilteredOut || (!bHasMappedKey && bHideWhenUnmapped))
	{
		if (bFilteredOut)
		{
			// Hidden, the row keeps the room of the keys it leaves out, for the device shown now.
			FillRow(MyKeyRow, BuildMappedKeySegments(GetDisplayedInputType(), false), true);
		}
		SetVisibility(ESlateVisibility::Hidden);
		SetShowsKeys(false);
		return;
	}

	const FKeySegment Unmapped{UnmappedText};
	FillRow(MyKeyRow, bHasMappedKey ? TConstArrayView<FKeySegment>(Segments) : MakeArrayView(&Unmapped, 1), true);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	OnInputIconUpdated.Broadcast();
	SetShowsKeys(bHasMappedKey);
}

void UReciMappedKeyText::SetShowsKeys(const bool bShowsKeys)
{
	if (LastShowsKeys != bShowsKeys)
	{
		LastShowsKeys = bShowsKeys;
		OnShowsKeysChanged.Broadcast(bShowsKeys);
	}
}

ECommonInputType UReciMappedKeyText::GetDisplayedInputType() const
{
	switch (KeyDevice)
	{
	case EReciKeyTextDevice::KeyboardAndMouse:
		return ECommonInputType::MouseAndKeyboard;
	case EReciKeyTextDevice::Gamepad:
		return ECommonInputType::Gamepad;
	default:
		break;
	}
	const TOptional<FReciInputMethod> InputMethod = GetInputMethod();
	return InputMethod.IsSet() ? InputMethod->InputType : ECommonInputType::MouseAndKeyboard;
}

const ULocalPlayer* UReciMappedKeyText::GetKeyPlayer() const
{
	// An action bar's button shows the keys of the player its binding belongs to.
	const ULocalPlayer* BoundLocalPlayer = DisplayedBindingHandle.GetBoundLocalPlayer();
	return BoundLocalPlayer ? BoundLocalPlayer : GetOwningLocalPlayer();
}

TOptional<int32> UReciMappedKeyText::GetKeyUserIndex() const
{
	const ULocalPlayer* BoundLocalPlayer = DisplayedBindingHandle.GetBoundLocalPlayer();
	if (BoundLocalPlayer && BoundLocalPlayer != GetOwningLocalPlayer() && FSlateApplication::IsInitialized())
	{
		return FSlateApplication::Get().GetUserIndexForController(BoundLocalPlayer->GetControllerId());
	}
	return FReciInputFocusCoordinator::Get().GetUserIndex(*this);
}

TOptional<FReciInputMethod> UReciMappedKeyText::GetInputMethod() const
{
	const ULocalPlayer* Player = GetKeyPlayer();
	return Player
		? UE::ReciCommonUI::Private::GetRoutingEnvironment().GetInputMethod(*Player)
		: TOptional<FReciInputMethod>();
}

void UReciMappedKeyText::ReserveHintLayout()
{
	if (!MyKeyBox.IsValid() || !FSlateApplication::IsInitialized()) return;
	// Let Slate reserve the real size of both rows, glyphs included, without a width constraint. A fixed device has one.
	const bool bShowsKeyboard = KeyDevice != EReciKeyTextDevice::Gamepad;
	const bool bShowsGamepad = KeyDevice != EReciKeyTextDevice::KeyboardAndMouse;
	TArray<FKeySegment> KeyboardSegments;
	if (IsDesignTime())
	{
		KeyboardSegments.Add({DesignTimePreviewText});
	}
	else if (bShowsKeyboard)
	{
		KeyboardSegments = BuildMappedKeySegments(ECommonInputType::MouseAndKeyboard, false);
	}
	// Unfiltered: the room stays the same while another handler takes some of the keys.
	const TArray<FKeySegment> GamepadSegments =
		IsDesignTime() || !bShowsGamepad ? TArray<FKeySegment>() : BuildMappedKeySegments(ECommonInputType::Gamepad, false);
	const bool bNoKeys = UE::ReciCommonUI::Private::JoinKeySegmentText(KeyboardSegments).IsEmpty() && GamepadSegments.IsEmpty();
	const FText FallbackText = !bHideWhenUnmapped ? UnmappedText : bNoKeys ? DesignTimePreviewText : FText::GetEmpty();
	const bool bReserve = bReserveAllDeviceWidths || IsDesignTime();
	FillRow(MyReservationRow, bReserve ? TConstArrayView<FKeySegment>(KeyboardSegments) : TConstArrayView<FKeySegment>(), false);
	FillRow(MyGamepadReservationRow, bReserve ? TConstArrayView<FKeySegment>(GamepadSegments) : TConstArrayView<FKeySegment>(), false);
	if (MyFallbackReservationText.IsValid())
	{
		ApplyTextAppearance(*MyFallbackReservationText, false);
		MyFallbackReservationText->SetText(bReserve ? FallbackText : FText::GetEmpty());
	}
	MyKeyBox->SetMinDesiredHeight(MinimumReservedHeight);
}

TArray<UReciMappedKeyText::FKeySegment> UReciMappedKeyText::BuildMappedKeySegments(
	const ECommonInputType InputType,
	const bool bFiltered) const
{
	const TOptional<FReciInputMethod> InputMethod = GetInputMethod();
	if (!InputMethod.IsSet())
	{
		return {};
	}

	// Each action's keys form a group.
	TArray<TArray<FKey>> KeyGroups;
	TArray<const UInputAction*> GroupActions;
	if (EnhancedInputAction && CommonUI::IsEnhancedInputSupportEnabled())
	{
		KeyGroups.Add(GetActionKeys(EnhancedInputAction));
		GroupActions.Add(EnhancedInputAction);
		for (const UInputAction* Action : AdditionalInputActions)
		{
			if (Action)
			{
				KeyGroups.Add(GetActionKeys(Action));
				GroupActions.Add(Action);
			}
		}
	}
	else if (const FCommonInputActionDataBase* InputActionData = GetInputActionData())
	{
		KeyGroups.Add({InputActionData->GetInputTypeInfo(InputType, InputMethod->GamepadName).GetKey()});
		GroupActions.Add(nullptr);
	}
	if (bFiltered && !KeyFilter)
	{
		LeaveOutTakenKeys(KeyGroups, GroupActions, InputType);
	}

	return UE::ReciCommonUI::Private::BuildKeySegments(
		MoveTemp(KeyGroups), MakeSegmentRules(InputType, InputMethod->GamepadName, bFiltered));
}

UE::ReciCommonUI::Private::FReciKeySegmentRules UReciMappedKeyText::MakeSegmentRules(
	const ECommonInputType InputType,
	const FName GamepadName,
	const bool bFiltered) const
{
	UE::ReciCommonUI::Private::FReciKeySegmentRules Rules;
	Rules.InputType = InputType;
	Rules.GamepadName = GamepadName;
	Rules.KeyNameOverrides = &KeyNameOverrides;
	Rules.bUseLongDisplayNames = bUseLongDisplayNames;
	Rules.bUppercaseKeyNames = bUppercaseKeyNames;
	Rules.MaximumDisplayedKeys = MaximumDisplayedKeys;
	Rules.KeySeparator = KeySeparator;
	Rules.ActionSeparator = ActionSeparator;
	Rules.KeyFilter = bFiltered && KeyFilter ? &KeyFilter : nullptr;
	return Rules;
}

TArray<FKey> UReciMappedKeyText::GetActionKeys(const UInputAction* Action) const
{
	const ULocalPlayer* Player = GetKeyPlayer();
	return Player && Action
		? UE::ReciCommonUI::Private::GetRoutingEnvironment().GetPlayerActionKeys(*Player, *Action)
		: TArray<FKey>();
}

bool UReciMappedKeyText::HasRepresentedAction() const
{
	return (EnhancedInputAction && CommonUI::IsEnhancedInputSupportEnabled())
		|| GetInputActionData() != nullptr;
}

void UReciMappedKeyText::FillRow(FKeyRow& Row, const TConstArrayView<FKeySegment> Segments, const bool bVisibleRow)
{
	if (!Row.Box.IsValid())
	{
		return;
	}
	// The images go before the brushes they draw.
	Row.Box->ClearChildren();
	Row.Texts.Reset();
	Row.Brushes.Reset();
	bool bHasGlyph = false;
	for (const FKeySegment& Segment : Segments)
	{
		bHasGlyph |= Segment.Glyph.IsSet();
	}
	// Names and separators in a row are one text, laid out as text alone; beside a glyph, text is centred on it.
	const EVerticalAlignment TextAlignment = bHasGlyph ? VAlign_Center : VAlign_Fill;
	TArray<FText> Run;
	auto AddText = [this, &Row, &Run, bVisibleRow, TextAlignment]()
	{
		const TSharedRef<STextBlock> Text =
			SNew(STextBlock).Text(Run.Num() == 1 ? Run[0] : FText::Join(FText::GetEmpty(), Run));
		ApplyTextAppearance(*Text, bVisibleRow);
		Row.Box->AddSlot().AutoWidth().VAlign(TextAlignment)[Text];
		Row.Texts.Add(Text);
		Run.Reset();
	};
	for (const FKeySegment& Segment : Segments)
	{
		if (!Segment.Glyph.IsSet())
		{
			Run.Add(Segment.Text);
			continue;
		}
		if (!Run.IsEmpty())
		{
			AddText();
		}
		// Drawn at the brush's own Image Size, as CommonUI draws an action's icon.
		const TSharedRef<FSlateBrush> Brush = MakeShared<FSlateBrush>(Segment.Glyph.GetValue());
		Row.Brushes.Add(Brush);
		Row.Box->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SImage).Image(&Brush.Get())];
	}
	// An empty row keeps an empty text, which holds a line's height.
	if (!Run.IsEmpty() || Row.Box->NumSlots() == 0)
	{
		AddText();
	}
}

const UCommonTextStyle* UReciMappedKeyText::GetStyleCDO() const
{
	return Style ? Cast<UCommonTextStyle>(Style->GetDefaultObject(false)) : nullptr;
}

void UReciMappedKeyText::ApplyTextAppearance(STextBlock& Text, const bool bVisibleRow) const
{
	// A style gives the font, color and shadow, as it does a Common Text Block's (UCommonTextStyle::ApplyToTextBlock).
	const UCommonTextStyle* TextStyle = GetStyleCDO();
	Text.SetFont(TextStyle ? TextStyle->Font : Font);
	if (bVisibleRow)
	{
		if (TextStyle)
		{
			Text.SetColorAndOpacity(TextStyle->Color);
			Text.SetShadowOffset(TextStyle->bUsesDropShadow ? TextStyle->ShadowOffset : FVector2D::ZeroVector);
			Text.SetShadowColorAndOpacity(TextStyle->bUsesDropShadow ? TextStyle->ShadowColor : FLinearColor::Transparent);
		}
		else
		{
			Text.SetColorAndOpacity(ColorAndOpacity);
			Text.SetShadowOffset(ShadowOffset);
			Text.SetShadowColorAndOpacity(ShadowColorAndOpacity);
		}
		Text.SetJustification(Justification.GetValue());
	}
}

void UReciMappedKeyText::ApplyAppearance()
{
	if (MyKeyBox.IsValid())
	{
		MyKeyBox->SetPadding(Padding);
	}
	if (MyKeyRowSlot)
	{
		MyKeyRowSlot->SetHorizontalAlignment(GetRowAlignment(Justification.GetValue()));
	}
	for (const TSharedRef<STextBlock>& Text : MyKeyRow.Texts)
	{
		ApplyTextAppearance(*Text, true);
	}
	for (const FKeyRow* Row : {&MyReservationRow, &MyGamepadReservationRow})
	{
		for (const TSharedRef<STextBlock>& Text : Row->Texts)
		{
			ApplyTextAppearance(*Text, false);
		}
	}
}

#undef LOCTEXT_NAMESPACE
