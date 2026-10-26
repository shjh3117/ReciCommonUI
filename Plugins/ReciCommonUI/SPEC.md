# Reci Common UI implementation spec

English · [한국어](SPEC.ko.md)

This file states what the plugin provides and how to assemble UI with it. It is written for people and AI agents
that build screens. [DESIGN.md](DESIGN.md) is the reference for the rules and their reasons.

Names are those the Unreal Editor shows (Details, Palette, Blueprint nodes). The C++ name follows in parentheses
where it differs.

## Setup

All of these are required:

| Where | Setting |
| --- | --- |
| Engine > General Settings > Default Classes | **Game Viewport Client Class** = `CommonGameViewportClient` (or a subclass) |
| Engine > User Interface > Focus | **Render Focus Rule** = `Never` |
| Game > Common Input Settings > Input | **Enable Enhanced Input Support** ✓; **Input Data** = a Blueprint subclass of `ReciUIInputData` |
| `Config/DefaultEngine.ini` | `[ConsoleVariables]` `CommonUI.ShouldVirtualAcceptSimulateMouseButton=0` |

- **The Input Data subclass** sets Navigation Up/Down/Left/Right Action, Enhanced Input Click Action and Enhanced
  Input Back Action. Mapped Keys Replace Default Keys stays ✓.
- **Restart the editor** after you change the Viewport Client, Enhanced Input support or Input Data.
- **C++ modules** that use the widgets depend on `ReciCommonUIWidgets`. Blueprints need nothing.
- **Optional:** Project Settings > Plugins > Reci Common UI has Tab Navigation (on), Stick Dominant Axis (on) and
  Default Motion Scale (1).

## Structure

Build every UI in four layers. References point down only; a lower layer asks upward through event dispatchers.

| Layer | Parent class | Owns |
| --- | --- | --- |
| Composer | Common Activatable Widget, **Auto Activate** ✓ | One per map. Adds the UI Input Mapping Context. Holds `Stack` (and `ModalStack` above it), both Common Activatable Widget Stack. Pushes the first Screen in Construct. Binds the Screen's `On…` dispatchers. Pushes modals. Changes maps. |
| Screen | Common Activatable Widget, Auto Activate off | One screen: layout, input, initial focus, Back. Asks for anything else through `On…` dispatchers. |
| Shared widgets and data | Reci Focus Style Button Base (buttons, entries), User Widget (display), Object (item data) | Entries, buttons, hints, item data. Never Common Activatable Widget. |
| Reci Common UI | C++ | Lists, scrolling, focus, screen input, key hints. |

- **Level Blueprint**, BeginPlay: Create Widget (Composer, Owning Player = player controller 0) → Add to Viewport →
  Set Input Mode UI Only → Set Show Mouse Cursor ✓.
- **Composer**, Construct: Get Owning Player → Get EnhancedInputLocalPlayerSubsystem → Add Mapping Context → Stack
  Push Widget (Screen class).

## Classes

### Core module (ReciCommonUI)

| Class | Use | Members you set or call |
| --- | --- | --- |
| **Reci List** (`UReciList`) | A list with one live widget per item. | Entry Widget Class (`ItemWidgetClass`), Selection Mode, Focus On Navigation, Selection Follows Navigation, Confirm On Item Click, Allow Pointer Hover, Focus On Pointer Hover, Show Driven Item Cursor, Looping Entries. Set List Items, Set Text List Items, Add/Insert/Remove Item, Clear List Items, Set Driven Item Index (Item Index, Give Focus), Get Driven Item Index, Get Hosted Focus Target, Get Hosted Entry Button, Set/Get Focused Item (Index), Set/Get Selected Item (Index), Confirm Focused Item. Events: On Item Selected, On Item Selection Changed, On Focused Item Changed, On Driven Item Changed, On Item Clicked, On Item Confirmed, On Navigation Input (Direction). |
| **Reci Vertical Box**, **Reci Horizontal Box**, **Reci Tile Box** | The list's layout host and key handler. | Item Spacing, Wrap Navigation. Horizontal: Fill Entries. Tile: Entry Size, Fixed Columns, Flow Orientation. |
| **Reci Scroll Box** | A viewport for a layout box. | Drag Trigger Distance, Wheel Scroll Amount, Navigate With Mouse Wheel, Scroll Motion, Inertia Deceleration. |
| **Reci Focus Style Button Base** (`UReciFocusStyleButtonBase`) | The base of every button and entry. Draws focus with its CommonUI Style. | Style, Focus On Hover, Hint Input Action, Show Triggering Action In Action Bar, Max Display Scale. Set Key Cursor Visual, Set Display Scale. CommonUI's Triggering Enhanced Input Action applies. |
| **Reci Mapped Key Text** (`UReciMappedKeyText`) | Shows the current device's keys of input actions. | Enhanced Input Action, Additional Input Actions, Key Device, Style, Hide When Unmapped (keep ✓), Key Capture Hint, Key Name Overrides. Set Enhanced Input Actions, Shows Keys. Event: On Shows Keys Changed. |
| **Reci Text Block** | Common Text with a line limit. | Max Lines. |
| **Reci Dropdown Base**, **Reci Dropdown Popup** | A dropdown and its popup. | Dropdown Popup Class, Dropdown Entry Widget Class. Set Dropdown Options, Set Dropdown Enum, Set Selected Index, Open/Close/Toggle Dropdown. Events: On Selection Changed, On Dropdown Open Changed. |
| **Reci UI Input Data** (`UReciUIInputData`) | The Input Data class (see Setup). | Navigation Up/Down/Left/Right Action, Mapped Keys Replace Default Keys. |
| Interface **Reci List Item** | Implemented by entry widgets. | Event Set List Item Object (Item Object). |
| Interface **Reci Text List Item** | Entries of Set Text List Items and of dropdowns. | Event Set List Item Text. Base class: Reci Text List Item Button Base (binds `ItemTextBlock`). |
| Interface **Reci List Entry Emphasis** | Optional, on entries. | Event On Item Emphasis Changed (0–1, driven by a viewport). |
| Interface **Reci Screen Input** (`IReciScreenInput`) | Screen keys that stock routing cannot deliver. | Handle Reci Screen Action, Is Reci Screen Action Suspended, Handle Reci Screen Key, Is Capturing Reci Screen Keys. Library: Register Screen Input, Unregister Screen Input, Check Screen Reachability. |
| Library | Helpers. | Keep Focus Or Target (Screen, Target), Is Key Mapped To Action. |
| **Reci Motion Subsystem** | Per-player motion scale. | Set Motion Scale, Get Motion Scale. |

### Optional module (ReciCommonUIWidgets)

| Class | Use | Members |
| --- | --- | --- |
| **Reci Carousel Scroll Box** | A looping carousel that centres the selected item. | Snap To Closest Item, Snap Delay, Center Motion, Presentation Blend Distance. Set Centered Item Index, Get Centered Item (Index). Event: On Centered Item Changed. |
| **Reci Advanced List** | A Reci List with a second, main presentation per item. | Main Entry Widget Class, Initial Main Item Index, Main Presentation Follows Focus, Confirm Main Item On Click. |
| **Reci Two Level List Base**, **Reci Two Level Item Base**, interface **Reci Two Level Entry** | Parent rows, each with a child list on the other axis. | List binds `ParentList`, row binds `ChildList` (BindWidget). Set Items (Items, Child Count provider), Synchronize Child Selection, Select Child, Confirm Selection. Events: On Focused Parent Changed, On Selection Changed, On Confirmed. Entry: Set Child Item, Update Two Level State. |

## Recipes

### List in a Screen

1. **Hierarchy:** Reci Vertical Box (or Horizontal or Tile) > **Reci List**, marked Is Variable. For scrolling, use
   Reci Scroll Box > Reci Vertical Box > Reci List; the box is the scroll box's only child.
2. **List:** set **Entry Widget Class**. Set **Selection Mode** by role, from [DESIGN.md, Lists](DESIGN.md#lists):
   None for action menus and value editors, Single for one choice, Multi for a set.
3. **Items:** in Construct, build the item objects, then call **Set List Items**. To change items later, edit the
   objects and call Set List Items again. Entries are rebuilt, and focus returns to the same item.
4. **Value changes from keys:** handle Left and Right in **On Navigation Input** (Switch on EReciNavigationDirection).
   Act on Get Hosted Entry Button (Get Driven Item Index). Leave the list's own axis unconnected.

### Entry widget

- **Parent and interface:** parent Reci Focus Style Button Base; Class Settings > Implemented Interfaces adds
  **Reci List Item**.
- **Item:** Event Set List Item Object casts Item Object to the item class, stores it, and refreshes the display.
- **Nested buttons** (−, +): Is Focusable off, so focus stays on the entry.
- **Style:** set Style in Class Defaults. Child Common Texts take the button style's text style.

### Button with its own key

- **Own key:** set **Triggering Enhanced Input Action**. Its keys press the button while its screen is active.
- **Key shown on the button:** add a Reci Mapped Key Text named exactly `InputActionWidget` to the button's tree.
- **Key the screen handles instead** (for example Back): set Hint Input Action, which is display only.

### Key hint line

- **Widget:** Reci Mapped Key Text with the actions (Set Enhanced Input Actions, or Enhanced Input Action plus
  Additional Input Actions).
- **Hide while keeping space:** in On Shows Keys Changed, set Visibility to Hidden when Shows Keys is false.
  On touch there are no keys.
- **Which keys:** put the keys that drive a list here, not on buttons.

### Initial focus

- **Override Get Desired Focus Target** to return **Keep Focus Or Target** (Target = list > Get Hosted Focus
  Target, or the authored control).
- **Start focus:** in Construct, after Set List Items, call **Set Driven Item Index** (0, Give Focus ✓). Get
  Desired Focus Target alone does not focus at start.
- **Desired Focus Widget:** leave it unset.

### Back

- **Screen Class Defaults:** **Is Back Handler** ✓.
- **Override On Handle Back Action:** do the screen's Back, then return true. Without it, Back deactivates the screen
  and the Stack pops it.
- **Back mapping:** map Back to Escape and to Backspace. In Play In Editor, Escape is the editor's Stop key and
  reaches the editor, so test Back with Backspace.

### Modal dialog

- **Dialog:** a Common Activatable Widget with **Is Modal** ✓ and Is Back Handler ✓.
- **Request:** the Screen raises an `On…Requested` dispatcher. The Composer pushes the dialog onto `ModalStack`,
  binds the dialog's close dispatcher (`On Closed` in the demo), and passes the result back through a Screen
  function.
- **Underneath:** while the dialog is open, the Screen gets no input and its hints show no keys.

### Screen keys (only when stock routing cannot do it)

Use this, for example, for a key that drives a list while focus is elsewhere, or for a press timed at key-down.

1. **Implement** **Reci Screen Input** on the Screen.
2. **Register:** call **Register Screen Input** (self) on activation and **Unregister Screen Input** on deactivation.
3. **Mapping:** the actions of the Screen's Input Mapping reach **Handle Reci Screen Action**. Return true for every
   action you handle. Triggers and modifiers do not apply.
4. **Key capture** (key-binding editors): use Handle Reci Screen Key while Is Capturing Reci Screen Keys is true.
5. **Check:** use **Check Screen Reachability** to find controls or actions without a key or hint.

## Do not

- **Layer classes:**
  - Do not use CommonUI List View or Tile View for lists whose entries hold controls. Use Reci List.
  - Do not derive entries or shared widgets from Common Activatable Widget. Reci takes the nearest one as the
    widget's screen.
- **Keys:**
  - Do not write key names in text. Every key comes from an input action through Reci Mapped Key Text.
  - Do not turn off Hide When Unmapped.
  - Do not build a button's key from Hint Input Action plus a screen branch. Use Triggering Enhanced Input Action.
- **Focus:** on Screens and dialogs, leave Class Defaults > Interaction > **Desired Focus Widget** empty. Keep Focus Or
  Target returns None to keep focus where it is, and CommonUI then focuses Desired Focus Widget instead. This is to
  change so that Desired Focus Widget can be used ([DESIGN.md, Known deviations](DESIGN.md#known-deviations)).
- **Long lists:** Reci List keeps every entry alive. For long lists use a List View without controls inside entries.

## Verify

- **Log:** Reci reports each setup it cannot handle once, as a Blueprint compile warning or in `LogReciCommonUI`.
  Fix every one.
- **Play:** run in Play In Editor with keyboard, gamepad, mouse and touch (Editor Preferences > Play > Use Mouse for
  Touch):
  - focus is visible at start;
  - every control is reachable by direction keys;
  - hints switch with the device and hide on touch;
  - Back works (Backspace in PIE).
