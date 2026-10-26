// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciList.h"

#include "Blueprint/WidgetTree.h"
#include "Widgets/ReciListPanel.h"
#include "CommonButtonBase.h"
#include "ReciReport.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciScrollBox.h"
#include "Widgets/ReciWidgetTreeUtils.h"
#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciList)

namespace
{
	const FName ItemsBoxWidgetName(TEXT("ItemsBox"));
}

void UReciList::NativeOnInitialized()
{
	ResolveItemsBox();
	Super::NativeOnInitialized();
}

TSharedRef<SWidget> UReciList::RebuildWidget()
{
	if (!ItemsBox) ResolveItemsBox();
	return Super::RebuildWidget();
}

void UReciList::ResolveItemsBox()
{
	// A Blueprint binds ItemsBox before initialisation; the compiler requires it.
	if (ItemsBox || !WidgetTree)
	{
		return;
	}
	if (!WidgetTree->RootWidget)
	{
		// A native instance, such as a Reci List placed in a screen: its panel is its root.
		ItemsBox = WidgetTree->ConstructWidget<UReciListPanel>(
			UReciListPanel::StaticClass(),
			ItemsBoxWidgetName);
		WidgetTree->RootWidget = ItemsBox;
		return;
	}
	// Only a native subclass that builds its own tree gets here. Replacing its root would discard that tree.
	RECI_ERROR_ONCE(TEXT("ItemsBox"), GetClass()->GetPathName(),
		TEXT("%s builds its own widget tree without setting ItemsBox to a Reci List Panel in it, so it shows no entries."),
		*GetClass()->GetName());
}

#if WITH_EDITOR
void UReciList::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);
	if (bLoopingEntries && GetParent() && GetParent()->IsA<UReciTileBox>())
	{
		CompileLog.Warning(FText::FromString(FString::Printf(
			TEXT("%s: Looping Entries has no effect in a Reci Tile Box."), *GetName())));
	}
	else if (bLoopingEntries && !HasAnyFlags(RF_ClassDefaultObject) && !IsHostedByLoopingViewport())
	{
		// A Blueprint derived from Reci List leaves the placement to the places it is used.
		CompileLog.Warning(FText::FromString(FString::Printf(
			TEXT("%s: Looping Entries has no effect outside a scroll box that loops its entries, so the list shows each item once."),
			*GetName())));
	}
	// A template in an authored tree has no tree of its own; its class's Blueprint has.
	if (const UWidgetTree* Tree = UE::ReciCommonUI::Private::GetAuthoredWidgetTree(GetClass()))
	{
		UE::ReciCommonUI::Private::ValidateHostedItemsBox(*this, *Tree, CompileLog);
	}
}
#endif

bool UReciList::IsHostedByLoopingViewport() const
{
	// The viewport hosts the layout box alone, and the box hosts the list (DESIGN.md principle 7).
	const UReciListLayoutBoxBase* Layout = Cast<UReciListLayoutBoxBase>(GetParent());
	const UReciScrollBox* Viewport = Layout ? Cast<UReciScrollBox>(Layout->GetParent()) : nullptr;
	return Viewport && Viewport->LoopsHostedEntries() && Viewport->GetHostedLayout() == Layout;
}

UPanelWidget* UReciList::GetGeneratedItemsPanel() const
{
	return ItemsBox;
}

void UReciList::NativeOnListItemsRebuilt()
{
	Super::NativeOnListItemsRebuilt();
	LoopEntries.Reset();ActiveLoopEntries.Reset();LoopCycles=0;
	if(ItemsBox)ItemsBox->SetRepeatedItemCount(0);
	if(!ItemsBox || !bLoopingEntries)return;
	if(GetParent() && GetParent()->IsA<UReciTileBox>())
	{
		RECI_WARN_ONCE(TEXT("LoopingTile"), UE::ReciCommonUI::Private::GetAuthoredPath(*this),
			TEXT("%s: Looping Entries has no effect in a Reci Tile Box."), *GetName());
		return;
	}
	// A two-level list's binding reports the Looping Entries it ignores.
	if(GetListController())return;
	if(!IsHostedByLoopingViewport())
	{
		// Only a viewport that loops its entries moves the copies, so without one the list shows each item once.
		RECI_WARN_ONCE(TEXT("LoopingOutsideViewport"), UE::ReciCommonUI::Private::GetAuthoredPath(*this),
			TEXT("%s: Looping Entries has no effect outside a scroll box that loops its entries, so the list shows each item once."), *GetName());
		return;
	}
	if(GetHostedItemCount()>1)BuildLoopPresentations(3);
}

void UReciList::NativeOnItemFocusReceived(UCommonButtonBase* FocusedButton)
{
	Super::NativeOnItemFocusReceived(FocusedButton);
	// Independent screen selectors deliberately keep their cursor separate from Slate focus.
	if (bFocusOnNavigation && !GetListController())
	{
		SetDrivenItemIndex(GetItemButtonIndex(FocusedButton), false);
	}
}

void UReciList::BuildLoopPresentations(int32 CycleCount)
{
	if(!ItemsBox)return;
	const int32 Count=GetHostedItemCount();
	ItemsBox->SetRepeatedItemCount(Count);
	TArray<UWidget*> Originals;
	for(int32 I=0;I<Count;++I)
	{
		auto* Original=Super::GetHostedEntryWidget(I);Originals.Add(Original);
		ReplaceEntrySelectionButton(I,ResolveEntrySelectionButton(Original,I));
	}
	ItemsBox->ClearChildren();LoopEntries.Reset();ActiveLoopEntries.Reset();LoopCycles=CycleCount;
	for(int32 Cycle=0;Cycle<CycleCount;++Cycle)for(int32 I=0;I<Count;++I)
	{
		auto* Entry=Cycle==CycleCount/2?Originals[I]:NativeCreateEntryWidget(GetItemAt(I),I);
		if(!Entry)
		{
			ItemsBox->ClearChildren();LoopEntries.Reset();ActiveLoopEntries.Reset();LoopCycles=0;
			ItemsBox->SetRepeatedItemCount(0);
			for(auto* Original:Originals)ItemsBox->AddChild(Original);
			return;
		}
		ItemsBox->AddChild(Entry);LoopEntries.Add(Entry);
		if(auto* Button=ResolveEntrySelectionButton(Entry,I))
		{
			// Only the nearest occurrence is registered with the logical selection.
			// A pointer can also click another visible copy in a short loop.
			Button->OnClicked().AddWeakLambda(this,[this,Weak=TWeakObjectPtr<UCommonButtonBase>(Button),I]()
			{
				if(auto* Clicked=Weak.Get())if(GetHostedEntryButton(I)!=Clicked)
				{Clicked->ClearSelection();HandleHostedItemClick(I);}
			});
		}
	}
	for(int32 I=0;I<Count;++I)ActiveLoopEntries.Add((CycleCount/2)*Count+I);
	InvalidateLayoutAndVolatility();
}

UWidget* UReciList::GetHostedEntryWidget(int32 ItemIndex) const
{
	if(ActiveLoopEntries.IsValidIndex(ItemIndex))
		if(LoopEntries.IsValidIndex(ActiveLoopEntries[ItemIndex]))return LoopEntries[ActiveLoopEntries[ItemIndex]];
	return Super::GetHostedEntryWidget(ItemIndex);
}

TArray<UUserWidget*> UReciList::GetEntryPresentations(int32 Index) const
{
	if(LoopEntries.IsEmpty())return Super::GetEntryPresentations(Index);
	TArray<UUserWidget*> Result;const int32 Count=GetHostedItemCount();
	if(Index<0||Index>=Count)return Result;
	for(int32 I=Index;I<LoopEntries.Num();I+=Count)if(auto* Entry=Cast<UUserWidget>(LoopEntries[I]))Result.Add(Entry);
	return Result;
}

FGeometry UReciList::GetLinearEntryGeometry(int32 PresentationIndex) const
{
	if(!ItemsBox||PresentationIndex<0||PresentationIndex>=ItemsBox->GetChildrenCount())return FGeometry();
	const FGeometry& Panel=ItemsBox->GetCachedGeometry();
	if(Panel.GetLocalSize().IsNearlyZero())return FGeometry();
	const auto Settings=ItemsBox->GetLayoutSettings();const bool Vertical=Settings.Mode==EReciListLayoutMode::Vertical;
	float Offset=0;
	for(int32 P=0;P<PresentationIndex;++P)if(ItemsBox->GetChildAt(P)->GetVisibility()!=ESlateVisibility::Collapsed)
	{
		const auto Size=ItemsBox->GetLinearChildSize(P);Offset+=(Vertical?Size.Y:Size.X)+Settings.ItemSpacing;
	}
	const auto Size=ItemsBox->GetLinearChildSize(PresentationIndex);
	return Panel.MakeChild(Vertical?FVector2D(Panel.GetLocalSize().X,Size.Y):FVector2D(Size.X,Panel.GetLocalSize().Y),FSlateLayoutTransform(Vertical?FVector2D(0,Offset):FVector2D(Offset,0)));
}

FGeometry UReciList::GetHostedEntryGeometry(int32 ItemIndex) const
{
	if(ActiveLoopEntries.IsValidIndex(ItemIndex))return GetLinearEntryGeometry(ActiveLoopEntries[ItemIndex]);
	// Offscreen entries are not painted, so their cached geometry can describe
	// an earlier offset. Finite lists need the same current panel arrangement.
	if(ItemsBox&&ItemsBox->GetChildrenCount()==GetHostedItemCount()&&ItemsBox->GetLayoutSettings().Mode!=EReciListLayoutMode::Tile)
		return GetLinearEntryGeometry(ItemIndex);
	return Super::GetHostedEntryGeometry(ItemIndex);
}

FGeometry UReciList::GetEntryPresentationGeometry(UUserWidget* Presentation) const
{
	const int32 Index=ItemsBox&&ItemsBox->GetLayoutSettings().Mode!=EReciListLayoutMode::Tile?ItemsBox->GetChildIndex(Presentation):INDEX_NONE;
	return Index!=INDEX_NONE?GetLinearEntryGeometry(Index):Super::GetEntryPresentationGeometry(Presentation);
}

void UReciList::RefreshLoopPresentations(const FGeometry& Viewport,float OffsetShift)
{
	const int32 Count=GetHostedItemCount();if(LoopEntries.IsEmpty()||Count<=1)return;
	const bool Vertical=ItemsBox->GetLayoutSettings().Mode==EReciListLayoutMode::Vertical;
	const FVector2D Center=Viewport.GetLocalSize()*.5f;
	bool Changed=false;
	for(int32 I=0;I<Count;++I)
	{
		int32 Closest=ActiveLoopEntries[I];float Best=TNumericLimits<float>::Max();
		for(int32 P=I;P<LoopEntries.Num();P+=Count)
		{
			const auto Geometry=GetLinearEntryGeometry(P);if(Geometry.GetLocalSize().IsNearlyZero())continue;
			const FVector2D Position=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f));
			const float Distance=FMath::Abs((Vertical?Position.Y-Center.Y:Position.X-Center.X)-OffsetShift);
			if(Distance<Best){Best=Distance;Closest=P;}
		}
		if(Closest!=ActiveLoopEntries[I])
		{
			Changed=true;
			UWidget* Previous=LoopEntries.IsValidIndex(ActiveLoopEntries[I])?LoopEntries[ActiveLoopEntries[I]].Get():nullptr;
			ActiveLoopEntries[I]=Closest;
			ReplaceEntrySelectionButton(I,ResolveEntrySelectionButton(LoopEntries[Closest],I));
			NativeOnLoopEntryReassigned(I,Previous,LoopEntries[Closest]);
		}
	}
	if(!Changed)return;
	for(int32 P=0;P<LoopEntries.Num();++P)
		if(auto* Button=ResolveEntrySelectionButton(LoopEntries[P],P%Count))if(Button!=GetHostedEntryButton(P%Count))
		{
			Button->OnClicked().RemoveAll(this);
			Button->OnClicked().AddWeakLambda(this,[this,Weak=TWeakObjectPtr<UCommonButtonBase>(Button),Index=P%Count]()
			{if(auto* Clicked=Weak.Get()){Clicked->ClearSelection();HandleHostedItemClick(Index);}});
		}
}

UWidget* UReciList::GetLoopAnchorEntry() const
{
	const int32 Index=(LoopCycles/2)*GetHostedItemCount();
	return LoopEntries.IsValidIndex(Index)?LoopEntries[Index]:nullptr;
}

FGeometry UReciList::GetLoopAnchorGeometry() const
{
	return GetLinearEntryGeometry((LoopCycles/2)*GetHostedItemCount());
}

float UReciList::GetLoopCycleExtent(EOrientation Orientation) const
{
	if(!ItemsBox||LoopCycles<3)return 0;
	const FVector2D Size=ItemsBox->GetDesiredSize();
	if((Orientation==Orient_Vertical?Size.Y:Size.X)<=0)return 0;
	return ((Orientation==Orient_Vertical?Size.Y:Size.X)+ItemsBox->GetLayoutSettings().ItemSpacing)/LoopCycles;
}

bool UReciList::EnsureLoopCoverage(float ViewportExtent,EOrientation Orientation)
{
	const float Cycle=GetLoopCycleExtent(Orientation);if(Cycle<=0)return false;
	const int32 Needed=FMath::Max(3,2*FMath::CeilToInt(.5f+ViewportExtent*.5f/Cycle)+1);
	if(Needed<=LoopCycles)return false;
	// The new copies replace every entry, as Set List Items does, and focus in the list returns as it does there.
	const bool bReturnFocus=HasOwningPlayerFocus();
	BuildLoopPresentations(Needed);
	if(bReturnFocus)ReturnFocusAfterRebuild();
	return true;
}
