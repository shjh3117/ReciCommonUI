// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonUserWidget.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Navigation/ReciNavigationTypes.h"
#include "ReciTwoLevelListBase.generated.h"

class UReciListBase;
class UReciListLayoutBoxBase;
class UReciScrollBox;
class UReciTwoLevelListBinding;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FReciTwoLevelParentEvent, UObject*, Parent, int32, ParentIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FReciTwoLevelPairEvent,
	UObject*, Parent, int32, ParentIndex, int32, ChildIndex);
DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(int32, FReciTwoLevelChildCount, UObject*, Parent);

/**
 * Behavior-only two-level list. WBP owns ParentList and each row owns ChildList.
 * Each list's hosts are its parents: its Reci Vertical Box or Reci Horizontal Box, and the Reci Scroll Box that holds
 * that box as its only child, if any. Axes come from those boxes; equal axes are a configuration error. The Widget
 * Blueprint compiler checks the placement, the entry classes and, through a row Blueprint, the axes, and warns about
 * a bound list's property the binding overwrites or ignores that the author changed from its default; native trees
 * and classes set at run time are checked when the list is built, once per class.
 * No native hierarchy, size, padding, alignment or motion defaults are imposed.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = UI, meta = (DisplayName = "Reci Two Level List Base"))
class RECICOMMONUIWIDGETS_API UReciTwoLevelListBase : public UCommonUserWidget, public IReciInputFocusTarget
{
	GENERATED_BODY()

public:
	/** Select the same child index in every row. Short rows clamp to their last child. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetSynchronizeChildSelection, Category = "Two Level List|Selection")
	bool bSynchronizeChildSelection = false;

	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|Two Level List")
	void SetSynchronizeChildSelection(bool bEnabled);

	/**
	 * Required WBP-authored list, directly inside a ReciVerticalBox or ReciHorizontalBox. While bound, the two-level
	 * list sets its Selection Mode to None, Clear Selection On Focus Return off, Allow Pointer Hover to this widget's
	 * Allow Pointer Hover and, on a Reci Advanced List, Main Presentation Follows Focus and Confirm Main Item On Click
	 * off, and restores them when it unbinds; it ignores Confirm On Item Click, Selection Follows Navigation and Looping
	 * Entries. Values the author changed from the default that are overwritten or ignored are reported.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Two Level List", meta = (BindWidget))
	TObjectPtr<UReciListBase> ParentList;

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Two Level List")
	bool IsConfigurationValid() const { return bConfigurationValid; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Two Level List|Input")
	bool bAllowPointerHover = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Two Level List|Input", meta = (ClampMin = "1"))
	float DragTriggerDistance = 10.0f;

	UPROPERTY(BlueprintAssignable, Category = "Two Level List|Events")
	FReciTwoLevelParentEvent OnFocusedParentChanged;

	/** Also emitted on parent changes, since the current pair has changed. */
	UPROPERTY(BlueprintAssignable, Category = "Two Level List|Events")
	FReciTwoLevelPairEvent OnSelectionChanged;

	/** Never emitted by child clicks or dragging; requires explicit Confirm. */
	UPROPERTY(BlueprintAssignable, Category = "Two Level List|Events")
	FReciTwoLevelPairEvent OnConfirmed;

	/** Parent objects only. The callback returns each parent's child count; no child objects are required. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Two Level List")
	void SetItems(const TArray<UObject*>& InItems, FReciTwoLevelChildCount InChildCountProvider);

	/** Requery child counts and recreate entries after the underlying parent data changes. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Two Level List")
	void RefreshItems();

	/** Apply edited design properties without changing the logical selection. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Two Level List")
	void RefreshPresentation();

	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Two Level List")
	bool SetFocusedParentIndex(int32 ParentIndex, bool bGiveFocus = true);

	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Two Level List")
	bool SetSelectedChildIndex(int32 ChildIndex);

	/** Clicking a child changes the parent and child in one transaction. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Two Level List")
	bool SelectChild(int32 ParentIndex, int32 ChildIndex, bool bGiveFocus = true);

	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Two Level List")
	bool Navigate(EReciNavigationDirection Direction);

	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Two Level List")
	bool ConfirmSelection();

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Two Level List")
	UObject* GetFocusedParent() const;

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Two Level List")
	int32 GetFocusedParentIndex() const { return FocusedParentIndex; }

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Two Level List")
	int32 GetSelectedChildIndex() const;

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Two Level List")
	UReciListBase* GetParentList() const;

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Two Level List")
	UReciListBase* GetChildList(int32 ParentIndex) const;

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	virtual void NativeOnInitialized() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }
	virtual FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	friend class UReciTwoLevelListBinding;
	friend class FReciTwoLevelListTest;
	void BuildPresentation();
	void DetachBindings();
	UReciTwoLevelListBinding* GetChildBinding(int32 ParentIndex) const;
	void ApplyState();
	void SynchronizeChildSelections();
	void BroadcastPair();
	void HandleSourceChanged(UReciTwoLevelListBinding* Source, int32 ParentIndex, int32 ItemIndex, bool bGiveFocus);
	bool NavigateAxis(bool bParentAxis, int32 Step);
	int32 HitParent(const FVector2D& Position) const;
	int32 HitChild(int32 ParentIndex, const FVector2D& Position) const;
	void CancelGesture();

	virtual const UWidget* GetReciInputSurfaceWidget() const override { return this; }
	virtual int32 GetReciInputUserIndex() const override;
	virtual bool CanHandleReciFocusedInput() const override;
	virtual bool HandleReciNavigation(EUINavigation Direction) override;
	virtual bool HandleReciAccept() override { ConfirmSelection(); return true; }
	virtual bool HandleReciMouseWheel(float Delta, const FVector2D& Position) override;
	virtual bool HandleReciPointerDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual bool HandleReciPointerMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual bool HandleReciPointerUp(const FPointerEvent& Event) override;
	virtual bool HasReciPointerCapture(int32 Index) const override { return PointerIndex == Index; }
	virtual void HandleReciPointerCaptureLost() override { CancelGesture(); }
	virtual void HandleReciPointerHover(const FVector2D& Position) override;
	virtual void ReturnReciFocusToDrivenItem() override {}

	bool IsValidChild(int32 ParentIndex, int32 ChildIndex) const;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Items;
	UPROPERTY(Transient) TArray<int32> ChildCounts;
	UPROPERTY(Transient) FReciTwoLevelChildCount ChildCountProvider;
	UPROPERTY(Transient) TArray<int32> SelectedChildren;
	UPROPERTY(Transient) TObjectPtr<UReciTwoLevelListBinding> ParentBinding;
	UPROPERTY(Transient) TObjectPtr<UReciListLayoutBoxBase> ParentLayout;
	UPROPERTY(Transient) TObjectPtr<UReciScrollBox> ParentViewport;
	UPROPERTY(Transient) TArray<TObjectPtr<UReciTwoLevelListBinding>> ChildBindings;
	UPROPERTY(Transient) TObjectPtr<UReciScrollBox> GestureViewport;
	int32 FocusedParentIndex = INDEX_NONE;
	int32 SharedChildIndex = 0;
	int32 PointerIndex = INDEX_NONE;
	int32 PressedParent = INDEX_NONE;
	int32 PressedChild = INDEX_NONE;
	FVector2D PressPosition = FVector2D::ZeroVector;
	float WheelAccumulator = 0.0f;
	bool bGestureStarted = false;
	bool bApplyingState = false;
	bool bBuilding = false;
	bool bConfigurationValid = false;
	EOrientation ParentOrientation = Orient_Vertical;
	uint32 StateRevision = 0;
};
