// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciListBase.h"
#include "Widgets/ReciListController.h"

#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "ReciReport.h"
#include "Widgets/ReciFocusStyleButtonBase.h"
#include "Widgets/ReciListEntryEmphasis.h"
#include "Widgets/ReciListItem.h"
#include "Widgets/ReciTextListItem.h"
#include "Widgets/ReciListEntryRuntime.h"
#include "Widgets/ReciListFocusReturnController.h"
#include "Widgets/ReciListSelectionController.h"
#include "Widgets/ReciWidgetTreeUtils.h"
#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciListBase)

DEFINE_LOG_CATEGORY_STATIC(LogReciListBase, Log, All);

using UE::ReciCommonUI::Private::CreateListEntryWidget;
using UE::ReciCommonUI::Private::FindCommonButtonDescendant;
using UE::ReciCommonUI::Private::FindFocusableDescendant;
using UE::ReciCommonUI::Private::ReportEntryButton;
using UE::ReciCommonUI::Private::ReportEntryClass;

UReciListBase::UReciListBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FocusReturnController(MakeUnique<FReciListFocusReturnController>())
	, SelectionController(MakeUnique<FReciListSelectionController>())
{
}

UReciListBase::~UReciListBase() = default;

void UReciListBase::SetListController(UObject* Owner, IReciListController* Controller)
{
	ListControllerOwner = Owner;
	ListController = Controller;
}

IReciListController* UReciListBase::GetListController() const
{
	return ListControllerOwner.IsValid() ? ListController : nullptr;
}

TArray<UUserWidget*> UReciListBase::GetEntryPresentations(int32 Index) const
{
	TArray<UUserWidget*> Result;
	if (auto* Widget = Cast<UUserWidget>(GetHostedEntryWidget(Index))) Result.Add(Widget);
	return Result;
}

int32 UReciListBase::GetHostedItemCount() const
{
	return GetItemCount();
}

UWidget* UReciListBase::GetHostedEntryWidget(
	const int32 ItemIndex) const
{
	return GetEntryWidgetAtIndex(ItemIndex);
}

UCommonButtonBase* UReciListBase::GetHostedEntryButton(
	const int32 ItemIndex) const
{
	return GetItemButtonAtIndex(ItemIndex);
}

FGeometry UReciListBase::GetHostedEntryGeometry(int32 ItemIndex) const
{
	const auto* Entry=GetHostedEntryWidget(ItemIndex);
	return Entry?Entry->GetCachedGeometry():FGeometry();
}

FGeometry UReciListBase::GetEntryPresentationGeometry(UUserWidget* Presentation) const
{
	return Presentation?Presentation->GetCachedGeometry():FGeometry();
}

UWidget* UReciListBase::GetHostedFocusTarget() const
{
	return FindFocusReturnTarget();
}

bool UReciListBase::IsHostedItemNavigable(const int32 ItemIndex) const
{
	const UWidget* Entry = GetHostedEntryWidget(ItemIndex);
	const UCommonButtonBase* Button = GetHostedEntryButton(ItemIndex);
	return Entry && Entry->IsVisible() && Entry->GetIsEnabled()
		&& Button && Button->IsVisible() && Button->GetIsEnabled()
		&& (Button->GetIsFocusable() || (GetListController() && GetListController()->IsSelectionOnly()));
}

bool UReciListBase::HandleHostedItemClick(const int32 ItemIndex)
{
	UCommonButtonBase* ItemButton = GetItemButtonAtIndex(ItemIndex);
	UWidget* EntryWidget = GetEntryWidgetAtIndex(ItemIndex);
	if (!ItemButton
		|| !ItemButton->GetIsEnabled()
		|| !ItemButton->IsInteractionEnabled()
		|| !EntryWidget
		|| !EntryWidget->GetIsEnabled()
		|| !EntryWidget->IsVisible())
	{
		return false;
	}

	UObject* ClickedItem = GetItemAt(ItemIndex);
	if (SelectionMode == ESelectionMode::Single
		|| SelectionMode == ESelectionMode::SingleToggle)
	{
		ItemButton->SetIsSelected(
			SelectionMode == ESelectionMode::Single
				|| !ItemButton->GetSelected(),
			true);
	}
	else if (SelectionMode == ESelectionMode::Multi)
	{
		ItemButton->SetIsSelected(!ItemButton->GetSelected(), true);
	}

	if (GetItemAt(ItemIndex) != ClickedItem || GetItemButtonAtIndex(ItemIndex) != ItemButton) return true;
	if (auto* Controller = GetListController()) Controller->ClickItem(ItemIndex);
	else NativeOnItemClicked(ItemButton);
	return true;
}

FReciListEntriesChangedNative& UReciListBase::OnEntriesChangedNative()
{
	return EntriesChangedNative;
}

FReciListFocusedIndexChangedNative&
UReciListBase::OnFocusedIndexChangedNative()
{
	return FocusedIndexChangedNative;
}

FReciListDrivenIndexChangedNative&
UReciListBase::OnDrivenIndexChangedNative()
{
	return DrivenIndexChangedNative;
}

bool UReciListBase::HasNavigationInputHandler() const
{
	return OnNavigationInput.IsBound();
}

bool UReciListBase::BroadcastNavigationInput(
	const EUINavigation Direction)
{
	if (!HasNavigationInputHandler())
	{
		return false;
	}

	EReciNavigationDirection ReciDirection;
	switch (Direction)
	{
	case EUINavigation::Up:
		ReciDirection = EReciNavigationDirection::Up;
		break;
	case EUINavigation::Down:
		ReciDirection = EReciNavigationDirection::Down;
		break;
	case EUINavigation::Left:
		ReciDirection = EReciNavigationDirection::Left;
		break;
	case EUINavigation::Right:
		ReciDirection = EReciNavigationDirection::Right;
		break;
	default:
		return false;
	}

	OnNavigationInput.Broadcast(ReciDirection);
	return true;
}

bool UReciListBase::SetDrivenItemIndex(
	const int32 ItemIndex,
	const bool bGiveFocus)
{
	if (!GetItemAt(ItemIndex))
	{
		return false;
	}

	if (bGiveFocus && !SetFocusedItemIndex(ItemIndex))
	{
		return false;
	}

	if (DrivenItemIndex != ItemIndex)
	{
		DrivenItemIndex = ItemIndex;
		NotifyDrivenItemIndexChanged(DrivenItemIndex);
	}
	if (auto* Controller = GetListController()) Controller->DrivenItemChanged(ItemIndex, bGiveFocus);
	return true;
}

int32 UReciListBase::GetDrivenItemIndex() const
{
	if (GetItemAt(DrivenItemIndex))
	{
		return DrivenItemIndex;
	}

	const int32 FocusedIndex = GetFocusedItemIndex();
	return FocusedIndex != INDEX_NONE
		? FocusedIndex
		: (GetItemCount() > 0 ? 0 : INDEX_NONE);
}

bool UReciListBase::ConfirmDrivenItem()
{
	if (auto* Controller = GetListController()) return Controller->ConfirmItem();
	const int32 ItemIndex = GetDrivenItemIndex();
	return ItemIndex != INDEX_NONE
		&& ConfirmItemAtIndex(ItemIndex);
}

bool UReciListBase::SetItemEmphasis(
	const int32 ItemIndex,
	const float Emphasis)
{
	if (!GetItemAt(ItemIndex))
	{
		return false;
	}
	const float Clamped = FMath::Clamp(Emphasis, 0.0f, 1.0f);
	for (UUserWidget* Presentation : GetEntryPresentations(ItemIndex))
	{
		if (Presentation && Presentation->Implements<UReciListEntryEmphasis>())
		{
			IReciListEntryEmphasis::Execute_OnItemEmphasisChanged(Presentation, Clamped);
		}
	}
	return true;
}

void UReciListBase::SetItemEmphasisDriven(
	const bool bDriven)
{
}

bool UReciListBase::ApplyHostedLayoutSettings(
	const FReciListLayoutSettings& InSettings)
{
	HostedLayoutSettings = InSettings;
	bHasHostedLayoutSettings = true;
	UReciListPanel* ListPanel = Cast<UReciListPanel>(
		GetGeneratedItemsPanel());
	if (!ListPanel)
	{
		return false;
	}

	if (!bPanelTookHostedLayout)
	{
		// The host owns the layout (DESIGN.md principle 8): what the panel was authored with goes, and says so.
		bPanelTookHostedLayout = true;
		if (ListPanel->HasAuthoredLayoutSettings())
		{
			RECI_WARN_ONCE(TEXT("HostedItemsBoxLayout"), UE::ReciCommonUI::Private::GetAuthoredPath(*this),
				TEXT("%s: the Layout Settings set on its ItemsBox have no effect, since its Reci layout box %s lays out the entries."),
				*GetName(), *GetNameSafe(GetParent()));
		}
	}
	ListPanel->ApplyLayoutSettings(HostedLayoutSettings);
	return true;
}

void UReciListBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// A host that applied its layout before the panel existed applies it now; a list outside a host keeps the panel's own.
	if (bHasHostedLayoutSettings)
	{
		ApplyHostedLayoutSettings(HostedLayoutSettings);
	}
}

#if WITH_EDITOR
void UReciListBase::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);
	UE::ReciCommonUI::Private::ValidateEntryClass(*this, ItemWidgetClass, CompileLog);
}

void UReciListBase::ValidateEntryWidgetClass(const UClass* EntryClass, IWidgetCompilerLog& CompileLog) const
{
	UE::ReciCommonUI::Private::ValidateEntryClass(*this, EntryClass, CompileLog);
}
#endif

void UReciListBase::ReportEntryWidgetClass(const UClass* EntryClass) const
{
	ReportEntryClass(*this, EntryClass);
}

UUserWidget* UReciListBase::CreateEntryForItem(UWidget& Owner, UObject& Item, const TSubclassOf<UUserWidget> EntryClass)
{
	return CreateListEntryWidget(Owner, Item, EntryClass);
}

UCommonButtonBase* UReciListBase::FindEntryButton(UWidget* Entry)
{
	return FindCommonButtonDescendant(Entry);
}

bool UReciListBase::SupportsEntryWidgetClass(const UClass* EntryClass) const
{
	if (auto* Controller = GetListController()) return Controller->SupportsEntryClass(EntryClass);
	return EntryClass && EntryClass->ImplementsInterface(UReciListItem::StaticClass());
}

void UReciListBase::SetListItems(
	const TArray<UObject*>& InListItems)
{
	// The rebuild destroys a focused entry. Focus that was on the list or inside it returns to the driven item below,
	// except in a list a controller composes: a two-level list returns focus itself once it has rebuilt all its lists.
	const bool bReturnFocus = !GetListController() && HasOwningPlayerFocus();
	const int32 PreviousDrivenItemIndex = DrivenItemIndex;
	TWeakObjectPtr<UObject> PreviousDrivenItem =
		GetItemAt(PreviousDrivenItemIndex);
	TArray<UObject*> PreviousSelectedItems;
	GetSelectedItems(PreviousSelectedItems);
	ClearListItemsInternal();

	if (!SupportsEntryWidgetClass(ItemWidgetClass))
	{
		if (!InListItems.IsEmpty())
		{
			UE_LOG(
				LogReciListBase,
				Warning,
				TEXT("SetListItems requires an Entry Widget Class supporting this source's data contract."));
		}
		NativeOnListItemsRebuilt();
		if (bReturnFocus)
		{
			ReturnFocusAfterRebuild();
		}
		EntriesChangedNative.Broadcast();
		return;
	}

	ReportEntryClass(*this, ItemWidgetClass);
	EntryRuntimes.Reserve(InListItems.Num());

	int32 ItemIndex = 0;
	for (UObject* ItemObject : InListItems)
	{
		if (ItemObject)
		{
			if (UWidget* EntryWidget =
				NativeCreateEntryWidget(ItemObject, ItemIndex))
			{
				if (UPanelWidget* ItemsPanel =
					GetGeneratedItemsPanel())
				{
					ItemsPanel->AddChild(EntryWidget);
					UReciListEntryRuntime* EntryRuntime =
						NewObject<UReciListEntryRuntime>(this);
					EntryRuntime->Initialize(*ItemObject, *EntryWidget);
					EntryRuntimes.Add(EntryRuntime);
					RegisterItemSelection(
						*EntryRuntime,
						ItemIndex);
					++ItemIndex;
				}
			}
		}
	}

	NativeOnListItemsRebuilt();
	if (GetItemCount() > 0)
	{
		int32 RestoredItemIndex = PreviousDrivenItem.IsValid()
			? FindItemIndex(PreviousDrivenItem.Get())
			: INDEX_NONE;
		if (RestoredItemIndex == INDEX_NONE
			&& PreviousDrivenItemIndex != INDEX_NONE)
		{
			RestoredItemIndex = FMath::Clamp(
				PreviousDrivenItemIndex,
				0,
				GetItemCount() - 1);
		}
		if (RestoredItemIndex == INDEX_NONE)
		{
			RestoredItemIndex = FMath::Clamp(
				GetDrivenItemIndex(),
				0,
				GetItemCount() - 1);
		}
		SetDrivenItemIndex(RestoredItemIndex, false);
	}
	// Rebuilding presentations must not discard model selection. Controllers own their state.
	if (!GetListController() && !(bSelectionFollowsNavigation && SelectionMode == ESelectionMode::Single))
	{
		for (UObject* Item : PreviousSelectedItems)
		{
			const int32 Index = FindItemIndex(Item);
			if (Index != INDEX_NONE) SelectItemAtIndex(Index, false);
		}
	}
	RefreshDrivenItemCursor();
	// Before the entries changed notice, so that the viewport handles the new entries last, as with no focus to return:
	// a carousel then jumps to the driven item instead of gliding there for the focus change.
	if (bReturnFocus)
	{
		ReturnFocusAfterRebuild();
	}
	EntriesChangedNative.Broadcast();
}

bool UReciListBase::AddItem(UObject* Item)
{
	TArray<UObject*> Items;
	CopyListItems(Items);
	if (!Item || Items.Contains(Item))
	{
		return false;
	}

	Items.Add(Item);
	SetListItems(Items);
	return true;
}

bool UReciListBase::InsertItem(
	UObject* Item,
	const int32 ItemIndex)
{
	TArray<UObject*> Items;
	CopyListItems(Items);
	if (!Item
		|| Items.Contains(Item)
		|| ItemIndex < 0
		|| ItemIndex > Items.Num())
	{
		return false;
	}

	Items.Insert(Item, ItemIndex);
	SetListItems(Items);
	return true;
}

bool UReciListBase::RemoveItem(UObject* Item)
{
	TArray<UObject*> Items;
	CopyListItems(Items);
	const int32 ItemIndex = Items.IndexOfByKey(Item);
	if (ItemIndex == INDEX_NONE)
	{
		return false;
	}

	Items.RemoveAt(ItemIndex);
	SetListItems(Items);
	return true;
}

bool UReciListBase::RemoveItemAt(const int32 ItemIndex)
{
	TArray<UObject*> Items;
	CopyListItems(Items);
	if (!Items.IsValidIndex(ItemIndex))
	{
		return false;
	}

	Items.RemoveAt(ItemIndex);
	SetListItems(Items);
	return true;
}

void UReciListBase::ClearListItems()
{
	const TArray<UObject*> EmptyItems;
	SetListItems(EmptyItems);
}

UObject* UReciListBase::GetItemAt(const int32 ItemIndex) const
{
	return GetItemObjectAtIndex(ItemIndex);
}

void UReciListBase::SetTextListItems(
	const TArray<FText>& InListItems)
{
	if (!ItemWidgetClass
		|| !ItemWidgetClass->ImplementsInterface(
			UReciTextListItem::StaticClass()))
	{
		ClearListItemsInternal();
		NativeOnListItemsRebuilt();
		EntriesChangedNative.Broadcast();
		if (!InListItems.IsEmpty())
		{
			UE_LOG(
				LogReciListBase,
				Warning,
				TEXT("SetTextListItems requires Entry Widget Class to implement ReciTextListItem."));
		}
		return;
	}

	TArray<UObject*> ItemObjects;
	ItemObjects.Reserve(InListItems.Num());
	for (const FText& ItemText : InListItems)
	{
		UReciTextListItemData* ItemData =
			NewObject<UReciTextListItemData>(this);
		ItemData->ItemText = ItemText;
		ItemObjects.Add(ItemData);
	}

	SetListItems(ItemObjects);
}

void UReciListBase::SetInitialFocusItemIndex(
	const int32 InItemIndex)
{
	FocusReturnController->SetInitialItemIndex(InItemIndex);
}

void UReciListBase::SetAllowPointerHover(
	const bool bInAllowPointerHover)
{
	bAllowPointerHover = bInAllowPointerHover;
}

bool UReciListBase::IsPointerHoverAllowed() const
{
	return bAllowPointerHover;
}

void UReciListBase::SetSelectionMode(
	const TEnumAsByte<ESelectionMode::Type> InSelectionMode)
{
	const ESelectionMode::Type PreviousMode = SelectionMode.GetValue();
	const ESelectionMode::Type NewMode = InSelectionMode.GetValue();

	if (NewMode == ESelectionMode::None)
	{
		ClearSelection();
	}
	else if (
		PreviousMode == ESelectionMode::Multi
		&& NewMode != ESelectionMode::Multi)
	{
		const int32 SelectedItemCount =
			SelectionController->GetSelectedItemCount(EntryRuntimes);
		if (SelectedItemCount != 1)
		{
			ClearSelection();
		}
	}

	SelectionMode = InSelectionMode;
	ApplySelectionModeToItems();
}

void UReciListBase::SetEntryWidgetClass(
	const TSubclassOf<UUserWidget> InEntryWidgetClass)
{
	if (ItemWidgetClass == InEntryWidgetClass)
	{
		return;
	}

	if (!SupportsEntryWidgetClass(InEntryWidgetClass))
	{
		UE_LOG(
			LogReciListBase,
			Warning,
			TEXT("SetEntryWidgetClass requires a widget class supporting this source's data contract."));
		return;
	}

	TArray<UObject*> ExistingItems;
	ExistingItems.Reserve(EntryRuntimes.Num());
	for (const UReciListEntryRuntime* Entry : EntryRuntimes)
	{
		UObject* ItemObject = Entry ? Entry->GetItemObject() : nullptr;
		if (ItemObject)
		{
			ExistingItems.Add(ItemObject);
		}
	}

	ItemWidgetClass = InEntryWidgetClass;
	SetListItems(ExistingItems);
}

void UReciListBase::ClearSelection()
{
	SelectionController->ClearSelection(EntryRuntimes);
}

bool UReciListBase::SetFocusedItem(UObject* Item)
{
	return SetFocusedItemIndex(FindItemIndex(Item));
}

bool UReciListBase::SetFocusedItemIndex(const int32 ItemIndex)
{
	UWidget* FocusTarget = FindItemFocusTargetAtIndex(ItemIndex);
	if (!FocusTarget)
	{
		return false;
	}

	SetInitialFocusItemIndex(ItemIndex);
	{
		TGuardValue<bool> FocusingRequestedItem(bFocusingRequestedItem, true);
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			FocusTarget->SetUserFocus(PlayerController);
		}
		else
		{
			FocusTarget->SetFocus();
		}
	}

	if (FocusedItemIndex != ItemIndex
		&& (FocusTarget->HasAnyUserFocus()
			|| FocusTarget->HasFocusedDescendants()))
	{
		SetFocusedItemState(ItemIndex);
	}
	return true;
}

UObject* UReciListBase::GetFocusedItem() const
{
	return GetItemObjectAtIndex(FocusedItemIndex);
}

int32 UReciListBase::GetFocusedItemIndex() const
{
	return EntryRuntimes.IsValidIndex(FocusedItemIndex)
		? FocusedItemIndex
		: INDEX_NONE;
}

bool UReciListBase::ConfirmFocusedItem()
{
	return ConfirmItemAtIndex(GetFocusedItemIndex());
}

bool UReciListBase::SetSelectedItem(
	UObject* Item,
	const bool bGiveFocus)
{
	return SetSelectedItemIndex(
		FindItemIndex(Item),
		bGiveFocus);
}

bool UReciListBase::SetSelectedItemIndex(
	const int32 ItemIndex,
	const bool bGiveFocus)
{
	return SelectItemAtIndex(ItemIndex, bGiveFocus);
}

UObject* UReciListBase::GetSelectedItem() const
{
	return GetItemObjectAtIndex(
		SelectionController->GetFirstSelectedItemIndex(EntryRuntimes));
}

bool UReciListBase::GetSelectedItems(
	TArray<UObject*>& OutItems) const
{
	OutItems.Reset();
	TArray<int32> SelectedItemIndices;
	SelectionController->GetSelectedItemIndices(
		EntryRuntimes,
		SelectedItemIndices);
	for (const int32 ItemIndex : SelectedItemIndices)
	{
		if (UObject* ItemObject = GetItemObjectAtIndex(ItemIndex))
		{
			OutItems.Add(ItemObject);
		}
	}

	return !OutItems.IsEmpty();
}

int32 UReciListBase::GetNumItemsSelected() const
{
	return SelectionController->GetSelectedItemCount(EntryRuntimes);
}

UWidget* UReciListBase::FindFirstItemFocusTarget() const
{
	// Entering a list lands on its first item that can take focus, skipping disabled ones.
	for (int32 ItemIndex = 0; ItemIndex < GetItemCount(); ++ItemIndex)
	{
		if (UWidget* FocusTarget = FindItemFocusTargetAtIndex(ItemIndex))
		{
			return FocusTarget;
		}
	}
	return nullptr;
}

UWidget* UReciListBase::FindItemFocusTargetAtIndex(
	const int32 ItemIndex) const
{
	// A disabled or hidden item never takes focus (it is also left out of navigation).
	UWidget* Item = GetEntryWidgetAtIndex(ItemIndex);
	if (Item && (!Item->GetIsEnabled() || !Item->IsVisible()))
	{
		return nullptr;
	}

	if (UCommonButtonBase* ItemButton = GetItemButtonAtIndex(ItemIndex))
	{
		return ItemButton->GetIsEnabled() && ItemButton->IsVisible() ? ItemButton : nullptr;
	}

	if (!Item)
	{
		return nullptr;
	}

	if (UUserWidget* ItemUserWidget = Cast<UUserWidget>(Item))
	{
		UWidget* ItemDesiredFocusWidget =
			ItemUserWidget->GetDesiredFocusWidget();
		if (UWidget* ItemFocusTarget =
			FindFocusableDescendant(
				ItemDesiredFocusWidget))
		{
			return ItemFocusTarget;
		}
	}

	return FindFocusableDescendant(Item);
}

UWidget* UReciListBase::FindFocusReturnTarget() const
{
	// Focus that comes back after leaving the list, before or after a rebuild, returns to the driven item, which may have
	// moved while focus was away; when its entry cannot take focus, to the first entry that can. That holds also before
	// the list is back in the focus path, as when Slate gives focus to the list's layout box. Reset Last Focused Item On
	// Focus Return sends it to the first entry instead.
	if (FocusReturnController->IsApplyingFocusReturn()
		|| FocusReturnController->IsAwaitingFocusReturn())
	{
		UWidget* DrivenItemFocusTarget = bResetLastFocusedItemOnFocusReturn
			? nullptr
			: FindItemFocusTargetAtIndex(GetDrivenItemIndex());
		return DrivenItemFocusTarget ? DrivenItemFocusTarget : FindFirstItemFocusTarget();
	}

	// Focus that enters the list for the first time lands on the item set for it, or on the first entry that can take it.
	const int32 InitialFocusItemIndex =
		FocusReturnController->GetInitialItemIndex();
	if (InitialFocusItemIndex != INDEX_NONE)
	{
		if (UWidget* InitialFocusTarget =
			FindItemFocusTargetAtIndex(InitialFocusItemIndex))
		{
			return InitialFocusTarget;
		}
	}

	return FindFirstItemFocusTarget();
}

bool UReciListBase::IsRegisteredItemButton(
	const UCommonButtonBase* ItemButton) const
{
	return GetItemButtonIndex(ItemButton) != INDEX_NONE;
}

bool UReciListBase::NativeSupportsKeyboardFocus() const
{
	return FindFirstItemFocusTarget() != nullptr
		|| Super::NativeSupportsKeyboardFocus();
}

FReply UReciListBase::NativeOnFocusReceived(
	const FGeometry& InGeometry,
	const FFocusEvent& InFocusEvent)
{
	FReply Reply =
		Super::NativeOnFocusReceived(InGeometry, InFocusEvent);

	if (UWidget* FocusTarget = FindFocusReturnTarget())
	{
		const TSharedRef<SWidget> FocusTargetSlate =
			FocusTarget->TakeWidget();
		if (FocusTargetSlate->SupportsKeyboardFocus())
		{
			if (!IsRegisteredItemButton(
				Cast<UCommonButtonBase>(FocusTarget)))
			{
				CompleteFocusReturn();
			}

			return FReply::Handled().SetUserFocus(
				FocusTargetSlate,
				InFocusEvent.GetCause());
		}
	}

	CompleteFocusReturn();
	return Reply;
}

void UReciListBase::NativeOnAddedToFocusPath(
	const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	FocusReturnController->BeginFocusReturn(
		InFocusEvent.GetCause(),
		InFocusEvent.GetUser(),
		bClearSelectionOnFocusReturn);
}

void UReciListBase::NativeOnRemovedFromFocusPath(
	const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	SetFocusedItemState(INDEX_NONE);
	FocusReturnController->NotifyRemovedFromFocusPath();
}

void UReciListBase::RegisterItemSelection(
	UReciListEntryRuntime& Entry,
	const int32 ItemIndex)
{
	UCommonButtonBase* ItemButton =
		ResolveEntrySelectionButton(Entry.GetEntryWidget(), ItemIndex);

	Entry.SetSelectionButton(ItemButton);
	if (!ItemButton)
	{
		if (SelectionMode != ESelectionMode::None)
		{
			UE_LOG(
				LogReciListBase,
				Warning,
				TEXT("Selection modes require the item or its Desired Focus Widget to derive from CommonButtonBase."));
		}
		return;
	}

	ReportEntryButton(*this, *ItemButton);
	BindItemSelectionButton(ItemButton);

	if (ItemButton->GetSelected())
	{
		HandleItemSelectionChanged(true, ItemButton);
	}
}

void UReciListBase::BindItemSelectionButton(
	UCommonButtonBase* ItemButton)
{
	if (!ItemButton)
	{
		return;
	}

	ItemButton->OnIsSelectedChanged().RemoveAll(this);
	ItemButton->OnFocusReceived().RemoveAll(this);
	ItemButton->OnClicked().RemoveAll(this);
	ItemButton->OnIsSelectedChanged().AddUObject(
		this,
		&UReciListBase::HandleItemSelectionChanged,
		ItemButton);
	ItemButton->OnFocusReceived().AddUObject(
		this,
		&UReciListBase::HandleItemFocusReceived,
		ItemButton);
	ItemButton->OnClicked().AddUObject(
		this,
		&UReciListBase::HandleItemClicked,
		ItemButton);
	ApplySelectionModeToItem(ItemButton);
}

void UReciListBase::UnbindItemSelectionButton(
	UCommonButtonBase* ItemButton)
{
	if (!ItemButton)
	{
		return;
	}

	ItemButton->OnIsSelectedChanged().RemoveAll(this);
	ItemButton->OnFocusReceived().RemoveAll(this);
	ItemButton->OnClicked().RemoveAll(this);
}

void UReciListBase::HandleItemFocusReceived(
	UCommonButtonBase* FocusedButton)
{
	if (!FocusedButton)
	{
		return;
	}

	if (FocusReturnController->IsRedirectingItemFocus())
	{
		CompleteFocusReturn();
		CommitItemFocus(FocusedButton);
		return;
	}

	if (!FocusReturnController->IsApplyingFocusReturn())
	{
		CommitItemFocus(FocusedButton);
		return;
	}

	// Focus that comes back to another entry, as Slate's geometric search can put it, moves on to the focus return
	// target. Pointer focus and an explicitly requested item keep the item that received focus.
	UWidget* FocusTarget = FocusReturnController->IsPointerFocusReturn() || bFocusingRequestedItem
		? FocusedButton
		: FindFocusReturnTarget();
	if (!FocusTarget || FocusTarget == FocusedButton)
	{
		CompleteFocusReturn();
		CommitItemFocus(FocusedButton);
		return;
	}

	const TSharedRef<SWidget> FocusTargetSlate =
		FocusTarget->TakeWidget();
	if (!FocusTargetSlate->SupportsKeyboardFocus())
	{
		CompleteFocusReturn();
		CommitItemFocus(FocusedButton);
		return;
	}

	FocusReturnController->BeginItemFocusRedirect();
	if (!FSlateApplication::Get().SetUserFocus(
		FocusReturnController->GetFocusReturnUserIndex(),
		FocusTargetSlate,
		EFocusCause::SetDirectly))
	{
		CompleteFocusReturn();
		CommitItemFocus(FocusedButton);
	}
	FocusReturnController->EndItemFocusRedirect();
}

void UReciListBase::NativeOnListItemsRebuilt()
{
}

void UReciListBase::NativeOnItemFocusReceived(
	UCommonButtonBase* FocusedButton)
{
}

void UReciListBase::NativeOnItemClicked(
	UCommonButtonBase* ClickedButton)
{
	const int32 ItemIndex = GetItemButtonIndex(ClickedButton);
	UObject* ClickedItem = GetItemAt(ItemIndex);
	if (!ClickedItem) return;
	OnItemClicked.Broadcast(ClickedItem, ItemIndex);
	if (GetItemAt(ItemIndex) != ClickedItem) return;
	if (SetDrivenItemIndex(ItemIndex, false))
	{
		if (bConfirmOnItemClick) ConfirmItemAtIndex(ItemIndex);
		// Toggle modes have already toggled in HandleHostedItemClick, or are owned
		// by CommonButton's native click. Never re-select a just-deselected item.
		else if (SelectionMode == ESelectionMode::Single) SelectItemAtIndex(ItemIndex, false);
	}
}

void UReciListBase::SetShowDrivenItemCursor(const bool bShow)
{
	bShowDrivenItemCursor = bShow;
	RefreshDrivenItemCursor();
}

void UReciListBase::SetFocusOnNavigation(const bool bInFocusOnNavigation)
{
	bFocusOnNavigation = bInFocusOnNavigation;
	RefreshDrivenItemCursor();
}

void UReciListBase::RefreshDrivenItemCursor()
{
	// Only a list that screen keys drive while focus is elsewhere needs a key cursor; an ordinary list's driven
	// item is where focus is whenever keys act on it.
	const bool bShowCursor = bShowDrivenItemCursor && !bFocusOnNavigation;
	const int32 DrivenIndex = GetItemAt(DrivenItemIndex) ? DrivenItemIndex : INDEX_NONE;
	for (int32 Index = 0; Index < GetItemCount(); ++Index)
	{
		if (auto* Button = Cast<UReciFocusStyleButtonBase>(GetItemButtonAtIndex(Index)))
		{
			Button->SetKeyCursorVisual(bShowCursor && Index == DrivenIndex);
		}
	}
}

bool UReciListBase::HasOwningPlayerFocus() const
{
	APlayerController* Player = GetOwningPlayer();
	return HasUserFocus(Player) || HasUserFocusedDescendants(Player);
}

void UReciListBase::ReturnFocusAfterRebuild()
{
	if (SetFocusedItemIndex(GetDrivenItemIndex()))
	{
		return;
	}
	// Where entering the list puts focus: its focus return target, or the list itself when no entry can take focus and
	// the list can. Otherwise focus stays where it is; Slate would move it to a widget around the list.
	APlayerController* Player = GetOwningPlayer();
	if (UWidget* FocusTarget = FindFocusReturnTarget())
	{
		FocusTarget->SetUserFocus(Player);
	}
	else if (NativeSupportsKeyboardFocus())
	{
		SetUserFocus(Player);
	}
}

void UReciListBase::NotifyDrivenItemIndexChanged(const int32 ItemIndex)
{
	RefreshDrivenItemCursor();
	UObject* Item = GetItemAt(ItemIndex);
	if (Item && bSelectionFollowsNavigation && !GetListController()
		&& SelectionMode == ESelectionMode::Single)
	{
		if (auto* Button = GetItemButtonAtIndex(ItemIndex); Button && !Button->GetSelected())
			SelectItemAtIndex(ItemIndex, false);
		// Selection callbacks can replace the list or redirect its cursor.
		if (GetItemAt(ItemIndex) != Item || GetDrivenItemIndex() != ItemIndex) return;
	}
	DrivenIndexChangedNative.Broadcast(ItemIndex);
	OnDrivenItemChanged.Broadcast(GetItemAt(ItemIndex), ItemIndex);
}

int32 UReciListBase::GetItemCount() const
{
	return EntryRuntimes.Num();
}

int32 UReciListBase::GetSelectedItemIndex() const
{
	return SelectionController->GetFirstSelectedItemIndex(EntryRuntimes);
}

int32 UReciListBase::GetItemButtonIndex(
	const UCommonButtonBase* ItemButton) const
{
	if (!ItemButton)
	{
		return INDEX_NONE;
	}

	for (int32 ItemIndex = 0; ItemIndex < EntryRuntimes.Num(); ++ItemIndex)
	{
		if (GetItemButtonAtIndex(ItemIndex) == ItemButton)
		{
			return ItemIndex;
		}
	}
	return INDEX_NONE;
}

UWidget* UReciListBase::GetEntryWidgetAtIndex(
	const int32 ItemIndex) const
{
	const UReciListEntryRuntime* Entry = EntryRuntimes.IsValidIndex(ItemIndex)
		? EntryRuntimes[ItemIndex]
		: nullptr;
	return Entry ? Entry->GetEntryWidget() : nullptr;
}

UCommonButtonBase* UReciListBase::GetItemButtonAtIndex(
	const int32 ItemIndex) const
{
	const UReciListEntryRuntime* Entry = EntryRuntimes.IsValidIndex(ItemIndex)
		? EntryRuntimes[ItemIndex]
		: nullptr;
	return Entry ? Entry->GetSelectionButton() : nullptr;
}

UObject* UReciListBase::GetItemObjectAtIndex(
	const int32 ItemIndex) const
{
	const UReciListEntryRuntime* Entry = EntryRuntimes.IsValidIndex(ItemIndex)
		? EntryRuntimes[ItemIndex]
		: nullptr;
	return Entry ? Entry->GetItemObject() : nullptr;
}

bool UReciListBase::SelectItemAtIndex(
	const int32 ItemIndex,
	const bool bGiveFocus)
{
	UCommonButtonBase* ItemButton =
		GetItemButtonAtIndex(ItemIndex);
	if (!ItemButton || SelectionMode == ESelectionMode::None)
	{
		return false;
	}

	ItemButton->SetIsSelected(true, true);
	if (bGiveFocus)
	{
		SetFocusedItemIndex(ItemIndex);
	}
	return true;
}

bool UReciListBase::ConfirmItemAtIndex(const int32 ItemIndex)
{
	if (auto* Controller = GetListController()) return Controller->ConfirmItem();
	UObject* Item = GetItemObjectAtIndex(ItemIndex);
	const UCommonButtonBase* Button = GetItemButtonAtIndex(ItemIndex);
	if (!Item || !GetIsEnabled() || (Button && (!Button->GetIsEnabled() || !Button->IsInteractionEnabled())))
	{
		return false;
	}
	if (SelectionMode == ESelectionMode::Single)
	{
		SelectItemAtIndex(ItemIndex, false);
		// The selection operation already consumed this activation. A callback
		// may replace the source; never dispatch to a new item or bubble into it.
		if (GetItemAt(ItemIndex) != Item) return true;
	}

	OnItemConfirmed.Broadcast(Item, ItemIndex);
	return true;
}

void UReciListBase::HandleItemClicked(
	UCommonButtonBase* ClickedButton)
{
	if (IsRegisteredItemButton(ClickedButton))
	{
		if (auto* Controller = GetListController()) Controller->ClickItem(GetItemButtonIndex(ClickedButton));
		else NativeOnItemClicked(ClickedButton);
	}
}

void UReciListBase::CompleteFocusReturn()
{
	const bool bShouldClearSelection =
		FocusReturnController->ShouldClearSelection();
	FocusReturnController->CompleteFocusReturn();
	if (bShouldClearSelection)
	{
		ClearSelection();
	}
}

void UReciListBase::ApplySelectionModeToItem(
	UCommonButtonBase* ItemButton)
{
	SelectionController->ApplyModeToButton(
		ItemButton,
		SelectionMode.GetValue());
}

void UReciListBase::ApplySelectionModeToItems()
{
	SelectionController->ApplyModeToEntries(
		EntryRuntimes,
		SelectionMode.GetValue());
}

void UReciListBase::HandleItemSelectionChanged(
	const bool bIsSelected,
	UCommonButtonBase* ChangedButton)
{
	if (!ChangedButton)
	{
		return;
	}

	PendingSelectionChanges.Add({ChangedButton, bIsSelected});
	if (!bProcessingSelectionChanges)
	{
		ProcessPendingSelectionChanges();
	}
}

void UReciListBase::ProcessPendingSelectionChanges()
{
	TGuardValue<bool> ProcessingGuard(bProcessingSelectionChanges, true);
	int32 ProcessedChangeCount = 0;
	while (!PendingSelectionChanges.IsEmpty())
	{
		// Selection callbacks are allowed to select another item. Serializing
		// those nested changes keeps Single and SingleToggle transactional.
		if (++ProcessedChangeCount > 1024)
		{
			UE_LOG(
				LogReciListBase,
				Error,
				TEXT("Selection callbacks produced an unbounded change loop; remaining changes were discarded."));
			PendingSelectionChanges.Reset();
			break;
		}

		const FPendingSelectionChange PendingChange =
			PendingSelectionChanges[0];
		PendingSelectionChanges.RemoveAt(0, 1, EAllowShrinking::No);
		UCommonButtonBase* ChangedButton = PendingChange.Button.Get();
		if (!IsRegisteredItemButton(ChangedButton))
		{
			continue;
		}

		if (!SelectionController->NormalizeSelectionChange(
			PendingChange.bIsSelected,
			*ChangedButton,
			SelectionMode.GetValue(),
			EntryRuntimes))
		{
			continue;
		}

		BroadcastItemSelectionChanged(
			ChangedButton,
			PendingChange.bIsSelected);
	}
}

void UReciListBase::BroadcastItemSelectionChanged(
	UCommonButtonBase* ChangedButton,
	const bool bIsSelected)
{
	const int32 ItemIndex = GetItemButtonIndex(ChangedButton);
	if (UObject* ItemObject = GetItemObjectAtIndex(ItemIndex))
	{
		OnItemSelectionChanged.Broadcast(
			ItemObject,
			bIsSelected);
		if (bIsSelected)
		{
			OnItemSelected.Broadcast(ItemObject);
		}
	}
}

TSubclassOf<UUserWidget> UReciListBase::ResolveEntryWidgetClass(
	UObject* ItemObject,
	const int32 ItemIndex) const
{
	return ItemWidgetClass;
}

UWidget* UReciListBase::NativeCreateEntryWidget(
	UObject* ItemObject,
	const int32 ItemIndex)
{
	const TSubclassOf<UUserWidget> EntryWidgetClass =
		ResolveEntryWidgetClass(ItemObject, ItemIndex);
	if (!EntryWidgetClass
		|| !ItemObject
		|| !SupportsEntryWidgetClass(EntryWidgetClass))
	{
		return nullptr;
	}

	if (auto* Controller = GetListController())
	{
		UUserWidget* Entry = UUserWidget::CreateWidgetInstance(*this, EntryWidgetClass, NAME_None);
		if (Entry) Controller->InitializeEntry(Entry, ItemObject, ItemIndex);
		return Entry;
	}
	return CreateListEntryWidget(
		*this,
		*ItemObject,
		EntryWidgetClass);
}

UCommonButtonBase*
UReciListBase::ResolveEntrySelectionButton(
	UWidget* EntryWidget,
	const int32 ItemIndex) const
{
	return FindCommonButtonDescendant(EntryWidget);
}

void UReciListBase::ReplaceEntrySelectionButton(
	const int32 ItemIndex,
	UCommonButtonBase* NewButton)
{
	if (!EntryRuntimes.IsValidIndex(ItemIndex)
		|| !EntryRuntimes[ItemIndex]
		|| EntryRuntimes[ItemIndex]->GetSelectionButton() == NewButton)
	{
		return;
	}

	UReciListEntryRuntime* Entry = EntryRuntimes[ItemIndex];
	UCommonButtonBase* PreviousButton = Entry->GetSelectionButton();
	const bool bWasSelected = PreviousButton
		&& PreviousButton->GetSelected();

	UnbindItemSelectionButton(PreviousButton);
	if (PreviousButton && PreviousButton->GetSelected())
	{
		PreviousButton->ClearSelection();
	}
	Entry->SetSelectionButton(NewButton);

	if (NewButton)
	{
		// An inactive presentation may still carry the previous selection mode.
		// Make it selectable before transferring the logical selection to it.
		ApplySelectionModeToItem(NewButton);
		if (bWasSelected && !NewButton->GetSelected())
		{
			NewButton->SetIsSelected(true, false);
		}
		else if (!bWasSelected && NewButton->GetSelected())
		{
			NewButton->ClearSelection();
		}
		BindItemSelectionButton(NewButton);
	}
}

void UReciListBase::CommitItemFocus(
	UCommonButtonBase* FocusedButton)
{
	SetFocusedItemState(GetItemButtonIndex(FocusedButton));
	NativeOnItemFocusReceived(FocusedButton);
}

void UReciListBase::SetFocusedItemState(const int32 ItemIndex)
{
	const int32 NewFocusedItemIndex =
		EntryRuntimes.IsValidIndex(ItemIndex)
			? ItemIndex
			: INDEX_NONE;
	if (FocusedItemIndex == NewFocusedItemIndex)
	{
		return;
	}

	FocusedItemIndex = NewFocusedItemIndex;
	FocusedIndexChangedNative.Broadcast(FocusedItemIndex);
	OnFocusedItemChanged.Broadcast(
		GetItemObjectAtIndex(FocusedItemIndex),
		FocusedItemIndex);
}

int32 UReciListBase::FindItemIndex(const UObject* Item) const
{
	if (!Item)
	{
		return INDEX_NONE;
	}

	for (int32 ItemIndex = 0;
		ItemIndex < EntryRuntimes.Num();
		++ItemIndex)
	{
		if (GetItemObjectAtIndex(ItemIndex) == Item)
		{
			return ItemIndex;
		}
	}
	return INDEX_NONE;
}

void UReciListBase::CopyListItems(
	TArray<UObject*>& OutItems) const
{
	OutItems.Reset(EntryRuntimes.Num());
	for (const UReciListEntryRuntime* Entry : EntryRuntimes)
	{
		if (UObject* Item = Entry ? Entry->GetItemObject() : nullptr)
		{
			OutItems.Add(Item);
		}
	}
}

void UReciListBase::ClearListItemsInternal()
{
	if (DrivenItemIndex != INDEX_NONE)
	{
		DrivenItemIndex = INDEX_NONE;
		NotifyDrivenItemIndexChanged(INDEX_NONE);
	}
	SetFocusedItemState(INDEX_NONE);
	PendingSelectionChanges.Reset();
	for (const UReciListEntryRuntime* Entry : EntryRuntimes)
	{
		UCommonButtonBase* ItemButton = Entry
			? Entry->GetSelectionButton()
			: nullptr;
		if (ItemButton)
		{
			ItemButton->OnIsSelectedChanged().RemoveAll(this);
			ItemButton->OnFocusReceived().RemoveAll(this);
			ItemButton->OnClicked().RemoveAll(this);
		}
	}
	EntryRuntimes.Reset();
	FocusReturnController->ResetForEntries();

	if (UPanelWidget* ItemsPanel = GetGeneratedItemsPanel())
	{
		ItemsPanel->ClearChildren();
	}
}
