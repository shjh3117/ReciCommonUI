// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Tests/ReciInteractionTestEntry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Widgets/ReciTextListItem.h"

TSharedRef<SWidget> UReciInteractionTestEntry::RebuildWidget()
{
 if(!WidgetTree->RootWidget)
 {
  auto* Bounds=WidgetTree->ConstructWidget<USizeBox>();Bounds->SetWidthOverride(60);Bounds->SetHeightOverride(40);
  WidgetTree->RootWidget=Bounds;
 }
 return Super::RebuildWidget();
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciInteractionContractTest,"ReciCommonUI.Interaction.Contract",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

void UE::ReciCommonUI::Tests::TestListContract(FAutomationTestBase& Test,UWorld& World,TFunctionRef<UReciListBase*()> MakeList,const bool bPlainList)
{
 auto* Recorder=NewObject<UReciInteractionTestEntry>(&World);
 UObject* A=NewObject<UReciTextListItemData>(&World);UObject* B=NewObject<UReciTextListItemData>(&World);UObject* C=NewObject<UReciTextListItemData>(&World);
 auto* List=MakeList();
 const auto Slate=List->TakeWidget();List->SetSelectionMode(ESelectionMode::None);List->SetListItems({A,B,C});
 // Record every Blueprint "...Confirmed" event, so a second completion event for the same confirm fails the counts below.
 UFunction* Record=Recorder->FindFunctionChecked(GET_FUNCTION_NAME_CHECKED(UReciInteractionTestEntry,RecordConfirm));
 int32 CompletionEvents=0;
 for(TFieldIterator<FMulticastDelegateProperty> It(List->GetClass());It;++It)
 {
  if(!It->HasAnyPropertyFlags(CPF_BlueprintAssignable)||!It->GetName().EndsWith(TEXT("Confirmed")))continue;
  if(!Test.TestTrue(FString::Printf(TEXT("%s takes an item and its index"),*It->GetName()),It->SignatureFunction&&It->SignatureFunction->IsSignatureCompatibleWith(Record)))continue;
  FScriptDelegate Delegate;Delegate.BindUFunction(Recorder,Record->GetFName());It->AddDelegate(MoveTemp(Delegate),List);++CompletionEvents;
 }
 Test.TestTrue(TEXT("List has a completion event"),CompletionEvents>0);
 List->SetDrivenItemIndex(1,false);
 Test.TestEqual(TEXT("Action navigation stores no selection"),List->GetNumItemsSelected(),0);
 const int32 Before=Recorder->ConfirmCount;Test.TestTrue(TEXT("Action can confirm"),List->ConfirmDrivenItem());
 Test.TestEqual(TEXT("Action emits once"),Recorder->ConfirmCount,Before+1);Test.TestTrue(TEXT("Action confirms logical item"),Recorder->LastConfirmed==B);
 Test.TestEqual(TEXT("Action confirmation stores no selection"),List->GetNumItemsSelected(),0);
 List->bConfirmOnItemClick=true;
 const int32 ClickBefore=Recorder->ConfirmCount;Test.TestTrue(TEXT("Driven item takes a click"),List->HandleHostedItemClick(1));
 Test.TestEqual(TEXT("Click on the driven item emits once"),Recorder->ConfirmCount,ClickBefore+1);Test.TestTrue(TEXT("Click confirms logical item"),Recorder->LastConfirmed==B);

 List->SetSelectionMode(ESelectionMode::Single);List->bSelectionFollowsNavigation=true;
 List->SetDrivenItemIndex(2,false);Test.TestTrue(TEXT("Automatic Single follows navigation"),List->GetSelectedItem()==C);
 List->SetDrivenItemIndex(0,false);Test.TestTrue(TEXT("Automatic Single replaces selection"),List->GetSelectedItem()==A);
 List->bSelectionFollowsNavigation=false;List->SetDrivenItemIndex(2,false);
 Test.TestTrue(TEXT("Explicit Single retains choice during navigation"),List->GetSelectedItem()==A);
 List->ConfirmDrivenItem();Test.TestTrue(TEXT("Explicit Single confirms cursor choice"),List->GetSelectedItem()==C);
 List->SetSelectedItemIndex(0,false);
 List->SetListItems({C,B,A});Test.TestTrue(TEXT("Reorder preserves selected object"),List->GetSelectedItem()==A);
 Test.TestTrue(TEXT("Reorder preserves cursor object"),List->GetItemAt(List->GetDrivenItemIndex())==C);
 List->SetListItems({C,B});Test.TestEqual(TEXT("Removed explicit choice is dropped"),List->GetNumItemsSelected(),0);

 List->SetSelectionMode(ESelectionMode::Multi);List->bSelectionFollowsNavigation=true;
 List->SetSelectedItemIndex(0,false);List->SetSelectedItemIndex(1,false);List->SetDrivenItemIndex(1,false);List->SetDrivenItemIndex(0,false);
 Test.TestEqual(TEXT("Navigation never reduces Multi selection"),List->GetNumItemsSelected(),2);
 List->SetListItems({B,C,A});Test.TestEqual(TEXT("Multi selection survives reorder"),List->GetNumItemsSelected(),2);
 List->SetDrivenItemIndex(2,false);List->ConfirmDrivenItem();
 Test.TestEqual(TEXT("Confirm does not change Multi selection"),List->GetNumItemsSelected(),2);
 List->bSelectionFollowsNavigation=false;List->bConfirmOnItemClick=false;
 // A plain list's hosted clicks toggle exactly once; a list with a second presentation keeps its own click policy.
 if(bPlainList)
 {
  List->HandleHostedItemClick(0);Test.TestEqual(TEXT("Multi click deselects"),List->GetNumItemsSelected(),1);
  List->HandleHostedItemClick(0);Test.TestEqual(TEXT("Multi click selects"),List->GetNumItemsSelected(),2);
  List->SetSelectionMode(ESelectionMode::SingleToggle);List->ClearSelection();
  List->HandleHostedItemClick(0);Test.TestEqual(TEXT("SingleToggle click selects"),List->GetNumItemsSelected(),1);
  List->HandleHostedItemClick(0);Test.TestEqual(TEXT("SingleToggle click deselects"),List->GetNumItemsSelected(),0);
  List->SetSelectionMode(ESelectionMode::Single);List->bSelectionFollowsNavigation=true;
  CastChecked<UReciInteractionTestList>(List)->ReceiveItemFocus(2);
  Test.TestEqual(TEXT("Focus synchronizes ordinary cursor"),List->GetDrivenItemIndex(),2);
  Test.TestTrue(TEXT("Focus updates automatic choice"),List->GetSelectedItem()==A);
  List->bFocusOnNavigation=false;CastChecked<UReciInteractionTestList>(List)->ReceiveItemFocus(0);
  Test.TestEqual(TEXT("Independent selector ignores foreign item focus"),List->GetDrivenItemIndex(),2);
 }
 List->SetDrivenItemIndex(0,false);List->GetHostedEntryButton(0)->SetIsEnabled(false);
 const int32 DisabledBefore=Recorder->ConfirmCount;
 Test.TestFalse(TEXT("Disabled item cannot confirm"),List->ConfirmDrivenItem());
 Test.TestEqual(TEXT("Disabled item emits no action"),Recorder->ConfirmCount,DisabledBefore);
 Test.TestTrue(TEXT("Focus entry skips a disabled item"),List->GetHostedFocusTarget()!=nullptr&&List->GetHostedFocusTarget()!=List->GetHostedEntryButton(0));
 List->SetListItems({});Test.TestFalse(TEXT("Empty list cannot confirm"),List->ConfirmDrivenItem());
 List->ReleaseSlateResources(true);
}

bool FReciInteractionContractTest::RunTest(const FString& Parameters)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 UE::ReciCommonUI::Tests::TestListContract(*this,*World,[World]{auto* List=CreateWidget<UReciInteractionTestList>(World);List->ItemWidgetClass=UReciInteractionTestEntry::StaticClass();return static_cast<UReciListBase*>(List);},true);
 World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDefaultListRoleTest,"ReciCommonUI.Interaction.DefaultListRole",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

void UE::ReciCommonUI::Tests::TestDefaultListRole(FAutomationTestBase& Test,UWorld& World,TFunctionRef<UReciListBase*()> MakeList)
{
 auto* Recorder=NewObject<UReciInteractionTestEntry>(&World);
 UObject* A=NewObject<UReciTextListItemData>(&World);UObject* B=NewObject<UReciTextListItemData>(&World);UObject* C=NewObject<UReciTextListItemData>(&World);
 // Built fresh with no property set: a default list is an explicit single choice (DESIGN.md, Lists).
 auto* List=MakeList();
 const auto Slate=List->TakeWidget();List->SetListItems({A,B,C});
 List->OnItemConfirmed.AddDynamic(Recorder,&UReciInteractionTestEntry::RecordConfirm);
 Test.TestTrue(TEXT("Default Selection Mode is Single"),List->SelectionMode==ESelectionMode::Single);
 Test.TestFalse(TEXT("Default Confirm On Item Click is off"),List->bConfirmOnItemClick);
 // The contract's other safe defaults.
 Test.TestFalse(TEXT("Default Selection Follows Navigation is off"),List->bSelectionFollowsNavigation);
 Test.TestFalse(TEXT("Default Focus On Pointer Hover is off"),List->bFocusOnPointerHover);
 Test.TestFalse(TEXT("Default Clear Selection On Focus Return is off"),List->bClearSelectionOnFocusReturn);
 Test.TestTrue(TEXT("Default Show Driven Item Cursor is on"),List->bShowDrivenItemCursor);
 // A list with a main item counts only a click on it, so every list is clicked on its driven item.
 List->SetDrivenItemIndex(1,false);
 Test.TestEqual(TEXT("Navigation moves only the cursor"),List->GetNumItemsSelected(),0);
 const int32 Before=Recorder->ConfirmCount;
 Test.TestTrue(TEXT("Item takes a click"),List->HandleHostedItemClick(1));
 Test.TestTrue(TEXT("Click selects"),List->GetSelectedItem()==B);
 Test.TestEqual(TEXT("Click emits no completion"),Recorder->ConfirmCount,Before);
 List->SetDrivenItemIndex(2,false);Test.TestTrue(TEXT("Navigation keeps the choice"),List->GetSelectedItem()==B);
 Test.TestTrue(TEXT("Driven item can confirm"),List->ConfirmDrivenItem());
 Test.TestTrue(TEXT("Confirm commits the cursor choice"),List->GetSelectedItem()==C);
 Test.TestEqual(TEXT("Confirm emits once"),Recorder->ConfirmCount,Before+1);
 // Action menu: Selection Mode None with Confirm On Item Click on.
 List->SetSelectionMode(ESelectionMode::None);List->bConfirmOnItemClick=true;
 Test.TestTrue(TEXT("Action item takes a click"),List->HandleHostedItemClick(2));
 Test.TestEqual(TEXT("Action click emits once"),Recorder->ConfirmCount,Before+2);
 Test.TestTrue(TEXT("Action click confirms the clicked item"),Recorder->LastConfirmed==C);
 Test.TestEqual(TEXT("Action click stores no selection"),List->GetNumItemsSelected(),0);
 List->ReleaseSlateResources(true);
}

bool FReciDefaultListRoleTest::RunTest(const FString& Parameters)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 UE::ReciCommonUI::Tests::TestDefaultListRole(*this,*World,[World]{auto* List=CreateWidget<UReciInteractionTestList>(World);List->ItemWidgetClass=UReciInteractionTestEntry::StaticClass();return static_cast<UReciListBase*>(List);});
 World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyCursorTest,"ReciCommonUI.Interaction.KeyCursor",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FReciKeyCursorTest::RunTest(const FString& Parameters)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 UObject* A=NewObject<UReciTextListItemData>(World);UObject* B=NewObject<UReciTextListItemData>(World);UObject* C=NewObject<UReciTextListItemData>(World);
 auto* List=CreateWidget<UReciInteractionTestList>(World);
 List->ItemWidgetClass=UReciInteractionTestEntry::StaticClass();
 TestTrue(TEXT("Show Driven Item Cursor is on by default"),List->bShowDrivenItemCursor);
 // A list that screen keys drive; everything else keeps its default.
 List->bFocusOnNavigation=false;
 const auto Slate=List->TakeWidget();List->SetListItems({A,B,C});
 // Which entries wear the key cursor, one character per entry ("-X-": only the second).
 auto Cursors=[List]
 {
  FString Result;
  for(int32 I=0;I<List->GetItemCount();++I)
  {
   const auto* Entry=Cast<UReciFocusStyleButtonBase>(List->GetHostedEntryButton(I));
   Result+=Entry&&Entry->IsShowingKeyCursor()?TEXT("X"):TEXT("-");
  }
  return Result;
 };
 List->SetDrivenItemIndex(1,false);TestEqual(TEXT("Only the driven item wears the key cursor"),Cursors(),FString(TEXT("-X-")));
 List->SetShowDrivenItemCursor(false);TestEqual(TEXT("A screen can turn the key cursor off"),Cursors(),FString(TEXT("---")));
 List->SetShowDrivenItemCursor(true);List->SetFocusOnNavigation(true);
 TestEqual(TEXT("No key cursor while Focus On Navigation is on"),Cursors(),FString(TEXT("---")));
 List->SetFocusOnNavigation(false);TestEqual(TEXT("Focus On Navigation off redraws the key cursor"),Cursors(),FString(TEXT("-X-")));
 List->SetListItems({B,C,A});TestEqual(TEXT("The key cursor follows the driven object"),Cursors(),FString(TEXT("X--")));
 List->ReleaseSlateResources(true);
 World->DestroyWorld(false);return true;
}
#endif
