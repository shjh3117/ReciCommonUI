// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciAdvancedList.h"

#include "CommonButtonBase.h"
#include "GameFramework/PlayerController.h"
#include "Components/PanelWidget.h"
#include "Widgets/ReciAdvancedListEntryHost.h"
#include "Widgets/ReciListItem.h"
#include "Widgets/ReciListController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciAdvancedList)

namespace
{
	int32 FindLogicalItemIndex(
		const TArray<UObject*>& Items,
		const UObject* ItemToFind)
	{
		if (!ItemToFind)
		{
			return INDEX_NONE;
		}

		int32 LogicalIndex = 0;
		for (const UObject* Item : Items)
		{
			if (!Item)
			{
				continue;
			}
			if (Item == ItemToFind)
			{
				return LogicalIndex;
			}
			++LogicalIndex;
		}
		return INDEX_NONE;
	}
}

#if WITH_EDITOR
void UReciAdvancedList::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);
	ValidateEntryWidgetClass(MainEntryWidgetClass, CompileLog);
}
#endif

void UReciAdvancedList::SetListItems(
	const TArray<UObject*>& InListItems)
{
	ReportEntryWidgetClass(MainEntryWidgetClass);
	PendingPreviousMainItemIndex = GetMainItemIndex();
	PendingPreviousMainItem = GetItemAt(PendingPreviousMainItemIndex);
	const int32 PreservedItemIndex = FindLogicalItemIndex(
		InListItems,
		PendingPreviousMainItem);
	PendingRebuildMainItemIndex = PreservedItemIndex != INDEX_NONE
		? PreservedItemIndex
		: (PendingPreviousMainItemIndex != INDEX_NONE
			? PendingPreviousMainItemIndex
			: InitialMainItemIndex);
	Super::SetListItems(InListItems);
	PendingRebuildMainItemIndex = INDEX_NONE;
	PendingPreviousMainItemIndex = INDEX_NONE;
	PendingPreviousMainItem = nullptr;
}

void UReciAdvancedList::SetEntryWidgetClass(
	const TSubclassOf<UUserWidget> InEntryWidgetClass)
{
	Super::SetEntryWidgetClass(InEntryWidgetClass);
}

bool UReciAdvancedList::SetDrivenItemIndex(
	const int32 ItemIndex,
	const bool bGiveFocus)
{
	const bool Result = SetMainItemIndex(ItemIndex, bGiveFocus);
	if (Result)
		if (auto* Controller = GetListController()) Controller->DrivenItemChanged(ItemIndex, bGiveFocus);
	return Result;
}

int32 UReciAdvancedList::GetDrivenItemIndex() const
{
	return GetMainItemIndex();
}

bool UReciAdvancedList::SetItemEmphasis(
	const int32 ItemIndex,
	const float Emphasis)
{
	const bool Result = SetItemMainPresentationBlend(ItemIndex, Emphasis);
	Super::SetItemEmphasis(ItemIndex, Emphasis);
	return Result;
}

void UReciAdvancedList::SetItemEmphasisDriven(
	const bool bDriven)
{
	if (bExternalPresentationDriverActive == bDriven)
	{
		return;
	}

	bExternalPresentationDriverActive = bDriven;
	if (!bExternalPresentationDriverActive)
	{
		ApplyBinaryMainPresentation(GetMainItemIndex());
	}
}

void UReciAdvancedList::SetMainEntryWidgetClass(
	const TSubclassOf<UUserWidget> InMainEntryWidgetClass)
{
	if (MainEntryWidgetClass == InMainEntryWidgetClass)
	{
		return;
	}
	if (InMainEntryWidgetClass && !SupportsEntryWidgetClass(InMainEntryWidgetClass))
	{
		return;
	}

	TArray<UObject*> ExistingItems;
	CopyAdvancedListItems(ExistingItems);
	MainEntryWidgetClass = InMainEntryWidgetClass;
	SetListItems(ExistingItems);
}

bool UReciAdvancedList::SetMainItemIndex(
	const int32 ItemIndex,
	const bool bGiveFocus)
{
	return SetMainItemIndexInternal(
		ItemIndex,
		bGiveFocus,
		!bExternalPresentationDriverActive);
}

int32 UReciAdvancedList::GetMainItemIndex() const
{
	return GetItemAt(MainItemIndex) ? MainItemIndex : INDEX_NONE;
}

bool UReciAdvancedList::SetItemMainPresentationBlend(
	const int32 ItemIndex,
	const float MainBlend)
{
	UReciAdvancedListEntryHost* EntryHost =
		GetAdvancedEntryHost(ItemIndex);
	if (!EntryHost)
	{
		return false;
	}

	FReciAdvancedPresentationChange Change;
	TGuardValue<int32> PresentationGuard(
		PresentationChangeDepth,
		PresentationChangeDepth + 1);
	if (!EntryHost->ApplyPresentation(MainBlend, Change))
	{
		return true;
	}

	ReplaceEntrySelectionButton(ItemIndex, Change.NewButton);
	if (Change.bPreviousButtonHadFocus && Change.NewButton)
	{
		// Detaching a viewport restores the binary presentation while Slate is
		// being released. The local controller may already have lost its player;
		// that presentation change must not issue another focus request.
		if (APlayerController* PlayerController = GetOwningPlayer();
			IsValid(PlayerController)
			&& PlayerController->IsLocalPlayerController()
			&& PlayerController->GetLocalPlayer())
		{
			Change.NewButton->SetUserFocus(PlayerController);
		}
	}
	return true;
}

void UReciAdvancedList::NativeOnLoopEntryReassigned(
	const int32 ItemIndex,
	UWidget* PreviousEntry,
	UWidget* NewEntry)
{
	Super::NativeOnLoopEntryReassigned(ItemIndex, PreviousEntry, NewEntry);

	// Carousel blends are only re-sent when they change, so the copy that becomes active after a loop rebase
	// takes over the old copy's presentation (and its focus) here; otherwise the main presentation stays on
	// the copy that just scrolled out of view.
	UReciAdvancedListEntryHost* PreviousHost = Cast<UReciAdvancedListEntryHost>(PreviousEntry);
	if (!PreviousHost || PreviousEntry == NewEntry)
	{
		return;
	}

	const float MainBlend = PreviousHost->GetMainBlend();
	const UCommonButtonBase* PreviousButton = PreviousHost->GetActiveButton();
	const bool bPreviousHadFocus = PreviousButton && PreviousButton->HasAnyUserFocus();
	{
		FReciAdvancedPresentationChange Unused;
		TGuardValue<int32> PresentationGuard(PresentationChangeDepth, PresentationChangeDepth + 1);
		PreviousHost->ApplyPresentation(0.0f, Unused);
	}
	SetItemMainPresentationBlend(ItemIndex, MainBlend);

	UReciAdvancedListEntryHost* NewHost = GetAdvancedEntryHost(ItemIndex);
	UCommonButtonBase* NewButton = NewHost ? NewHost->GetActiveButton() : nullptr;
	if (bPreviousHadFocus && NewButton && !NewButton->HasAnyUserFocus())
	{
		if (APlayerController* PlayerController = GetOwningPlayer();
			IsValid(PlayerController)
			&& PlayerController->IsLocalPlayerController()
			&& PlayerController->GetLocalPlayer())
		{
			NewButton->SetUserFocus(PlayerController);
		}
	}
}

void UReciAdvancedList::RefreshAdvancedListPresentationSizes()
{
	ForceLayoutPrepass();
	for (int32 ItemIndex = 0; ItemIndex < GetItemCount(); ++ItemIndex)
	{
		if (UReciAdvancedListEntryHost* EntryHost =
			GetAdvancedEntryHost(ItemIndex))
		{
			EntryHost->RefreshPresentationSizes();
		}
	}
	if (UPanelWidget* ItemsPanel = GetGeneratedItemsPanel())
	{
		ItemsPanel->InvalidateLayoutAndVolatility();
	}
}

void UReciAdvancedList::SetInitialMainItemIndex(
	const int32 InInitialMainItemIndex)
{
	InitialMainItemIndex = FMath::Max(InInitialMainItemIndex, 0);
	if (GetItemCount() > 0)
	{
		SetMainItemIndex(
			FMath::Clamp(InitialMainItemIndex, 0, GetItemCount() - 1),
			false);
	}
}

void UReciAdvancedList::NativeConstruct()
{
	Super::NativeConstruct();
	SetInitialFocusItemIndex(GetMainItemIndex());
}

UWidget* UReciAdvancedList::NativeCreateEntryWidget(
	UObject* ItemObject,
	const int32 ItemIndex)
{
	if (!ItemObject || !ItemWidgetClass)
	{
		return nullptr;
	}

	UReciAdvancedListEntryHost* EntryHost =
		NewObject<UReciAdvancedListEntryHost>(this);
	if (!EntryHost
		|| !EntryHost->Initialize(
			*this,
			ItemObject,
			ItemWidgetClass,
			MainEntryWidgetClass,
			GetListController() ? TFunction<void(UUserWidget*)>([this, ItemObject, ItemIndex](UUserWidget* Entry)
			{
				if (auto* Controller = GetListController()) Controller->InitializeEntry(Entry, ItemObject, ItemIndex);
			}) : TFunction<void(UUserWidget*)>()))
	{
		return nullptr;
	}

	FReciAdvancedPresentationChange InitialChange;
	EntryHost->ApplyPresentation(
		!bExternalPresentationDriverActive
			&& ItemIndex == PendingRebuildMainItemIndex
			? 1.0f
			: 0.0f,
		InitialChange);
	return EntryHost;
}

TArray<UUserWidget*> UReciAdvancedList::GetEntryPresentations(int32 Index) const
{
	TArray<UUserWidget*> Result;
	if (auto* Host = GetAdvancedEntryHost(Index))
	{
		if (Host->GetNormalWidget()) Result.Add(Host->GetNormalWidget());
		if (Host->GetMainWidget()) Result.Add(Host->GetMainWidget());
	}
	return Result;
}

UCommonButtonBase* UReciAdvancedList::ResolveEntrySelectionButton(
	UWidget* EntryWidget,
	const int32 ItemIndex) const
{
	if (const UReciAdvancedListEntryHost* EntryHost =
		Cast<UReciAdvancedListEntryHost>(EntryWidget))
	{
		return EntryHost->GetActiveButton();
	}

	return Super::ResolveEntrySelectionButton(EntryWidget, ItemIndex);
}

void UReciAdvancedList::NativeOnListItemsRebuilt()
{
	const int32 PreviousMainItemIndex = PendingPreviousMainItemIndex;
	UObject* const PreviousMainItem = PendingPreviousMainItem.Get();

	Super::NativeOnListItemsRebuilt();

	if (GetItemCount() <= 0)
	{
		const bool bMainItemChanged =
			PreviousMainItemIndex != INDEX_NONE
			|| PreviousMainItem != nullptr;
		MainItemIndex = INDEX_NONE;
		if (bMainItemChanged)
		{
			NotifyDrivenItemIndexChanged(INDEX_NONE);
			ClearSelection();
		}
		return;
	}

	const int32 ResolvedIndex = FMath::Clamp(
		PendingRebuildMainItemIndex != INDEX_NONE
			? PendingRebuildMainItemIndex
			: InitialMainItemIndex,
		0,
		GetItemCount() - 1);
	UObject* NewMainItem = GetItemAt(ResolvedIndex);
	const bool bMainItemChanged =
		PreviousMainItemIndex != ResolvedIndex
		|| PreviousMainItem != NewMainItem;
	MainItemIndex = ResolvedIndex;
	SetInitialFocusItemIndex(ResolvedIndex);
	if (!bExternalPresentationDriverActive)
	{
		ApplyBinaryMainPresentation(ResolvedIndex);
	}
	RefreshAdvancedListPresentationSizes();
	if (bMainItemChanged)
	{
		NotifyDrivenItemIndexChanged(ResolvedIndex);
	}
}

void UReciAdvancedList::NativeOnItemFocusReceived(
	UCommonButtonBase* FocusedButton)
{
	Super::NativeOnItemFocusReceived(FocusedButton);
	if (!bMainPresentationFollowsFocus
		|| PresentationChangeDepth > 0
		|| bExternalPresentationDriverActive)
	{
		return;
	}

	const int32 ItemIndex =
		FindAdvancedEntryHostIndexForButton(FocusedButton);
	if (ItemIndex != INDEX_NONE)
	{
		SetMainItemIndexInternal(ItemIndex, false, true);
	}
}

void UReciAdvancedList::NativeOnItemClicked(
	UCommonButtonBase* ClickedButton)
{
	const int32 ItemIndex =
		FindAdvancedEntryHostIndexForButton(ClickedButton);
	if (ItemIndex == INDEX_NONE)
	{
		return;
	}

	UObject* Item = GetItemAt(ItemIndex);
	OnItemClicked.Broadcast(Item, ItemIndex);
	if (GetItemAt(ItemIndex) != Item) return;
	if (ItemIndex != GetMainItemIndex()) SetMainItemIndex(ItemIndex, bFocusOnNavigation);
	if (GetItemAt(ItemIndex) != Item || GetMainItemIndex() != ItemIndex) return;
	if (bConfirmOnItemClick && bConfirmMainItemOnClick)
	{
		ConfirmItemAtIndex(ItemIndex);
	}
	else if (SelectionMode == ESelectionMode::Single) SelectItemAtIndex(ItemIndex, false);
}

UReciAdvancedListEntryHost* UReciAdvancedList::GetAdvancedEntryHost(
	const int32 ItemIndex) const
{
	// The hosted entry is the copy that currently represents the item; in a looping list that is the visible copy
	// nearest the carousel centre, not necessarily the originally generated one.
	return Cast<UReciAdvancedListEntryHost>(
		GetHostedEntryWidget(ItemIndex));
}

int32 UReciAdvancedList::FindAdvancedEntryHostIndexForButton(
	const UCommonButtonBase* Button) const
{
	if (!Button)
	{
		return INDEX_NONE;
	}

	for (int32 ItemIndex = 0; ItemIndex < GetItemCount(); ++ItemIndex)
	{
		const UReciAdvancedListEntryHost* EntryHost =
			GetAdvancedEntryHost(ItemIndex);
		if (EntryHost
			&& (EntryHost->GetNormalButton() == Button
				|| EntryHost->GetMainButton() == Button))
		{
			return ItemIndex;
		}
	}
	return INDEX_NONE;
}

bool UReciAdvancedList::SetMainItemIndexInternal(
	const int32 ItemIndex,
	const bool bGiveFocus,
	const bool bApplyBinaryPresentation)
{
	if (ItemIndex < 0 || ItemIndex >= GetItemCount())
	{
		return false;
	}

	const bool bChanged = MainItemIndex != ItemIndex;
	MainItemIndex = ItemIndex;
	SetInitialFocusItemIndex(ItemIndex);
	if (bApplyBinaryPresentation)
	{
		ApplyBinaryMainPresentation(ItemIndex);
	}

	if (bGiveFocus)
	{
		SetFocusedItemIndex(ItemIndex);
	}

	if (bChanged)
	{
		NotifyDrivenItemIndexChanged(ItemIndex);
	}
	return true;
}

void UReciAdvancedList::ApplyBinaryMainPresentation(
	const int32 InMainItemIndex)
{
	TGuardValue<int32> PresentationGuard(
		PresentationChangeDepth,
		PresentationChangeDepth + 1);
	for (int32 ItemIndex = 0; ItemIndex < GetItemCount(); ++ItemIndex)
	{
		SetItemMainPresentationBlend(
			ItemIndex,
			ItemIndex == InMainItemIndex ? 1.0f : 0.0f);
	}
}

void UReciAdvancedList::CopyAdvancedListItems(
	TArray<UObject*>& OutItems) const
{
	OutItems.Reset();
	OutItems.Reserve(GetItemCount());
	for (int32 ItemIndex = 0; ItemIndex < GetItemCount(); ++ItemIndex)
	{
		if (UObject* Item = GetItemAt(ItemIndex))
		{
			OutItems.Add(Item);
		}
	}
}
