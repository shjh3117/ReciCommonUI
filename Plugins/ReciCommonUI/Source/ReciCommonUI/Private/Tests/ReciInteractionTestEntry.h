// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "Widgets/ReciFocusStyleButtonBase.h"
#include "Widgets/ReciListItem.h"
#include "Widgets/ReciList.h"
#include "ReciInteractionTestEntry.generated.h"

UCLASS(Hidden, NotBlueprintable, Transient)
class RECICOMMONUI_API UReciInteractionTestEntry : public UReciFocusStyleButtonBase, public IReciListItem
{
 GENERATED_BODY()
public:
 virtual void SetListItemObject_Implementation(UObject* Item) override { Data=Item; }
 UFUNCTION() void RecordConfirm(UObject* Item,int32 Index) { ++ConfirmCount; LastConfirmed=Item; }
 UPROPERTY() TObjectPtr<UObject> Data;
 UPROPERTY() TObjectPtr<UObject> LastConfirmed;
 int32 ConfirmCount=0;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
};

#if WITH_DEV_AUTOMATION_TESTS
class FAutomationTestBase;
class UWorld;

namespace UE::ReciCommonUI::Tests
{
 /**
  * The interaction contract every Reci list source keeps, on a fresh list from MakeList whose entries are Reci
  * Interaction Test Entries (ReciCommonUI.Interaction.Contract). bPlainList adds what a list with one presentation per
  * item keeps besides: hosted clicks toggle once, and an entry's focus moves the cursor.
  */
 RECICOMMONUI_API void TestListContract(FAutomationTestBase& Test,UWorld& World,TFunctionRef<UReciListBase*()> MakeList,bool bPlainList);
 /** The contract's defaults: a list built with no property set is an Explicit single choice (ReciCommonUI.Interaction.DefaultListRole). */
 RECICOMMONUI_API void TestDefaultListRole(FAutomationTestBase& Test,UWorld& World,TFunctionRef<UReciListBase*()> MakeList);
}
#endif

UCLASS(Hidden, NotBlueprintable, Transient)
class UReciInteractionTestList : public UReciList
{
 GENERATED_BODY()
public:
 void ReceiveItemFocus(int32 Index) { NativeOnItemFocusReceived(GetHostedEntryButton(Index)); }
};
