// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#include "Widgets/ReciTwoLevelListBase.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateUser.h"
#include "Widgets/ReciTwoLevelItemBase.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciAdvancedList.h"
#include "Widgets/ReciScrollBox.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTwoLevelListBinding.h"
#include "ReciReport.h"
#if WITH_EDITOR
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Editor/WidgetCompilerLog.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogReciTwoLevelList, Log, All);

#if WITH_EDITOR
void UReciTwoLevelListBase::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	using namespace UE::ReciCommonUIWidgets::Private;
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);
	// A missing ParentList is the BindWidget check's to report.
	const UReciListBase* Parent = Cast<UReciListBase>(BlueprintWidgetTree.FindWidget(GET_MEMBER_NAME_CHECKED(UReciTwoLevelListBase, ParentList)));
	if (!Parent)
	{
		return;
	}
	const TOptional<EOrientation> ParentAxis = ValidateTwoLevelPlacement(*Parent, CompileLog);
	ValidateTwoLevelEntryClasses(*Parent, true, CompileLog);
	ValidateDroppedListSettings(*Parent, true, bAllowPointerHover, CompileLog);
	if (!ParentAxis.IsSet())
	{
		return;
	}
	TArray<const UClass*, TInlineAllocator<2>> RowClasses;
	RowClasses.Add(Parent->ItemWidgetClass.Get());
	if (const UReciAdvancedList* Advanced = Cast<UReciAdvancedList>(Parent))
	{
		RowClasses.AddUnique(Advanced->MainEntryWidgetClass.Get());
	}
	for (const UClass* RowClass : RowClasses)
	{
		// The tree the row class's Blueprint authors (its own, or the nearest parent Blueprint's).
		const UWidgetBlueprintGeneratedClass* RowBlueprint = Cast<UWidgetBlueprintGeneratedClass>(RowClass);
		const UWidgetBlueprintGeneratedClass* TreeOwner = RowBlueprint ? RowBlueprint->FindWidgetTreeOwningClass() : nullptr;
		if (const UWidgetTree* RowTree = TreeOwner ? TreeOwner->GetWidgetTreeArchetype() : nullptr)
		{
			ValidateTwoLevelRowTree(*Parent, ParentAxis.GetValue(), *RowClass, *RowTree, CompileLog);
		}
	}
}

void UReciTwoLevelItemBase::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	using namespace UE::ReciCommonUIWidgets::Private;
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);
	// A missing ChildList is the BindWidget check's to report. The owner's Allow Pointer Hover is unknown here.
	if (const UReciListBase* Child = Cast<UReciListBase>(BlueprintWidgetTree.FindWidget(GET_MEMBER_NAME_CHECKED(UReciTwoLevelItemBase, ChildList))))
	{
		ValidateTwoLevelPlacement(*Child, CompileLog);
		ValidateTwoLevelEntryClasses(*Child, false, CompileLog);
		ValidateDroppedListSettings(*Child, false, {}, CompileLog);
	}
}
#endif

void UReciTwoLevelListBase::SetItems(const TArray<UObject*>& InItems, FReciTwoLevelChildCount InChildCountProvider)
{
	if (bBuilding)
	{
		UE_LOG(LogReciTwoLevelList, Warning, TEXT("SetItems cannot be called while entry widgets are being created."));
		return;
	}
	const uint32 Revision = ++StateRevision;
	UObject* PreviousParent = GetFocusedParent();
	TMap<UObject*, int32> PreviousChildren;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (SelectedChildren.IsValidIndex(Index)) PreviousChildren.Add(Items[Index], SelectedChildren[Index]);
	}
	TArray<TObjectPtr<UObject>> NewItems;
	TArray<int32> NewCounts;
	TArray<int32> NewSelections;
	TSet<UObject*> Parents;
	for (UObject* Parent : InItems)
	{
		if (!IsValid(Parent) || Parents.Contains(Parent)) continue;
		Parents.Add(Parent);
		const int32 Count = InChildCountProvider.IsBound() ? FMath::Max(0, InChildCountProvider.Execute(Parent)) : 0;
		// A BP count callback may replace the list. Never overwrite its newer transaction.
		if (Revision != StateRevision) return;
		NewItems.Add(Parent);
		NewCounts.Add(Count);
		const int32* Previous = PreviousChildren.Find(Parent);
		NewSelections.Add(Count == 0 ? INDEX_NONE : (Previous && *Previous >= 0 && *Previous < Count ? *Previous : 0));
	}
	if (!NewItems.IsEmpty() && !InChildCountProvider.IsBound())
		UE_LOG(LogReciTwoLevelList, Warning, TEXT("SetItems has no child count provider; all parents will have zero children."));
	CancelGesture();
	Items = MoveTemp(NewItems);
	ChildCounts = MoveTemp(NewCounts);
	SelectedChildren = MoveTemp(NewSelections);
	ChildCountProvider = InChildCountProvider;
	FocusedParentIndex = Items.IndexOfByKey(PreviousParent);
	if (FocusedParentIndex == INDEX_NONE && !Items.IsEmpty()) FocusedParentIndex = 0;
	SynchronizeChildSelections();
	RefreshPresentation();
	OnFocusedParentChanged.Broadcast(GetFocusedParent(), FocusedParentIndex);
	BroadcastPair();
}

void UReciTwoLevelListBase::RefreshItems()
{
	TArray<UObject*> Parents;
	for (UObject* Parent : Items) Parents.Add(Parent);
	SetItems(Parents, ChildCountProvider);
}

bool UReciTwoLevelListBase::IsValidChild(int32 Parent, int32 Child) const
{
	return ChildCounts.IsValidIndex(Parent) && Child >= 0 && Child < ChildCounts[Parent];
}

UObject* UReciTwoLevelListBase::GetFocusedParent() const
{
	return Items.IsValidIndex(FocusedParentIndex) ? Items[FocusedParentIndex] : nullptr;
}

int32 UReciTwoLevelListBase::GetSelectedChildIndex() const
{
	return SelectedChildren.IsValidIndex(FocusedParentIndex) ? SelectedChildren[FocusedParentIndex] : INDEX_NONE;
}

UReciListBase* UReciTwoLevelListBase::GetParentList() const { return ParentList; }
UReciListBase* UReciTwoLevelListBase::GetChildList(int32 Index) const
{
	if (auto* Binding = GetChildBinding(Index)) return Binding->List;
	return nullptr;
}

UReciTwoLevelListBinding* UReciTwoLevelListBase::GetChildBinding(int32 Index) const
{
	const auto* ActiveRow = ParentList ? Cast<UReciTwoLevelItemBase>(ParentList->GetHostedEntryButton(Index)) : nullptr;
	for (UReciTwoLevelListBinding* Binding : ChildBindings)
		if (Binding && Binding->ParentIndex == Index && ActiveRow && Binding->List == ActiveRow->ChildList) return Binding;
	return nullptr;
}

bool UReciTwoLevelListBase::SetFocusedParentIndex(int32 Index, bool bGiveFocus)
{
	if (!bConfigurationValid || !Items.IsValidIndex(Index)) return false;
	const bool bChanged = FocusedParentIndex != Index;
	if (bChanged) ++StateRevision;
	FocusedParentIndex = Index;
	ApplyState();
	if (bGiveFocus && ParentList && GetOwningLocalPlayer()) ParentList->SetFocusedItemIndex(Index);
	if (bChanged)
	{
		OnFocusedParentChanged.Broadcast(GetFocusedParent(), FocusedParentIndex);
		BroadcastPair();
	}
	return true;
}

bool UReciTwoLevelListBase::SetSelectedChildIndex(int32 Index)
{
	return SelectChild(FocusedParentIndex, Index, false);
}

void UReciTwoLevelListBase::SetSynchronizeChildSelection(bool bEnabled)
{
	if (bSynchronizeChildSelection == bEnabled) return;
	CancelGesture();
	bSynchronizeChildSelection = bEnabled;
	if (bEnabled) SharedChildIndex = FMath::Max(0, GetSelectedChildIndex());
	++StateRevision;
	ApplyState();
}

void UReciTwoLevelListBase::SynchronizeChildSelections()
{
	if (!bSynchronizeChildSelection) return;
	for (int32 Index = 0; Index < SelectedChildren.Num(); ++Index)
	{
		const int32 Selected = ChildCounts[Index] > 0
			? FMath::Clamp(SharedChildIndex, 0, ChildCounts[Index] - 1) : INDEX_NONE;
		if (SelectedChildren[Index] != Selected)
		{
			SelectedChildren[Index] = Selected;
			++StateRevision;
		}
	}
}

bool UReciTwoLevelListBase::SelectChild(int32 Parent, int32 Child, bool bGiveFocus)
{
	if (!bConfigurationValid || !IsValidChild(Parent, Child)) return false;
	const bool bParentChanged = FocusedParentIndex != Parent;
	const bool bChanged = bParentChanged || SelectedChildren[Parent] != Child
		|| (bSynchronizeChildSelection && SharedChildIndex != Child);
	if (bChanged) ++StateRevision;
	FocusedParentIndex = Parent;
	SelectedChildren[Parent] = Child;
	if (bSynchronizeChildSelection) SharedChildIndex = Child;
	ApplyState();
	if (bGiveFocus && ParentList && GetOwningLocalPlayer()) ParentList->SetFocusedItemIndex(Parent);
	if (bParentChanged) OnFocusedParentChanged.Broadcast(GetFocusedParent(), FocusedParentIndex);
	if (bChanged) BroadcastPair();
	return true;
}

void UReciTwoLevelListBase::BroadcastPair()
{
	OnSelectionChanged.Broadcast(GetFocusedParent(), FocusedParentIndex, GetSelectedChildIndex());
}

bool UReciTwoLevelListBase::ConfirmSelection()
{
	if (!bConfigurationValid || !GetFocusedParent() || !IsValidChild(FocusedParentIndex, GetSelectedChildIndex())) return false;
	if (ParentList && !ParentList->IsHostedItemNavigable(FocusedParentIndex)) return false;
	if (UReciListBase* Children = GetChildList(FocusedParentIndex))
		if (!Children->IsHostedItemNavigable(GetSelectedChildIndex())) return false;
	OnConfirmed.Broadcast(GetFocusedParent(), FocusedParentIndex, GetSelectedChildIndex());
	return true;
}

void UReciTwoLevelListBase::HandleSourceChanged(UReciTwoLevelListBinding* Source, int32 Parent, int32 Item, bool bGiveFocus)
{
	if (!bConfigurationValid || bBuilding || bApplyingState) return;
	if (Parent == INDEX_NONE)
	{
		if (Source == ParentBinding) SetFocusedParentIndex(Item, bGiveFocus);
		return;
	}
	if (!ChildBindings.Contains(Source) || !IsValidChild(Parent, Item)) return;
	if (!bGiveFocus && (Parent != FocusedParentIndex || Source != GetChildBinding(Parent)))
	{
		// A follower's old inertia must not overwrite the shared selection.
		if (bSynchronizeChildSelection)
		{
			ApplyState();
			return;
		}
		// A previous row's inertia must never steal the current parent's focus.
		if (SelectedChildren[Parent] != Item) ++StateRevision;
		SelectedChildren[Parent] = Item;
		ApplyState();
		return;
	}
	SelectChild(Parent, Item, bGiveFocus);
}

void UReciTwoLevelListBase::ApplyState()
{
	SynchronizeChildSelections();
	if (!bConfigurationValid || bBuilding || bApplyingState) return;
	TGuardValue<bool> Guard(bApplyingState, true);
	// Blueprint design events may replace data or select another pair. Reapply
	// the new state instead of continuing to send the old state to new entries.
	for (int32 Pass = 0; Pass < 16; ++Pass)
	{
		const uint32 Revision = StateRevision;
		if (ParentList)
		{
			ParentBinding->ApplyPointerHover(bAllowPointerHover);
			if (ParentList->GetDrivenItemIndex() != FocusedParentIndex)
				ParentList->SetDrivenItemIndex(FocusedParentIndex, false);
			ParentList->SetInitialFocusItemIndex(FocusedParentIndex);
			ParentBinding->UpdateDesignState(FocusedParentIndex, INDEX_NONE);
		}
		if (Revision != StateRevision) continue;
		const auto Bindings = ChildBindings;
		for (UReciTwoLevelListBinding* Binding : Bindings)
		{
			if (Revision != StateRevision) break;
			if (!Binding || !Binding->List || !SelectedChildren.IsValidIndex(Binding->ParentIndex)) continue;
			auto* Source = Binding->List.Get();
			const int32 Selected = SelectedChildren[Binding->ParentIndex];
			Binding->ApplyPointerHover(bAllowPointerHover);
			if (Source->GetDrivenItemIndex() != Selected) Source->SetDrivenItemIndex(Selected, false);
			Source->SetSelectedItemIndex(Selected, false);
			Binding->UpdateDesignState(FocusedParentIndex, Selected);
		}
		if (Revision == StateRevision) return;
	}
	UE_LOG(LogReciTwoLevelList, Warning, TEXT("Design callbacks repeatedly changed selection. Stopped recursive state updates."));
}

bool UReciTwoLevelListBase::NavigateAxis(bool bParentAxis, int32 Step)
{
	if (!bConfigurationValid) return false;
	const int32 Count = bParentAxis ? Items.Num()
		: (ChildCounts.IsValidIndex(FocusedParentIndex) ? ChildCounts[FocusedParentIndex] : 0);
	if (!Count) return false;
	const int32 Current = bParentAxis ? FocusedParentIndex : GetSelectedChildIndex();
	const auto* ChildBinding = GetChildBinding(FocusedParentIndex);
	const auto* Layout = bParentAxis ? ParentLayout.Get() : (ChildBinding ? ChildBinding->Layout.Get() : nullptr);
	UReciListBase* Source = bParentAxis ? GetParentList() : GetChildList(FocusedParentIndex);
	int32 Next = Current;
	for (int32 Attempt = 0; Attempt < Count; ++Attempt)
	{
		Next += Step;
		if (Layout && Layout->bWrapNavigation) Next = (Next % Count + Count) % Count;
		else if (Next < 0 || Next >= Count) return false;
		if (Next == Current) return false;
		if (Source && !Source->IsHostedItemNavigable(Next)) continue;
		return bParentAxis ? SetFocusedParentIndex(Next) : SetSelectedChildIndex(Next);
	}
	return false;
}

bool UReciTwoLevelListBase::Navigate(EReciNavigationDirection Direction)
{
	const bool bVertical = Direction == EReciNavigationDirection::Up || Direction == EReciNavigationDirection::Down;
	const int32 Step = Direction == EReciNavigationDirection::Up || Direction == EReciNavigationDirection::Left ? -1 : 1;
	return NavigateAxis(bVertical == (ParentOrientation == Orient_Vertical), Step);
}

void UReciTwoLevelListBase::RefreshPresentation()
{
	if (!WidgetTree || bBuilding) return;
	++StateRevision;
	const bool bHadFocus = HasAnyUserFocus() || HasFocusedDescendants();
	BuildPresentation();
	if (bHadFocus && ParentList) ParentList->SetFocusedItemIndex(FocusedParentIndex);
}

void UReciTwoLevelListBase::NativeOnInitialized()
{
	ParentList = WidgetTree ? Cast<UReciListBase>(WidgetTree->FindWidget(TEXT("ParentList"))) : nullptr;
	Super::NativeOnInitialized();
}

#if WITH_EDITOR
const FText UReciTwoLevelListBase::GetPaletteCategory()
{
	return NSLOCTEXT("ReciCommonUI", "PaletteCategory", "RECI COMMON UI");
}
#endif

void UReciTwoLevelListBase::DetachBindings()
{
	bConfigurationValid = false;
	CancelGesture();
	for (UReciTwoLevelListBinding* Binding : ChildBindings) if (Binding) Binding->Detach();
	ChildBindings.Reset();
	if (ParentBinding) ParentBinding->Detach();
	ParentBinding = nullptr;
	ParentLayout = nullptr;
	ParentViewport = nullptr;
	bConfigurationValid = false;
}

void UReciTwoLevelListBase::BuildPresentation()
{
	{
		TGuardValue<bool> Guard(bBuilding, true);
		DetachBindings();
		ParentList = WidgetTree ? Cast<UReciListBase>(WidgetTree->FindWidget(TEXT("ParentList"))) : nullptr;
		if (!ParentList)
		{
			RECI_ERROR_ONCE(TEXT("TwoLevelParentList"), GetClass()->GetPathName(),
				TEXT("%s requires a ReciList named ParentList in its WBP. Two-level navigation disabled."), *GetName());
			return;
		}
		ParentBinding = NewObject<UReciTwoLevelListBinding>(this);
		if (!ParentBinding->Initialize(this, ParentList, INDEX_NONE)) return;
		ParentLayout = ParentBinding->Layout;
		ParentViewport = ParentBinding->Viewport;
		ParentOrientation = ParentLayout->GetHostedScrollOrientation();
		TArray<UObject*> Parents;
		for (UObject* Parent : Items) Parents.Add(Parent);
		ParentList->SetListItems(Parents);
		if (ParentList->GetHostedItemCount() != Items.Num())
		{
			RECI_ERROR_ONCE(TEXT("TwoLevelParentEntries"), GetClass()->GetPathName(),
				TEXT("%s: ParentList failed to create every entry. Two-level navigation disabled."), *GetName());
			return;
		}
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			for (UUserWidget* Presentation : ParentList->GetEntryPresentations(Index))
			{
				auto* Row = Cast<UReciTwoLevelItemBase>(Presentation);
				if (!Row) return;
				Row->TakeWidget();
				Row->ChildList = Row->WidgetTree ? Cast<UReciListBase>(Row->WidgetTree->FindWidget(TEXT("ChildList"))) : nullptr;
				if (!Row->ChildList)
				{
					RECI_ERROR_ONCE(TEXT("TwoLevelChildList"), GetClass()->GetPathName() + TEXT(" ") + Row->GetClass()->GetPathName(),
						TEXT("%s: parent entry %s requires a ReciList named ChildList. Two-level navigation disabled."), *GetName(), *GetNameSafe(Row));
					return;
				}
				auto* Binding = NewObject<UReciTwoLevelListBinding>(this);
				ChildBindings.Add(Binding);
				if (!Binding->Initialize(this, Row->ChildList, Index)) return;
				if (Binding->Layout->GetHostedScrollOrientation() == ParentOrientation)
				{
					RECI_ERROR_ONCE(TEXT("TwoLevelSameAxis"), GetClass()->GetPathName() + TEXT(" ") + Row->GetClass()->GetPathName(),
						TEXT("%s: ParentList and ChildList in %s have the same direction. Use perpendicular Reci layout boxes. Two-level navigation disabled."), *GetName(), *GetNameSafe(Row));
					return;
				}
				TArray<UObject*> Children;
				Children.Init(Items[Index], ChildCounts[Index]);
				Row->ChildList->SetListItems(Children);
				if (Row->ChildList->GetHostedItemCount() != Children.Num())
				{
					RECI_ERROR_ONCE(TEXT("TwoLevelChildEntries"), GetClass()->GetPathName(),
						TEXT("%s: ChildList failed to create every child."), *GetName());
					return;
				}
				for (int32 Child = 0; Child < Children.Num(); ++Child)
					if (!Row->ChildList->GetHostedEntryButton(Child))
					{
						RECI_ERROR_ONCE(TEXT("TwoLevelChildButton"), GetClass()->GetPathName(),
							TEXT("%s: child entries must expose a CommonButtonBase."), *GetName());
						return;
					}
			}
		}
		// Parent presentations were initially measured before their ChildLists
		// received data. Remeasure without modifying any authored layout settings.
		ParentList->ForceLayoutPrepass();
		if (auto* Advanced = Cast<UReciAdvancedList>(ParentList)) Advanced->RefreshAdvancedListPresentationSizes();
		bConfigurationValid = true;
	}
	ApplyState();
}

void UReciTwoLevelListBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (!IsDesignTime() && !ParentBinding) RefreshPresentation();
	if (!IsDesignTime()) FReciInputFocusCoordinator::Get().RegisterTarget(this, this);
}

void UReciTwoLevelListBase::NativeDestruct()
{
	CancelGesture();
	FReciInputFocusCoordinator::Get().UnregisterTarget(this);
	DetachBindings();
	Super::NativeDestruct();
}

void UReciTwoLevelListBase::ReleaseSlateResources(bool bReleaseChildren)
{
	CancelGesture();
	FReciInputFocusCoordinator::Get().UnregisterTarget(this);
	DetachBindings();
	Super::ReleaseSlateResources(bReleaseChildren);
}

FReply UReciTwoLevelListBase::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
	if (bConfigurationValid && ParentList)
		if (UWidget* Target = ParentList->GetHostedEntryButton(FocusedParentIndex))
			return FReply::Handled().SetUserFocus(Target->TakeWidget(), Event.GetCause());
	return Super::NativeOnFocusReceived(Geometry, Event);
}

int32 UReciTwoLevelListBase::GetReciInputUserIndex() const
{
	if (const auto* Player = GetOwningLocalPlayer())
	{
		if (auto User = Player->GetSlateUser()) return User->GetUserIndex();
		return Player->GetLocalPlayerIndex();
	}
	return 0;
}

bool UReciTwoLevelListBase::CanHandleReciFocusedInput() const
{
	return HasAnyUserFocus() || HasFocusedDescendants();
}

bool UReciTwoLevelListBase::HandleReciNavigation(EUINavigation Direction)
{
	// As any Reci list: a direction it cannot move in (an end its box does not wrap, or an invalid setup) goes on to
	// Slate navigation. Child buttons take no keyboard focus, so Slate never lands inside a row.
	switch (Direction)
	{
	case EUINavigation::Up: return Navigate(EReciNavigationDirection::Up);
	case EUINavigation::Down: return Navigate(EReciNavigationDirection::Down);
	case EUINavigation::Left: return Navigate(EReciNavigationDirection::Left);
	case EUINavigation::Right: return Navigate(EReciNavigationDirection::Right);
	default: return false;
	}
}

int32 UReciTwoLevelListBase::HitParent(const FVector2D& Position) const
{
	return ParentLayout ? ParentLayout->FindHostedItemIndexAtScreenPosition(Position) : INDEX_NONE;
}

int32 UReciTwoLevelListBase::HitChild(int32 Parent, const FVector2D& Position) const
{
	const auto* Binding = GetChildBinding(Parent);
	if (!Binding || !Binding->Layout) return INDEX_NONE;
	if (Binding->Viewport && !Binding->Viewport->GetCachedGeometry().IsUnderLocation(Position)) return INDEX_NONE;
	return Binding->Layout->FindHostedItemIndexAtScreenPosition(Position);
}

bool UReciTwoLevelListBase::HandleReciMouseWheel(float Delta, const FVector2D& Position)
{
	if (!bConfigurationValid) return false;
	if (ParentViewport) return ParentViewport->RouteExternalMouseWheel(Delta, Position);
	if (!FMath::IsNearlyZero(WheelAccumulator) && FMath::Sign(Delta) != FMath::Sign(WheelAccumulator)) WheelAccumulator = 0;
	WheelAccumulator += Delta;
	const int32 Steps = FMath::TruncToInt(WheelAccumulator);
	WheelAccumulator -= Steps;
	for (int32 Index = 0; Index < FMath::Abs(Steps); ++Index) NavigateAxis(true, Steps > 0 ? -1 : 1);
	return !Items.IsEmpty();
}

bool UReciTwoLevelListBase::HandleReciPointerDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bConfigurationValid || PointerIndex != INDEX_NONE || Items.IsEmpty()) return false;
	PressedParent = HitParent(Event.GetScreenSpacePosition());
	if (PressedParent == INDEX_NONE) return false;
	PressedChild = HitChild(PressedParent, Event.GetScreenSpacePosition());
	PressPosition = Event.GetScreenSpacePosition();
	PointerIndex = Event.GetPointerIndex();
	bGestureStarted = false;
	return true;
}

bool UReciTwoLevelListBase::HandleReciPointerMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (PointerIndex != Event.GetPointerIndex()) return false;
	const FVector2D Delta = Event.GetScreenSpacePosition() - PressPosition;
	if (!bGestureStarted && Delta.Size() >= FMath::Max(1.0f, DragTriggerDistance))
	{
		bGestureStarted = true;
		SetFocusedParentIndex(PressedParent);
		const bool bVertical = FMath::Abs(Delta.Y) >= FMath::Abs(Delta.X);
		const bool bParentAxis = bVertical == (ParentOrientation == Orient_Vertical);
		GestureViewport = bParentAxis ? ParentViewport
			: (GetChildBinding(PressedParent) ? GetChildBinding(PressedParent)->Viewport : nullptr);
		if (GestureViewport) GestureViewport->BeginExternalDrag(PointerIndex, PressPosition);
		else NavigateAxis(bParentAxis, (bVertical ? Delta.Y : Delta.X) > 0 ? -1 : 1);
	}
	if (GestureViewport) GestureViewport->UpdateExternalDrag(PointerIndex, Event.GetScreenSpacePosition());
	return true;
}

bool UReciTwoLevelListBase::HandleReciPointerUp(const FPointerEvent& Event)
{
	if (PointerIndex != Event.GetPointerIndex()) return false;
	const int32 Parent = PressedParent;
	const int32 Child = PressedChild;
	const bool bClick = !bGestureStarted;
	if (GestureViewport) GestureViewport->EndExternalDrag(PointerIndex);
	GestureViewport = nullptr;
	PointerIndex = INDEX_NONE;
	PressedParent = PressedChild = INDEX_NONE;
	bGestureStarted = false;
	if (bClick && Parent == HitParent(Event.GetScreenSpacePosition()))
	{
		if (Child != INDEX_NONE && Child == HitChild(Parent, Event.GetScreenSpacePosition())) SelectChild(Parent, Child);
		else if (Child == INDEX_NONE && HitChild(Parent, Event.GetScreenSpacePosition()) == INDEX_NONE) SetFocusedParentIndex(Parent);
	}
	return true;
}

void UReciTwoLevelListBase::CancelGesture()
{
	if (GestureViewport) GestureViewport->CancelExternalDrag();
	GestureViewport = nullptr;
	PointerIndex = PressedParent = PressedChild = INDEX_NONE;
	bGestureStarted = false;
}

void UReciTwoLevelListBase::HandleReciPointerHover(const FVector2D& Position)
{
	if (bAllowPointerHover) SetFocusedParentIndex(HitParent(Position));
}
