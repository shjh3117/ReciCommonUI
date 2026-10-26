// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciInteractionTestEntry.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "Widgets/ReciAdvancedList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTextListItem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciAdvancedListContractTest,"ReciCommonUI.Interaction.AdvancedListContract",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FReciAdvancedListContractTest::RunTest(const FString& Parameters)
{
 // A Reci Advanced List keeps the list roles and defaults (DESIGN.md, Lists) as a Reci List does, with its second
 // presentation per item.
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 auto MakeList=[World]
 {
  auto* List=CreateWidget<UReciAdvancedList>(World);
  List->ItemWidgetClass=UReciInteractionTestEntry::StaticClass();List->MainEntryWidgetClass=UReciInteractionTestEntry::StaticClass();
  return static_cast<UReciListBase*>(List);
 };
 UE::ReciCommonUI::Tests::TestListContract(*this,*World,MakeList,false);
 UE::ReciCommonUI::Tests::TestDefaultListRole(*this,*World,MakeList);
 World->DestroyWorld(false);return true;
}

// A Reci Advanced List's driven item is its main item. Focus leaves the main first item, and the main item moves to the
// third while focus is away (Set Driven Item Index without focus, as a screen key or a carousel moves it). Focus that comes
// back lands on the third item's main presentation, which stays the main item; the first item, now in its normal
// presentation, takes no focus on the way.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciAdvancedListFocusReturnTest,"ReciCommonUI.Interaction.AdvancedListFocusReturn",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FReciAdvancedListFocusReturnTest::RunTest(const FString& Parameters)
{
 FReciTestHarness Harness(*this);
 if(!Harness.IsValid())return false;
 UReciTestScreen* Screen=Harness.CreateScreen(TEXT("Screen"));
 Harness.ActivateScreen(*Screen);
 UWidgetTree& Tree=*Screen->WidgetTree;
 auto* Layout=Tree.ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(),TEXT("Layout"));
 auto* List=Tree.ConstructWidget<UReciAdvancedList>(UReciAdvancedList::StaticClass(),TEXT("Entries"));
 List->ItemWidgetClass=UReciTestListEntry::StaticClass();List->MainEntryWidgetClass=UReciTestListEntry::StaticClass();
 Layout->SetContent(List);Screen->GetContent()->AddChild(Layout);
 // Down from the widget beside the list moves focus to the list, by an explicit rule: Slate does not hit-test the harness window.
 UWidget* Beside=Harness.CreateTarget(TEXT("Beside"),Screen);
 Beside->SetNavigationRuleExplicit(EUINavigation::Down,List);
 TArray<UObject*> Items;
 for(int32 Index=0;Index<4;++Index)Items.Add(NewObject<UReciTextListItemData>(List));
 List->SetListItems(Items);
 Harness.Paint();
 APlayerController* Player=Harness.GetPlayerController();
 const auto EntryHasFocus=[List,Player](const int32 Index)
 {
  const UWidget* Button=List->GetHostedEntryButton(Index);
  return List->GetFocusedItemIndex()==Index&&Button&&(Button->HasUserFocus(Player)||Button->HasUserFocusedDescendants(Player));
 };
 TArray<int32> Reported;
 const FDelegateHandle Handle=List->OnFocusedIndexChangedNative().AddLambda([&Reported](const int32 Index){if(Index!=INDEX_NONE)Reported.Add(Index);});

 TestTrue(TEXT("The main first item takes focus"),List->GetDrivenItemIndex()==0&&List->SetFocusedItemIndex(0)&&EntryHasFocus(0));
 TestTrue(TEXT("Focus moves beside the list"),Harness.Focus(*Beside));
 TestTrue(TEXT("Set Driven Item Index without focus makes the third item main"),List->SetDrivenItemIndex(2,false));
 const UWidget* MainButton=List->GetHostedEntryButton(2);
 Reported.Reset();
 Harness.SlateKeyDown(EKeys::Down);
 Harness.SlateKeyUp(EKeys::Down);
 TestTrue(TEXT("Down returns focus to the third item"),EntryHasFocus(2));
 TestTrue(TEXT("on its main presentation"),MainButton&&List->GetHostedEntryButton(2)==MainButton);
 TestEqual(TEXT("which stays the main item"),List->GetDrivenItemIndex(),2);
 TestTrue(TEXT("The list reports focus on the third item alone"),Reported.Num()==1&&Reported[0]==2);
 List->OnFocusedIndexChangedNative().Remove(Handle);
 return true;
}
#endif
