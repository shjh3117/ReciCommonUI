// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#pragma once
#include "CommonButtonBase.h"
#include "Widgets/ReciListEntryEmphasis.h"
#include "Widgets/ReciTwoLevelEntry.h"
#include "Widgets/ReciTwoLevelItemBase.h"
#include "Widgets/ReciTwoLevelListBase.h"
#include "ReciTwoLevelTestEntry.generated.h"

UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTwoLevelTestEntry : public UCommonButtonBase, public IReciTwoLevelEntry, public IReciListEntryEmphasis
{
	GENERATED_BODY()
public:
	virtual void SetChildItem_Implementation(UObject* Parent, int32 Index) override { Data = Parent; ChildIndex = Index; ++DataUpdates; }
	virtual void UpdateTwoLevelState_Implementation(bool, bool, int32, int32) override { ++StateUpdates; }
	virtual void OnItemEmphasisChanged_Implementation(float Emphasis) override { CarouselBlend = Emphasis; }
	UFUNCTION() int32 CountChildren(UObject* Parent) { return Counts.FindRef(Parent); }
	UFUNCTION() void RecordPair(UObject* Parent, int32 ParentIndex, int32 Index) { ++PairEvents; Data = Parent; ChildIndex = Index; }
	UPROPERTY() TObjectPtr<UObject> Data;
	TMap<UObject*, int32> Counts;
	int32 ChildIndex = INDEX_NONE;
	int32 DataUpdates = 0;
	int32 StateUpdates = 0;
	int32 PairEvents = 0;
	/** The last emphasis the entry received; -1 before any. */
	float CarouselBlend = -1.0f;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTwoLevelTestRow : public UReciTwoLevelItemBase
{
	GENERATED_BODY()
public:
	static EOrientation ChildAxis;
	static int32 ViewportMode;
	static bool bAdvanced;
	static bool bOmitChild;
	/** Turns on the ChildList's Looping Entries, which the two-level list ignores. */
	static bool bLoopingChild;
	/**
	 * Builds the row's tree at Initialize, as a Widget Blueprint's row has it, so that CommonUI wraps it in its internal
	 * button and focus on the row reaches the row's focus events, and with them ParentList. Off, the tree is built when
	 * the row's Slate widget is.
	 */
	static bool bTreeAtInitialize;
protected:
	virtual bool Initialize() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	void BuildTree();
};

UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTwoLevelTestList : public UReciTwoLevelListBase
{
	GENERATED_BODY()
public:
	void BuildAuthoredTree(EOrientation Axis, int32 ViewportMode, bool bAdvanced);
};
