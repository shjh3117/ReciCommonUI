// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciDropdownBase.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/MenuAnchor.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "ReciReport.h"
#include "UObject/Class.h"
#include "Widgets/ReciDropdownPopup.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciTextListItem.h"
#include "Widgets/ReciWidgetTreeUtils.h"
#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciDropdownBase)

namespace
{
	bool IsSelectableEnumIndex(const UEnum& Enum, const int32 Index)
	{
		const int64 EnumValue = Enum.GetValueByIndex(Index);
#if WITH_EDITOR
		if (Enum.HasMetaData(TEXT("Hidden"), Index))
		{
			return false;
		}
#endif
		return EnumValue != Enum.GetMaxEnumValue();
	}

	const TCHAR* DescribeClickMethod(const EButtonClickMethod::Type Method)
	{
		switch (Method)
		{
		case EButtonClickMethod::DownAndUp:
			return TEXT("Down And Up");
		case EButtonClickMethod::MouseDown:
			return TEXT("Mouse Down");
		case EButtonClickMethod::MouseUp:
			return TEXT("Mouse Up");
		default:
			return TEXT("Precise Click");
		}
	}
}

UReciDropdownBase::UReciDropdownBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The button is the popup's toggle. Match Slate's SComboButton: with DownAndUp, pressing an open anchor dismisses it
	// on mouse-down and the later mouse-up immediately opens it again. MouseDown keeps dismissal and toggle handling in
	// the same tick.
	ClickMethod = EButtonClickMethod::MouseDown;
	bSelectable = true;
	bToggleable = true;
	bInteractableWhenSelected = true;
}

TArray<FString> UReciDropdownBase::DescribeChangedToggleSettings() const
{
	TArray<FString> Changed;
	if (ClickMethod != EButtonClickMethod::MouseDown)
	{
		Changed.Add(FString::Printf(TEXT("Click Method (%s)"), DescribeClickMethod(ClickMethod)));
	}
	if (!bSelectable)
	{
		Changed.Add(TEXT("Is Selectable (off)"));
	}
	if (!bToggleable)
	{
		Changed.Add(TEXT("Is Toggleable (off)"));
	}
	if (!bInteractableWhenSelected)
	{
		Changed.Add(TEXT("Interactable When Selected (off)"));
	}
	return Changed;
}

void UReciDropdownBase::NativeConstruct()
{
	Super::NativeConstruct();

	// The popup's toggle needs its defaults (see the class comment): a value the Blueprint changed is set back, and said.
	const TArray<FString> Changed = DescribeChangedToggleSettings();
	if (!Changed.IsEmpty())
	{
		RECI_WARN_ONCE(TEXT("DropdownToggleSettings"), UE::ReciCommonUI::Private::GetAuthoredPath(*this),
			TEXT("%s: a dropdown's button is its popup's toggle, which needs Click Method Mouse Down, Is Selectable, Is "
				"Toggleable and Interactable When Selected, so it sets back %s."),
			*GetName(), *FString::Join(Changed, TEXT(", ")));
	}
	SetClickMethod(EButtonClickMethod::MouseDown);
	SetIsSelectable(true);
	SetIsToggleable(true);
	SetIsInteractableWhenSelected(true);

	if (!DropdownAnchor)
	{
		// Only a native subclass gets here: a Blueprint binds DropdownAnchor to compile.
		RECI_ERROR_ONCE(TEXT("DropdownAnchor"), GetClass()->GetPathName(),
			TEXT("%s requires a Menu Anchor named DropdownAnchor."), *GetClass()->GetName());
		return;
	}

	DropdownAnchor->OnGetUserMenuContentEvent.BindDynamic(
		this,
		&UReciDropdownBase::CreateDropdownContent);
	DropdownAnchor->OnMenuOpenChanged.AddDynamic(
		this,
		&UReciDropdownBase::HandleDropdownOpenChanged);
	UpdateSelectedValueText();
}

void UReciDropdownBase::NativeDestruct()
{
	FReciInputFocusCoordinator::Get().UnregisterOpenPopup(this);
	if (DropdownAnchor)
	{
		DropdownAnchor->OnMenuOpenChanged.RemoveDynamic(
			this,
			&UReciDropdownBase::HandleDropdownOpenChanged);
		DropdownAnchor->OnGetUserMenuContentEvent.Unbind();
		DropdownAnchor->Close();
	}

	ActiveDropdownList = nullptr;
	ActiveDropdownItems.Reset();
	Super::NativeDestruct();
}

void UReciDropdownBase::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (OptionDisplayNames.IsEmpty())
	{
		SelectedIndex = INDEX_NONE;
	}
	else if (!OptionDisplayNames.IsValidIndex(SelectedIndex))
	{
		SelectedIndex = 0;
	}

	UpdateSelectedValueText();
}

void UReciDropdownBase::NativeOnSelected(
	const bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	// The click that ends a press begun on the open popup: the press closed the popup, so the click does not open it.
	if (bPressBeganOnOpenPopup)
	{
		bPressBeganOnOpenPopup = false;
		ClearSelection();
		return;
	}

	OpenDropdown();
	if (!IsDropdownOpen())
	{
		ClearSelection();
	}
}

void UReciDropdownBase::NativeOnDeselected(
	const bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	if (IsDropdownOpen())
	{
		// The button closes the popup itself (a click on it, or Close Dropdown): nothing is left for a click to undo.
		bPressBeganOnOpenPopup = false;
		DropdownAnchor->Close();
	}
}

void UReciDropdownBase::NativeOnPressed()
{
	Super::NativeOnPressed();
	bPressBeganOnOpenPopup = IsDropdownOpen();
}

void UReciDropdownBase::NativeOnReleased()
{
	Super::NativeOnReleased();
	// The button's click on release comes right after this; a release without one leaves nothing for a later click.
	UWorld* World = bPressBeganOnOpenPopup ? GetWorld() : nullptr;
	if (World)
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bPressBeganOnOpenPopup = false;
		}));
	}
}

void UReciDropdownBase::SetDropdownEnum(
	UEnum* InEnum,
	const FName CurrentEnumValue)
{
	OptionEnum = InEnum;
	OptionEnumValueNames.Reset();
	OptionDisplayNames.Reset();
	SelectedIndex = INDEX_NONE;

	if (InEnum)
	{
		for (int32 Index = 0; Index < InEnum->NumEnums(); ++Index)
		{
			if (!IsSelectableEnumIndex(*InEnum, Index))
			{
				continue;
			}

			const FName EnumValueName = InEnum->GetNameByIndex(Index);
			OptionEnumValueNames.Add(EnumValueName);
			OptionDisplayNames.Add(InEnum->GetDisplayNameTextByIndex(Index));
			if (EnumValueName == CurrentEnumValue)
			{
				SelectedIndex = OptionEnumValueNames.Num() - 1;
			}
		}
	}

	if (SelectedIndex == INDEX_NONE && !OptionDisplayNames.IsEmpty())
	{
		SelectedIndex = 0;
	}

	UpdateSelectedValueText();
	RefreshOpenDropdown();
}

void UReciDropdownBase::SetDropdownOptions(
	const TArray<FText>& Options,
	const int32 InSelectedIndex)
{
	OptionEnum = nullptr;
	OptionEnumValueNames.Reset();
	OptionDisplayNames = Options;
	SelectedIndex = OptionDisplayNames.IsEmpty()
		? INDEX_NONE
		: FMath::Clamp(InSelectedIndex, 0, OptionDisplayNames.Num() - 1);

	UpdateSelectedValueText();
	RefreshOpenDropdown();
}

bool UReciDropdownBase::SetSelectedIndex(
	const int32 InSelectedIndex)
{
	return ApplySelectedIndex(InSelectedIndex, true);
}

FName UReciDropdownBase::GetSelectedEnumValue() const
{
	return OptionEnum
		&& OptionEnumValueNames.IsValidIndex(SelectedIndex)
		? OptionEnumValueNames[SelectedIndex]
		: NAME_None;
}

void UReciDropdownBase::OpenDropdown()
{
	if (!GetSelected())
	{
		SetIsSelected(true, false);
		return;
	}

	if (!DropdownAnchor
		|| DropdownAnchor->IsOpen()
		|| !DropdownAnchor->ShouldOpenDueToClick())
	{
		return;
	}

	if (OptionDisplayNames.IsEmpty())
	{
		RECI_WARN_ONCE(TEXT("DropdownWithoutOptions"), GetClass()->GetPathName(),
			TEXT("%s cannot open because it has no options."), *GetName());
		return;
	}

	if (!DropdownPopupClass)
	{
		RECI_WARN_ONCE(TEXT("DropdownPopupClass"), GetClass()->GetPathName(),
			TEXT("%s cannot open because Dropdown Popup Class is unset."), *GetName());
		return;
	}

	// The opener may be a focusable row hosting a non-focusable dropdown; closing returns focus there.
	FocusReturnWidget = FSlateApplication::Get().GetUserFocusedWidget(GetOwningUserIndex());
	DropdownAnchor->Open(true);
}

void UReciDropdownBase::CloseDropdown()
{
	if (ActiveDropdownList)
	{
		ActiveDropdownList->ClearSelection();
	}

	if (GetSelected())
	{
		ClearSelection();
	}
	else if (IsDropdownOpen())
	{
		DropdownAnchor->Close();
	}
}

void UReciDropdownBase::ToggleDropdown()
{
	if (IsDropdownOpen())
	{
		CloseDropdown();
	}
	else
	{
		SetIsSelected(true, false);
	}
}

bool UReciDropdownBase::IsDropdownOpen() const
{
	return DropdownAnchor && DropdownAnchor->IsOpen();
}

#if WITH_EDITOR
void UReciDropdownBase::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);
	// A dropdown Blueprint's own defaults may leave the class to the places it is used.
	if (!HasAnyFlags(RF_ClassDefaultObject) && !DropdownPopupClass)
	{
		CompileLog.Warning(FText::FromString(FString::Printf(
			TEXT("%s has no Dropdown Popup Class, so it cannot open."), *GetName())));
	}
	const TArray<FString> Changed = DescribeChangedToggleSettings();
	if (!Changed.IsEmpty())
	{
		CompileLog.Warning(FText::FromString(FString::Printf(
			TEXT("%s: a dropdown's button is its popup's toggle, which needs Click Method Mouse Down, Is Selectable, Is "
				"Toggleable and Interactable When Selected, so it sets back %s when it is constructed."),
			HasAnyFlags(RF_ClassDefaultObject) ? *GetClass()->GetName() : *GetName(), *FString::Join(Changed, TEXT(", ")))));
	}
}
#endif

UUserWidget* UReciDropdownBase::CreateDropdownContent()
{
	if (!DropdownPopupClass)
	{
		RECI_WARN_ONCE(TEXT("DropdownPopupClass"), GetClass()->GetPathName(),
			TEXT("%s cannot open because Dropdown Popup Class is unset."), *GetName());
		return nullptr;
	}

	UReciDropdownPopup* Popup = CreateWidget<UReciDropdownPopup>(this, DropdownPopupClass, NAME_None);
	UReciList* DropdownList = Popup ? Popup->Options.Get() : nullptr;
	if (!DropdownList)
	{
		// A Blueprint binds Options to compile; only a native popup class can leave it unset.
		RECI_WARN_ONCE(TEXT("DropdownOptions"), DropdownPopupClass->GetPathName(),
			TEXT("%s cannot open: its popup %s has no Options list."), *GetName(), *DropdownPopupClass->GetName());
		return nullptr;
	}

	if (DropdownEntryWidgetClass)
	{
		DropdownList->SetEntryWidgetClass(
			DropdownEntryWidgetClass);
	}

	DropdownList->SetSelectionMode(ESelectionMode::Single);
	PopulateDropdownList(*DropdownList);
	// The current value shows as selected and holds the cursor; choosing is a click or an Accept.
	DropdownList->SetSelectedItemIndex(SelectedIndex);
	DropdownList->SetInitialFocusItemIndex(SelectedIndex);
	DropdownList->OnItemClicked.AddDynamic(
		this,
		&UReciDropdownBase::HandleListItemChosen);
	DropdownList->OnItemConfirmed.AddDynamic(
		this,
		&UReciDropdownBase::HandleListItemChosen);
	ActiveDropdownList = DropdownList;
	return Popup;
}

void UReciDropdownBase::HandleDropdownOpenChanged(
	const bool bIsOpen)
{
	if (bIsOpen)
	{
		TWeakObjectPtr<UReciDropdownBase> WeakThis(this);
		FReciInputFocusCoordinator::Get().RegisterOpenPopup(
			this,
			GetOwningUserIndex(),
			[WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->CloseDropdown();
				}
			});
		// The menu focuses its content root after this notification; move focus onto the current option (which
		// also scrolls it into view) once the popup is in place.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (ActiveDropdownList && IsDropdownOpen())
				{
					ActiveDropdownList->SetDrivenItemIndex(SelectedIndex, true);
				}
			}));
		}
		OnDropdownOpenChanged.Broadcast(true);
		return;
	}

	FReciInputFocusCoordinator::Get().UnregisterOpenPopup(this);
	const bool bDropdownListHadFocus =
		ActiveDropdownList
		&& (ActiveDropdownList->HasAnyUserFocus()
			|| ActiveDropdownList->HasFocusedDescendants());

	ActiveDropdownList = nullptr;
	ActiveDropdownItems.Reset();
	if (GetSelected())
	{
		ClearSelection();
	}

	if (bDropdownListHadFocus)
	{
		if (const TSharedPtr<SWidget> ReturnWidget = FocusReturnWidget.Pin())
		{
			FSlateApplication::Get().SetUserFocus(
				GetOwningUserIndex(),
				ReturnWidget,
				EFocusCause::SetDirectly);
		}
		else if (GetIsEnabled()
			&& IsInteractionEnabled()
			&& GetIsFocusable())
		{
			SetFocus();
		}
	}
	FocusReturnWidget.Reset();

	OnDropdownOpenChanged.Broadcast(false);
}

int32 UReciDropdownBase::GetOwningUserIndex() const
{
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	return Player != nullptr
		? FSlateApplication::Get().GetUserIndexForController(Player->GetControllerId())
		: 0;
}

void UReciDropdownBase::HandleListItemChosen(
	UObject* Item,
	const int32 ItemIndex)
{
	(void)ItemIndex;
	const UReciTextListItemData* TextItem =
		Cast<UReciTextListItemData>(Item);
	if (!TextItem)
	{
		return;
	}

	const int32 OptionIndex =
		ActiveDropdownItems.IndexOfByPredicate(
			[TextItem](
				const TObjectPtr<UReciTextListItemData>&
					Candidate)
			{
				return Candidate == TextItem;
			});
	if (OptionIndex == INDEX_NONE)
	{
		return;
	}

	const bool bSelectionChanged =
		ApplySelectedIndex(OptionIndex, false);
	CloseDropdown();

	if (bSelectionChanged)
	{
		BroadcastSelectionChanged();
	}
}

bool UReciDropdownBase::ApplySelectedIndex(
	const int32 InSelectedIndex,
	const bool bBroadcast)
{
	if (!OptionDisplayNames.IsValidIndex(InSelectedIndex))
	{
		return false;
	}

	if (SelectedIndex == InSelectedIndex)
	{
		return false;
	}

	SelectedIndex = InSelectedIndex;
	UpdateSelectedValueText();
	if (bBroadcast)
	{
		BroadcastSelectionChanged();
	}
	return true;
}

void UReciDropdownBase::BroadcastSelectionChanged()
{
	OnSelectionChanged.Broadcast(
		OptionEnum,
		GetSelectedEnumValue(),
		SelectedIndex);
}

FText UReciDropdownBase::GetSelectedDisplayName() const
{
	return OptionDisplayNames.IsValidIndex(SelectedIndex)
		? OptionDisplayNames[SelectedIndex]
		: FText::GetEmpty();
}

void UReciDropdownBase::RefreshOpenDropdown()
{
	if (ActiveDropdownList)
	{
		PopulateDropdownList(*ActiveDropdownList);
	}
}

void UReciDropdownBase::PopulateDropdownList(
	UReciList& DropdownList)
{
	ActiveDropdownItems.Reset();
	ActiveDropdownItems.Reserve(OptionDisplayNames.Num());

	TArray<UObject*> ItemObjects;
	ItemObjects.Reserve(OptionDisplayNames.Num());
	for (const FText& OptionDisplayName : OptionDisplayNames)
	{
		UReciTextListItemData* Item =
			NewObject<UReciTextListItemData>(this);
		Item->ItemText = OptionDisplayName;
		ActiveDropdownItems.Add(Item);
		ItemObjects.Add(Item);
	}

	DropdownList.SetListItems(ItemObjects);
}

void UReciDropdownBase::UpdateSelectedValueText()
{
	if (SelectedValueText)
	{
		SelectedValueText->SetText(GetSelectedDisplayName());
	}
}

