# Reci Common UI 구현 스펙

[English](SPEC.md) · 한국어

이 문서는 플러그인이 무엇을 제공하는지, 그것으로 UI를 어떻게 조립하는지 적습니다. screen을 만드는 사람과 AI
agent를 위한 문서입니다. 규칙과 그 이유는 참고 문서 [DESIGN.md](DESIGN.md)에 있습니다.

이름은 Unreal Editor가 보여 주는 그대로입니다(Details, Palette, Blueprint node). C++ 이름이 다르면 괄호 안에
적습니다.

## 설정

모두 필요합니다.

| 위치 | 설정 |
| --- | --- |
| Engine > General Settings > Default Classes | **Game Viewport Client Class** = `CommonGameViewportClient`(또는 그 subclass) |
| Engine > User Interface > Focus | **Render Focus Rule** = `Never` |
| Game > Common Input Settings > Input | **Enable Enhanced Input Support** ✓, **Input Data** = `ReciUIInputData`의 Blueprint subclass |
| `Config/DefaultEngine.ini` | `[ConsoleVariables]` `CommonUI.ShouldVirtualAcceptSimulateMouseButton=0` |

- **Input Data subclass:** Navigation Up/Down/Left/Right Action, Enhanced Input Click Action, Enhanced Input Back
  Action을 지정합니다. Mapped Keys Replace Default Keys는 ✓로 둡니다.
- **에디터 재시작:** Viewport Client, Enhanced Input support, Input Data를 바꾸면 에디터를 다시 시작합니다.
- **C++ module:** widget을 쓰는 C++ module은 `ReciCommonUIWidgets`에 의존합니다. Blueprint는 아무것도 필요
  없습니다.
- **선택 설정:** Project Settings > Plugins > Reci Common UI에 Tab Navigation(켜짐), Stick Dominant
  Axis(켜짐), Default Motion Scale(1)이 있습니다.

## 구조

모든 UI는 네 layer로 만듭니다. 참조는 아래로만 향하고, 아래 layer는 event dispatcher로 위에 요청합니다.

| Layer | 부모 class | 맡는 것 |
| --- | --- | --- |
| Composer | Common Activatable Widget, **Auto Activate** ✓ | map마다 하나입니다. UI Input Mapping Context를 추가합니다. `Stack`과 그 위의 `ModalStack`(둘 다 Common Activatable Widget Stack)을 가집니다. Construct에서 첫 Screen을 push합니다. Screen의 `On…` dispatcher를 연결합니다. modal을 push합니다. map을 바꿉니다. |
| Screen | Common Activatable Widget, Auto Activate 꺼짐 | 한 화면의 layout, 입력, 초기 focus, Back입니다. 그 밖의 일은 `On…` dispatcher로 요청합니다. |
| 공용 widget·data | Reci Focus Style Button Base(button, entry), User Widget(표시), Object(item data) | entry, button, hint, item data입니다. Common Activatable Widget을 상속하지 않습니다. |
| Reci Common UI | C++ | list, scrolling, focus, screen input, key hint입니다. |

- **Level Blueprint**, BeginPlay: Create Widget(Composer, Owning Player = player controller 0) → Add to Viewport
  → Set Input Mode UI Only → Set Show Mouse Cursor ✓.
- **Composer**, Construct: Get Owning Player → Get EnhancedInputLocalPlayerSubsystem → Add Mapping Context →
  Stack의 Push Widget(Screen class).

## Class

### Core module (ReciCommonUI)

| Class | 용도 | 설정하거나 호출하는 member |
| --- | --- | --- |
| **Reci List** (`UReciList`) | item마다 살아 있는 widget을 두는 list입니다. | Entry Widget Class(`ItemWidgetClass`), Selection Mode, Focus On Navigation, Selection Follows Navigation, Confirm On Item Click, Allow Pointer Hover, Focus On Pointer Hover, Show Driven Item Cursor, Looping Entries. Set List Items, Set Text List Items, Add/Insert/Remove Item, Clear List Items, Set Driven Item Index(Item Index, Give Focus), Get Driven Item Index, Get Hosted Focus Target, Get Hosted Entry Button, Set/Get Focused Item (Index), Set/Get Selected Item (Index), Confirm Focused Item. Event: On Item Selected, On Item Selection Changed, On Focused Item Changed, On Driven Item Changed, On Item Clicked, On Item Confirmed, On Navigation Input(Direction). |
| **Reci Vertical Box**, **Reci Horizontal Box**, **Reci Tile Box** | list의 layout host이자 key handler입니다. | Item Spacing, Wrap Navigation. Horizontal: Fill Entries. Tile: Entry Size, Fixed Columns, Flow Orientation. |
| **Reci Scroll Box** | layout box를 담는 viewport입니다. | Drag Trigger Distance, Wheel Scroll Amount, Navigate With Mouse Wheel, Scroll Motion, Inertia Deceleration. |
| **Reci Focus Style Button Base** (`UReciFocusStyleButtonBase`) | 모든 button과 entry의 base입니다. CommonUI Style로 focus를 그립니다. | Style, Focus On Hover, Hint Input Action, Show Triggering Action In Action Bar, Max Display Scale. Set Key Cursor Visual, Set Display Scale. CommonUI의 Triggering Enhanced Input Action이 적용됩니다. |
| **Reci Mapped Key Text** (`UReciMappedKeyText`) | input action의 현재 device key를 보여 줍니다. | Enhanced Input Action, Additional Input Actions, Key Device, Style, Hide When Unmapped(✓ 유지), Key Capture Hint, Key Name Overrides. Set Enhanced Input Actions, Shows Keys. Event: On Shows Keys Changed. |
| **Reci Text Block** | 줄 수 제한이 있는 Common Text입니다. | Max Lines. |
| **Reci Dropdown Base**, **Reci Dropdown Popup** | dropdown과 그 popup입니다. | Dropdown Popup Class, Dropdown Entry Widget Class. Set Dropdown Options, Set Dropdown Enum, Set Selected Index, Open/Close/Toggle Dropdown. Event: On Selection Changed, On Dropdown Open Changed. |
| **Reci UI Input Data** (`UReciUIInputData`) | Input Data class입니다(설정 참고). | Navigation Up/Down/Left/Right Action, Mapped Keys Replace Default Keys. |
| Interface **Reci List Item** | entry widget이 구현합니다. | Event Set List Item Object(Item Object). |
| Interface **Reci Text List Item** | Set Text List Items와 dropdown의 entry입니다. | Event Set List Item Text. base class: Reci Text List Item Button Base(`ItemTextBlock` bind). |
| Interface **Reci List Entry Emphasis** | 선택 사항이며 entry에 둡니다. | Event On Item Emphasis Changed(0–1, viewport가 drive). |
| Interface **Reci Screen Input** (`IReciScreenInput`) | stock routing이 전달할 수 없는 screen key입니다. | Handle Reci Screen Action, Is Reci Screen Action Suspended, Handle Reci Screen Key, Is Capturing Reci Screen Keys. Library: Register Screen Input, Unregister Screen Input, Check Screen Reachability. |
| Library | 보조 함수입니다. | Keep Focus Or Target(Screen, Target), Is Key Mapped To Action. |
| **Reci Motion Subsystem** | player별 motion scale입니다. | Set Motion Scale, Get Motion Scale. |

### 선택 module (ReciCommonUIWidgets)

| Class | 용도 | Member |
| --- | --- | --- |
| **Reci Carousel Scroll Box** | selected item을 가운데로 보내는 looping carousel입니다. | Snap To Closest Item, Snap Delay, Center Motion, Presentation Blend Distance. Set Centered Item Index, Get Centered Item (Index). Event: On Centered Item Changed. |
| **Reci Advanced List** | item마다 두 번째 main presentation을 가진 Reci List입니다. | Main Entry Widget Class, Initial Main Item Index, Main Presentation Follows Focus, Confirm Main Item On Click. |
| **Reci Two Level List Base**, **Reci Two Level Item Base**, interface **Reci Two Level Entry** | 다른 axis의 child list를 가진 parent row입니다. | list는 `ParentList`, row는 `ChildList`를 bind합니다(BindWidget). Set Items(Items, Child Count provider), Synchronize Child Selection, Select Child, Confirm Selection. Event: On Focused Parent Changed, On Selection Changed, On Confirmed. Entry: Set Child Item, Update Two Level State. |

## 조립 방법

### Screen 안의 list

1. **Hierarchy:** Reci Vertical Box(또는 Horizontal, Tile) > **Reci List**이며, list는 Is Variable로 둡니다.
   scroll하려면 Reci Scroll Box > Reci Vertical Box > Reci List로 하고, box는 scroll box의 유일한 child입니다.
2. **List:** **Entry Widget Class**를 지정합니다. **Selection Mode**는 [DESIGN.md, Lists](DESIGN.md#lists)의
   role에 따릅니다. action menu와 value editor는 None, 하나를 고르면 Single, 여럿을 고르면 Multi입니다.
3. **Item:** Construct에서 item object를 만든 뒤 **Set List Items**를 호출합니다. 나중에 item이 바뀌면 object를
   고치고 Set List Items를 다시 호출합니다. entry는 다시 만들어지고, focus는 같은 item으로 돌아옵니다.
4. **key로 value 바꾸기:** Left와 Right는 **On Navigation Input**에서 처리합니다(Switch on
   EReciNavigationDirection). 대상은 Get Hosted Entry Button(Get Driven Item Index)입니다. list 자신의 axis는
   연결하지 않습니다.

### Entry widget

- **부모와 interface:** 부모는 Reci Focus Style Button Base이고, Class Settings > Implemented Interfaces에
  **Reci List Item**을 추가합니다.
- **Item:** Event Set List Item Object에서 Item Object를 item class로 cast해 저장하고 표시를 갱신합니다.
- **안쪽 button**(−, +): Is Focusable을 꺼서 focus가 entry에 남게 합니다.
- **Style:** Class Defaults에서 Style을 지정합니다. 안의 Common Text는 button style의 text style을 받습니다.

### 자기 key를 가진 button

- **자기 key:** **Triggering Enhanced Input Action**을 지정합니다. 그 screen이 active인 동안 그 key가 button을
  누릅니다.
- **button 위에 key 표시:** button의 tree에 이름이 정확히 `InputActionWidget`인 Reci Mapped Key Text를
  넣습니다.
- **screen이 대신 처리하는 key**(예: Back): 표시 전용인 Hint Input Action을 지정합니다.

### Key hint line

- **Widget:** action을 지정한 Reci Mapped Key Text입니다(Set Enhanced Input Actions, 또는 Enhanced Input
  Action과 Additional Input Actions).
- **자리를 유지하며 숨기기:** On Shows Keys Changed에서 Shows Keys가 false이면 Visibility를 Hidden으로
  합니다. 터치에서는 key가 없습니다.
- **어떤 key:** list를 drive하는 key는 button이 아니라 여기에 둡니다.

### 초기 focus

- **Get Desired Focus Target override:** **Keep Focus Or Target**(Target = list > Get Hosted Focus Target, 또는
  지정한 control)을 반환합니다.
- **시작 focus:** Construct에서 Set List Items 다음에 **Set Driven Item Index**(0, Give Focus ✓)를 호출합니다.
  Get Desired Focus Target만으로는 시작 시 focus가 잡히지 않습니다.
- **Desired Focus Widget:** 설정하지 않습니다.

### Back

- **Screen의 Class Defaults:** **Is Back Handler** ✓.
- **On Handle Back Action override:** screen의 Back을 처리하고 true를 반환합니다. 이 override가 없으면 Back이
  screen을 deactivate하고 Stack이 그것을 뺍니다.
- **Back mapping:** Back은 Escape와 Backspace에 map합니다. Play In Editor에서는 Escape가 에디터의 Stop
  key라서 에디터로 가므로, Back은 Backspace로 확인합니다.

### Modal dialog

- **Dialog:** **Is Modal** ✓, Is Back Handler ✓인 Common Activatable Widget입니다.
- **요청:** Screen이 `On…Requested` dispatcher를 부릅니다. Composer가 dialog를 `ModalStack`에 push하고,
  dialog의 닫힘 dispatcher(데모에서는 `On Closed`)를 연결하고, 결과를 Screen function으로 돌려줍니다.
- **아래 Screen:** dialog가 열린 동안 Screen은 입력을 받지 않고, 그 hint는 key를 보이지 않습니다.

### Screen key (stock routing으로 안 될 때만)

예를 들어 focus가 다른 곳에 있는 동안 list를 drive하는 key나, key-down 시점으로 시간을 재는 press에 씁니다.

1. **구현:** Screen에 **Reci Screen Input**을 구현합니다.
2. **등록:** activation 때 **Register Screen Input**(self)을, deactivation 때 **Unregister Screen Input**을
   호출합니다.
3. **Mapping:** Screen의 Input Mapping에 있는 action이 **Handle Reci Screen Action**에 도착합니다. 처리한
   action마다 true를 반환합니다. trigger와 modifier는 적용되지 않습니다.
4. **Key capture**(key-binding editor): Is Capturing Reci Screen Keys가 true인 동안 Handle Reci Screen Key를
   씁니다.
5. **확인:** **Check Screen Reachability**로 key나 hint가 없는 control과 action을 찾습니다.

## 하지 말 것

- **Class:**
  - entry 안에 control이 있는 list에 CommonUI List View나 Tile View를 쓰지 않습니다. Reci List를 씁니다.
  - entry나 공용 widget을 Common Activatable Widget에서 파생하지 않습니다. Reci는 가장 가까운 것을 그 widget의
    screen으로 봅니다.
- **Key:**
  - text에 key 이름을 적지 않습니다. 모든 key는 Reci Mapped Key Text를 통해 input action에서 옵니다.
  - Hide When Unmapped를 끄지 않습니다.
  - button의 key를 Hint Input Action과 screen branch로 만들지 않습니다. Triggering Enhanced Input Action을
    씁니다.
- **Focus:** Desired Focus Widget을 설정하지 않습니다.
- **긴 list:** Reci List는 모든 entry를 살려 둡니다. 긴 list는 entry 안에 control이 없는 List View를 씁니다.

## 확인

- **Log:** Reci는 처리할 수 없는 구성을 Blueprint compile 경고나 `LogReciCommonUI`로 한 번 보고합니다. 모두
  고칩니다.
- **Play:** Play In Editor에서 키보드, 게임패드, 마우스, 터치(Editor Preferences > Play > Use Mouse for
  Touch)로 확인합니다.
  - 시작 시 focus가 보입니다.
  - 모든 control에 direction key로 닿습니다.
  - hint가 device에 따라 바뀌고 터치에서 숨습니다.
  - Back이 동작합니다(PIE에서는 Backspace).
- **Test:** automation test 이름은 `ReciCommonUI.*`입니다. 저장소 README를 참고합니다.
