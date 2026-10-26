// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "Components/PanelWidget.h"
#include "CoreMinimal.h"
#include "Types/SlateEnums.h"

#include "ReciListPanel.generated.h"

class SReciListPanel;

/** Layout policy applied to the stable children of a Reci list panel. */
UENUM(BlueprintType)
enum class EReciListLayoutMode : uint8
{
	Vertical,
	Horizontal,
	Tile
};

/**
 * Layout-only settings shared by Reci list sources and their layout hosts.
 *
 * The settings never own item data or entry widgets. Applying new settings
 * only invalidates arrangement; existing UWidget and Slate instances remain
 * attached to the same Reci list panel.
 */
USTRUCT(BlueprintType)
struct RECICOMMONUI_API FReciListLayoutSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	EReciListLayoutMode Mode = EReciListLayoutMode::Vertical;

	/** Empty space between adjacent entries on each active layout axis. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Layout",
		meta = (ClampMin = "0.0"))
	float ItemSpacing = 0.0f;

	/** Shares the allotted horizontal width equally; entries never overflow it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bFillEntries = false;

	/** Fixed entry extent used in Tile mode. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Tile",
		meta = (ClampMin = "1.0"))
	FVector2D TileEntrySize = FVector2D(128.0f, 128.0f);

	/** Zero derives the column count from the panel's allotted width. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Tile",
		meta = (ClampMin = "0"))
	int32 TileFixedColumns = 0;

	/** Horizontal is row-major; Vertical is column-major. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	TEnumAsByte<EOrientation> TileFlowOrientation = Orient_Horizontal;
};

/**
 * Stable entry panel used by every composable Reci list source.
 *
 * Children remain real UPanelWidget children while the underlying Slate panel
 * changes only its arrangement policy between vertical, horizontal and tile
 * modes. A layout change therefore does not reparent or recreate entries.
 */
UCLASS(
	BlueprintType,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci List Panel",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciListPanel : public UPanelWidget
{
	GENERATED_BODY()

public:
	UReciListPanel(const FObjectInitializer& ObjectInitializer);

	/** Equalizes matching occurrences in a repeated linear list (zero disables). */
	void SetRepeatedItemCount(int32 Count);
	FVector2D GetLinearChildSize(int32 ChildIndex) const;

	/**
	 * Applies a new arrangement policy without rebuilding child widgets. The
	 * owning list applies its layout host's settings here.
	 */
	void ApplyLayoutSettings(const FReciListLayoutSettings& InSettings);

	FReciListLayoutSettings GetLayoutSettings() const;

	/**
	 * Internal (lists): whether Layout Settings differ from their defaults, as a Blueprint derived from Reci List may set
	 * them on its ItemsBox. A Reci layout box that hosts the list replaces them.
	 */
	bool HasAuthoredLayoutSettings() const;

	/** Re-invalidates the current child arrangement. */
	void RefreshLayout();

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	void AddSlateChild(UPanelSlot* InSlot);

	/**
	 * The arrangement of a list that no Reci layout box hosts. A Reci Vertical, Horizontal or Tile Box that hosts the list
	 * owns its layout and replaces these: a hosted list whose ItemsBox sets them is a compile warning, and is reported
	 * once at run time.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Layout",
		meta = (ShowOnlyInnerProperties))
	FReciListLayoutSettings LayoutSettings;

	TSharedPtr<SReciListPanel> MyListPanel;
	int32 RepeatedItemCount = 0;
};
