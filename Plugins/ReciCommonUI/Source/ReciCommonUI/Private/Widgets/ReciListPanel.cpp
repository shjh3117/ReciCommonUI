// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciListPanel.h"

#include "Components/PanelSlot.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Children.h"
#include "Layout/WidgetPath.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SPanel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciListPanel)

#define LOCTEXT_NAMESPACE "ReciCommonUI"

namespace
{
	FReciListLayoutSettings SanitizeLayoutSettings(
		const FReciListLayoutSettings& InSettings)
	{
		FReciListLayoutSettings Result = InSettings;
		Result.ItemSpacing = FMath::IsFinite(Result.ItemSpacing)
			? FMath::Max(Result.ItemSpacing, 0.0f)
			: 0.0f;
		Result.TileEntrySize.X = FMath::IsFinite(Result.TileEntrySize.X)
			? FMath::Max(Result.TileEntrySize.X, 1.0f)
			: 1.0f;
		Result.TileEntrySize.Y = FMath::IsFinite(Result.TileEntrySize.Y)
			? FMath::Max(Result.TileEntrySize.Y, 1.0f)
			: 1.0f;
		Result.TileFixedColumns = FMath::Max(Result.TileFixedColumns, 0);
		return Result;
	}

	bool AreLayoutSettingsEqual(
		const FReciListLayoutSettings& A,
		const FReciListLayoutSettings& B)
	{
		return A.Mode == B.Mode
			&& A.bFillEntries == B.bFillEntries
			&& FMath::IsNearlyEqual(A.ItemSpacing, B.ItemSpacing)
			&& A.TileEntrySize.Equals(B.TileEntrySize)
			&& A.TileFixedColumns == B.TileFixedColumns
			&& A.TileFlowOrientation == B.TileFlowOrientation;
	}
}

class SReciListPanel final : public SPanel
{
public:
	SReciListPanel()
		: Children(this)
	{
	}

	struct FSlot : public TSlotBase<FSlot>
	{
		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
		SLATE_SLOT_END_ARGS()

		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs)
		{
			TSlotBase<FSlot>::Construct(SlotOwner, MoveTemp(InArgs));
		}
	};

	using FScopedWidgetSlotArguments =
		TPanelChildren<FSlot>::FScopedWidgetSlotArguments;

	SLATE_BEGIN_ARGS(SReciListPanel)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetVisibility(InArgs._Visibility);
	}

	FScopedWidgetSlotArguments AddSlot()
	{
		return FScopedWidgetSlotArguments{
			MakeUnique<FSlot>(),
			Children,
			INDEX_NONE};
	}

	bool RemoveSlot(const TSharedRef<SWidget>& SlotWidget)
	{
		const bool bRemoved = Children.Remove(SlotWidget) != INDEX_NONE;
		if (bRemoved)
		{
			Invalidate(EInvalidateWidgetReason::Layout);
		}
		return bRemoved;
	}

	void SetLayoutSettings(const FReciListLayoutSettings& InSettings)
	{
		const FReciListLayoutSettings NewSettings =
			SanitizeLayoutSettings(InSettings);
		if (AreLayoutSettingsEqual(LayoutSettings, NewSettings))
		{
			return;
		}

		LayoutSettings = NewSettings;
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	void SetRepeatedItemCount(int32 Count)
	{
		RepeatedItemCount = FMath::Max(Count, 0);
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	FVector2D GetLinearChildSize(int32 ChildIndex) const
	{
		FVector2D Size = Children[ChildIndex].GetWidget()->GetDesiredSize();
		if (RepeatedItemCount > 0)
			for (int32 Index = ChildIndex % RepeatedItemCount; Index < Children.Num(); Index += RepeatedItemCount)
			{
				const FVector2D Other = Children[Index].GetWidget()->GetDesiredSize();
				Size.X = FMath::Max(Size.X, Other.X);
				Size.Y = FMath::Max(Size.Y, Other.Y);
			}
		return Size;
	}

	virtual FChildren* GetChildren() override
	{
		return &Children;
	}

	virtual void OnArrangeChildren(
		const FGeometry& AllottedGeometry,
		FArrangedChildren& ArrangedChildren) const override
	{
		switch (LayoutSettings.Mode)
		{
		case EReciListLayoutMode::Horizontal:
			ArrangeLinearChildren(
				AllottedGeometry,
				ArrangedChildren,
				Orient_Horizontal);
			break;

		case EReciListLayoutMode::Tile:
			ArrangeTileChildren(AllottedGeometry, ArrangedChildren);
			break;

		case EReciListLayoutMode::Vertical:
		default:
			ArrangeLinearChildren(
				AllottedGeometry,
				ArrangedChildren,
				Orient_Vertical);
			break;
		}
	}

protected:
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		switch (LayoutSettings.Mode)
		{
		case EReciListLayoutMode::Horizontal:
			return ComputeLinearDesiredSize(Orient_Horizontal);

		case EReciListLayoutMode::Tile:
			return ComputeTileDesiredSize();

		case EReciListLayoutMode::Vertical:
		default:
			return ComputeLinearDesiredSize(Orient_Vertical);
		}
	}

private:
	int32 GetVisibleChildCount() const
	{
		int32 VisibleCount = 0;
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			if (Children[ChildIndex].GetWidget()->GetVisibility()
				!= EVisibility::Collapsed)
			{
				++VisibleCount;
			}
		}
		return VisibleCount;
	}

	int32 ResolveTileColumnCount(
		const int32 VisibleChildCount,
		const float AvailableWidth) const
	{
		if (VisibleChildCount <= 0)
		{
			return 0;
		}
		if (LayoutSettings.TileFixedColumns > 0)
		{
			return FMath::Clamp(
				LayoutSettings.TileFixedColumns,
				1,
				VisibleChildCount);
		}

		const float CellStride = LayoutSettings.TileEntrySize.X
			+ LayoutSettings.ItemSpacing;
		const int32 ColumnsFromWidth = CellStride > KINDA_SMALL_NUMBER
			? FMath::FloorToInt(
				(AvailableWidth + LayoutSettings.ItemSpacing) / CellStride)
			: 1;
		return FMath::Clamp(ColumnsFromWidth, 1, VisibleChildCount);
	}

	FVector2D ComputeLinearDesiredSize(const EOrientation Orientation) const
	{
		FVector2D ResultSize = FVector2D::ZeroVector;
		int32 VisibleChildCount = 0;
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			const TSharedRef<SWidget> Child =
				Children[ChildIndex].GetWidget();
			if (Child->GetVisibility() == EVisibility::Collapsed)
			{
				continue;
			}

			const FVector2D ChildDesiredSize = GetLinearChildSize(ChildIndex);
			if (Orientation == Orient_Vertical)
			{
				ResultSize.X = FMath::Max(
					ResultSize.X,
					ChildDesiredSize.X);
				ResultSize.Y += ChildDesiredSize.Y;
			}
			else
			{
				ResultSize.X += ChildDesiredSize.X;
				ResultSize.Y = FMath::Max(
					ResultSize.Y,
					ChildDesiredSize.Y);
			}
			++VisibleChildCount;
		}

		const float TotalSpacing = LayoutSettings.ItemSpacing
			* FMath::Max(VisibleChildCount - 1, 0);
		if (Orientation == Orient_Vertical)
		{
			ResultSize.Y += TotalSpacing;
		}
		else
		{
			ResultSize.X += TotalSpacing;
			if (LayoutSettings.bFillEntries) ResultSize.X = 0;
		}
		return ResultSize;
	}

	FVector2D ComputeTileDesiredSize() const
	{
		const int32 VisibleChildCount = GetVisibleChildCount();
		if (VisibleChildCount <= 0)
		{
			return FVector2D::ZeroVector;
		}

		// With automatic columns this reports the stable minimum of one column.
		// A fill-aligned parent can allot more width and ArrangeTileChildren will
		// derive additional columns without creating a desired-size feedback loop.
		const int32 ColumnCount = LayoutSettings.TileFixedColumns > 0
			? FMath::Clamp(
				LayoutSettings.TileFixedColumns,
				1,
				VisibleChildCount)
			: 1;
		const int32 RowCount = FMath::DivideAndRoundUp(
			VisibleChildCount,
			ColumnCount);
		return FVector2D(
			LayoutSettings.TileEntrySize.X * ColumnCount
				+ LayoutSettings.ItemSpacing * FMath::Max(ColumnCount - 1, 0),
			LayoutSettings.TileEntrySize.Y * RowCount
				+ LayoutSettings.ItemSpacing * FMath::Max(RowCount - 1, 0));
	}

	void ArrangeLinearChildren(
		const FGeometry& AllottedGeometry,
		FArrangedChildren& ArrangedChildren,
		const EOrientation Orientation) const
	{
		const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
		const int32 VisibleCount = GetVisibleChildCount();
		const float FillWidth = VisibleCount > 0
			? FMath::Max(0.0f, AllottedSize.X - LayoutSettings.ItemSpacing * (VisibleCount - 1)) / VisibleCount : 0;
		float MainAxisOffset = 0.0f;
		bool bHasArrangedChild = false;
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			const TSharedRef<SWidget> Child =
				Children[ChildIndex].GetWidget();
			const EVisibility ChildVisibility = Child->GetVisibility();
			if (!ArrangedChildren.Accepts(ChildVisibility))
			{
				continue;
			}

			if (bHasArrangedChild)
			{
				MainAxisOffset += LayoutSettings.ItemSpacing;
			}
			const FVector2D ChildDesiredSize = GetLinearChildSize(ChildIndex);
			const FVector2D ChildPosition = Orientation == Orient_Vertical
				? FVector2D(0.0f, MainAxisOffset)
				: FVector2D(MainAxisOffset, 0.0f);
			const FVector2D ChildSize = Orientation == Orient_Vertical
				? FVector2D(AllottedSize.X, ChildDesiredSize.Y)
				: FVector2D(LayoutSettings.bFillEntries ? FillWidth : ChildDesiredSize.X, AllottedSize.Y);
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(
				Child,
				ChildSize,
				FSlateLayoutTransform(ChildPosition)));
			MainAxisOffset += Orientation == Orient_Vertical
				? ChildSize.Y
				: ChildSize.X;
			bHasArrangedChild = true;
		}
	}

	void ArrangeTileChildren(
		const FGeometry& AllottedGeometry,
		FArrangedChildren& ArrangedChildren) const
	{
		const int32 VisibleChildCount = GetVisibleChildCount();
		const int32 ColumnCount = ResolveTileColumnCount(
			VisibleChildCount,
			AllottedGeometry.GetLocalSize().X);
		if (ColumnCount <= 0)
		{
			return;
		}

		const int32 RowCount = FMath::DivideAndRoundUp(
			VisibleChildCount,
			ColumnCount);
		const FVector2D CellStride = LayoutSettings.TileEntrySize
			+ FVector2D(LayoutSettings.ItemSpacing);
		int32 VisibleChildIndex = 0;
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			const TSharedRef<SWidget> Child =
				Children[ChildIndex].GetWidget();
			const EVisibility ChildVisibility = Child->GetVisibility();
			if (!ArrangedChildren.Accepts(ChildVisibility))
			{
				continue;
			}

			int32 ColumnIndex = 0;
			int32 RowIndex = 0;
			if (LayoutSettings.TileFlowOrientation == Orient_Vertical)
			{
				ColumnIndex = VisibleChildIndex / RowCount;
				RowIndex = VisibleChildIndex % RowCount;
			}
			else
			{
				ColumnIndex = VisibleChildIndex % ColumnCount;
				RowIndex = VisibleChildIndex / ColumnCount;
			}
			const FVector2D ChildPosition(
				ColumnIndex * CellStride.X,
				RowIndex * CellStride.Y);
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(
				Child,
				LayoutSettings.TileEntrySize,
				FSlateLayoutTransform(ChildPosition)));
			++VisibleChildIndex;
		}
	}

	TPanelChildren<FSlot> Children;
	FReciListLayoutSettings LayoutSettings;
	int32 RepeatedItemCount = 0;
};

UReciListPanel::UReciListPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
	SetVisibilityInternal(ESlateVisibility::SelfHitTestInvisible);
}

void UReciListPanel::SetRepeatedItemCount(int32 Count)
{
	RepeatedItemCount = FMath::Max(Count, 0);
	if (MyListPanel.IsValid()) MyListPanel->SetRepeatedItemCount(RepeatedItemCount);
	RefreshLayout();
}

FVector2D UReciListPanel::GetLinearChildSize(int32 ChildIndex) const
{
	if (MyListPanel.IsValid() && ChildIndex >= 0 && ChildIndex < GetChildrenCount())
	{
		FVector2D Size = MyListPanel->GetLinearChildSize(ChildIndex);
		if(LayoutSettings.bFillEntries && LayoutSettings.Mode == EReciListLayoutMode::Horizontal)
		{
			int32 Count = 0;
			for(UWidget* Child : GetAllChildren()) if(Child->GetVisibility() != ESlateVisibility::Collapsed) ++Count;
			Size.X = Count > 0 ? FMath::Max(0.0f, GetCachedGeometry().GetLocalSize().X - LayoutSettings.ItemSpacing * (Count - 1)) / Count : 0;
		}
		return Size;
	}
	return FVector2D::ZeroVector;
}

void UReciListPanel::ApplyLayoutSettings(
	const FReciListLayoutSettings& InSettings)
{
	LayoutSettings = SanitizeLayoutSettings(InSettings);
	if (MyListPanel.IsValid())
	{
		MyListPanel->SetLayoutSettings(LayoutSettings);
	}
	RefreshLayout();
}

FReciListLayoutSettings UReciListPanel::GetLayoutSettings() const
{
	return LayoutSettings;
}

bool UReciListPanel::HasAuthoredLayoutSettings() const
{
	return !AreLayoutSettingsEqual(SanitizeLayoutSettings(LayoutSettings), FReciListLayoutSettings());
}

void UReciListPanel::RefreshLayout()
{
	if (MyListPanel.IsValid())
	{
		MyListPanel->Invalidate(EInvalidateWidgetReason::Layout);
	}
	InvalidateLayoutAndVolatility();
}

void UReciListPanel::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	LayoutSettings = SanitizeLayoutSettings(LayoutSettings);
	if (MyListPanel.IsValid())
	{
		MyListPanel->SetLayoutSettings(LayoutSettings);
	}
}

void UReciListPanel::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyListPanel.Reset();
}

TSharedRef<SWidget> UReciListPanel::RebuildWidget()
{
	MyListPanel = SNew(SReciListPanel);
	MyListPanel->SetLayoutSettings(LayoutSettings);
	MyListPanel->SetRepeatedItemCount(RepeatedItemCount);
	for (UPanelSlot* PanelSlot : Slots)
	{
		AddSlateChild(PanelSlot);
	}
	return MyListPanel.ToSharedRef();
}

void UReciListPanel::OnSlotAdded(UPanelSlot* InSlot)
{
	AddSlateChild(InSlot);
}

void UReciListPanel::OnSlotRemoved(UPanelSlot* InSlot)
{
	if (!MyListPanel.IsValid() || !InSlot || !InSlot->Content)
	{
		return;
	}

	if (const TSharedPtr<SWidget> ChildWidget =
		InSlot->Content->GetCachedWidget())
	{
		MyListPanel->RemoveSlot(ChildWidget.ToSharedRef());
	}
}

void UReciListPanel::AddSlateChild(UPanelSlot* InSlot)
{
	if (!MyListPanel.IsValid() || !InSlot || !InSlot->Content)
	{
		return;
	}

	MyListPanel->AddSlot()
	[
		InSlot->Content->TakeWidget()
	];
}

#if WITH_EDITOR
const FText UReciListPanel::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "RECI COMMON UI");
}
#endif

#undef LOCTEXT_NAMESPACE
