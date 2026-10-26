// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ReciList.h"

#include "ReciAdvancedList.generated.h"

class UCommonButtonBase;
class UReciAdvancedListEntryHost;

/**
 * A layout-independent Reci list with two stable widget presentations for
 * every logical item.
 *
 * Entry Widget Class is the normal presentation. Main Entry Widget Class is
 * the emphasized presentation. Focus can switch them locally, while an
 * external presentation driver can supply a continuous blend without
 * rebuilding either widget tree.
 */
UCLASS(
	BlueprintType,
	Blueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Advanced List",
		Category = "Reci Common UI"))
class RECICOMMONUIWIDGETS_API UReciAdvancedList : public UReciList
{
	GENERATED_BODY()

public:
	virtual TArray<UUserWidget*> GetEntryPresentations(int32 Index) const override;
	/** Optional emphasized presentation created beside each normal entry. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "List Entries",
		meta = (
			DisplayName = "Main Entry Widget Class"))
	TSubclassOf<UUserWidget> MainEntryWidgetClass;

	/** Initial logical item displayed with its main presentation. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		BlueprintSetter = SetInitialMainItemIndex,
		Category = "Main Item",
		meta = (ClampMin = "0"))
	int32 InitialMainItemIndex = 0;

	/**
	 * Makes an entry's main presentation follow ordinary focus changes when
	 * no external presentation driver is active.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Item")
	bool bMainPresentationFollowsFocus = true;

	/**
	 * With Confirm On Item Click, a click confirms the item it makes main. In a carousel only a click on the centred
	 * (main) item counts; outside one, a click on another item makes it main and confirms it in the same click.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Item")
	bool bConfirmMainItemOnClick = true;

	virtual void SetListItems(const TArray<UObject*>& InListItems) override;
#if WITH_EDITOR
	/** Also checks Main Entry Widget Class. */
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif
	virtual void SetEntryWidgetClass(
		TSubclassOf<UUserWidget> InEntryWidgetClass) override;
	/** The driven item is the item that owns the main presentation. */
	virtual bool SetDrivenItemIndex(
		int32 ItemIndex,
		bool bGiveFocus) override;
	virtual int32 GetDrivenItemIndex() const override;
	/** Also blends the item's main presentation by its emphasis. */
	virtual bool SetItemEmphasis(
		int32 ItemIndex,
		float Emphasis) override;
	/** While a viewport drives emphasis, the main presentation follows it instead of the main item. */
	virtual void SetItemEmphasisDriven(
		bool bDriven) override;

	/** Changes the emphasized entry class and rebuilds only list entries. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Advanced List")
	void SetMainEntryWidgetClass(
		TSubclassOf<UUserWidget> InMainEntryWidgetClass);

	/** Updates one entry's continuous normal-to-main presentation blend. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Advanced List")
	bool SetItemMainPresentationBlend(int32 ItemIndex, float MainBlend);

	/** Remeasures every live normal/main presentation. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Advanced List")
	void RefreshAdvancedListPresentationSizes();

	/** Updates the initial main index and the live list when populated. */
	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|Advanced List")
	void SetInitialMainItemIndex(int32 InInitialMainItemIndex);

protected:
	virtual void NativeConstruct() override;
	virtual UWidget* NativeCreateEntryWidget(
		UObject* ItemObject,
		int32 ItemIndex) override;
	virtual UCommonButtonBase* ResolveEntrySelectionButton(
		UWidget* EntryWidget,
		int32 ItemIndex) const override;
	virtual void NativeOnListItemsRebuilt() override;
	virtual void NativeOnItemFocusReceived(
		UCommonButtonBase* FocusedButton) override;
	virtual void NativeOnItemClicked(
		UCommonButtonBase* ClickedButton) override;
	virtual void NativeOnLoopEntryReassigned(
		int32 ItemIndex,
		UWidget* PreviousEntry,
		UWidget* NewEntry) override;

	int32 FindAdvancedEntryHostIndexForButton(
		const UCommonButtonBase* Button) const;
	UReciAdvancedListEntryHost* GetAdvancedEntryHost(
		int32 ItemIndex) const;

private:
	/**
	 * Changes the main logical item. Without an external presentation driver,
	 * this also applies a binary normal/main presentation.
	 */
	bool SetMainItemIndex(int32 ItemIndex, bool bGiveFocus);
	/** Returns the logical item currently owning the main presentation. */
	int32 GetMainItemIndex() const;
	bool SetMainItemIndexInternal(
		int32 ItemIndex,
		bool bGiveFocus,
		bool bApplyBinaryPresentation);
	void ApplyBinaryMainPresentation(int32 MainItemIndex);
	void CopyAdvancedListItems(TArray<UObject*>& OutItems) const;

	int32 MainItemIndex = INDEX_NONE;
	int32 PendingRebuildMainItemIndex = INDEX_NONE;
	int32 PendingPreviousMainItemIndex = INDEX_NONE;
	int32 PresentationChangeDepth = 0;

	UPROPERTY(Transient)
	TObjectPtr<UObject> PendingPreviousMainItem;

	bool bExternalPresentationDriverActive = false;
};
