// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#include "Widgets/ReciTwoLevelListBinding.h"
#include "Widgets/ReciTwoLevelListBase.h"
#include "Widgets/ReciTwoLevelItemBase.h"
#include "Widgets/ReciTwoLevelEntry.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciAdvancedList.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciScrollBox.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "ReciReport.h"
#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

namespace UE::ReciCommonUIWidgets::Private
{
	namespace
	{
		FString DescribeBool(const bool bValue)
		{
			return bValue ? TEXT("on") : TEXT("off");
		}

		FString DescribeSelectionMode(const ESelectionMode::Type Mode)
		{
			switch (Mode)
			{
			case ESelectionMode::None: return TEXT("None");
			case ESelectionMode::Single: return TEXT("Single");
			case ESelectionMode::SingleToggle: return TEXT("Single Toggle");
			case ESelectionMode::Multi: return TEXT("Multi");
			default: return TEXT("?");
			}
		}
	}

	TArray<FReciDroppedListSetting> CollectDroppedListSettings(
		const UReciListBase& List,
		const bool bParentList,
		const TOptional<bool> OwnerAllowsHover)
	{
		TArray<FReciDroppedListSetting> Dropped;
		const UReciListBase& Defaults = *List.GetClass()->GetDefaultObject<UReciListBase>();
		auto Overwritten = [&Dropped, &List](const TCHAR* Property, const FString& Authored, const FString& Applied)
		{
			Dropped.Add({Property, FString::Printf(TEXT("the two-level list sets %s's %s to %s while it binds the list, over the %s set on it."),
				*List.GetName(), Property, *Applied, *Authored)});
		};
		auto Ignored = [&Dropped, &List](const TCHAR* Property, const FString& Authored)
		{
			Dropped.Add({Property, FString::Printf(TEXT("the two-level list ignores %s's %s (%s) while it binds the list."),
				*List.GetName(), Property, *Authored)});
		};

		const ESelectionMode::Type AppliedMode = bParentList ? ESelectionMode::None : ESelectionMode::Single;
		if (List.SelectionMode != Defaults.SelectionMode && List.SelectionMode != AppliedMode)
		{
			Overwritten(TEXT("Selection Mode"), DescribeSelectionMode(List.SelectionMode), DescribeSelectionMode(AppliedMode));
		}
		if (List.bClearSelectionOnFocusReturn != Defaults.bClearSelectionOnFocusReturn && List.bClearSelectionOnFocusReturn)
		{
			Overwritten(TEXT("Clear Selection On Focus Return"), DescribeBool(true), DescribeBool(false));
		}
		if (List.bAllowPointerHover != Defaults.bAllowPointerHover
			&& (!OwnerAllowsHover.IsSet() || List.bAllowPointerHover != OwnerAllowsHover.GetValue()))
		{
			Overwritten(TEXT("Allow Pointer Hover"), DescribeBool(List.bAllowPointerHover),
				OwnerAllowsHover.IsSet() ? DescribeBool(OwnerAllowsHover.GetValue()) : FString(TEXT("the two-level list's Allow Pointer Hover")));
		}
		if (const UReciAdvancedList* Advanced = Cast<UReciAdvancedList>(&List))
		{
			const UReciAdvancedList& AdvancedDefaults = *CastChecked<UReciAdvancedList>(&Defaults);
			if (Advanced->bMainPresentationFollowsFocus != AdvancedDefaults.bMainPresentationFollowsFocus && Advanced->bMainPresentationFollowsFocus)
			{
				Overwritten(TEXT("Main Presentation Follows Focus"), DescribeBool(true), DescribeBool(false));
			}
			if (Advanced->bConfirmMainItemOnClick != AdvancedDefaults.bConfirmMainItemOnClick && Advanced->bConfirmMainItemOnClick)
			{
				Overwritten(TEXT("Confirm Main Item On Click"), DescribeBool(true), DescribeBool(false));
			}
		}
		if (List.bConfirmOnItemClick != Defaults.bConfirmOnItemClick)
		{
			Ignored(TEXT("Confirm On Item Click"), DescribeBool(List.bConfirmOnItemClick));
		}
		if (List.bSelectionFollowsNavigation != Defaults.bSelectionFollowsNavigation)
		{
			Ignored(TEXT("Selection Follows Navigation"), DescribeBool(List.bSelectionFollowsNavigation));
		}
		if (const UReciList* Plain = Cast<UReciList>(&List); Plain && Plain->bLoopingEntries != CastChecked<UReciList>(&Defaults)->bLoopingEntries)
		{
			Ignored(TEXT("Looping Entries"), DescribeBool(Plain->bLoopingEntries));
		}
		return Dropped;
	}

#if WITH_EDITOR
	TOptional<EOrientation> ValidateTwoLevelPlacement(const UReciListBase& List, IWidgetCompilerLog& CompileLog)
	{
		const UReciListLayoutBoxBase* Layout = Cast<UReciListLayoutBoxBase>(List.GetParent());
		if (!Layout || (!Layout->IsA<UReciVerticalBox>() && !Layout->IsA<UReciHorizontalBox>()))
		{
			CompileLog.Error(FText::FromString(FString::Printf(TEXT("%s must be the content of a Reci Vertical Box or Reci "
				"Horizontal Box: the two-level list takes the list's axis and navigation from that box."), *List.GetName())));
			return {};
		}
		for (const UWidget* Ancestor = Layout->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
		{
			if (const UReciScrollBox* Scroll = Cast<UReciScrollBox>(Ancestor))
			{
				if (Scroll->GetHostedLayout() != Layout)
				{
					CompileLog.Error(FText::FromString(FString::Printf(TEXT("%s: the Reci Scroll Box %s around its layout box must "
						"hold that box as its only child, or the two-level list cannot use it as the list's viewport."),
						*List.GetName(), *Scroll->GetName())));
					return {};
				}
				break;
			}
		}
		return Layout->IsA<UReciVerticalBox>() ? Orient_Vertical : Orient_Horizontal;
	}

	void ValidateTwoLevelEntryClasses(const UReciListBase& List, const bool bParentList, IWidgetCompilerLog& CompileLog)
	{
		TArray<const UClass*, TInlineAllocator<2>> EntryClasses;
		EntryClasses.Add(List.ItemWidgetClass.Get());
		if (const UReciAdvancedList* Advanced = Cast<UReciAdvancedList>(&List))
		{
			EntryClasses.Add(Advanced->MainEntryWidgetClass.Get());
		}
		for (const UClass* EntryClass : EntryClasses)
		{
			const bool bKeepsContract = !EntryClass || (bParentList
				? EntryClass->IsChildOf(UReciTwoLevelItemBase::StaticClass())
				: EntryClass->ImplementsInterface(UReciTwoLevelEntry::StaticClass()));
			if (!bKeepsContract)
			{
				CompileLog.Error(FText::FromString(FString::Printf(TEXT("%s: %s must %s."), *List.GetName(), *EntryClass->GetName(),
					bParentList ? TEXT("derive from Reci Two Level Item Base") : TEXT("implement Reci Two Level Entry"))));
			}
		}
	}

	void ValidateTwoLevelRowTree(
		const UReciListBase& ParentList,
		const EOrientation ParentAxis,
		const UClass& RowClass,
		const UWidgetTree& RowTree,
		IWidgetCompilerLog& CompileLog)
	{
		// The row's own compile reports a wrong ChildList placement; only the axes meet here.
		const UReciListBase* ChildList = Cast<UReciListBase>(RowTree.FindWidget(GET_MEMBER_NAME_CHECKED(UReciTwoLevelItemBase, ChildList)));
		const UReciListLayoutBoxBase* ChildLayout = ChildList ? Cast<UReciListLayoutBoxBase>(ChildList->GetParent()) : nullptr;
		const bool bSameAxis = ChildLayout
			&& ((ParentAxis == Orient_Vertical && ChildLayout->IsA<UReciVerticalBox>())
				|| (ParentAxis == Orient_Horizontal && ChildLayout->IsA<UReciHorizontalBox>()));
		if (bSameAxis)
		{
			CompileLog.Error(FText::FromString(FString::Printf(TEXT("%s and the ChildList of %s have the same direction. Put them "
				"in perpendicular Reci layout boxes."), *ParentList.GetName(), *RowClass.GetName())));
		}
	}

	void ValidateDroppedListSettings(
		const UReciListBase& List,
		const bool bParentList,
		const TOptional<bool> OwnerAllowsHover,
		IWidgetCompilerLog& CompileLog)
	{
		for (const FReciDroppedListSetting& Setting : CollectDroppedListSettings(List, bParentList, OwnerAllowsHover))
		{
			CompileLog.Warning(FText::FromString(Setting.Message));
		}
	}
#endif
}

bool UReciTwoLevelListBinding::Initialize(UReciTwoLevelListBase* InOwner, UReciListBase* InList, int32 InParentIndex)
{
	Owner = InOwner;
	List = InList;
	ParentIndex = InParentIndex;
	if (!List) return false;
	List->TakeWidget();
	// A Widget Blueprint's compile checks this placement; native trees and classes set at run time are checked here,
	// once per owner class and list.
	const FString ListKey = GetPathNameSafe(InOwner ? InOwner->GetClass() : nullptr) + TEXT(" ")
		+ UE::ReciCommonUI::GetAuthoredPath(*List) + TEXT(" ") + List->GetName();
	Layout = Cast<UReciListLayoutBoxBase>(List->GetParent());
	if (!Layout || (!Layout->IsA<UReciVerticalBox>() && !Layout->IsA<UReciHorizontalBox>()))
	{
		RECI_ERROR_ONCE(TEXT("TwoLevelPlacement"), ListKey,
			TEXT("%s: %s must be directly inside a ReciVerticalBox or ReciHorizontalBox. Two-level navigation disabled."),
			*GetNameSafe(InOwner), *GetNameSafe(List));
		return false;
	}
	bLayoutInputWasEnabled = Layout->IsInputRoutingEnabled();
	Layout->SetInputRoutingEnabled(false);
	for (UWidget* Ancestor = Layout->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
	{
		if (auto* Scroll = Cast<UReciScrollBox>(Ancestor))
		{
			if (Scroll->GetHostedLayout() == Layout) Viewport = Scroll;
			break;
		}
	}
	if (Viewport)
	{
		bViewportInputWasEnabled = Viewport->IsInputRoutingEnabled();
		Viewport->SetInputRoutingEnabled(false);
	}
	// What the binding overwrites or ignores below, among what the author changed, before it is overwritten.
	const TOptional<bool> OwnerAllowsHover = InOwner ? TOptional<bool>(InOwner->bAllowPointerHover) : TOptional<bool>();
	for (const UE::ReciCommonUIWidgets::Private::FReciDroppedListSetting& Setting :
		UE::ReciCommonUIWidgets::Private::CollectDroppedListSettings(*List, !IsSelectionOnly(), OwnerAllowsHover))
	{
		RECI_WARN_ONCE(TEXT("TwoLevelDroppedSetting"), ListKey + TEXT(" ") + Setting.Property,
			TEXT("%s: %s"), *GetNameSafe(InOwner ? InOwner->GetClass() : nullptr), *Setting.Message);
	}
	List->SetListController(this, this);
	bControllerAttached = true;
	bSavedClearSelection = List->bClearSelectionOnFocusReturn;
	bSavedHover = List->IsPointerHoverAllowed();
	SavedSelectionMode = static_cast<uint8>(List->SelectionMode.GetValue());
	List->bClearSelectionOnFocusReturn = false;
	List->SetSelectionMode(IsSelectionOnly() ? ESelectionMode::Single : ESelectionMode::None);
	if (auto* Advanced = Cast<UReciAdvancedList>(List))
	{
		bSavedFollowFocus = Advanced->bMainPresentationFollowsFocus;
		bSavedConfirmClick = Advanced->bConfirmMainItemOnClick;
		Advanced->bMainPresentationFollowsFocus = false;
		Advanced->bConfirmMainItemOnClick = false;
		if (Advanced->MainEntryWidgetClass && !SupportsEntryClass(Advanced->MainEntryWidgetClass))
		{
			RECI_ERROR_ONCE(TEXT("TwoLevelEntryClass"), ListKey + TEXT(" Main"),
				TEXT("%s: invalid Main Entry Widget Class on %s."), *GetNameSafe(InOwner), *GetNameSafe(List));
			return false;
		}
	}
	if (!SupportsEntryClass(List->ItemWidgetClass))
	{
		RECI_ERROR_ONCE(TEXT("TwoLevelEntryClass"), ListKey,
			TEXT("%s: %s requires %s as its entry contract."), *GetNameSafe(InOwner), *GetNameSafe(List),
			IsSelectionOnly() ? TEXT("ReciTwoLevelEntry (Set Child Item)") : TEXT("ReciTwoLevelItemBase"));
		return false;
	}
	List->OnFocusedIndexChangedNative().AddUObject(this, &ThisClass::FocusChanged);
	Layout->SynchronizeProperties();
	if (Viewport) Viewport->SynchronizeProperties();
	return true;
}

void UReciTwoLevelListBinding::Detach()
{
	if (List)
	{
		List->OnFocusedIndexChangedNative().RemoveAll(this);
		if (bControllerAttached && List->GetListController() == this)
		{
			List->SetListController(nullptr, nullptr);
			List->bClearSelectionOnFocusReturn = bSavedClearSelection;
			List->SetAllowPointerHover(bSavedHover);
			List->SetSelectionMode(static_cast<ESelectionMode::Type>(SavedSelectionMode));
			if (auto* Advanced = Cast<UReciAdvancedList>(List))
			{
				Advanced->bMainPresentationFollowsFocus = bSavedFollowFocus;
				Advanced->bConfirmMainItemOnClick = bSavedConfirmClick;
			}
		}
	}
	if (Layout) Layout->SetInputRoutingEnabled(bLayoutInputWasEnabled);
	if (Viewport) { Viewport->CancelExternalDrag(); Viewport->SetInputRoutingEnabled(bViewportInputWasEnabled); }
	Owner.Reset();
	AppliedHover.Reset();
}

void UReciTwoLevelListBinding::ApplyPointerHover(const bool bAllowPointerHover)
{
	if (!List)
	{
		return;
	}
	if (AppliedHover.IsSet() && List->IsPointerHoverAllowed() != AppliedHover.GetValue())
	{
		RECI_WARN_ONCE(TEXT("TwoLevelHoverChanged"),
			GetPathNameSafe(Owner.IsValid() ? Owner->GetClass() : nullptr) + TEXT(" ") + List->GetName(),
			TEXT("%s: %s's Allow Pointer Hover was changed while the two-level list binds it; the two-level list's Allow "
				"Pointer Hover applies."), *GetNameSafe(Owner.Get()), *List->GetName());
	}
	List->SetAllowPointerHover(bAllowPointerHover);
	AppliedHover = bAllowPointerHover;
}

bool UReciTwoLevelListBinding::SupportsEntryClass(const UClass* Class) const
{
	return Class && (IsSelectionOnly() ? Class->ImplementsInterface(UReciTwoLevelEntry::StaticClass())
		: Class->IsChildOf(UReciTwoLevelItemBase::StaticClass()));
}

void UReciTwoLevelListBinding::InitializeEntry(UUserWidget* Entry, UObject* Item, int32 Index)
{
	if (IsSelectionOnly())
	{
		IReciTwoLevelEntry::Execute_SetChildItem(Entry, Item, Index);
		if (auto* Button = UReciListBase::FindEntryButton(Entry)) Button->SetIsFocusable(false);
	}
	else if (auto* Row = Cast<UReciTwoLevelItemBase>(Entry))
	{
		Row->ParentItem = Item;
		IReciListItem::Execute_SetListItemObject(Row, Item);
	}
}

void UReciTwoLevelListBinding::ClickItem(int32 Index)
{
	if (Owner.IsValid()) Owner->HandleSourceChanged(this, ParentIndex, Index, true);
}

void UReciTwoLevelListBinding::DrivenItemChanged(int32 Index, bool bGiveFocus)
{
	if (Owner.IsValid()) Owner->HandleSourceChanged(this, ParentIndex, Index, bGiveFocus);
}

void UReciTwoLevelListBinding::FocusChanged(int32 Index)
{
	if (ParentIndex == INDEX_NONE && Index != INDEX_NONE && Owner.IsValid())
		Owner->HandleSourceChanged(this, ParentIndex, Index, false);
}

bool UReciTwoLevelListBinding::ConfirmItem()
{
	return Owner.IsValid() && Owner->ConfirmSelection();
}

void UReciTwoLevelListBinding::UpdateDesignState(int32 FocusedParent, int32 SelectedChild)
{
	if (!List || !Owner.IsValid()) return;
	const uint32 Revision = Owner->StateRevision;
	const int32 PreviousFocused = LastFocused;
	const int32 PreviousSelected = LastSelected;
	LastFocused = FocusedParent;
	LastSelected = SelectedChild;
	for (int32 Index = 0; Index < List->GetHostedItemCount(); ++Index)
	{
		const int32 Row = IsSelectionOnly() ? ParentIndex : Index;
		if (PreviousFocused != -2 && PreviousSelected != -2
			&& (PreviousFocused == Row) == (FocusedParent == Row)
			&& (!IsSelectionOnly() || (PreviousSelected == Index) == (SelectedChild == Index))) continue;
		const auto Widgets = List->GetEntryPresentations(Index);
		for (UUserWidget* Widget : Widgets)
		{
			if (Widget && Widget->Implements<UReciTwoLevelEntry>())
				IReciTwoLevelEntry::Execute_UpdateTwoLevelState(Widget, Row == FocusedParent,
					IsSelectionOnly() && Index == SelectedChild, Row, IsSelectionOnly() ? Index : INDEX_NONE);
			if (!Owner.IsValid() || Owner->StateRevision != Revision) return;
		}
	}
}
