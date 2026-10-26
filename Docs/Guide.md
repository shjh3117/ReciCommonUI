# Reci Common UI guide: a menu screen from scratch

English · [한국어](Guide.ko.md)

This guide starts from an empty C++ project and builds one menu screen with Reci Common UI, in the Unreal Editor,
step by step. When you finish, a map's **Composer** pushes one **Screen**. The Screen shows a Reci List of five
entries, each with a label and a value:

- Up and Down move between the entries. Left and Right, or the entry's **−** and **+**, change its value.
- **Reset** (R, or gamepad Y) sets every value back to 0. The list is rebuilt and the focused entry keeps focus.
- Back (Backspace or Escape, or gamepad B) returns focus to the first entry.
- Key hints show the keys of the device in use, and hide on touch.
- Keyboard, gamepad, mouse and touch all work.

The assets follow the structure in [DESIGN.md, 화면 구조](../Plugins/ReciCommonUI/DESIGN.md#화면-구조):
Composer → Screen → shared widgets and data → Reci Common UI. References point down only.

| Layer | What you make |
| --- | --- |
| Composer | `WBP_MenuComposer`: adds the input mapping and pushes the Screen onto its `Stack`. |
| Screen | `WBP_MenuScreen`: the list, Reset, the key hints, initial focus and Back. |
| Shared widgets and data | `W_MenuButton`, `W_MenuEntry`, `W_MenuKeyHint`, `BP_MenuItem`. |
| Input | `IA_Menu*`, `IMC_Menu`, `MenuInputData`. |

You need Unreal Engine 5.8, Visual Studio 2022 with the workload **Game development with C++**, and the
`Plugins/ReciCommonUI` folder of this repository.

## Contents

1. [Project and plugin](#1-project-and-plugin)
2. [Input](#2-input)
3. [Item data](#3-item-data)
4. [Widgets](#4-widgets)
5. [Screen](#5-screen)
6. [Composer](#6-composer)
7. [Map](#7-map)
8. [Check in PIE](#8-check-in-pie)

## Conventions

- The menu and field names are those of the English editor (Edit > Editor Preferences > General > Region & Language
  > **Editor Language** = `English`).
- `Category > Field = value` is a field in the Details panel. Leave every field this guide does not name at its
  default.
- After each change, press **Compile** (Blueprints) and **Save**.
- Assets go under `Content/UI` (`Input`, `Data`, `Widgets`, `Screens`, `Composers`) and `Content/Maps`. Create a
  folder with right-click > **New Folder** in the Content Browser.
- This guide makes no styles. Where a widget needs one, it picks a demo style that ships with the plugin, from
  `/ReciCommonUI/Demo/Style` (`BS_*` button styles, `TS_*` text styles). The Style fields list them as they are. To
  see them in the Content Browser, turn on Content Browser > **Settings** > **Show Plugin Content**.
- Many images of the Details panel show only the rows this guide sets, stacked; the rows between them are left out.
- A new Blank project creates widgets from the Palette with **Is Variable** off (Project Settings > User Interface >
  Authorize Automatic Widget Variable Creation is off). Tick **Is Variable**, at the top of the Details panel, on the
  widgets marked ✓ below: a graph can use only those, and only those list Events.

## 1. Project and plugin

### 1.1 Create a C++ project

In the Epic Games Launcher, launch Unreal Engine 5.8. In the **New Project** window choose **GAMES** > **Blank**,
and under Project Defaults choose **C++**. Enter Project Location and Project Name, then press **Create**.

Keep the project folder's path short (under about 110 characters). The plugin's build files sit deep in the folder,
and Windows stops a build whose paths pass 260 characters.

![New Project window with GAMES > Blank and C++ selected](Images/Guide/project-new-cpp.png)

*The New Project window: GAMES > Blank, Project Defaults C++, Project Location `C:\Projects`, Project Name `MyMenu`.*

### 1.2 Copy the plugin

Close the editor. Copy this repository's `Plugins/ReciCommonUI` folder to `<project folder>/Plugins/ReciCommonUI`.
Create the `Plugins` folder if there is none.

Open `<project>.uproject` and answer **Yes** when the editor asks to rebuild the missing modules. If the build fails,
open `<project>.sln` in Visual Studio, choose **Development Editor** and **Win64**, run Build > **Build Solution**,
and open the `.uproject` again.

Check it in Edit > **Plugins**: type `Reci` in the search field. **Reci Common UI** is ticked. It turns on the
Common UI and Enhanced Input plugins it needs.

![Plugins window searched for Reci](Images/Guide/plugins-reci.png)

*Edit > Plugins with `Reci` in the search field: Reci Common UI (Beta), version 0.1.0, is ticked.*

### 1.3 Project settings

Open Edit > **Project Settings** and set three fields.

Engine > General Settings > Default Classes > **Game Viewport Client Class** = `CommonGameViewportClient`. CommonUI
routes button keys and Back through it.

![General Settings with Game Viewport Client Class](Images/Guide/settings-viewport-client.png)

*Engine - General Settings: Default Classes > Game Viewport Client Class is CommonGameViewportClient.*

Engine > User Interface > Focus > **Render Focus Rule** = `Never`. The button styles draw focus; Slate then draws no
rectangle over them.

![User Interface settings with Render Focus Rule](Images/Guide/settings-render-focus-rule.png)

*Engine - User Interface: Focus > Render Focus Rule is Never.*

Game > Common Input Settings > Input > **Enable Enhanced Input Support** ✓. Input Data on the same page is set in 2.4.

![Common Input Settings with Enhanced Input support on](Images/Guide/settings-enhanced-input.png)

*Game - Common Input Settings: Enable Enhanced Input Support is ticked; Input Data is still None.*

### 1.4 One line in DefaultEngine.ini

Project Settings has no field for this one. Close the editor, open `<project folder>/Config/DefaultEngine.ini` in a
text editor and add the lines below. If a `[ConsoleVariables]` section already exists, add only the second line
under it.

```ini
[ConsoleVariables]
CommonUI.ShouldVirtualAcceptSimulateMouseButton=0
```

With this on, CommonUI turns a gamepad Accept into a click at the mouse cursor. After keyboard or mouse use the cursor
is not over the focused control, so the first Accept is lost. Off, Accept goes to focus.

### 1.5 Restart

Open the project again. Game Viewport Client Class and Enable Enhanced Input Support take effect only after the
restart, and the Enhanced Input fields of 2.3 appear only after it.

Blueprints need nothing more. A C++ module that uses the plugin's widgets adds `ReciCommonUIWidgets` to its
dependencies.

## 2. Input

### 2.1 Input Actions

In `Content/UI/Input`, choose **+ Add** > Input > **Input Action** seven times and name them:

`IA_MenuUp`, `IA_MenuDown`, `IA_MenuLeft`, `IA_MenuRight`, `IA_MenuSelect`, `IA_MenuBack`, `IA_MenuReset`

Leave their fields at the defaults (Value Type `Digital (bool)`).

![Content Browser with the seven Input Actions](Images/Guide/input-actions.png)

*Content > UI > Input holds the seven Input Actions.*

### 2.2 IMC_Menu

In the same folder, choose **+ Add** > Input > **Input Mapping Context** and name it `IMC_Menu`. Open it. Press **+**
beside **Mappings** to add an action row, pick the action, then press the **+** on the action row once per key and
pick the key. Add no Triggers or Modifiers.

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| IA_MenuUp | Up | Gamepad D-pad Up |
| IA_MenuDown | Down | Gamepad D-pad Down |
| IA_MenuLeft | Left | Gamepad D-pad Left |
| IA_MenuRight | Right | Gamepad D-pad Right |
| IA_MenuSelect | Enter | Gamepad Face Button Bottom |
| IA_MenuBack | Escape, Backspace | Gamepad Face Button Right |
| IA_MenuReset | R | Gamepad Face Button Top |

Backspace is there for Play In Editor: there Escape is the editor's Stop key, and CommonUI lets that key through to
the editor.

![IMC_Menu with all mappings expanded](Images/Guide/imc-mappings.png)

*IMC_Menu, every action row expanded: each action has its keyboard key or keys and its gamepad key.*

### 2.3 MenuInputData

In the same folder, choose **+ Add** > **Blueprint Class**. In Pick Parent Class, open **All Classes**, search
`ReciUIInputData`, choose **ReciUIInputData** and press **Select**. Name it `MenuInputData` and open it.

| Properties > Field | Value |
| --- | --- |
| Navigation Up Action | IA_MenuUp |
| Navigation Down Action | IA_MenuDown |
| Navigation Left Action | IA_MenuLeft |
| Navigation Right Action | IA_MenuRight |
| Enhanced Input Click Action | IA_MenuSelect |
| Enhanced Input Back Action | IA_MenuBack |

With this Input Data, Slate's focus navigation follows the keys mapped to the four navigation actions, and the
keys of the click and Back actions act as Accept and Back.

![MenuInputData class defaults](Images/Guide/input-data-defaults.png)

*MenuInputData: the four navigation actions, Enhanced Input Click Action IA_MenuSelect and Enhanced Input Back Action
IA_MenuBack. Mapped Keys Replace Default Keys stays ticked.*

### 2.4 Input Data

Project Settings > Game > Common Input Settings > Input > **Input Data** = `MenuInputData`.

![Common Input Settings with Input Data MenuInputData](Images/Guide/settings-input-data.png)

*Game - Common Input Settings: Input Data is MenuInputData, Enable Enhanced Input Support is ticked.*

Common Input reads Input Data when the editor starts. Restart the editor before you press Play in section 8.

## 3. Item data

In `Content/UI/Data`, choose **+ Add** > **Blueprint Class** > All Classes > **Object** > Select, and name it
`BP_MenuItem`. One item is one entry of the list.

Under My Blueprint > **Variables**, press **+** twice:

| Name | Type | Details |
| --- | --- | --- |
| Label | Text | Instance Editable ✓, Expose on Spawn ✓ |
| Value | Integer | defaults |

Expose on Spawn gives the node that creates an item (5.3) a Label pin.

![BP_MenuItem variables and the Label details](Images/Guide/item-variables.png)

*BP_MenuItem: My Blueprint lists Label (Text) and Value (Integer); the Details panel for Label shows Instance Editable
and Expose on Spawn ticked.*

## 4. Widgets

All three go in `Content/UI/Widgets`. Create each with **+ Add** > User Interface > **Widget Blueprint**, search the
parent under **All Classes** and press **Select**. Build the Hierarchy by dragging widgets from the Palette (type the
name in its search field); drop the first one on the Blueprint's name at the top to make it the root. Rename a widget
in the name field at the top of Details. Your own widgets are in the Palette under **User Created**.

### 4.1 W_MenuButton

A button with a label, for −, + and Reset. Parent: **Reci Focus Style Button Base** (search
`ReciFocusStyleButtonBase`).

Hierarchy:

- **Row** [Horizontal Box]
  - **Label** [Common Text] ✓ — Slot: Size = **Fill**, Horizontal Alignment = **Center**, Vertical Alignment =
    **Center**

![W_MenuButton hierarchy and Label slot](Images/Guide/button-hierarchy.png)

*W_MenuButton: Row > Label. Label has Is Variable ticked, and its slot is Fill, centred both ways.*

Label needs no text style: the button gives every Common Text inside it the text style of its own button style.

In the **Graph** tab, add a variable `LabelText` (Text) with Instance Editable ✓ (the eye icon).

![LabelText variable](Images/Guide/button-variable.png)

*My Blueprint lists the widget variable Label and the variable LabelText (Text, eye open); Details shows LabelText as
Text with Instance Editable ticked.*

Press **Class Defaults** in the toolbar: Style > **Style** = `BS_DemoTonalButton`.

![W_MenuButton class defaults Style](Images/Guide/button-defaults.png)

*Class Defaults of W_MenuButton: Style > Style is BS_DemoTonalButton.*

Event Graph: drag a wire from the grey **Event PreConstruct** and place **SetText (Text)**. Target = `Label` (drag it
from My Blueprint > Variables and choose Get), In Text = `LabelText`.

![W_MenuButton graph](Images/Guide/button-graph.png)

*Event PreConstruct → SetText (Text), with Target Label and In Text LabelText.*

### 4.2 W_MenuEntry

One entry of the list. Parent: **Reci Focus Style Button Base**.

Toolbar **Class Settings** > Details > Interfaces > Implemented Interfaces > **Add** > `Reci List Item`. Compile. The
list hands each entry its item through this interface.

![Implemented Interfaces with Reci List Item](Images/Guide/entry-interface.png)

*Class Settings of W_MenuEntry: Interfaces > Implemented Interfaces lists Reci List Item.*

Hierarchy (Row first, then its four children in this order):

- **Row** [Horizontal Box]
  - **Label** [Common Text] ✓ — Slot: Size = **Fill**, Vertical Alignment = **Center**
  - **Minus** [W_MenuButton] ✓ — Slot: Vertical Alignment = **Center**
  - **Value** [Common Text] ✓ — Appearance > **Min Desired Width** = `64`, **Justification** = Center; Slot:
    Vertical Alignment = **Center**
  - **Plus** [W_MenuButton] ✓ — Slot: Vertical Alignment = **Center**

![W_MenuEntry hierarchy and Label slot](Images/Guide/entry-hierarchy.png)

*W_MenuEntry: Row > Label, Minus, Value, Plus. Label is a variable, its slot Fill and vertically centred.*

Minus: Default > **Label Text** = `−`, Style > **Style** = `BS_DemoIconButton`, Interaction > **Is Focusable** off.
Plus: the same with Label Text `+`. A press on − or + then leaves focus with the entry, so Left and Right keep working
on it.

![Minus details](Images/Guide/entry-minus.png)

*Minus: Label Text −, Style BS_DemoIconButton, Is Focusable off.*

![Value details](Images/Guide/entry-value.png)

*Value: Min Desired Width 64 and Justification Center, so the − and + stay put as the number changes.*

In the Graph tab, add a variable `Item`: in the type list search `BP_MenuItem` and choose **BP Menu Item** >
**Object Reference**.

![Item variable](Images/Guide/entry-variable.png)

*Variable Item of type BP Menu Item.*

Class Defaults: Style > **Style** = `BS_DemoListItem`.

![W_MenuEntry class defaults Style](Images/Guide/entry-defaults.png)

*Class Defaults of W_MenuEntry: Style > Style is BS_DemoListItem.*

Function **Refresh** (My Blueprint > Functions > **+**): show the item.

Refresh → SetText (Text) [Target = `Label` (the widget), In Text = Item's Label] → SetText (Text) [Target = `Value`
(the widget), In Text = Item's Value]

Drag a wire from **Get Item** and search `Get Label` / `Get Value` for the item's fields. Plugging the Integer into
In Text adds **To Text (Integer)** by itself.

![Refresh](Images/Guide/entry-refresh.png)

*Refresh: SetText on Label with Item's Label, then SetText on Value with Item's Value through To Text (Integer).*

Function **Step**: select its entry node and add an input `Delta` (Integer) under Details > Inputs.

Step → Set Value [Target = Item, Value = Item's Value + Delta] → Refresh

Type `+` in the action menu for the add node. Find **Set Value** by dragging a wire from Get Item.

![Step](Images/Guide/entry-step.png)

*Step: SET Value on Item to Item's Value + Delta, then Refresh. Below, the entry node's Details: input Delta, Integer.*

Event Graph:

- Right-click, search `Event Set List Item Object` and place it. Event Set List Item Object → Cast To BP_MenuItem
  [Object = Item Object] → Set Item [Item = As BP Menu Item] → Refresh
- Select Minus in the Designer, press **+** beside Details > Events > **On Clicked**: On Clicked (Minus) → Step
  [Delta = -1]
- The same for Plus: On Clicked (Plus) → Step [Delta = 1]

![W_MenuEntry event graph](Images/Guide/entry-events.png)

*Event Set List Item Object stores the item and calls Refresh; On Clicked of Minus and Plus call Step with -1 and 1.*

### 4.3 W_MenuKeyHint

One line of key hints: the keys of some actions and a label. Parent: **User Widget**.

Hierarchy:

- **Row** [Horizontal Box]
  - **Keys** [Reci Mapped Key Text] ✓ — Appearance > **Style** = `TS_DemoLabelLarge`; Slot: Vertical Alignment =
    **Center**
  - **Label** [Common Text] ✓ — Common Text > **Style** = `TS_DemoBodyMedium`; Slot: Vertical Alignment = **Center**

![W_MenuKeyHint hierarchy, Keys and Label](Images/Guide/keyhint-hierarchy.png)

*W_MenuKeyHint: Row > Keys, Label. Keys: vertically centred, Style TS_DemoLabelLarge. Label: vertically centred,
Style TS_DemoBodyMedium.*

Variables, both Instance Editable ✓:

| Name | Type |
| --- | --- |
| Actions | Input Action > Object Reference, as an **Array** (the icon beside the type) |
| LabelText | Text |

![W_MenuKeyHint variables](Images/Guide/keyhint-variables.png)

*Actions is an array of Input Action, Instance Editable ticked; LabelText is Text.*

Event Graph:

- Event PreConstruct → SetText (Text) [Target = `Label`, In Text = `LabelText`] → Set Enhanced Input Actions [Target =
  `Keys`, In Actions = `Actions`]
- Select Keys, Details > Events > **On Shows Keys Changed** → Branch [Condition = Shows Keys]
  - True → Set Visibility [Target = self, In Visibility = Not Hit-Testable (Self Only)]
  - False → Set Visibility [Target = self, In Visibility = Hidden]

Keys shows the current device's keys for those actions. On touch it shows none, and the line hides while keeping its
space, so nothing above it moves.

![W_MenuKeyHint graph](Images/Guide/keyhint-graph.png)

*Top: PreConstruct sets the label and passes Actions to Keys. Bottom: On Shows Keys Changed shows or hides the line.*

## 5. Screen

### 5.1 WBP_MenuScreen

In `Content/UI/Screens`, create a Widget Blueprint with parent **Common Activatable Widget** (search
`CommonActivatableWidget`) and name it `WBP_MenuScreen`.

Class Defaults: Back > **Is Back Handler** ✓. Leave Activation > **Auto Activate** off: the Composer's Stack activates
the screen.

![Is Back Handler and Auto Activate](Images/Guide/screen-defaults.png)

*Class Defaults, found with the Details search: Back > Is Back Handler ticked; Activation > Auto Activate off.*

### 5.2 Hierarchy

- **Root** [Overlay]
  - **Panel** [Size Box] — Child Layout > **Width Override** ✓ `640`; Slot: Horizontal Alignment = **Center**,
    Vertical Alignment = **Center**
    - **Menu** [Vertical Box]
      - **EntriesBox** [Reci Vertical Box]
        - **Items** [Reci List] ✓ — List Entries > **Entry Widget Class** = `W_MenuEntry`; Selection > **Selection
          Mode** = None
      - **Reset** [W_MenuButton] ✓ — Default > **Label Text** = `Reset`; Input > **Triggering Enhanced Input Action**
        = `IA_MenuReset`; Slot: Horizontal Alignment = **Left**
      - **MoveHint**, **ChangeHint**, **ResetHint**, **BackHint** [W_MenuKeyHint] — the table below

The Reci List goes inside a Reci Vertical Box. The box lays the entries out and handles keys for the list. Selection
Mode None keeps a click or Enter from marking an entry as selected: these entries hold values, not a choice.

![WBP_MenuScreen hierarchy](Images/Guide/screen-hierarchy.png)

*WBP_MenuScreen: Root > Panel > Menu > EntriesBox > Items, then Reset and the four hints in Menu.*

![Panel details](Images/Guide/screen-panel.png)

*Panel: centred both ways in Root, Width Override 640.*

![Items details](Images/Guide/screen-items.png)

*Items: Is Variable ticked, Entry Widget Class W_MenuEntry, Selection Mode None.*

![Reset details](Images/Guide/screen-reset.png)

*Reset: Horizontal Alignment Left, Label Text Reset, Triggering Enhanced Input Action IA_MenuReset.*

With Triggering Enhanced Input Action set, the keys of IA_MenuReset press this button while the screen is active.

Key hints (Default > **Actions**: press **+** per element, then pick the action; Default > **Label Text**):

| Widget | Actions | Label Text |
| --- | --- | --- |
| MoveHint | IA_MenuUp, IA_MenuDown | `Move` |
| ChangeHint | IA_MenuLeft, IA_MenuRight | `Change value` |
| ResetHint | IA_MenuReset | `Reset` |
| BackHint | IA_MenuBack | `Go to first entry` |

![MoveHint details](Images/Guide/screen-hint.png)

*MoveHint: Actions holds IA_MenuUp and IA_MenuDown, Label Text Move.*

### 5.3 Items and initial focus

In the Graph tab, add a variable `ItemData`: type **BP Menu Item** > Object Reference, as an **Array**.

![ItemData variable](Images/Guide/screen-variable.png)

*My Blueprint lists Reset, Items and ItemData; ItemData is an array of BP Menu Item.*

Event Graph, from the grey **Event Construct**:

Event Construct → For Loop [First Index = 1, Last Index = 5]

- Loop Body → Construct Object from Class [Class = BP_MenuItem; the node becomes **Construct BP Menu Item**; Label =
  Format Text's Result] → Add [Target Array = ItemData, New Item = Return Value]
  - Format Text: Format = `Item {Index}`. Its Index pin appears once you type the format; connect For Loop's Index.
  - Add: drag a wire from **Get ItemData** and choose `Add`.
- Completed → Set List Items [Target = Items, In List Items = ItemData] → Set Driven Item Index [Target = Items, Item
  Index = 0, Give Focus ✓]

Set List Items gives the list its items; the list makes one W_MenuEntry for each. Set Driven Item Index focuses Item
1 when the screen opens.

![Event Construct](Images/Guide/screen-construct.png)

*Event Construct makes five items named Item 1 to Item 5, gives them to Items, and focuses the first entry.*

### 5.4 Reset

Select Reset, Details > Events > **On Clicked**:

On Clicked (Reset) → For Each Loop [Array = ItemData]

- Loop Body → Set Value [Target = Array Element, Value = 0]
- Completed → Set List Items [Target = Items, In List Items = ItemData]

Set List Items rebuilds the entries. When focus was in the list, the list gives it back to the same entry, so R keeps
you where you were.

![Reset graph](Images/Guide/screen-reset-graph.png)

*On Clicked (Reset) sets every item's Value to 0, then Set List Items rebuilds the list.*

### 5.5 Left and Right

Select Items, Details > Events > **On Navigation Input**:

On Navigation Input (Items) → Switch on EReciNavigationDirection [Selection = Direction]

- Left → Cast To W_MenuEntry [Object = Get Hosted Entry Button's Return Value] → Step [Target = As W Menu Entry,
  Delta = -1]
  - Get Hosted Entry Button [Target = Items, Item Index = Get Driven Item Index (Target = Items)]
- Right → a second Cast To W_MenuEntry from the same Get Hosted Entry Button → Step [Delta = 1]

Leave Up and Down unconnected: the list moves focus itself.

![On Navigation Input](Images/Guide/screen-navigation.png)

*Left and Right call Step on the list's driven entry with -1 and 1.*

### 5.6 Focus target

My Blueprint > Functions > **Override** > **Get Desired Focus Target**:

Get Desired Focus Target → Return Node [Return Value = Keep Focus Or Target (Target = Items > Get Hosted Focus
Target)]

CommonUI asks this when the screen becomes active and when the input device changes. Keep Focus Or Target keeps
focus where it is while a control of this screen holds it, and otherwise gives the list's current entry.

![Get Desired Focus Target](Images/Guide/screen-focus-target.png)

*Get Desired Focus Target returns Keep Focus Or Target, with Target from Items > Get Hosted Focus Target.*

### 5.7 Back

My Blueprint > Functions > **Override** > **On Handle Back Action**:

On Handle Back Action → Set Driven Item Index [Target = Items, Item Index = 0, Give Focus ✓] → Return Node [Return
Value ✓]

Is Back Handler (5.1) sends Back here. Returning true tells CommonUI that the screen handled it; without this
override, Back would deactivate the screen and the Stack would remove it.

![On Handle Back Action](Images/Guide/screen-back.png)

*On Handle Back Action focuses the first entry and returns true.*

## 6. Composer

The Composer is the top widget of the map. It applies the input mapping and pushes the first Screen onto its Stack.

In `Content/UI/Composers`, create a Widget Blueprint with parent **Common Activatable Widget** and name it
`WBP_MenuComposer`.

Class Defaults: Activation > **Auto Activate** ✓. The Composer activates itself once it is in the viewport.

![Composer Auto Activate](Images/Guide/composer-defaults.png)

*Class Defaults of WBP_MenuComposer, found with the Details search: Activation > Auto Activate ticked.*

Hierarchy:

- **Root** [Overlay]
  - **Stack** [Common Activatable Widget Stack] ✓ — Slot: Horizontal Alignment = **Fill**, Vertical Alignment =
    **Fill**

![Composer hierarchy](Images/Guide/composer-hierarchy.png)

*WBP_MenuComposer: Root > Stack. Stack is a variable and fills Root both ways.*

Event Graph:

Event Construct → Add Mapping Context [Target = Enhanced Input Local Player Subsystem; Mapping Context = IMC_Menu] →
Push Widget [Target = Stack; Activatable Widget Class = WBP_MenuScreen]

- For the subsystem, right-click, place **Get Owning Player**, drag a wire from its Return Value and choose **Get
  EnhancedInputLocalPlayerSubsystem**.
- Push Widget: drag a wire from **Get Stack** and search it.

![Composer Event Construct](Images/Guide/composer-construct.png)

*Event Construct adds IMC_Menu to the owning player's Enhanced Input subsystem and pushes WBP_MenuScreen onto Stack.*

## 7. Map

### 7.1 MenuMap

File > **New Level** > **Empty Level** > Create. File > **Save Current Level As** > `Content/Maps`, name `MenuMap`.

### 7.2 Level Blueprint

Toolbar Blueprints button > **Open Level Blueprint**. Place one **Get Player Controller** [Player Index = 0]; its
Return Value (PC) feeds three nodes.

Event BeginPlay → Create Widget [Class = WBP_MenuComposer, Owning Player = PC] → Add to Viewport [Target = Return
Value] → Set Input Mode UI Only [Player Controller = PC] → Set Show Mouse Cursor [Target = PC, Show Mouse Cursor ✓]

Set Show Mouse Cursor: drag a wire from PC and search it.

![Level Blueprint](Images/Guide/level-blueprint.png)

*Event BeginPlay creates the Composer, adds it to the viewport, switches to UI input and shows the mouse cursor.*

### 7.3 Default maps

Project Settings > Project > Maps & Modes > Default Maps: **Editor Startup Map** = `MenuMap`, **Game Default Map** =
`MenuMap`.

![Maps & Modes](Images/Guide/settings-maps.png)

*Project - Maps & Modes: Editor Startup Map and Game Default Map are MenuMap.*

## 8. Check in PIE

Restart the editor if you have not since 2.4, open MenuMap and press **Play**.

![PIE with the keyboard](Images/Guide/pie-keyboard.png)

*The start: Item 1 has focus, every value is 0, and the hints show keyboard keys (↑ / ↓, ← / →, R, ESC / BS).*

Check each of these:

- **Keyboard**: Up and Down move between the entries. Down from Item 5 reaches Reset. Left and Right change the
  focused entry's value. R resets every value, and the same entry keeps focus. Backspace returns to Item 1.
- **Gamepad**: the D-pad moves and changes values, Y resets, B returns to Item 1. The hints switch to gamepad keys.
- **Mouse**: a click on − or + changes that entry's value and focuses the entry. A click on Reset resets.
- **Touch**: a tap on − or + changes the value, and the hints hide. To try it on a PC, turn on Edit > Editor
  Preferences > Level Editor > Play > Play in Editor > **Use Mouse for Touch**, and turn it off again afterwards.

![PIE with a gamepad](Images/Guide/pie-gamepad.png)

*After D-pad Down and D-pad Right twice: Item 2 has focus and the value 2; the hints show D↑ / D↓, D← / D→, Y and B.*

![PIE with touch](Images/Guide/pie-touch.png)

*After a tap on + of Item 3: its value is 1, Item 3 has focus, and the hints are hidden.*

In Play In Editor, Escape stops the session: CommonUI lets the editor's Stop key through. In a Standalone Game or a
packaged build, Escape acts as Back.
