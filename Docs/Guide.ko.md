# Reci Common UI 가이드: 처음부터 만드는 menu screen

[English](Guide.md) · 한국어

이 가이드는 빈 C++ 프로젝트에서 시작해 Reci Common UI로 menu screen 하나를 Unreal Editor에서 단계별로 만듭니다.
완성하면 map의 **Composer**가 **Screen** 하나를 push합니다. Screen은 label과 value를 가진 entry 다섯 개의 Reci
List를 보입니다.

- Up과 Down은 entry 사이를 옮깁니다. Left와 Right, 또는 entry의 **−**와 **+**는 그 value를 바꿉니다.
- **Reset**(R 또는 게임패드 Y)은 모든 value를 0으로 되돌립니다. list가 rebuild되어도 focus된 entry는 focus를
  유지합니다.
- Back(Backspace나 Escape, 또는 게임패드 B)은 첫 entry로 focus를 돌려놓습니다.
- key hint는 지금 쓰는 장치의 key를 보이고, 터치에서는 숨습니다.
- 키보드, 게임패드, 마우스, 터치가 모두 동작합니다.

자산은 [DESIGN.md의 화면 구조](../Plugins/ReciCommonUI/DESIGN.md#화면-구조)를 따릅니다. Composer → Screen → 공용
widget·data → Reci Common UI이고, 참조는 위에서 아래로만 향합니다.

| Layer | 만드는 것 |
| --- | --- |
| Composer | `WBP_MenuComposer`: input mapping을 더하고 Screen을 자기 `Stack`에 push합니다. |
| Screen | `WBP_MenuScreen`: list, Reset, key hint, 초기 focus, Back. |
| 공용 widget·data | `W_MenuButton`, `W_MenuEntry`, `W_MenuKeyHint`, `BP_MenuItem`. |
| Input | `IA_Menu*`, `IMC_Menu`, `MenuInputData`. |

준비물은 Unreal Engine 5.8, workload **Game development with C++**를 설치한 Visual Studio 2022, 이 저장소의
`Plugins/ReciCommonUI` 폴더입니다.

## 목차

1. [프로젝트와 플러그인](#1-프로젝트와-플러그인)
2. [Input](#2-input)
3. [Item data](#3-item-data)
4. [Widget](#4-widget)
5. [Screen](#5-screen)
6. [Composer](#6-composer)
7. [Map](#7-map)
8. [PIE에서 확인](#8-pie에서-확인)

## 표기

- 메뉴와 field 이름은 영어 에디터의 것입니다(Edit > Editor Preferences > General > Region & Language > **Editor
  Language** = `English`).
- `Category > Field = 값`은 Details panel의 field입니다. 이 가이드가 말하지 않은 field는 모두 기본값으로 둡니다.
- 바꿀 때마다 **Compile**(Blueprint)과 **Save**를 누릅니다.
- 자산은 `Content/UI`(`Input`, `Data`, `Widgets`, `Screens`, `Composers`)와 `Content/Maps`에 둡니다. 폴더는
  Content Browser에서 우클릭 > **New Folder**로 만듭니다.
- 이 가이드는 style을 만들지 않습니다. style이 필요한 widget은 플러그인에 들어 있는 데모 style을
  `/ReciCommonUI/Demo/Style`(`BS_*` button style, `TS_*` text style)에서 고릅니다. Style field가 이것들을 그대로
  보입니다. Content Browser에서 보려면 Content Browser > **Settings** > **Show Plugin Content**를 켭니다.
- Details panel 그림 상당수는 이 가이드가 정하는 row만 쌓아 보입니다. 그 사이의 row는 빠져 있습니다.
- 새 Blank 프로젝트는 Palette에서 만든 widget의 **Is Variable**을 끈 채로 만듭니다(Project Settings > User
  Interface > Authorize Automatic Widget Variable Creation이 꺼져 있음). 아래에서 ✓로 표시한 widget은 Details
  panel 맨 위의 **Is Variable**을 켭니다. graph는 이런 widget만 쓸 수 있고, Events도 이런 widget에만 나옵니다.

## 1. 프로젝트와 플러그인

### 1.1 C++ 프로젝트 만들기

Epic Games Launcher에서 Unreal Engine 5.8을 실행합니다. **New Project** 창에서 **GAMES** > **Blank**를 고르고
Project Defaults에서 **C++**를 고릅니다. Project Location과 Project Name을 넣고 **Create**를 누릅니다.

프로젝트 폴더 경로는 짧게(약 110자 이하) 둡니다. 플러그인의 build 파일은 폴더 깊숙이 생기고, Windows는 경로가
260자를 넘는 build를 멈춥니다.

![GAMES > Blank와 C++를 고른 New Project 창](Images/Guide/project-new-cpp.png)

*New Project 창: GAMES > Blank, Project Defaults C++, Project Location `C:\Projects`, Project Name `MyMenu`.*

### 1.2 플러그인 복사

에디터를 닫습니다. 이 저장소의 `Plugins/ReciCommonUI` 폴더를 `<프로젝트 폴더>/Plugins/ReciCommonUI`로 복사합니다.
`Plugins` 폴더가 없으면 만듭니다.

`<프로젝트>.uproject`를 열고, 빠진 module을 rebuild할지 물으면 **Yes**를 누릅니다. build가 실패하면
`<프로젝트>.sln`을 Visual Studio로 열어 **Development Editor**와 **Win64**를 고르고 Build > **Build Solution**을
실행한 뒤 `.uproject`를 다시 엽니다.

Edit > **Plugins**에서 검색 칸에 `Reci`를 넣어 확인합니다. **Reci Common UI**가 체크되어 있습니다. 이 플러그인이
필요한 Common UI와 Enhanced Input 플러그인을 켭니다.

![Reci로 검색한 Plugins 창](Images/Guide/plugins-reci.png)

*검색 칸에 `Reci`를 넣은 Edit > Plugins: Reci Common UI(Beta), version 0.1.0이 체크되어 있습니다.*

### 1.3 프로젝트 설정

Edit > **Project Settings**를 열고 field 세 개를 정합니다.

Engine > General Settings > Default Classes > **Game Viewport Client Class** = `CommonGameViewportClient`.
CommonUI는 button key와 Back을 이것을 통해 routing합니다.

![Game Viewport Client Class가 보이는 General Settings](Images/Guide/settings-viewport-client.png)

*Engine - General Settings: Default Classes > Game Viewport Client Class가 CommonGameViewportClient입니다.*

Engine > User Interface > Focus > **Render Focus Rule** = `Never`. focus는 button style이 그리므로, Slate는 그 위에
사각형을 그리지 않습니다.

![Render Focus Rule이 보이는 User Interface 설정](Images/Guide/settings-render-focus-rule.png)

*Engine - User Interface: Focus > Render Focus Rule이 Never입니다.*

Game > Common Input Settings > Input > **Enable Enhanced Input Support** ✓. 같은 page의 Input Data는 2.4에서
정합니다.

![Enhanced Input support를 켠 Common Input Settings](Images/Guide/settings-enhanced-input.png)

*Game - Common Input Settings: Enable Enhanced Input Support가 체크되어 있고 Input Data는 아직 None입니다.*

### 1.4 DefaultEngine.ini에 한 줄

이것은 Project Settings에 field가 없습니다. 에디터를 닫고 `<프로젝트 폴더>/Config/DefaultEngine.ini`를 텍스트
편집기로 열어 아래 줄을 더합니다. `[ConsoleVariables]` section이 이미 있으면 둘째 줄만 그 아래에 더합니다.

```ini
[ConsoleVariables]
CommonUI.ShouldVirtualAcceptSimulateMouseButton=0
```

이 값이 켜져 있으면 CommonUI는 게임패드 Accept를 마우스 cursor 위치의 click으로 바꿉니다. 키보드나 마우스를 쓴
뒤에는 cursor가 focus된 control 위에 있지 않으므로 첫 Accept가 사라집니다. 꺼져 있으면 Accept는 focus로 갑니다.

### 1.5 다시 열기

프로젝트를 다시 엽니다. Game Viewport Client Class와 Enable Enhanced Input Support는 다시 연 뒤에야 적용되고,
2.3의 Enhanced Input field도 그 뒤에야 보입니다.

Blueprint에는 더 필요한 것이 없습니다. 플러그인의 widget을 쓰는 C++ module은 dependency에
`ReciCommonUIWidgets`를 더합니다.

## 2. Input

### 2.1 Input Action

`Content/UI/Input`에서 **+ Add** > Input > **Input Action**을 일곱 번 골라 다음 이름을 붙입니다.

`IA_MenuUp`, `IA_MenuDown`, `IA_MenuLeft`, `IA_MenuRight`, `IA_MenuSelect`, `IA_MenuBack`, `IA_MenuReset`

field는 모두 기본값(Value Type `Digital (bool)`)으로 둡니다.

![Input Action 일곱 개가 있는 Content Browser](Images/Guide/input-actions.png)

*Content > UI > Input에 Input Action 일곱 개가 있습니다.*

### 2.2 IMC_Menu

같은 폴더에서 **+ Add** > Input > **Input Mapping Context**를 골라 `IMC_Menu`라고 이름 붙이고 엽니다.
**Mappings** 옆의 **+**로 action row를 더해 action을 고르고, action row의 **+**를 key마다 한 번씩 눌러 key를
고릅니다. Triggers와 Modifiers는 더하지 않습니다.

| Action | 키보드 | 게임패드 |
| --- | --- | --- |
| IA_MenuUp | Up | Gamepad D-pad Up |
| IA_MenuDown | Down | Gamepad D-pad Down |
| IA_MenuLeft | Left | Gamepad D-pad Left |
| IA_MenuRight | Right | Gamepad D-pad Right |
| IA_MenuSelect | Enter | Gamepad Face Button Bottom |
| IA_MenuBack | Escape, Backspace | Gamepad Face Button Right |
| IA_MenuReset | R | Gamepad Face Button Top |

Backspace는 Play In Editor를 위한 것입니다. 거기서 Escape는 에디터의 Stop key이고, CommonUI는 그 key를 에디터로
보냅니다.

![mapping을 모두 펼친 IMC_Menu](Images/Guide/imc-mappings.png)

*action row를 모두 펼친 IMC_Menu: action마다 키보드 key와 게임패드 key가 있습니다.*

### 2.3 MenuInputData

같은 폴더에서 **+ Add** > **Blueprint Class**를 고릅니다. Pick Parent Class에서 **All Classes**를 펼쳐
`ReciUIInputData`를 검색하고 **ReciUIInputData**를 골라 **Select**를 누릅니다. 이름을 `MenuInputData`로 하고
엽니다.

| Properties > Field | 값 |
| --- | --- |
| Navigation Up Action | IA_MenuUp |
| Navigation Down Action | IA_MenuDown |
| Navigation Left Action | IA_MenuLeft |
| Navigation Right Action | IA_MenuRight |
| Enhanced Input Click Action | IA_MenuSelect |
| Enhanced Input Back Action | IA_MenuBack |

이 Input Data를 쓰면 Slate의 focus 이동은 navigation action 네 개에 map한 key를 따르고, click action과 Back action의
key는 Accept와 Back으로 동작합니다.

![MenuInputData의 class defaults](Images/Guide/input-data-defaults.png)

*MenuInputData: navigation action 네 개, Enhanced Input Click Action IA_MenuSelect, Enhanced Input Back Action
IA_MenuBack. Mapped Keys Replace Default Keys는 체크된 채로 둡니다.*

### 2.4 Input Data

Project Settings > Game > Common Input Settings > Input > **Input Data** = `MenuInputData`.

![Input Data가 MenuInputData인 Common Input Settings](Images/Guide/settings-input-data.png)

*Game - Common Input Settings: Input Data가 MenuInputData이고 Enable Enhanced Input Support가 체크되어 있습니다.*

Common Input은 에디터가 시작할 때 Input Data를 읽습니다. 8에서 Play를 누르기 전에 에디터를 다시 엽니다.

## 3. Item data

`Content/UI/Data`에서 **+ Add** > **Blueprint Class** > All Classes > **Object** > Select를 고르고 이름을
`BP_MenuItem`으로 합니다. item 하나가 list의 entry 하나입니다.

My Blueprint > **Variables**의 **+**를 두 번 누릅니다.

| 이름 | Type | Details |
| --- | --- | --- |
| Label | Text | Instance Editable ✓, Expose on Spawn ✓ |
| Value | Integer | 기본값 |

Expose on Spawn이 켜져 있으면 item을 만드는 node(5.3)에 Label pin이 생깁니다.

![BP_MenuItem의 변수와 Label의 Details](Images/Guide/item-variables.png)

*BP_MenuItem: My Blueprint에 Label(Text)과 Value(Integer)가 있고, Label의 Details에서 Instance Editable과 Expose on
Spawn이 체크되어 있습니다.*

## 4. Widget

세 widget 모두 `Content/UI/Widgets`에 둡니다. **+ Add** > User Interface > **Widget Blueprint**로 만들고, parent는
**All Classes**에서 검색해 **Select**를 누릅니다. Hierarchy는 Palette에서 widget을 끌어 만듭니다(Palette 검색 칸에
이름을 넣습니다). 첫 widget은 맨 위의 Blueprint 이름 위에 놓아 root로 만듭니다. widget 이름은 Details 맨 위의 이름
칸에서 바꿉니다. 직접 만든 widget은 Palette의 **User Created**에 있습니다.

### 4.1 W_MenuButton

label 하나를 가진 button으로, −, +, Reset에 씁니다. parent: **Reci Focus Style Button Base**
(`ReciFocusStyleButtonBase`로 검색).

Hierarchy:

- **Row** [Horizontal Box]
  - **Label** [Common Text] ✓ — Slot: Size = **Fill**, Horizontal Alignment = **Center**, Vertical Alignment =
    **Center**

![W_MenuButton의 hierarchy와 Label slot](Images/Guide/button-hierarchy.png)

*W_MenuButton: Row > Label. Label은 Is Variable이 체크되어 있고, slot은 Fill이며 가로세로 가운데입니다.*

Label에는 text style이 필요 없습니다. button이 자기 button style의 text style을 그 안의 모든 Common Text에
입힙니다.

**Graph** tab에서 변수 `LabelText`(Text)를 더하고 Instance Editable ✓(눈 아이콘)로 합니다.

![LabelText 변수](Images/Guide/button-variable.png)

*My Blueprint에 widget 변수 Label과 변수 LabelText(Text, 눈 열림)가 있고, Details에서 LabelText는 Text이며
Instance Editable이 체크되어 있습니다.*

toolbar의 **Class Defaults**: Style > **Style** = `BS_DemoTonalButton`.

![W_MenuButton class defaults의 Style](Images/Guide/button-defaults.png)

*W_MenuButton의 Class Defaults: Style > Style이 BS_DemoTonalButton입니다.*

Event Graph: 회색 **Event PreConstruct**에서 wire를 끌어 **SetText (Text)**를 놓습니다. Target = `Label`(My
Blueprint > Variables에서 끌어 Get), In Text = `LabelText`.

![W_MenuButton graph](Images/Guide/button-graph.png)

*Event PreConstruct → SetText (Text), Target은 Label, In Text는 LabelText.*

### 4.2 W_MenuEntry

list의 entry 하나입니다. parent: **Reci Focus Style Button Base**.

toolbar **Class Settings** > Details > Interfaces > Implemented Interfaces > **Add** > `Reci List Item`. Compile.
list는 이 interface로 entry마다 item을 넘깁니다.

![Reci List Item이 있는 Implemented Interfaces](Images/Guide/entry-interface.png)

*W_MenuEntry의 Class Settings: Interfaces > Implemented Interfaces에 Reci List Item이 있습니다.*

Hierarchy(Row를 먼저, 그다음 child 네 개를 이 순서로):

- **Row** [Horizontal Box]
  - **Label** [Common Text] ✓ — Slot: Size = **Fill**, Vertical Alignment = **Center**
  - **Minus** [W_MenuButton] ✓ — Slot: Vertical Alignment = **Center**
  - **Value** [Common Text] ✓ — Appearance > **Min Desired Width** = `64`, **Justification** = Center; Slot:
    Vertical Alignment = **Center**
  - **Plus** [W_MenuButton] ✓ — Slot: Vertical Alignment = **Center**

![W_MenuEntry의 hierarchy와 Label slot](Images/Guide/entry-hierarchy.png)

*W_MenuEntry: Row > Label, Minus, Value, Plus. Label은 변수이고, slot은 Fill이며 세로 가운데입니다.*

Minus: Default > **Label Text** = `−`, Style > **Style** = `BS_DemoIconButton`, Interaction > **Is Focusable** 끔.
Plus: 같게 하고 Label Text만 `+`. 그러면 − 나 +를 눌러도 focus는 entry에 남으므로 Left와 Right가 계속 그 entry에
작용합니다.

![Minus의 Details](Images/Guide/entry-minus.png)

*Minus: Label Text −, Style BS_DemoIconButton, Is Focusable 꺼짐.*

![Value의 Details](Images/Guide/entry-value.png)

*Value: Min Desired Width 64, Justification Center. 숫자가 바뀌어도 −와 +가 움직이지 않습니다.*

Graph tab에서 변수 `Item`을 더합니다. type 목록에서 `BP_MenuItem`을 검색해 **BP Menu Item** > **Object
Reference**를 고릅니다.

![Item 변수](Images/Guide/entry-variable.png)

*type이 BP Menu Item인 변수 Item.*

Class Defaults: Style > **Style** = `BS_DemoListItem`.

![W_MenuEntry class defaults의 Style](Images/Guide/entry-defaults.png)

*W_MenuEntry의 Class Defaults: Style > Style이 BS_DemoListItem입니다.*

function **Refresh**(My Blueprint > Functions > **+**): item을 보입니다.

Refresh → SetText (Text) [Target = `Label`(widget), In Text = Item의 Label] → SetText (Text) [Target = `Value`
(widget), In Text = Item의 Value]

item의 field는 **Get Item**에서 wire를 끌어 `Get Label` / `Get Value`로 검색합니다. Integer를 In Text에 꽂으면
**To Text (Integer)**가 저절로 생깁니다.

![Refresh](Images/Guide/entry-refresh.png)

*Refresh: Label에 Item의 Label을, 이어서 Value에 To Text (Integer)를 거친 Item의 Value를 SetText합니다.*

function **Step**: entry node를 골라 Details > Inputs에 input `Delta`(Integer)를 더합니다.

Step → Set Value [Target = Item, Value = Item의 Value + Delta] → Refresh

더하기 node는 action menu에 `+`를 넣어 찾습니다. **Set Value**는 Get Item에서 wire를 끌어 찾습니다.

![Step](Images/Guide/entry-step.png)

*Step: Item의 Value를 Item의 Value + Delta로 SET한 뒤 Refresh. 아래는 entry node의 Details로, input Delta가
Integer입니다.*

Event Graph:

- 우클릭해 `Event Set List Item Object`를 검색해 놓습니다. Event Set List Item Object → Cast To BP_MenuItem [Object
  = Item Object] → Set Item [Item = As BP Menu Item] → Refresh
- Designer에서 Minus를 골라 Details > Events > **On Clicked** 옆의 **+**를 누릅니다. On Clicked (Minus) → Step
  [Delta = -1]
- Plus도 같게 합니다. On Clicked (Plus) → Step [Delta = 1]

![W_MenuEntry event graph](Images/Guide/entry-events.png)

*Event Set List Item Object가 item을 저장하고 Refresh를 부릅니다. Minus와 Plus의 On Clicked는 -1과 1로 Step을
부릅니다.*

### 4.3 W_MenuKeyHint

key hint 한 줄로, action 몇 개의 key와 label을 보입니다. parent: **User Widget**.

Hierarchy:

- **Row** [Horizontal Box]
  - **Keys** [Reci Mapped Key Text] ✓ — Appearance > **Style** = `TS_DemoLabelLarge`; Slot: Vertical Alignment =
    **Center**
  - **Label** [Common Text] ✓ — Common Text > **Style** = `TS_DemoBodyMedium`; Slot: Vertical Alignment = **Center**

![W_MenuKeyHint의 hierarchy, Keys와 Label](Images/Guide/keyhint-hierarchy.png)

*W_MenuKeyHint: Row > Keys, Label. Keys는 세로 가운데, Style TS_DemoLabelLarge. Label은 세로 가운데, Style
TS_DemoBodyMedium.*

변수 두 개, 모두 Instance Editable ✓:

| 이름 | Type |
| --- | --- |
| Actions | Input Action > Object Reference, **Array**(type 옆 아이콘) |
| LabelText | Text |

![W_MenuKeyHint의 변수](Images/Guide/keyhint-variables.png)

*Actions는 Input Action 배열이고 Instance Editable이 체크되어 있습니다. LabelText는 Text입니다.*

Event Graph:

- Event PreConstruct → SetText (Text) [Target = `Label`, In Text = `LabelText`] → Set Enhanced Input Actions [Target =
  `Keys`, In Actions = `Actions`]
- Keys를 골라 Details > Events > **On Shows Keys Changed** → Branch [Condition = Shows Keys]
  - True → Set Visibility [Target = self, In Visibility = Not Hit-Testable (Self Only)]
  - False → Set Visibility [Target = self, In Visibility = Hidden]

Keys는 그 action들의 key를 지금 장치에 맞춰 보입니다. 터치에서는 key가 없으므로 줄이 자리를 유지한 채 숨고, 위의
것은 움직이지 않습니다.

![W_MenuKeyHint graph](Images/Guide/keyhint-graph.png)

*위: PreConstruct가 label을 정하고 Actions를 Keys에 넘깁니다. 아래: On Shows Keys Changed가 줄을 보이거나
숨깁니다.*

## 5. Screen

### 5.1 WBP_MenuScreen

`Content/UI/Screens`에서 parent가 **Common Activatable Widget**(`CommonActivatableWidget`로 검색)인 Widget
Blueprint를 만들고 이름을 `WBP_MenuScreen`으로 합니다.

Class Defaults: Back > **Is Back Handler** ✓. Activation > **Auto Activate**는 꺼진 채로 둡니다. screen은
Composer의 Stack이 activate합니다.

![Is Back Handler와 Auto Activate](Images/Guide/screen-defaults.png)

*Details 검색으로 찾은 Class Defaults: Back > Is Back Handler 체크, Activation > Auto Activate 꺼짐.*

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
      - **MoveHint**, **ChangeHint**, **ResetHint**, **BackHint** [W_MenuKeyHint] — 아래 표

Reci List는 Reci Vertical Box 안에 둡니다. box가 entry를 배치하고 list의 key를 처리합니다. Selection Mode None은
click이나 Enter가 entry를 selected로 표시하지 않게 합니다. 이 entry들은 선택지가 아니라 value를 가집니다.

![WBP_MenuScreen의 hierarchy](Images/Guide/screen-hierarchy.png)

*WBP_MenuScreen: Root > Panel > Menu > EntriesBox > Items, 그리고 Menu 안의 Reset과 hint 네 개.*

![Panel의 Details](Images/Guide/screen-panel.png)

*Panel: Root 안에서 가로세로 가운데, Width Override 640.*

![Items의 Details](Images/Guide/screen-items.png)

*Items: Is Variable 체크, Entry Widget Class W_MenuEntry, Selection Mode None.*

![Reset의 Details](Images/Guide/screen-reset.png)

*Reset: Horizontal Alignment Left, Label Text Reset, Triggering Enhanced Input Action IA_MenuReset.*

Triggering Enhanced Input Action을 정하면 screen이 active인 동안 IA_MenuReset의 key가 이 button을 누릅니다.

key hint(Default > **Actions**: element마다 **+**를 누르고 action을 고릅니다. Default > **Label Text**):

| Widget | Actions | Label Text |
| --- | --- | --- |
| MoveHint | IA_MenuUp, IA_MenuDown | `Move` |
| ChangeHint | IA_MenuLeft, IA_MenuRight | `Change value` |
| ResetHint | IA_MenuReset | `Reset` |
| BackHint | IA_MenuBack | `Go to first entry` |

![MoveHint의 Details](Images/Guide/screen-hint.png)

*MoveHint: Actions에 IA_MenuUp과 IA_MenuDown, Label Text Move.*

### 5.3 Item과 초기 focus

Graph tab에서 변수 `ItemData`를 더합니다. type은 **BP Menu Item** > Object Reference, **Array**입니다.

![ItemData 변수](Images/Guide/screen-variable.png)

*My Blueprint에 Reset, Items, ItemData가 있고, ItemData는 BP Menu Item 배열입니다.*

Event Graph, 회색 **Event Construct**에서:

Event Construct → For Loop [First Index = 1, Last Index = 5]

- Loop Body → Construct Object from Class [Class = BP_MenuItem; node 이름이 **Construct BP Menu Item**으로 바뀝니다;
  Label = Format Text의 Result] → Add [Target Array = ItemData, New Item = Return Value]
  - Format Text: Format = `Item {Index}`. format을 넣으면 Index pin이 생기고, 여기에 For Loop의 Index를 잇습니다.
  - Add: **Get ItemData**에서 wire를 끌어 `Add`를 고릅니다.
- Completed → Set List Items [Target = Items, In List Items = ItemData] → Set Driven Item Index [Target = Items, Item
  Index = 0, Give Focus ✓]

Set List Items가 list에 item을 넘기면 list가 item마다 W_MenuEntry를 만듭니다. Set Driven Item Index는 screen이 열릴
때 Item 1에 focus를 줍니다.

![Event Construct](Images/Guide/screen-construct.png)

*Event Construct가 Item 1부터 Item 5까지 item 다섯 개를 만들어 Items에 넘기고 첫 entry에 focus를 줍니다.*

### 5.4 Reset

Reset을 골라 Details > Events > **On Clicked**:

On Clicked (Reset) → For Each Loop [Array = ItemData]

- Loop Body → Set Value [Target = Array Element, Value = 0]
- Completed → Set List Items [Target = Items, In List Items = ItemData]

Set List Items는 entry를 rebuild합니다. focus가 list 안에 있었으면 list가 같은 entry에 focus를 돌려주므로, R을
눌러도 있던 자리에 그대로 있습니다.

![Reset graph](Images/Guide/screen-reset-graph.png)

*On Clicked (Reset)이 모든 item의 Value를 0으로 하고, Set List Items가 list를 rebuild합니다.*

### 5.5 Left와 Right

Items를 골라 Details > Events > **On Navigation Input**:

On Navigation Input (Items) → Switch on EReciNavigationDirection [Selection = Direction]

- Left → Cast To W_MenuEntry [Object = Get Hosted Entry Button의 Return Value] → Step [Target = As W Menu Entry,
  Delta = -1]
  - Get Hosted Entry Button [Target = Items, Item Index = Get Driven Item Index (Target = Items)]
- Right → 같은 Get Hosted Entry Button에서 Cast To W_MenuEntry를 하나 더 → Step [Delta = 1]

Up과 Down은 잇지 않습니다. list가 직접 focus를 옮깁니다.

![On Navigation Input](Images/Guide/screen-navigation.png)

*Left와 Right가 list의 driven entry에 -1과 1로 Step을 부릅니다.*

### 5.6 Focus target

My Blueprint > Functions > **Override** > **Get Desired Focus Target**:

Get Desired Focus Target → Return Node [Return Value = Keep Focus Or Target (Target = Items > Get Hosted Focus
Target)]

CommonUI는 screen이 active가 될 때와 입력 장치가 바뀔 때 이것을 묻습니다. Keep Focus Or Target은 이 screen의
control이 focus를 쥐고 있는 동안에는 focus를 그대로 두고, 그 밖에는 list의 현재 entry를 줍니다.

![Get Desired Focus Target](Images/Guide/screen-focus-target.png)

*Get Desired Focus Target이 Keep Focus Or Target을 반환하고, Target은 Items > Get Hosted Focus Target에서 옵니다.*

### 5.7 Back

My Blueprint > Functions > **Override** > **On Handle Back Action**:

On Handle Back Action → Set Driven Item Index [Target = Items, Item Index = 0, Give Focus ✓] → Return Node [Return
Value ✓]

Is Back Handler(5.1)가 Back을 여기로 보냅니다. true를 반환하면 CommonUI는 screen이 처리한 것으로 봅니다. 이
override가 없으면 Back이 screen을 deactivate하고 Stack이 screen을 치웁니다.

![On Handle Back Action](Images/Guide/screen-back.png)

*On Handle Back Action이 첫 entry에 focus를 주고 true를 반환합니다.*

## 6. Composer

Composer는 map의 최상위 widget입니다. input mapping을 적용하고 첫 Screen을 자기 Stack에 push합니다.

`Content/UI/Composers`에서 parent가 **Common Activatable Widget**인 Widget Blueprint를 만들고 이름을
`WBP_MenuComposer`로 합니다.

Class Defaults: Activation > **Auto Activate** ✓. Composer는 viewport에 들어가면 스스로 activate합니다.

![Composer의 Auto Activate](Images/Guide/composer-defaults.png)

*Details 검색으로 찾은 WBP_MenuComposer의 Class Defaults: Activation > Auto Activate 체크.*

Hierarchy:

- **Root** [Overlay]
  - **Stack** [Common Activatable Widget Stack] ✓ — Slot: Horizontal Alignment = **Fill**, Vertical Alignment =
    **Fill**

![Composer의 hierarchy](Images/Guide/composer-hierarchy.png)

*WBP_MenuComposer: Root > Stack. Stack은 변수이고 Root를 가로세로로 채웁니다.*

Event Graph:

Event Construct → Add Mapping Context [Target = Enhanced Input Local Player Subsystem; Mapping Context = IMC_Menu] →
Push Widget [Target = Stack; Activatable Widget Class = WBP_MenuScreen]

- subsystem은 우클릭으로 **Get Owning Player**를 놓고, 그 Return Value에서 wire를 끌어 **Get
  EnhancedInputLocalPlayerSubsystem**을 고릅니다.
- Push Widget: **Get Stack**에서 wire를 끌어 검색합니다.

![Composer의 Event Construct](Images/Guide/composer-construct.png)

*Event Construct가 owning player의 Enhanced Input subsystem에 IMC_Menu를 더하고 WBP_MenuScreen을 Stack에 push합니다.*

## 7. Map

### 7.1 MenuMap

File > **New Level** > **Empty Level** > Create. File > **Save Current Level As** > `Content/Maps`, 이름 `MenuMap`.

### 7.2 Level Blueprint

toolbar의 Blueprints 버튼 > **Open Level Blueprint**. **Get Player Controller** [Player Index = 0]를 하나 놓고, 그
Return Value(PC)를 node 세 곳에 잇습니다.

Event BeginPlay → Create Widget [Class = WBP_MenuComposer, Owning Player = PC] → Add to Viewport [Target = Return
Value] → Set Input Mode UI Only [Player Controller = PC] → Set Show Mouse Cursor [Target = PC, Show Mouse Cursor ✓]

Set Show Mouse Cursor: PC에서 wire를 끌어 검색합니다.

![Level Blueprint](Images/Guide/level-blueprint.png)

*Event BeginPlay가 Composer를 만들어 viewport에 넣고, UI 입력으로 바꾸고, 마우스 cursor를 보입니다.*

### 7.3 기본 map

Project Settings > Project > Maps & Modes > Default Maps: **Editor Startup Map** = `MenuMap`, **Game Default Map** =
`MenuMap`.

![Maps & Modes](Images/Guide/settings-maps.png)

*Project - Maps & Modes: Editor Startup Map과 Game Default Map이 MenuMap입니다.*

## 8. PIE에서 확인

2.4 이후 에디터를 다시 열지 않았으면 다시 열고, MenuMap을 열어 **Play**를 누릅니다.

![키보드로 연 PIE](Images/Guide/pie-keyboard.png)

*시작 화면: Item 1에 focus가 있고 value는 모두 0이며, hint는 키보드 key(↑ / ↓, ← / →, R, ESC / BS)를 보입니다.*

하나씩 확인합니다.

- **키보드**: Up과 Down이 entry 사이를 옮깁니다. Item 5에서 Down을 누르면 Reset으로 갑니다. Left와 Right가 focus된
  entry의 value를 바꿉니다. R이 모든 value를 되돌리고, 같은 entry가 focus를 유지합니다. Backspace가 Item 1로
  돌아갑니다.
- **게임패드**: D-pad가 옮기고 value를 바꾸며, Y가 되돌리고, B가 Item 1로 돌아갑니다. hint가 게임패드 key로
  바뀝니다.
- **마우스**: − 나 +를 click하면 그 entry의 value가 바뀌고 그 entry에 focus가 갑니다. Reset을 click하면 되돌립니다.
- **터치**: − 나 +를 tap하면 value가 바뀌고 hint가 숨습니다. PC에서 해 보려면 Edit > Editor Preferences > Level
  Editor > Play > Play in Editor > **Use Mouse for Touch**를 켜고, 끝나면 다시 끕니다.

![게임패드로 쓴 PIE](Images/Guide/pie-gamepad.png)

*D-pad Down 한 번, D-pad Right 두 번 뒤: Item 2에 focus가 있고 value는 2이며, hint는 D↑ / D↓, D← / D→, Y, B를
보입니다.*

![터치로 쓴 PIE](Images/Guide/pie-touch.png)

*Item 3의 +를 tap한 뒤: value는 1이고 Item 3에 focus가 있으며, hint는 숨어 있습니다.*

Play In Editor에서는 Escape가 session을 멈춥니다. CommonUI가 에디터의 Stop key를 그대로 보내기 때문입니다.
Standalone Game이나 package된 build에서는 Escape가 Back으로 동작합니다.
