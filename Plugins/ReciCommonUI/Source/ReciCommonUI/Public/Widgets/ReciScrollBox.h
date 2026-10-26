// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "Components/ScrollBox.h"
#include "CoreMinimal.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Motion/ReciMotion.h"

#include "ReciScrollBox.generated.h"

class FActiveTimerHandle;
class UReciListLayoutBoxBase;

/**
 * Scroll viewport for a single Reci list layout.
 *
 * The child layout owns navigation topology. This viewport owns wheel,
 * pointer/touch dragging, inertia and scrolling focused entries into view.
 * It animates only through ReciMotion, so every motion is frame-rate
 * independent and follows the player's motion scale (UReciMotionSubsystem),
 * and calls Slate's own scrolling only unanimated: a drag moves the content
 * 1:1 at any motion scale.
 */
UCLASS(
	BlueprintType,
	Blueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Scroll Box",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciScrollBox
	: public UScrollBox,
	  public IReciInputFocusTarget
{
	GENERATED_BODY()

public:
	UReciScrollBox(const FObjectInitializer& ObjectInitializer);

	/** Pointer movement required before a routed press becomes a drag. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Scroll|Input",
		meta = (ClampMin = "0.0"))
	float DragTriggerDistance = 10.0f;

	/** Slate units moved by one mouse-wheel notch. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Scroll|Input",
		meta = (ClampMin = "0.0"))
	float WheelScrollAmount = 32.0f;

	/** Moves the hosted list cursor with the wheel, scrolling it into view as needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll|Input")
	bool bNavigateWithMouseWheel = false;

	/**
	 * Smooth motion used for routed wheel scrolling and for scrolling the focused entry into view. It follows the
	 * player's motion scale; at 0 the offset moves straight to its target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll|Motion")
	FReciMotionSettings ScrollMotion;

	/**
	 * Exponential rate (1/s) of the glide after a pointer/touch drag. The glide covers (release speed - Minimum
	 * Inertia Speed) / rate at any frame rate; 0 keeps the release speed until an end. The release speed is the
	 * drag's velocity smoothed over about 39 ms, and it decays while the pointer holds still before release. Under
	 * the player's motion scale s the rate is divided by s, so the glide's distance and time shrink by s; at 0 a
	 * released drag does not glide.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Scroll|Motion",
		meta = (ClampMin = "0.0"))
	float InertiaDeceleration = 8.0f;

	/** A glide starts only at this release speed or above, and ends exactly at it (at least 0.01). */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Scroll|Motion",
		meta = (ClampMin = "0.0"))
	float MinimumInertiaSpeed = 5.0f;

	/**
	 * For a composite that routes pointer input itself (it has turned this viewport's own routing off with Set Input
	 * Routing Enabled): a drag that began on a widget outside this viewport, moved through it with the same drag
	 * threshold, 1:1 motion and glide as the viewport's own drags. Begin returns false while another pointer drags; End
	 * returns whether the press became a drag.
	 */
	bool BeginExternalDrag(
		int32 PointerIndex,
		const FVector2D& ScreenPosition);
	bool UpdateExternalDrag(
		int32 PointerIndex,
		const FVector2D& ScreenPosition);
	bool EndExternalDrag(int32 PointerIndex);
	bool HasExternalDrag(int32 PointerIndex) const;
	void CancelExternalDrag();

	/**
	 * Off: the viewport stops taking routed input itself and keeps its motion and layout, for a composite that owns
	 * the input of the lists in it and passes it on (External Drag, Route External Mouse Wheel).
	 */
	void SetInputRoutingEnabled(bool bEnabled);
	bool IsInputRoutingEnabled() const { return bInputRoutingEnabled; }
	/** Handles a wheel turn at Position as the viewport's own wheel handling does; returns whether it was handled. */
	bool RouteExternalMouseWheel(float Delta, const FVector2D& Position);

	/** The hosted item whose presentation (any copy) is under ScreenPosition, or INDEX_NONE. */
	int32 FindHostedItemIndexAtScreenPosition(
		const FVector2D& ScreenPosition) const;
	/** The Reci layout box this viewport hosts as its only child, or null. */
	UReciListLayoutBoxBase* GetHostedLayout() const;

	/**
	 * Whether this viewport drives its hosted list's item emphasis (UReciListBase::SetItemEmphasis) as it scrolls; false
	 * for a Reci Scroll Box. Such a viewport keeps the driven item emphasised, so a click on another item first makes
	 * it the driven item (it receives focus), and only a click on the driven item confirms it.
	 */
	virtual bool DrivesItemEmphasis() const;

	/**
	 * Whether this viewport moves the copies of a hosted list with Looping Entries on, through UReciList's loop
	 * functions; false for a Reci Scroll Box. A looping list makes its copies only while such a viewport hosts it.
	 */
	virtual bool LoopsHostedEntries() const;

	/** Called by the attached layout after its entry collection changes. */
	virtual void NotifyHostedEntriesChanged();

	/** Called by the attached layout after actual item focus changes. */
	virtual void NotifyHostedFocusChanged(int32 ItemIndex);

	/** Called by the attached layout after its semantic driven item changes. */
	virtual void NotifyHostedDrivenItemChanged(int32 ItemIndex);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

	/**
	 * The scroll viewport's extension points, for a viewport derived from this one (a carousel). Handle Hosted Mouse
	 * Wheel takes a routed wheel turn (returns whether it was handled; this class scrolls by Wheel Scroll Amount or moves
	 * the list cursor). The drag hooks follow a routed or external drag: started once it passes Drag Trigger Distance,
	 * updated as it moves the content, and ended (with whether it was a drag) or canceled. The routed scroll hooks follow
	 * the offset this class moves through Scroll Motion and a drag's glide: updated on each step, settled when it stops.
	 */
	virtual bool HandleHostedMouseWheel(
		float WheelDelta,
		const FVector2D& ScreenPosition);
	virtual void HandleHostedDragStarted();
	virtual void HandleHostedDragUpdated();
	virtual void HandleHostedDragEnded(bool bWasDragging);
	virtual void HandleHostedDragCanceled();
	virtual void HandleHostedRoutedScrollUpdated();
	virtual void HandleHostedRoutedScrollSettled();
	virtual void UpdateHostedInputRegistration();
	bool HasHostedInteractionFocus() const;
	bool IsExternallyDragging() const;
	/** Whether the external drag runs across the axis: a drag across the axis moves no content; centring leaves it held. */
	bool IsExternallyCrossAxisDragging() const;
	bool HasExternalPointerCapture() const;
	bool IsRoutedScrollMotionActive() const;
	void RequestScrollInteractionTimer();
	void StopScrollInteractionTimer();
	void StopRoutedMotion();

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

private:
	EActiveTimerReturnType HandleScrollInteractionTimer(
		double CurrentTime,
		float DeltaTime);
	void AttachHostedLayout();
	void DetachHostedLayout();
	/**
	 * A press on a button nested in an entry (a stepper's -/+, a dropdown field), or on a stock key selector nested in
	 * it, belongs to that control.
	 */
	bool IsPressOnNestedButton(
		const FVector2D& ScreenPosition,
		int32 ItemIndex) const;
	/**
	 * Moves the routed scroll offset to TargetOffset through Scroll Motion, or at once at MotionScale 0 (reporting
	 * the move as updated and settled, with no timer).
	 */
	void MoveRoutedScrollTo(
		float TargetOffset,
		float MotionScale);
	/**
	 * Measures the entry that should come into view and moves toward it. Returns whether the request is still
	 * pending. Runs before the timer moves anything, so the measure matches the last arrangement.
	 */
	bool UpdatePendingIntoView(float MotionScale);
	/** How far the scroll offset must move to bring Entry into view by Slate's Into View rule; false without sizes. */
	bool MeasureIntoViewDelta(
		const UWidget& Entry,
		float& OutDelta) const;
	void ClearPendingIntoView();
	TWeakPtr<FActiveTimerHandle> ScrollInteractionTimerHandle;
	FReciFloatMotion ScrollOffsetMotion;
	FReciInertiaMotion DragInertia;
	double ExternalDragLastTime = 0.0;
	int32 PendingIntoViewItemIndex = INDEX_NONE;
	int32 IntoViewRetryFrames = 0;
	int32 ExternalPointerIndex = INDEX_NONE;
	int32 PressedItemIndex = INDEX_NONE;
	/** The hosted item under the press that began the external drag, or INDEX_NONE: a drag across the axis acts on it. */
	int32 ExternalDragItemIndex = INDEX_NONE;
	FVector2D ExternalDragStartScreenPosition = FVector2D::ZeroVector;
	FVector2D ExternalDragLastScreenPosition = FVector2D::ZeroVector;
	bool bExternalDragging = false;
	bool bExternalCrossAxisDragging = false;
	bool bExternalNavigationInputEmitted = false;
	bool bInputTargetRegistered = false;
	bool bInputRoutingEnabled = true;
	float NavigationWheelAccumulator = 0.0f;
	double LastNavigationWheelTime = 0.0;
};
