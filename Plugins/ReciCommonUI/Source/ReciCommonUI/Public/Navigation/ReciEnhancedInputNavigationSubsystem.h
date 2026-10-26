// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "ReciEnhancedInputNavigationSubsystem.generated.h"

class FNavigationConfig;
class FReciEnhancedInputNavigationConfig;
class FSlateUser;
class UEnhancedInputLocalPlayerSubsystem;

/**
 * Replaces this local player's Slate navigation config with one that wraps it, while the project asks for it. With a
 * Reci UI Input Data as Input Data and CommonUI's Enhanced Input support on, Reci follows Enhanced Input: the player's
 * active mappings of its navigation actions and of CommonUI's click and Back actions stay in sync with Slate focus
 * navigation (with the Input Data's Mapped Keys Replace Default Keys, a mapped key of a device turns off Slate's default
 * keys for the same direction or action there). Tab Navigation off takes Tab and Shift+Tab out of it. With stock Input
 * Data and the settings' defaults it is not created, and no player's navigation config is touched. All of this is read
 * when a local player is created.
 */
UCLASS()
class RECICOMMONUI_API UReciEnhancedInputNavigationSubsystem final
	: public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Only while Reci follows Enhanced Input or Tab Navigation is off. Warns once when Input Data is a Reci UI Input Data
	 * while CommonUI's Enhanced Input support is off, which hides and ignores its fields.
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	/** Lets tests start a subsystem on a local player that has no subsystem collection, and rebuild its mappings. */
	friend struct FReciNavigationSubsystemTestAccess;
#endif

	/** What Initialize does once its dependencies are initialised: listens for changes and installs the config. */
	void Start();

	/**
	 * Immediately reads the active Enhanced Input mappings and applies them
	 * to this local player's Slate navigation configuration.
	 */
	void RefreshNavigationMappings();

	UFUNCTION()
	void HandleControlMappingsRebuilt();

	void HandleControllerIdChanged(
		int32 NewControllerId,
		int32 OldControllerId);
	void HandleSlateUserRegistered(int32 UserIndex);

	void InstallNavigationConfig();
	void RestoreNavigationConfig();

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem;

	TSharedPtr<FReciEnhancedInputNavigationConfig> NavigationConfig;
	TSharedPtr<FNavigationConfig> PreviousNavigationConfig;
	TWeakPtr<FSlateUser> SlateUser;
	FDelegateHandle ControllerIdChangedHandle;
	FDelegateHandle SlateUserRegisteredHandle;
};
