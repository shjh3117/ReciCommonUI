// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciFocusStyleButtonBase.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/Spacer.h"
#include "CommonInputSettings.h"
#include "CommonUITypes.h"
#include "ICommonInputModule.h"
#include "InputAction.h"
#include "Input/UIActionBindingHandle.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Interaction/ReciRoutingEnvironment.h"
#include "ReciReport.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciMappedKeyText.h"
#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciFocusStyleButtonBase)

UCommonButtonInternalBase*
UReciFocusStyleButtonBase::ConstructInternalButton()
{
	UCommonButtonInternalBase* Button = Super::ConstructInternalButton();
	InternalButton = Button;
	bHasBaseButtonStyle = false;
	bInteractionButtonStyleApplied = false;
	return Button;
}

bool UReciFocusStyleButtonBase::IsPointerHoverAllowedByOwningList() const
{
	const UReciListBase* OwningList = GetTypedOuter<UReciListBase>();
	return !OwningList || OwningList->IsPointerHoverAllowed();
}

void UReciFocusStyleButtonBase::SynchronizeProperties()
{
	bStateEnabled = GetIsEnabled();
	bStateHovered =
		bUsePointerHoverVisual
			&& IsPointerHoverAllowedByOwningList()
			&& UCommonButtonBase::IsHovered();
	bStateFocused = HasAnyUserFocus() || HasFocusedDescendants();

	Super::SynchronizeProperties();
	CaptureBaseButtonStyle();
	ApplyInteractionStyle();
}

void UReciFocusStyleButtonBase::NativeOnHovered()
{
	if (!ShouldProcessHoverEvent(EHoverEventSource::Unknown))
	{
		return;
	}

	if (!IsPointerHoverAllowedByOwningList())
	{
		bStateHovered = false;
		ApplyInteractionStyle();
		NativeOnCurrentTextStyleChanged();
		return;
	}

	bStateHovered = bUsePointerHoverVisual;
	ApplyInteractionStyle();
	Super::NativeOnHovered();

	const UReciListBase* OwningList = GetTypedOuter<UReciListBase>();
	if (bFocusOnHover && (!OwningList || OwningList->bFocusOnPointerHover)
		&& GetIsFocusable()
		&& GetIsEnabled()
		&& IsInteractionEnabled()
		&& !HasAnyUserFocus()
		&& !HasFocusedDescendants())
	{
		SetFocus();
	}
}

void UReciFocusStyleButtonBase::SetUsePointerHoverVisual(
	const bool bInUsePointerHoverVisual)
{
	if (bUsePointerHoverVisual == bInUsePointerHoverVisual)
	{
		return;
	}

	bUsePointerHoverVisual = bInUsePointerHoverVisual;
	bStateHovered =
		bUsePointerHoverVisual
			&& IsPointerHoverAllowedByOwningList()
			&& UCommonButtonBase::IsHovered();
	ApplyInteractionStyle();
	NativeOnCurrentTextStyleChanged();
}

void UReciFocusStyleButtonBase::NativeOnUnhovered()
{
	if (!ShouldProcessHoverEvent(EHoverEventSource::Unknown))
	{
		return;
	}

	if (!IsPointerHoverAllowedByOwningList())
	{
		bStateHovered = false;
		ApplyInteractionStyle();
		NativeOnCurrentTextStyleChanged();
		return;
	}

	bStateHovered = false;
	ApplyInteractionStyle();
	Super::NativeOnUnhovered();
}

void UReciFocusStyleButtonBase::NativeOnEnabled()
{
	bStateEnabled = true;
	Super::NativeOnEnabled();
	ApplyInteractionStyle();
	// CommonUI's EnableButton updates the Input Action Widget next.
}

void UReciFocusStyleButtonBase::NativeOnDisabled()
{
	bStateEnabled = false;
	Super::NativeOnDisabled();
	ApplyInteractionStyle();
	// CommonUI's DisableButton updates the Input Action Widget next.
}

bool UReciFocusStyleButtonBase::Initialize()
{
	const bool bInitializedThisCall = Super::Initialize();
	if (bInitializedThisCall && MaxDisplayScale > 1.0f && WidgetTree && WidgetTree->RootWidget && !DisplayScaleBox)
	{
		// The internal root button carries the style background and outline: scaling above it scales the whole button.
		UWidget* Button = WidgetTree->RootWidget;
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DisplayScaleRow"));
		DisplayScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("DisplayScaleBox"));
		DisplayScaleBox->SetStretch(EStretch::UserSpecified);
		if (UScaleBoxSlot* ButtonSlot = Cast<UScaleBoxSlot>(DisplayScaleBox->AddChild(Button)))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}
		DisplayLeadingSlot = Row->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
		DisplayScaleSlot = Row->AddChildToHorizontalBox(DisplayScaleBox);
		DisplayTrailingSlot = Row->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
		DisplayScaleSlot->SetVerticalAlignment(VAlign_Fill);
		WidgetTree->RootWidget = Row;
		ApplyDisplayScale();
	}
	return bInitializedThisCall;
}

void UReciFocusStyleButtonBase::SetDisplayScale(const float InScale)
{
	DisplayScale = FMath::Clamp(InScale, 1.0f, FMath::Max(1.0f, MaxDisplayScale));
	ApplyDisplayScale();
}

void UReciFocusStyleButtonBase::ApplyDisplayScale()
{
	if (!DisplayScaleBox || !DisplayLeadingSlot || !DisplayScaleSlot || !DisplayTrailingSlot)
	{
		return;
	}
	// The button's own width stays (given width / Max) at every scale: fill shares S / Max for the button and the
	// rest split evenly around it, then the scale box lays the button out at that width divided by S.
	auto FillShare = [](const float Value)
	{
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Value;
		return Size;
	};
	const float Margin = (MaxDisplayScale - DisplayScale) * 0.5f;
	DisplayScaleBox->SetUserSpecifiedScale(DisplayScale);
	DisplayLeadingSlot->SetSize(FillShare(Margin));
	DisplayScaleSlot->SetSize(FillShare(DisplayScale));
	DisplayTrailingSlot->SetSize(FillShare(Margin));
}

void UReciFocusStyleButtonBase::NativeConstruct()
{
	Super::NativeConstruct();
	// The hint leaves out keys an earlier handler takes now, so it follows changes of the routing.
	if (!IsDesignTime() && Cast<UReciMappedKeyText>(InputActionWidget) && !KeyRoutingListener.IsValid())
	{
		KeyRoutingListener = FReciInputFocusCoordinator::Get().AddKeyRoutingListener(this, [this](const int32 UserIndex)
		{
			HandleKeyRoutingChanged(UserIndex);
		});
	}
}

void UReciFocusStyleButtonBase::NativeDestruct()
{
	if (KeyRoutingListener.IsValid())
	{
		FReciInputFocusCoordinator::Get().RemoveKeyRoutingListener(KeyRoutingListener);
		KeyRoutingListener.Reset();
	}
	Super::NativeDestruct();
}

void UReciFocusStyleButtonBase::HandleKeyRoutingChanged(const int32 UserIndex)
{
	const TOptional<int32> ButtonUser = FReciInputFocusCoordinator::Get().GetUserIndex(*this);
	UReciMappedKeyText* KeyText = Cast<UReciMappedKeyText>(InputActionWidget);
	if (KeyText && ButtonUser.IsSet() && ButtonUser.GetValue() == UserIndex)
	{
		// The action stays; which of its keys an earlier handler takes may not.
		KeyText->RefreshMappedKeyText();
	}
}

bool UReciFocusStyleButtonBase::DoesHintKeyReach(const FKey& Key) const
{
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	if (!bHintKeysReach)
	{
		return false;
	}
	const TOptional<int32> ButtonUser = Coordinator.GetUserIndex(*this);
	if (!ButtonUser.IsSet())
	{
		return true;
	}
	// Outside CommonUI's active root the button's bindings are inactive and its screen gets no screen input.
	return UE::ReciCommonUI::Private::IsLayerInActiveRoot(*this)
		&& !Coordinator.IsKeyTakenBefore(ButtonUser.GetValue(), Key, HintStage, HintOwnAction.Get());
}

void UReciFocusStyleButtonBase::BindTriggeringInputActionToClick()
{
	Super::BindTriggeringInputActionToClick();
	if (bShowTriggeringActionInActionBar && TriggeringBindingHandle.IsValid())
	{
		TriggeringBindingHandle.SetDisplayInActionBar(true);
	}
}

void UReciFocusStyleButtonBase::UpdateInputActionWidget()
{
	// CommonUI's choice, extended in this one place (DESIGN.md principle 6). Super is not called: it shows the click action
	// on pointer hover, where keys do not act. Every change of the button's actions, state or flags ends here.
	if (!InputActionWidget || IsDesignTime())
	{
		return;
	}
	UReciMappedKeyText* KeyText = Cast<UReciMappedKeyText>(InputActionWidget);
	if (KeyText && !KeyText->bHideWhenUnmapped)
	{
		// A key that does not exist is no hint: an action with no keys shows nothing, rather than Unmapped Text.
		RECI_WARN_ONCE(TEXT("HintHideWhenUnmapped"), GetClass()->GetPathName(),
			TEXT("%s: InputActionWidget has Hide When Unmapped off. A button's hint shows nothing for an action with no keys, "
				"so Reci turns it on and Unmapped Text never shows."), *GetClass()->GetName());
		KeyText->bHideWhenUnmapped = true;
	}

	const bool bEnhancedInput = CommonUI::IsEnhancedInputSupportEnabled();
	UInputAction* Action = nullptr;
	FDataTableRowHandle Row;
	// Where the action's keys go, for the key text's filter: CommonUI's bindings unless said otherwise.
	EReciKeyHintStage Stage = EReciKeyHintStage::Stock;
	if (!bStateEnabled)
	{
		// Reci's extension: a disabled button cannot act, so it shows no key, where CommonUI would grey it.
	}
	else if (bEnhancedInput && TriggeringEnhancedInputAction)
	{
		Action = TriggeringEnhancedInputAction;
	}
	else if (!TriggeringInputAction.IsNull())
	{
		Row = TriggeringInputAction;
	}
	else if (!TriggeredInputAction.IsNull())
	{
		Row = TriggeredInputAction;
	}
	else if (HintInputAction)
	{
		// Reci's extension: a key the screen handles on the button's behalf, shown whatever has focus, as long as it
		// reaches the screen's action (or, if no screen maps it, CommonUI's handling, such as Back).
		Action = HintInputAction;
		Stage = EReciKeyHintStage::ScreenAction;
	}
	else if (bShouldUseFallbackDefaultInputAction)
	{
		// The click action confirms the focused control only. CommonUI's "hovered" reads as "keys act here". Without focus
		// its keys are left out, and their room stays reserved, so the button keeps its width as focus moves.
		Action = bEnhancedInput ? ICommonInputModule::GetSettings().GetEnhancedInputClickAction() : nullptr;
		if (!Action)
		{
			Row = ICommonInputModule::GetSettings().GetDefaultClickAction();
		}
		Stage = EReciKeyHintStage::FocusedControl;
	}
	const bool bKeysReach = Stage != EReciKeyHintStage::FocusedControl || bStateFocused;
	const bool bRoutingChanged = Stage != HintStage || Action != HintOwnAction.Get() || bKeysReach != bHintKeysReach;
	HintStage = Stage;
	HintOwnAction = Action;
	bHintKeysReach = bKeysReach;
	if (KeyText && FilteredKeyText.Get() != KeyText)
	{
		// The hint leaves out the keys an earlier handler takes now (DESIGN.md principle 6).
		FilteredKeyText = KeyText;
		KeyText->SetKeyFilter([WeakThis = TWeakObjectPtr<const UReciFocusStyleButtonBase>(this)](const FKey& Key)
		{
			const UReciFocusStyleButtonBase* Button = WeakThis.Get();
			return !Button || Button->DoesHintKeyReach(Key);
		});
	}
	if (!ShowHintAction(Action, Row) && bRoutingChanged && KeyText)
	{
		KeyText->RefreshMappedKeyText();
	}
	// CommonUI's flags (Hide Input Action, Hide Input Action With Keyboard, and the selection rules) decide the rest. They
	// are also applied directly by CommonUI's setters, so Reci never hides the widget itself.
	UpdateInputActionWidgetVisibility();
}

bool UReciFocusStyleButtonBase::ShowHintAction(UInputAction* Action, const FDataTableRowHandle& Row)
{
	UReciMappedKeyText* KeyText = Cast<UReciMappedKeyText>(InputActionWidget);
	if (Action)
	{
		// One action only: a key text's Additional Input Actions are cleared.
		if (InputActionWidget->GetEnhancedInputAction() != Action || (KeyText && !KeyText->AdditionalInputActions.IsEmpty()))
		{
			if (KeyText)
			{
				KeyText->SetEnhancedInputActions({Action});
			}
			else
			{
				InputActionWidget->SetEnhancedInputAction(Action);
			}
			return true;
		}
		return false;
	}
	if (!Row.IsNull())
	{
		InputActionWidget->SetInputAction(Row);
		return true;
	}
	// No action, as CommonUI's own empty state.
	if (KeyText)
	{
		KeyText->SetEnhancedInputActions({});
	}
	else
	{
		InputActionWidget->SetEnhancedInputAction(nullptr);
	}
	InputActionWidget->SetInputAction(FDataTableRowHandle());
	return true;
}

#if WITH_EDITOR
void UReciFocusStyleButtonBase::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);
	// No InputActionWidget is a button without a hint; one that is not a Common Action Widget is the BindWidget check's.
	const UWidget* Widget = BlueprintWidgetTree.FindWidget(GET_MEMBER_NAME_CHECKED(UReciFocusStyleButtonBase, InputActionWidget));
	if (!Widget || !Widget->IsA<UCommonActionWidget>())
	{
		return;
	}
	const UReciMappedKeyText* KeyText = Cast<UReciMappedKeyText>(Widget);
	if (!KeyText)
	{
		CompileLog.Warning(FText::FromString(FString::Printf(TEXT("InputActionWidget is a %s: it draws only CommonUI's icon for "
			"an action's first key, and cannot leave out keys another handler takes first. Use a Reci Mapped Key Text."),
			*Widget->GetClass()->GetName())));
	}
	else if (!KeyText->bHideWhenUnmapped)
	{
		CompileLog.Warning(FText::FromString(TEXT("InputActionWidget: a button's hint shows nothing for an action with no keys; "
			"turn on Hide When Unmapped (Reci turns it on at run time, so Unmapped Text never shows).")));
	}
}
#endif

void UReciFocusStyleButtonBase::SetHintInputAction(UInputAction* InHintInputAction)
{
	HintInputAction = InHintInputAction;
	UpdateInputActionWidget();
}

void UReciFocusStyleButtonBase::HandleFocusReceived()
{
	bStateFocused = true;
	UpdateInputActionWidget();
	ApplyInteractionStyle();

	if (!bStateHovered)
	{
		NativeOnCurrentTextStyleChanged();
	}

	Super::HandleFocusReceived();
}

void UReciFocusStyleButtonBase::HandleFocusLost()
{
	bStateFocused = false;
	UpdateInputActionWidget();
	ApplyInteractionStyle();

	if (!bStateHovered)
	{
		NativeOnCurrentTextStyleChanged();
	}

	Super::HandleFocusLost();
}

void UReciFocusStyleButtonBase::NativeOnCurrentTextStyleChanged()
{
	CaptureBaseButtonStyle();
	ApplyInteractionStyle();
	Super::NativeOnCurrentTextStyleChanged();
	ApplyCurrentTextStyleToDescendants();
}

void UReciFocusStyleButtonBase::CaptureBaseButtonStyle()
{
	UCommonButtonInternalBase* Button = InternalButton.Get();
	if (!Button)
	{
		return;
	}

	if (IsCurrentInternalStyleProjected())
	{
		return;
	}

	BaseButtonStyle = Button->GetStyle();
	bHasBaseButtonStyle = true;
	bInteractionButtonStyleApplied = false;
}

void UReciFocusStyleButtonBase::ApplyInteractionStyle()
{
	UCommonButtonInternalBase* Button = InternalButton.Get();
	if (!Button || !bHasBaseButtonStyle)
	{
		return;
	}

	const bool bUseHoveredStyle = UsesHoveredVisual();
	const bool bShowsFocus = ShowsFocusVisual();
	const bool bPointerHoverVisualEnabled =
		bUsePointerHoverVisual && IsPointerHoverAllowedByOwningList();
	const bool bSuppressPointerHover =
		!bPointerHoverVisualEnabled && !bUseHoveredStyle;
	if (!bUseHoveredStyle && !bSuppressPointerHover && !bReserveOutlineForFocus)
	{
		if (bInteractionButtonStyleApplied)
		{
			Button->SetStyle(BaseButtonStyle);
			bInteractionButtonStyleApplied = false;
		}
		return;
	}

	FButtonStyle DesiredStyle = BaseButtonStyle;
	if (bReserveOutlineForFocus && bShowsFocus)
	{
		// Press feedback changes fill, but must not erase the input location.
		DesiredStyle.Pressed.OutlineSettings.Width = BaseButtonStyle.Hovered.OutlineSettings.Width;
		DesiredStyle.Pressed.OutlineSettings.Color = BaseButtonStyle.Hovered.OutlineSettings.Color;
	}
	if (bReserveOutlineForFocus && !bShowsFocus)
	{
		// A rounded box blends its outline colour into the anti-aliased edge even at
		// width 0, so the hidden outline takes the fill colour instead of leaving a hairline.
		DesiredStyle.Hovered.OutlineSettings.Width = 0;
		DesiredStyle.Hovered.OutlineSettings.Color = DesiredStyle.Hovered.TintColor;
	}
	if (bUseHoveredStyle)
	{
		DesiredStyle.Normal = bShowsFocus ? BaseButtonStyle.Hovered : DesiredStyle.Hovered;
		DesiredStyle.NormalForeground =
			BaseButtonStyle.HoveredForeground;
	}
	else
	{
		// SButton independently chooses its Hovered brush from the current
		// FButtonStyle. Replacing only our logical hover flag is insufficient:
		// a stationary cursor can still light an entry while the carousel moves
		// underneath it. Flatten Hovered to Normal while pointer hover is locked.
		DesiredStyle.Hovered = BaseButtonStyle.Normal;
		DesiredStyle.HoveredForeground =
			BaseButtonStyle.NormalForeground;
	}

	if (!bInteractionButtonStyleApplied
		|| !FButtonStyle::StaticStruct()->CompareScriptStruct(
			&InteractionButtonStyle,
			&DesiredStyle,
			0))
	{
		InteractionButtonStyle = DesiredStyle;
		Button->SetStyle(InteractionButtonStyle);
		bInteractionButtonStyleApplied = true;
	}
}

void UReciFocusStyleButtonBase::ApplyCurrentTextStyleToDescendants()
{
	const UCommonButtonStyle* ButtonStyle = GetStyleCDO();
	if (!ButtonStyle || !WidgetTree)
	{
		return;
	}

	// A normal-state style is required before automatic text styling is
	// enabled. This guarantees that leaving hover or focus always has a
	// defined style to restore instead of leaving the hovered style behind.
	if (!ButtonStyle->GetNormalTextStyle())
	{
		return;
	}

	const TSubclassOf<UCommonTextStyle> CurrentTextStyle =
		ResolveInteractionTextStyleClass();
	if (!CurrentTextStyle)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(Widget))
		{
			TextBlock->SetStyle(CurrentTextStyle);
		}
	}
}

TSubclassOf<UCommonTextStyle>
UReciFocusStyleButtonBase::ResolveInteractionTextStyleClass() const
{
	const UCommonButtonStyle* ButtonStyle = GetStyleCDO();
	if (!ButtonStyle)
	{
		return nullptr;
	}

	UCommonTextStyle* TextStyle = nullptr;
	if (!GetIsEnabled())
	{
		TextStyle = ButtonStyle->GetDisabledTextStyle();
	}
	else if (GetSelected())
	{
		if (UsesHoveredVisual())
		{
			TextStyle = ButtonStyle->GetSelectedHoveredTextStyle();
		}

		if (!TextStyle)
		{
			TextStyle = ButtonStyle->GetSelectedTextStyle();
		}
	}

	if (!TextStyle)
	{
		if (UsesHoveredVisual())
		{
			TextStyle = ButtonStyle->GetNormalHoveredTextStyle();
		}

		if (!TextStyle)
		{
			TextStyle = ButtonStyle->GetNormalTextStyle();
		}
	}

	return TextStyle ? TextStyle->GetClass() : nullptr;
}

void UReciFocusStyleButtonBase::SetKeyCursorVisual(const bool bShow)
{
	if (bKeyCursorVisual == bShow)
	{
		return;
	}
	bKeyCursorVisual = bShow;
	ApplyInteractionStyle();
	NativeOnCurrentTextStyleChanged();
}

bool UReciFocusStyleButtonBase::ShowsFocusVisual() const
{
	return bStateFocused || bKeyCursorVisual;
}

bool UReciFocusStyleButtonBase::UsesHoveredVisual() const
{
	return bStateEnabled && (bStateHovered || bStateFocused || bKeyCursorVisual);
}

bool UReciFocusStyleButtonBase::IsCurrentInternalStyleProjected() const
{
	const UCommonButtonInternalBase* Button = InternalButton.Get();
	return Button
		&& bInteractionButtonStyleApplied
		&& FButtonStyle::StaticStruct()->CompareScriptStruct(
			&Button->GetStyle(),
			&InteractionButtonStyle,
			0);
}
