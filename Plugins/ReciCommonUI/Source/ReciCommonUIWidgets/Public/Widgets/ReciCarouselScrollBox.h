// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Motion/ReciMotion.h"
#include "Widgets/ReciScrollBox.h"

#include "ReciCarouselScrollBox.generated.h"

class FActiveTimerHandle;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FReciCarouselCenteredItemEvent,
	UObject*,
	Item,
	int32,
	ItemIndex);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FReciCarouselCenteredIndexChangedNative,
	int32);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FReciCarouselItemPresentationNative,
	int32,
	float);

/**
 * A scroll viewport that adds centered carousel motion to the hosted entries
 * of its single Reci list-layout child.
 *
 * ReciScrollBox owns wheel, touch, drag and inertia routing. This class adds
 * discrete wheel navigation, center snapping and a continuous center-distance
 * presentation blend. Item data, selection, focus and confirmation remain in
 * the contained Reci list source.
 */
UCLASS(
	BlueprintType,
	Blueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Carousel Scroll Box",
		Category = "Reci Common UI"))
class RECICOMMONUIWIDGETS_API UReciCarouselScrollBox : public UReciScrollBox
{
	GENERATED_BODY()

public:
	UReciCarouselScrollBox(const FObjectInitializer& ObjectInitializer);

	/** Snaps the nearest hosted item to the viewport center after scrolling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carousel|Snap")
	bool bSnapToClosestItem = true;

	/** Idle time before automatic snap begins. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Carousel|Snap",
		meta = (ClampMin = "0.0", Units = "s"))
	float SnapDelay = 0.12f;

	/**
	 * Distance from the viewport center where the presentation blend reaches
	 * zero. Zero uses half of the viewport extent.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Carousel|Presentation",
		meta = (ClampMin = "0.0"))
	float PresentationBlendDistance = 0.0f;

	/**
	 * Reusable motion profile used when an item is centered programmatically. It follows the player's motion scale
	 * (UReciMotionSubsystem); at 0 the item is centred at once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carousel|Motion")
	FReciMotionSettings CenterMotion;

	/** Emitted when another hosted item becomes closest to the center. */
	UPROPERTY(BlueprintAssignable, Category = "Carousel|Events")
	FReciCarouselCenteredItemEvent OnCenteredItemChanged;

	/** Recomputes the centered item and presentation blends. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Carousel Scroll Box")
	void RefreshCarousel();

	/** Scrolls a hosted item to the center. Animate has no effect while the player's motion scale is 0. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Carousel Scroll Box")
	bool SetCenteredItemIndex(int32 ItemIndex, bool bAnimate = true);

	/** Returns the hosted item currently nearest the viewport center. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Carousel Scroll Box")
	int32 GetCenteredItemIndex() const;

	/** Returns the source item currently nearest the viewport center. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Carousel Scroll Box")
	UObject* GetCenteredItem() const;

	/** Returns the live entry widget currently nearest the viewport center. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Carousel Scroll Box")
	UWidget* GetCenteredEntryWidget() const;

	/** True while a requested item is being moved to the center. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Carousel Scroll Box")
	bool IsCenteringProgrammatically() const;

	/** Snaps the nearest hosted item to the center. Animate has no effect while the player's motion scale is 0. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Carousel Scroll Box")
	bool SnapToClosestItem(bool bAnimate = true);

	/** Internal: native notifications for Reci's own classes. */
	FReciCarouselCenteredIndexChangedNative&
	OnCenteredIndexChangedNative();
	FReciCarouselItemPresentationNative&
	OnItemPresentationNative();

	virtual void SynchronizeProperties() override;
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual bool DrivesItemEmphasis() const override;
	virtual bool LoopsHostedEntries() const override;
	virtual void NotifyHostedEntriesChanged() override;
	virtual void NotifyHostedFocusChanged(int32 ItemIndex) override;
	virtual void NotifyHostedDrivenItemChanged(int32 ItemIndex) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	UFUNCTION()
	void HandleUserScrolled(float CurrentOffset);

	EActiveTimerReturnType HandleActiveTimer(
		double CurrentTime,
		float DeltaTime);
	void RequestActiveTimer();
	void CancelProgrammaticCenter();
	bool UpdateCarouselState();
	int32 GetCarouselItemCount() const;
	UWidget* GetCarouselItemAt(int32 ItemIndex) const;
	bool IsItemEligibleForCentering(int32 ItemIndex) const;
	int32 FindClosestItemIndex(float* OutSignedDistance = nullptr) const;
	float GetItemCenterDistance(int32 ItemIndex) const;
	float GetViewportExtent() const;
	float ResolvePresentationBlendDistance() const;
	void StopActiveTimer();
	bool RebaseLoopScroll();
	int32 ConsumeWheelSteps(float WheelDelta);

	virtual bool HandleHostedMouseWheel(
		float WheelDelta,
		const FVector2D& ScreenPosition) override;
	virtual void HandleHostedDragStarted() override;
	virtual void HandleHostedDragUpdated() override;
	virtual void HandleHostedDragEnded(bool bWasDragging) override;
	virtual void HandleHostedDragCanceled() override;
	virtual void HandleHostedRoutedScrollUpdated() override;
	virtual void HandleHostedRoutedScrollSettled() override;

	FReciCarouselCenteredIndexChangedNative
		CenteredIndexChangedNative;
	FReciCarouselItemPresentationNative
		ItemPresentationNative;
	FReciFloatMotion CenterOffsetMotion;
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;
	TArray<float> LastPresentationBlends;
	int32 CenteredItemIndex = INDEX_NONE;
	int32 PendingCenteredItemIndex = INDEX_NONE;
	double CenterRequestStartTime = 0.0;
	double LastMovementTime = 0.0;
	float LastObservedScrollOffset = 0.0f;
	bool bPendingRefresh = true;
	bool bWaitingToSnap = false;
	bool bPendingCenterAnimation = true;
	int32 CenterStableFrameCount = 0;
	int32 GeometryRetryFrameCount = 0;
	uint32 CarouselRevision = 0;
	double LastWheelInputTime = 0.0;
	float WheelDeltaAccumulator = 0.0f;
	bool bApplyingCenteredItem = false;
	bool bUpdatingCarouselScrollOffset = false;
	bool bUserDrivenScroll = false;
	/** Set Centered Item Index jumped to an item since the entries were last painted. */
	bool bAwaitingLayout = false;
};
