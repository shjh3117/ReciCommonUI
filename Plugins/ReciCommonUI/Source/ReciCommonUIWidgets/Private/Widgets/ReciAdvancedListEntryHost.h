// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Components/SizeBox.h"

#include "ReciAdvancedListEntryHost.generated.h"

class UCommonButtonBase;
class UOverlay;
class UUserWidget;

struct FReciAdvancedPresentationChange
{
	TObjectPtr<UCommonButtonBase> PreviousButton;
	TObjectPtr<UCommonButtonBase> NewButton;
	bool bPreviousButtonHadFocus = false;
};

/**
 * Stable runtime host for the two presentations of one logical list item.
 *
 * Both widgets are created once. Changing emphasis only blends their opacity,
 * which presentation takes focus and desired size; it never rebuilds either widget tree.
 */
UCLASS(NotBlueprintable, Transient)
class UReciAdvancedListEntryHost final : public USizeBox
{
	GENERATED_BODY()

public:
	bool Initialize(
		UWidget& OwningWidget,
		UObject* ItemObject,
		TSubclassOf<UUserWidget> NormalEntryClass,
		TSubclassOf<UUserWidget> MainEntryClass,
		const TFunction<void(UUserWidget*)>& InitializeEntry = {});

	/** Applies visual blend and interaction ownership as one transaction. */
	bool ApplyPresentation(
		float MainBlend,
		FReciAdvancedPresentationChange& OutChange);

	/** Remeasures both presentations and reapplies the current blended size. */
	void RefreshPresentationSizes();
	void SetFallbackDesiredHeight(float InFallbackDesiredHeight);

	bool HasMainPresentation() const;
	float GetMainBlend() const { return LastMainBlend; }
	UCommonButtonBase* GetActiveButton() const;
	UCommonButtonBase* GetNormalButton() const;
	UCommonButtonBase* GetMainButton() const;
	UUserWidget* GetNormalWidget() const { return NormalEntryWidget; }
	UUserWidget* GetMainWidget() const { return MainEntryWidget; }
	FVector2D GetNormalDesiredSize() const;
	FVector2D GetMainDesiredSize() const;
	FVector2D GetMaximumDesiredSize() const;

private:
	void SetMainPresentationActive(bool bInMainPresentationActive);
	void SetMainBlend(float InMainBlend);
	void ApplyBlendedDesiredSize();
	bool IsMainPresentationActive() const;

	UUserWidget* CreatePresentation(
		UWidget& OwningWidget,
		UObject* ItemObject,
		TSubclassOf<UUserWidget> EntryClass,
		const TFunction<void(UUserWidget*)>& InitializeEntry);

	virtual void SynchronizeProperties() override;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> PresentationOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> NormalEntryWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainEntryWidget;

	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> NormalButton;

	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> MainButton;

	FVector2D CachedNormalDesiredSize = FVector2D::ZeroVector;
	FVector2D CachedMainDesiredSize = FVector2D::ZeroVector;
	bool bMainPresentationActive = false;
	bool bNormalPresentationFocusable = false;
	bool bMainPresentationFocusable = false;
	float LastMainBlend = 0.0f;
	float FallbackDesiredHeight = 0.0f;
};
