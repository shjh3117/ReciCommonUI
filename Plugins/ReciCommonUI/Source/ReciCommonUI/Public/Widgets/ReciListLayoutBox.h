// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "Components/ContentWidget.h"
#include "CoreMinimal.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Widgets/ReciListPanel.h"

#include "ReciListLayoutBox.generated.h"

class SBox;
class UReciListBase;
class UReciScrollBox;

/** One-child layout policy host for a Reci list source. */
UCLASS(
	Abstract,
	BlueprintType,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci List Layout Box Base",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciListLayoutBoxBase
	: public UContentWidget,
	  public IReciInputFocusTarget
{
	GENERATED_BODY()

public:
	UReciListLayoutBoxBase(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		BlueprintSetter = SetItemSpacing,
		Category = "Layout",
		meta = (ClampMin = "0.0"))
	float ItemSpacing = 0.0f;

	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|List Layout")
	void SetItemSpacing(float InItemSpacing);

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List Layout")
	UReciListBase* GetListSource() const;

	/** Whether the hosted list permits pointer-hover interaction. */
	bool AllowsPointerHover() const;

	/**
	 * Off: the box stops taking routed input itself and keeps its layout, for a composite that routes the input of all
	 * the lists in it once (as UReciScrollBox::SetInputRoutingEnabled).
	 */
	void SetInputRoutingEnabled(bool bEnabled);
	bool IsInputRoutingEnabled() const { return bInputRoutingEnabled; }

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List Layout")
	FReciListLayoutSettings GetHostedLayoutSettings() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	bool bWrapNavigation = false;

	int32 GetHostedItemCount() const;
	UWidget* GetHostedEntryWidget(int32 ItemIndex) const;
	/** The hosted item whose presentation (any copy) is under ScreenPosition, or INDEX_NONE. */
	int32 FindHostedItemIndexAtScreenPosition(
		const FVector2D& ScreenPosition) const;
	bool RouteNavigationInput(EUINavigation Direction);
	bool NavigateHostedList(EUINavigation Direction);
	bool ConfirmHostedList();
	bool FocusHostedItem(int32 ItemIndex);
	/** Makes an interactable hosted item the driven item, as navigation does (with focus while Focus On Navigation is on). */
	bool DriveHostedItem(int32 ItemIndex);
	bool ClickHostedItem(int32 ItemIndex);
	bool FocusDrivenItem();
	EOrientation GetHostedScrollOrientation() const;
	bool HasNavigationInputHandler() const;
	bool IsCrossAxisNavigation(EUINavigation Direction) const;
	EUINavigation ResolvePointerNavigation(
		const FVector2D& ScreenDelta,
		float TriggerDistance) const;
	bool BroadcastNavigationInput(EUINavigation Direction);
	void AttachViewport(UReciScrollBox* InViewport);
	void DetachViewport(UReciScrollBox* InViewport);
	UReciScrollBox* GetAttachedViewport() const;

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;
	virtual FReciListLayoutSettings BuildLayoutSettings() const PURE_VIRTUAL(
		UReciListLayoutBoxBase::BuildLayoutSettings,
		return FReciListLayoutSettings(););
	void ApplyHostedLayout();

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	void BindListSource();
	void UnbindListSource();
	void HandleEntriesChanged();
	void HandleFocusedIndexChanged(int32 ItemIndex);
	void HandleDrivenIndexChanged(int32 ItemIndex);
	void RefreshSlateContent();
	void UpdateInputTargetRegistration();
	bool HasHostedInteractionFocus() const;
	int32 FindDirectionalTileItemIndex(EUINavigation Direction) const;
	int32 FindWrappedTileItemIndex(EUINavigation Direction) const;
	bool TryGetHostedItemCenter(
		int32 ItemIndex,
		FVector2D& OutCenter) const;
	bool IsHostedItemInteractable(int32 ItemIndex) const;
	FReply HandleSlateFocusReceived(const FFocusEvent& FocusEvent);

	virtual const UWidget* GetReciInputSurfaceWidget() const override;
	virtual int32 GetReciInputUserIndex() const override;
	virtual bool CanHandleReciFocusedInput() const override;
	virtual bool HandleReciNavigation(EUINavigation Direction) override;
	virtual bool HandleReciAccept() override;
	virtual bool HandleReciMouseWheel(
		float WheelDelta,
		const FVector2D& ScreenPosition) override;
	virtual bool HandleReciPointerDown(
		const FGeometry& SurfaceGeometry,
		const FPointerEvent& PointerEvent) override;
	virtual bool HandleReciPointerMove(
		const FGeometry& SurfaceGeometry,
		const FPointerEvent& PointerEvent) override;
	virtual bool HandleReciPointerUp(
		const FPointerEvent& PointerEvent) override;
	virtual bool HasReciPointerCapture(int32 PointerIndex) const override;
	virtual void HandleReciPointerCaptureLost() override;
	virtual void HandleReciPointerHover(
		const FVector2D& ScreenPosition) override;
	virtual void ReturnReciFocusToDrivenItem() override;

	UPROPERTY(Transient)
	TObjectPtr<UReciListBase> BoundListSource;

	UPROPERTY(Transient)
	TObjectPtr<UReciScrollBox> AttachedViewport;

	TSharedPtr<SBox> MyContentBox;
	bool bInputTargetRegistered = false;
	bool bInputRoutingEnabled = true;
};

/** Applies vertical linear arrangement to a contained Reci list source. */
UCLASS(
	BlueprintType,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Vertical Box",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciVerticalBox final
	: public UReciListLayoutBoxBase
{
	GENERATED_BODY()

protected:
	virtual FReciListLayoutSettings BuildLayoutSettings() const override;
};

/** Applies horizontal linear arrangement to a contained Reci list source. */
UCLASS(
	BlueprintType,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Horizontal Box",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciHorizontalBox final
	: public UReciListLayoutBoxBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetFillEntries, Category = "Layout")
	bool bFillEntries = false;

	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|List Layout")
	void SetFillEntries(bool bInFillEntries);

protected:
	virtual FReciListLayoutSettings BuildLayoutSettings() const override;
};

/** Applies fixed-size wrapping tile arrangement to a Reci list source. */
UCLASS(
	BlueprintType,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Tile Box",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciTileBox final
	: public UReciListLayoutBoxBase
{
	GENERATED_BODY()

public:
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		BlueprintSetter = SetEntrySize,
		Category = "Tile",
		meta = (ClampMin = "1.0"))
	FVector2D EntrySize = FVector2D(128.0f, 128.0f);

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		BlueprintSetter = SetFixedColumns,
		Category = "Tile",
		meta = (ClampMin = "0"))
	int32 FixedColumns = 0;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		BlueprintSetter = SetFlowOrientation,
		Category = "Tile")
	TEnumAsByte<EOrientation> FlowOrientation = Orient_Horizontal;

	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|Tile Layout")
	void SetEntrySize(FVector2D InEntrySize);

	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|Tile Layout")
	void SetFixedColumns(int32 InFixedColumns);

	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|Tile Layout")
	void SetFlowOrientation(EOrientation InFlowOrientation);

protected:
	virtual FReciListLayoutSettings BuildLayoutSettings() const override;
};
