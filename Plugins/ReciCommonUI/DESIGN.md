# Reci Common UI 설계

이 문서는 Reci Common UI가 무엇을 위한 것인지, 무엇을 소유하는지, control이 어떻게 동작하는지, 새
기능이 어디에 속하는지를 말한다. 모든 변경은 이 문서로 판단한다. 규칙은 목표이며, 코드가 규칙에 못
미치는 곳은 [Known deviations](#known-deviations)에 적는다.

동작 규칙은
[W3C APG keyboard practices](https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/)를
Unreal에 맞게 옮기고
[CommonUI input technical guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/commonui-input-technical-guide-for-unreal-engine?application_version=5.7)를
따른다. 웹 ARIA나 WCAG 준수를 주장하지 않으며, native Slate focus와 accessibility는 여전히 의미를
가진다.

## 목적

CommonUI는 activatable screen, button input routing, style, input-method awareness를 제공한다.
CommonUI의 list view는 ListView를 기반으로 하는데, ListView는 entry widget을 재활용하고 focus를 row
단위로 옮기므로 focus가 entry 안의 widget에 닿을 수 없다. 또 CommonUI는 hover, focus, selection,
confirmation을 느슨하게만 묶어 둔다. Reci Common UI는 CommonUI, Enhanced Input, UMG, Slate 위에
다음을 더한다.

- **persistent entries의 list**: 모든 item이 살아 있는 widget을 가지므로 entry 안의 control이
  focus를 받을 수 있고, list는 driven item, focus return, key cursor, centring을 selection과 분리해
  다룬다.
- **하나의 interaction model**: hover, focus, selection, confirmation이 서로 분리되어 있고 키보드,
  게임패드, 마우스, 터치에서 똑같이 동작한다.
- **선택적 input layer**: stock routing이 할 수 없는 일을 맡는다. persistent entries 사이의
  navigation, focus된 widget보다 먼저 받는 screen key, 타이밍 플러그인이 주는 key-down time,
  Enhanced Input을 따르는 Slate navigation이다. input preprocessor는 Reci control, screen, popup이
  등록되어 있거나 Reci가 가져간 key나 stick deflection이 유지되는 동안 설치되며, 마지막 것이 사라진
  다음 프레임에 제거된다.

이 플러그인은 gap을 메울 뿐, 그 시스템들이 하는 일을 대체하지 않는다. runtime module 두 개가 함께
배포된다. core인 **ReciCommonUI**(위의 내용과, widget이 기반으로 삼는 extension point)와, stock
widget이 그 구조 때문에 만들 수 없는 presentation form을 담은 선택 module
**ReciCommonUIWidgets**(carousel은 persist해야 하는 entry 위에서 centring을 애니메이션한다)다.
widget은 core의 public API만 쓰며, core는 widget을 참조하지 않는다. 배치하지 않은 widget은 비용이
없다.

## 원칙

1. **stock system을 쓰고, 명시된 gap에서만 개입한다.** CommonUI, Enhanced Input, UMG, Slate가 하는
   일이면 Reci는 그것을 쓰며, 명시된 한계가 강제하는 곳에서만 stock path를 넘겨받는다. 그런 곳은 이
   문서가 하나하나 밝힌다. Reci가 거기서 판단하는 것(active layer, Is Modal, input mode, focus,
   action의 key)은 stock system에서 읽으며 사본을 두지 않는다. Reci가 가로채는 path 위의 stock
   widget(text entry, key selector)은 계속 동작하거나, 그렇지 않으면 그 gap을 목록에 올린다. stock
   widget이 만들 수 없는 presentation은 gap이지만, stock widget으로 낼 수 있는 외형은 gap이 아니다.

2. **입력마다 목적지는 하나, 순서는 선언된 대로; 처리한 것만 consume한다.** 각 입력은 그 user의
   handler 하나로 가며, 순서는 다음과 같다. Reci가 보기 전에 focus가 이동한(device 변경) Reci
   layer의 key-down은 아무 데도 가지 않으며, direction이 그 stick의 direction이 아닌(Stick Dominant
   Axis) Reci layer의 stick-direction key도 마찬가지다. 다음은 열린 popup의 Back과 그 popup 자신의
   list, popup이 없으면 screen의 key capture와 그다음 action(가장 안쪽 screen부터), focus된 Reci
   control, stock path 순이다. Reci layer란 등록된 screen, 열린 popup, 또는 Reci control의 activated
   screen이 focus를 쥐고 있는 곳이다. Reci는 handler가 받았거나, press가 device를 바꾸며 focus를
   옮겼거나, direction이 그 stick의 direction이 아니었을 때만 press를 consume하며, 그 밖에는 Slate의
   navigation state를 건드리지 않는다. 한 번의 press는 최대 한 번만 confirm한다. 그 press의 repeat와
   release는 screen이 바뀌더라도 press가 간 곳으로만 간다.

3. **수명이 다른 state는 따로 둔다.** hover, focus, driven item, selection, pressed, confirm은 서로
   다르며, control이 선언한 coupling을 통하지 않고는 어느 것도 다른 것을 바꾸지 않는다. hover는
   selection이나 value를 절대 바꾸지 않으며, control이 opt-in한 곳에서만 focus를 옮긴다.
   navigation은 opt-in(Single에서의 Selection Follows Navigation)으로만 저장된 choice를 바꾸며,
   layout과 device 변경은 절대 바꾸지 않는다.

4. **범위가 한정된 효과, 안전한 기본값, 조용히 넘어가는 것은 없다.** 플러그인은 Reci control이나
   등록된 screen이 없는 screen에서 아무것도 바꾸지 않으며, 아무것도 등록되지 않은 동안에는 입력마다
   하는 작업이 없다. 모든 screen이나 player에 미치는 변경(navigation config, Tab, 억제된 default
   key)은 프로젝트의 opt-in, 즉 Reci UI Input Data나 프로젝트 설정을 기다린다. control의 기본값은
   하나의 [role](#lists)이며, 그 이상의 coupling은 opt-in이다. Reci는 작성자가 설정한 것(Enhanced
   Input trigger, CommonUI hold data, Is Modal, widget property)을 조용히 버리거나 처리할 수 없는
   설정을 조용히 무시하지 않고, [보고한다](#보고).

5. **key가 작용하는 곳은 모두 보이며, control 자신의 style로 그린다.** key를 쓰는 동안 focus는
   보이며, key cursor도 같은 외형을 입는다. hover는 focus outline을 그리지 않고, selection은 key가
   작용하는 곳처럼 보이지 않는다. focus는 control의 CommonUI style slot(Hovered에 focus outline을
   더한 것)을 재사용하며, Focused style은 없다.

6. **key는 input action에서 오고, hint는 사실을 말한다.** 표시되는 모든 key는 현재 device나 명시된
   device에 대한, action의 player 현재 binding이다. widget에 key 이름을 적어 넣지 않는다. Reci
   button의 hint는 하나이며, CommonUI의 선택을 확장한 규칙 하나로 정해진다. hint는 앞선 handler가
   먼저 가져가는 key를 절대 보이지 않는다. key를 이름이나 glyph로 그리는 일은 hint widget의 몫이며,
   CommonUI의 controller data가 있으면 그것을 쓴다. [Key hints](#key-hints)를 보라.

7. **플러그인의 동작은 C++이고, hierarchy와 외형은 Widget Blueprint의 것이다.** 동작은 Blueprint가
   property와 event로 구성하는 C++ base class에 있으며, 게임 자체의 screen 동작은 Blueprint여도
   된다. C++는 style state를 고르고, Blueprint는 hierarchy, layout, 각 state의 외형을 소유한다.
   base는 필요한 것을 compiler가 검사하는 bound name으로 찾으며, bound list의 Reci host(layout box,
   viewport)는 compiler가 검사하는 placement로 놓인 그 list의 parent다. base는 작성된 hierarchy를
   편집하거나 교체하지 않는다. 작성된 root 위에 runtime wrapper를 두려면, CommonButtonBase가 root를
   감싸듯, 무엇을 끼워 넣는지 문서에 밝힌 property가 필요하다. asset을 다시 쓰는 것은 없다.

8. **작은 조각, 관심사마다 owner 하나.** 데이터, selection, focus return은 list source에, topology와
   entry layout은 layout host에, scrolling은 viewport에, centring은 carousel에, animated value는
   motion에, screen의 key는 screen에, popup의 수명은 그 owner에, layer 사이의 focus restoration은
   CommonUI에 속한다. shared input preprocessor는 입력의 owner를 고르기만 하며 그에 필요한 state만
   둔다. core는 semantics, input, extension point를 소유하고, ReciCommonUIWidgets는 그 위의
   presentation form을 소유한다.

9. **플러그인은 데이터를 모른다.** list는 어떤 UObject든 담으며, object identity가 키다. 의미와
   semantic ID는 게임의 것이다.

10. **게임 중립적이며, 게임 없이도 완결된다.** 플러그인 코드, 주석, 문서, 데모는 어떤 게임도
    언급하지 않는다. 입력 device, 엔진, 또는 많은 게임에 있는 종류의 screen(list, carousel,
    key-binding editor, timed press)에서 비롯되며 다른 게임이 그대로 쓸 수 있는 기능이 여기에
    속한다. 한 게임의 콘텐츠나 규칙에서 나온 요구는, 그 게임의 여러 screen이 공유하더라도 그 게임에
    남는다. 플러그인이 기대는 모든 규칙은 여기에 적혀 있고, 모든 공개 기능은 데모나 게임 없는
    테스트가 보여 준다.

11. **의존성보다 seam.** 플러그인은 엔진 module, CommonUI, Enhanced Input에만 의존한다. 그 밖의
    것(device key time)은 게임이 연결하는 seam을 통해 붙는다. 각 seam은 언제, 얼마나 자주, 어떤
    event에 호출되는지를, Reci가 호출 없이 consume하는 event까지 포함해 밝히며, 플러그인은 아무것도
    연결되지 않아도 동작한다.

12. **모든 device, 하나의 model.** 모든 device는 같은 command와 state에 닿는다. 차이는 목록에
    적는다. 마우스만 hover하고, 터치는 key-binding row의 hint 외에는 hint를 보이지 않으며, pointer
    click은 release 때 완료되고, stick은 threshold 단위로 step한다. device 변경은 presentation을
    바꿀 뿐 저장된 choice는 절대 바꾸지 않으며, active screen 안의 유효한 focus를 그 자리에 둔다.
    device를 바꾸는 press는 그 전에 눈에 보이게 focus되어 있지 않던 target에 절대 작용하지 않는다.

13. **directional navigation, 그리고 모든 action에 닿을 수 있어야 한다.** 모든 Reci control은
    directional navigation으로 닿고 조작할 수 있으며, shortcut이 유일한 경로인 경우는 없다. 예외는
    direction key를 스스로 받는 screen(focus가 다른 곳에 있는 동안 screen action이 list를 drive하는
    경우)이다. 그런 screen은 모든 action에, 키보드와 게임패드에서 hint가 보이는 다른 key를 준다.
    `RegisterScreenInput`은 그런 screen을 class마다 한 번 검사해 그런 key가 없는 control이나
    action마다 경고하며, `Check Screen Reachability`는 같은 검사를 query로 제공한다. 이 검사는 어떤
    action이 list의 entry나 그 안의 nested control에 닿는지 볼 수 없으므로, 그 부분은 수동 확인으로
    남는다. Tab은 프로젝트가 Tab Navigation을 끄지 않는 한 Slate에서처럼 focus를 옮기며, 이는
    APG에서 의도적으로 벗어난 것이다.

14. **안정된 presentation.** entry는 persist한다. navigation과 layout은 entry를 다시 만들거나
    reparent하지 않으므로, Reci list는 virtualize되지 않으며 모두 살려 둘 수 있을 만큼 작은 list에
    맞는다. 긴 list는 ListView를 쓴다. motion은 프레임 레이트와 무관하며, motion을 줄이거나 멈추는
    scale을 따른다.

15. **작고, 쓰임이 입증된 public surface.** Blueprint에 보이는 모든 것, 게임이 호출하는 모든 C++
    entry point(`IReciScreenInput`, `SetEventTimeResolver`), ReciCommonUIWidgets가 쓰는 모든
    extension point는 사용처(데모나 테스트), 문서, 테스트를 가지며, 작업마다 지원하는 방법은 하나다.
    그 밖의 public C++는 module 내부의 배관이며, "Internal" 주석을 달고 가능하면 friend를 둔
    private으로 한다. ReciCommonUIWidgets는 Internal member를 쓰지 않는다. 첫 릴리스에는 쓰이지
    않거나 legacy인 API가 없다.

## 누가 무엇을 소유하는가

| Owner | 소유하는 것 |
| --- | --- |
| Core | list semantics(item 수명, selection mode, driven item, focus return, confirm 순서). input preprocessor: Slate hit path에서 얻는 pointer target, capture, hover와 leave notice, repeat와 release가 가는 곳, non-activatable popup을 위한 popup layer, screen-input dispatch, 각 stick deflection의 handler와 dominant axis. style slot 위의 focus visual. hint key choice. Reci UI Input Data 아래에서 각 player의 navigation config. scrolling, motion, motion scale. emphasis protocol(item마다 0에서 1, viewport가 drive하고 entry가 받음). Looping Entries의 copy(item 하나, index 하나, presentation 여럿)와, looping viewport가 그 copy를 옮기는 hook. |
| ReciCommonUIWidgets | core public API 위의 form: carousel의 centring과 looping, advanced list의 item별 두 번째 presentation, two-level list, 그리고 marquee text나 counting number 같은 이후의 form. |
| 게임 | 모든 layout, style, Widget Blueprint. 데이터와 semantic ID. 각 list의 role. 각 screen의 key, 정책, 등록. [프로젝트 설정](#프로젝트-설정). screen lifecycle과 Back flow. seam 연결. player의 motion 선호를 저장하고 세션마다 설정하기. accessibility label. |
| CommonUI | activation, stack, modality; input mode; layer 사이의 focus restoration; screen의 Input Mapping 적용; click action과 Back action; button의 Triggering Input Action과 persistent binding; Bound Action Bar; style; input-method detection. |
| Enhanced Input | binding, remapping, 저장; action의 key. |
| Slate | focus; hit path; key와 stick을 direction으로 바꾸기; Reci control 밖의 navigation; native accessibility. |

## 화면 구조

Reci를 쓰는 UI는 Composer → Screen → 공용 widget·data → ReciCommonUI의 네 layer로 만들기를 권하며,
데모가 이 구조를 따른다. 참조는 위에서 아래로만 향한다. 아래 layer는 자신을 쓰는 위 layer를 모르고,
event dispatcher로 의도만 알린다.

| Layer | 부모 class | 맡는 것 | 하지 않는 것 |
| --- | --- | --- | --- |
| Composer | `CommonActivatableWidget`(Auto Activate) | map마다 하나. UI input mapping, `Stack`과 그 위의 `ModalStack`(Common Activatable Widget Stack), `Construct`에서 첫 Screen push, Screen의 `On…` dispatcher 연결, modal push, map 이동 | Screen 안의 layout과 list 조작 |
| Screen | `CommonActivatableWidget` | 한 화면의 표시, 입력, 초기 focus. 다음 행동은 `On…` dispatcher로 요청한다. | Composer나 다른 Screen 참조, map 열기, modal push |
| 공용 widget·data | button은 `ReciFocusStyleButtonBase`, 표시용은 `UserWidget`, data는 `Object` | list entry, button, hint와 그 item data | Screen 참조, `CommonActivatableWidget` 상속 |
| ReciCommonUI | C++ | list, scrolling, carousel, focus, screen input, key hint의 공통 동작 | 게임 module 참조 |

- level Blueprint는 `BeginPlay`에서 Composer를 만들어 viewport에 넣고 input mode를 정한다. Screen은
  Composer의 `Stack`이 activate하므로 Auto Activate를 끈다.
- 초기 focus: CommonUI는 Screen이 active root에 들 때와 input method가 바뀔 때
  Get Desired Focus Target을 묻는다. `Keep Focus Or Target`(self, 그 control)을 반환하면 Screen
  안의 focus는 그대로 남는다. 시작 focus가 그 target과 다른 Screen은 `Construct`에서 시작 control에
  focus를 준다(데모: target은 tab row, 시작은 Lists page의 첫 entry).
- Screen 안의 부분(page 같은 `UserWidget`)도 dispatcher로 요청하고, Screen이 같은 이름의 자기
  dispatcher로 넘긴다. Composer는 결과를 Screen의 function으로 돌려준다.
- Is Modal dialog는 Composer가 `ModalStack`에 push한다. CommonUI가 Is Modal widget을 따로 root로
  두므로, 열려 있는 동안 Screen은 activated 상태로 `Stack`에 남지만 입력을 받지 않고 Reci hint는
  key를 보이지 않는다.
- 공용 widget이 `CommonActivatableWidget`을 상속하면 안 되는 이유: Reci는 widget이 속한 screen을
  가장 가까운 `CommonActivatableWidget`으로 찾으므로, 그 class인 entry 안의 button은 activate된 적
  없는 entry를 자기 screen으로 본다.
- 데모: `DemoMap` → `WBP_DemoComposer` → `WBP_DemoScreen`(tab과 page 여섯 개)과
  `WBP_DemoConfirmModal` → `Widgets`, `Data`. Modal page의 Open dialog가 `On Dialog Requested`를
  page → Screen → Composer로 보내면 Composer가 `WBP_DemoConfirmModal`을 `ModalStack`에 push하고,
  dialog의 `On Closed`를 받아 Screen의 `Show Dialog Choice`로 고른 결과를 돌려준다.
- split-screen이나 modular UI가 필요한 프로젝트는 이 Composer를 Lyra의 Primary Game Layout처럼
  player마다 하나인 root layout과 이름 붙은 layer stack으로 확장할 수 있다.

## 프로젝트 설정

- **Game Viewport Client Class** (General Settings > Default Classes): `CommonGameViewportClient`
  또는 그 subclass. CommonUI는 button key, Back, persistent binding을 이것을 통해 routing하며, Menu
  mode에서 처리되지 않은 key를 여기서 막는다.
- **Input Data** (Common Input Settings): click, Back, 네 가지 navigation action을 갖춘 **Reci UI
  Input Data**(`ReciUIInputData`)로 하고, **Enable Enhanced Input Support**를 켠다(CommonUI의 click
  action과 Back action에도 필요하다). 그러면 각 player의 Slate navigation이 그 player의 mapping을
  따른다. navigation action의 key는 direction이 되고, click과 Back의 key는 Accept와 Back이 된다.
  **Mapped Keys Replace Default Keys**(기본값 켜짐)가 켜져 있으면 mapped key가 그 device 종류(키보드
  또는 게임패드)에서 해당 direction이나 action의 Slate default key를 대체하고, 꺼져 있으면 default
  key에 더해진다. 마우스 버튼과 터치는 절대 navigation key가 아니다. 다른 Input Data는 navigation,
  click, Back을 Slate에 맡긴다. Enhanced Input support가 꺼진 Reci UI Input Data는 보고된다.
- **Reci Common UI > Navigation**: Tab Navigation(기본값 켜짐; 끄면 모든 player에 적용)과 [Stick
  Dominant Axis](#sticks)(켜짐). 둘 다 Reci가 설치하는 config에 속하므로, navigation이 Slate 자체의
  것인 동안(Tab Navigation이 켜진 stock Input Data)에는 어느 쪽도 적용되지 않는다. Reci는 opt-in이
  요구하는 동안에만 player의 config를 교체하고 그 뒤에 복원하며, 이 모든 것을 local player가 생성될
  때 읽는다.
- **Reci Common UI > Motion > Default Motion Scale**: [Motion](#motion)을 보라.
- **Render Focus Rule** (User Interface > Focus): `Never`. 그래야 style이 그리는 focus 위에 Slate가
  사각형을 그리지 않는다.
- **`CommonUI.ShouldVirtualAcceptSimulateMouseButton=0`** (`DefaultEngine.ini`,
  `[ConsoleVariables]`). 켜져 있으면 CommonUI는 게임패드 Accept를 cursor 위치의 click으로 바꾸는데,
  키보드나 마우스 입력 뒤에는 cursor가 focus된 control 위에 있지 않으므로 첫 Accept가 사라진다. 꺼져
  있으면 Accept는 focus로 간다.
- widget을 쓰는 C++ module은 `ReciCommonUIWidgets`에 의존한다. Blueprint는 아무것도 필요 없다.

## 상태

| 상태 | 의미 | 수명 |
| --- | --- | --- |
| Hover | interactive element 위에 있는 pointer | pointer가 떠날 때까지; 기본적으로 navigation이나 selection을 바꾸지 않음 |
| Focus | 한 user에 대한 Slate의 입력 수신자 | layer가 닫히면 복원됨; key navigation 중에는 항상 보임 |
| Driven item | list의 navigation cursor | focus가 떠나도 유지됨; screen input이 focus 없이 drive할 수 있음 |
| Selected / value | control에 저장된 choice | focus, hover와 독립; single choice면 하나, Multi면 여럿 |
| Pressed | press feedback | release, 취소, capture loss까지 |
| Confirm | 상태가 아닌 event | 물리적 activation 한 번에 handler 하나 |

일반 list는 driven item을 focus된 entry로 옮긴다. screen이 drive하는 selector는 Focus On
Navigation을 끈다. screen은 여러 selection과 cursor를 둘 수 있으며, command마다 목적지는 하나다.
focus가 다른 곳에 있는 동안 screen key가 옮기는 driven item은 key cursor이며, 두 번째 outline
옆에서도 focus 외형을 입는다. Focus On Navigation이 꺼진 list가 key cursor를 그리며(Show Driven Item
Cursor는 기본값 켜짐이고, entry에 Set Key Cursor Visual을 호출한다), 그런 list 여럿을 번갈아
drive하는 screen은 쉬고 있는 list에서 이것을 끈다. Focus On Navigation이 켜진 list는 아무것도 그리지
않는다. controller-backed source(two-level list)는 자신의 navigation과 selection coupling을
소유한다.

## Lists

| Role | Navigation | Pointer | Confirm |
| --- | --- | --- | --- |
| Action menu | driven item과 focus를 옮김; Selection Mode None | action을 실행 | driven action을 실행 |
| Immediate single choice | cursor와 selection을 옮김(Selection Follows Navigation) | select | 완료하거나, 선언된 action을 실행 |
| Explicit single choice | cursor만 옮김 | select | cursor를 commit한 뒤 완료 |
| Multiple choice | cursor를 옮김; set은 그대로 | toggle | set을 바꾸지 않고 완료 |
| Value editor | 진입; adjust하면 value가 바뀜 | adjust | 편집에 진입하거나 편집을 완료 |
| Content scrolling | viewport만 | drag로 scroll | 없음 |

- **기본값**: Focus On Pointer Hover, Clear Selection On Focus Return, Selection Follows Navigation,
  Confirm On Item Click은 꺼짐, Show Driven Item Cursor는 켜짐. 기본(Single) list는 explicit single
  choice다.
- 자동 selection은 Single 전용이며 Multi를 item 하나로 줄이지 않는다. choice는 그 갱신이 예측
  가능하고 빠를 때만 즉시 적용한다. 느리거나, 원격이거나, 파괴적인 action은 명시적 activation이
  필요하다. rounded outline은 focus용이며, hover와 selection은 role fill을 유지한다.
- **Confirm**은 enabled 상태와 interaction 상태를 검사하고, Single selection을 commit한 뒤, 유일한
  completion event인 On Item Confirmed를 한 번 발생시킨다. Multi는 절대 바꾸지 않는다. 빈 list에는
  target이 없다.
- **Identity**: Set List Items와 reorder는 살아남은 object의 selection을 유지한다. object를 다시
  만드는 게임은 ID를 remap한다. 제거된 selection은 사라지고, 자동 Single choice는 유효한 cursor를
  따른다. disabled entry는 실행되지도 focus를 받지도 않으며, list에 진입하면 첫 번째 enabled entry에
  도달한다. rebuild(Set List Items) 전에 owning player의 focus가 list나 그 안에 있었으면 focus는
  복원된 driven item으로 간다. 그 item이 focus를 받을 수 없으면(disabled, hidden) list에 진입할 때처럼
  첫 번째 enabled entry로 가고, 그런 entry가 없으면 focusable한 list 자신으로 가며, 그렇지 않으면
  그대로 있다. 다른 곳의 focus는 그대로 있고, 나중에 list로 돌아오면 rebuild가 없었던 것처럼 driven item에
  도달한다. two-level list의 list에서는 two-level list가 rebuild 뒤 focus를 focused parent의 row로
  되돌린다.
- **Hold**: list는 Reci preprocessor에서 key-down 시점에 Accept를 받고 Reci Scroll Box는 entry
  click을 스스로 인식하므로, 둘 다 CommonUI의 hold 없이 confirm한다. hold를 요구하는 entry는
  보고된다. Common Activatable Widget에서 파생된 entry class도 보고된다(Reci는 widget에서 가장
  가까운 Common Activatable Widget을 그 widget의 screen으로 삼는다).
- **Looping Entries**는 looping scroll box(carousel)가 list의 Reci Vertical Box나 Reci Horizontal
  Box를 host하는 동안에만 entry를 반복한다. 각 item은 index 하나와 selection 하나를 유지하며, 어느
  copy를 click해도 그 item이 select된다. 그 밖의 곳이나 Reci Tile Box 안에서는 각 item이 한 번만
  보이며, 이는 보고된다.
- 선택하며 scroll하는 list는 scroll하면서 driven item을 바꿀 수 있다. 텍스트나 정보 scroll view는
  절대 selection을 바꾸지 않는다.
- carousel 밖에서 Reci Advanced List의 main item이 아닌 item을 click하면 그 item이 main item이 되며,
  Confirm On Item Click과 Confirm Main Item On Click이 켜져 있으면 같은 click으로 confirm까지 한다.
- Reci layout box는 자기 list의 layout을 소유한다. 그 설정이 list의 ItemsBox 설정을 대체하며, 이는
  보고된다.

## Input routing

[원칙 2](#원칙)가 순서를 정한다. 그 밖에:

- routing은 local user와 active layer별로 이루어진다. screen push나 pop, 또는 focus 이동 뒤에
  command가 두 번째 action으로 bubble할 수 없다.
- Reci는 자신이 받은 press의 repeat를, handler가 여전히 그것을 받든 말든 consume한다. Accept와
  Back의 repeat는 Reci가 press를 받았거나 press가 Reci layer에서 일어났을 때 release까지 막힌다.
  direction은 repeat해도 된다. key-up은 Reci가 그 key-down을 받았을 때, 또는 focus가 이동한 뒤 Reci
  layer에서 stock path가 받은 Accept나 Back을 release할 때 삼켜진다.
- key와 stick routing(popup Back, screen input, focus된 control)은 Menu binding이 발동하는
  CommonUI의 Menu 또는 All input mode에서만 실행된다. Game에서는 모든 것이 stock path와 게임의
  Enhanced Input으로 간다.
- **Raw input**: text field나 선택 중인 key selector가 focus를 가진 동안, Reci는 popup Back을 포함한
  모든 key, stick, pointer press에서 비켜선다. 선택 중이 아닌 focus된 key selector는 Accept를 받고
  direction은 Slate에 맡기며, 이를 둘러싼 Reci control보다 먼저 처리한다.
- device를 바꾸며 focus를 옮기는 Reci layer의 key-down은 그 repeat와 release까지 함께 consume된다.
  key hint 공간은 숨겨진 동안에도 예약된 채로 남는다.
- popup 닫기와 held-key 검사는 player의 navigation config를 쓴다.

### Sticks

- stick은 focus된 Reci control을 Slate의 analog navigation처럼 player의 navigation config(threshold,
  repeat timing)에 따라 step하며, deflection마다 owner는 하나다. 첫 step을 받은 control은 axis가
  threshold 안으로 돌아오거나, 방향을 틀거나, (Stick Dominant Axis에서) stick의 direction이 아니게
  될 때까지 이후의 step을 모두 받는다. 거절된 deflection은 통째로 Slate로 가므로, stick으로 list
  끝에서 list를 벗어날 수 있다. Reci는 config의 repeat state를 건드리지 않고 첫 step을 읽는다.
  Reci가 설치하는 config는 몇 번을 묻든 각 stick 값에 한 번만 답하므로, 거절된 step도 Slate에
  닿는다.
- **Stick Dominant Axis**는 Unity uGUI의 StandaloneInputModule을 Slate의 threshold와 timing으로
  따른다. stick은 Slate, focus된 control, screen key 모두에 대해 한 번에 한 direction으로만
  움직이며, threshold를 넘은 뒤 더 많이 민 axis를 따른다. 유지하면 repeat마다 그때 더 많이 민 axis를
  따라 repeat한다. 첫 repeat는 마지막 이동과 90도 이내일 때만 repeat delay만큼 기다리고, 90도 이상
  틀면 repeat rate로 움직이며, 중립으로 돌아오면 처음부터 다시 시작한다. device는 한 poll의 axis들을
  순서대로 보내므로(XInput에서는 X가 먼저), 한 poll에서 도달한 대각선은 X를 따라 먼저 움직일 수
  있다. 꺼져 있을 때와 Slate의 config 아래에서는 각 axis가 따로 navigate한다.
- screen action은 stick을 플랫폼의 stick-direction key(Gamepad Left Stick Up 등)로 받는다. 이 key는
  axis별로 dead zone을 넘으면 눌리고, 누르고 있는 button처럼 repeat된다(기본값 Initial Button Repeat
  Delay 0.2초, Button Repeat Delay 0.1초). Stick Dominant Axis에서는 direction이 stick의 direction이
  아닌 key가 Reci layer에서 repeat와 release를 포함해 아무 데도 가지 않다가, stick의 direction이
  되면 다음 repeat가 press로 도착한다. 눌린 채로 stick의 direction이 아니게 된 key는 repeat가
  consume된다. Reci layer 밖에서는 press가 그대로 진행된다.
- screen이 그 direction의 press를 쥐고 있는 동안, 또는 stick이 screen이 map한 direction으로
  threshold를 넘었으나 그 key가 아직 도착하지 않았거나 붙잡혀 있는 동안에는 stick이 focus를 옮기지
  않는다. 어떤 screen도 받지 않는 direction은 그대로 진행한다. key는 focus된 navigation과
  CommonUI로, deflection은 focus된 control과 Slate로 간다.

## 포인터와 터치

- pointer activation은 hit target에, key activation은 focus나 command target에 속한다. hover,
  pointer가 list를 떠나는 것, list 위에서의 wheel 회전은 절대 focus를 가져가지 않는다. 이미 keyboard
  focus를 가진 list만 그 focus를 driven item으로 되돌린다. list로 돌아오는 focus는 driven item에
  도달하며(Reset Last Focused Item On Focus Return이 켜져 있으면 첫 번째 enabled entry에), 명시적으로
  요청된 item을 다른 곳으로 돌리지 않는다.
- click이나 tap은 유효한 release에서 완료되며, 인식된 drag나 capture loss는 이를 취소한다. 두 번째
  손가락은 gesture를 넘겨받을 수 없다.
- carousel에서 옆 item을 click하면 그 item을 centring하고 select한다. centred item을 click할 때만
  confirm한다.
- Reci Scroll Box의 axis를 가로지르는 drag는 Drag Trigger Distance를 넘을 때 한 번, direction으로서
  On Navigation Input에 도달한다. 이 drag는 시작된 entry에 속하며, 그 entry는 먼저 key로 그렇게 되듯
  driven item이 된다(Focus On Navigation이 켜져 있으면 focus도 받는다). 옆 carousel card 위에서는
  card를 centring하기만 한다. interactable entry가 아닌 곳에서 시작하면 broadcast되지 않고 아무것도
  움직이지 않는다. 이 drag는 절대 scroll하거나 click하지 않으며, box가 release까지 붙잡는다. axis를
  따르는 drag는 focus를 driven item으로 되돌리고 scroll한다. handler가 bind된 동안, Reci Vertical
  Box나 Horizontal Box 안의 list는 cross-axis direction을 Slate에 넘기지 않는다.
- entry에 nested된 control(stepper의 -/+, dropdown field, key selector)에 대한 press는 그 control의
  것이다. entry는 자기 자신에 대한 press에서만 drag하고 click한다.

## 팝업과 드롭다운

- 열린 popup(dropdown의 list)은 그 user의 key와 pointer에 대한 최상위 layer다. Back은 먼저 popup을
  한 번에 한 layer씩 닫는다. 그동안 어떤 screen도 capture, action, stick을 받지 않고, popup 안의
  Reci control만 focus로 가는 key를 받으며, 아래의 list는 절대 popup의 pointer를 받거나 popup 밖으로
  focus를 끌어내지 않는다. popup의 list가 거절한 key는 stock path로 간다. popup은 key-modal이
  아니므로, Slate가 focus를 밖으로 옮겨 popup을 닫을 수 있고 CommonUI의 binding이 발동할 수 있다.
- Reci Dropdown은 그런 popup 안의 explicit single choice이며, popup의 Blueprint가 Reci layout box
  안에 `Options`라는 이름의 Reci List를 제공한다. 열면 현재 option을 select하고 다음 tick에 그
  option에 focus해 보이는 범위로 가져온다. navigation은 cursor를 옮길 뿐 value는 절대 바꾸지 않는다.
  option 위의 click이나 Accept는 고르고 닫는다. On Selection Changed는 새 value일 때만 발생한다.
  Back, Close Dropdown, 그 밖의 모든 닫기는 value를 유지한다. focus가 option 안에 있는 채로 닫으면
  focus는 열 때 focus를 쥐고 있던 widget으로 돌아가고(그래서 dropdown을 host하는 focusable row가
  focus를 되찾는다), 그 widget이 사라졌으면 dropdown이 enabled이고 focusable할 때 dropdown으로
  돌아가며, 그 밖에는 focus가 그대로 있다. On Dropdown Open Changed는 열기와 닫기를 매번 알린다.
- button은 popup의 toggle이다: Click Method Mouse Down, Is Selectable, Is Toggleable, Interactable
  When Selected. 열린 popup의 anchor에서 시작한 press는 mouse든 touch든 popup을 닫고, 그 release는
  popup을 다시 열지 않는다. 이 설정 중 하나를 바꾸면 compile 시 경고하고, 한 번 보고하며,
  construction 시 재설정된다. compiler는 layout box 밖의 Options list와, Dropdown Popup Class 없이
  배치된 dropdown도 보고한다.

## Key hints

- **Reci button의 hint**는 ReciFocusStyleButtonBase가 drive하는 Reci Mapped Key Text인
  `InputActionWidget`이다. 자기 key를 가진 button은 CommonUI의 순서대로 항상 그 key를 보인다.
  Triggering(또는 Triggered) Input Action, 없으면 Hint Input Action(표시 전용으로, Back처럼 screen이
  button 대신 처리하는 key용)이다. 그 밖의 button은 click action을 focus된 동안에만 보이며, hover
  때는 보이지 않는다. disabled button은 아무것도 보이지 않는다. CommonUI의 Hide Input Action, Hide
  Input Action With Keyboard, Should Use Fallback Default Input Action이 적용된다. 그 자리에 다른
  Common Action Widget이 있으면 compile 시 경고한다.
- key가 없는 action은 아무것도 보이지 않는다. Hide When Unmapped는 켜진 채로 둔다(Reci가 켜고 한 번
  경고하며, compiler도 경고한다).
- **Left-out keys**: hint는 앞선 handler가 지금 가져가는 key를 뺀다. 열린 popup의 Back, capture 중인
  screen(모든 key), button 자신의 것이 아닌 suspend되지 않은 screen action, 그리고 CommonUI
  binding보다 앞서는 focus된 widget의 Accept, direction, typing이다. focus path 위의 screen이 map한
  Hint Input Action은 그 screen이 key를 받고 그 action을 suspend하지 않는 동안 보인다. button의
  activatable widget이 CommonUI active root 밖에 있는 동안(nested Is Modal dialog 아래) key는 보이지
  않는다. hint는 변경을 한 프레임 안에 따라간다.
- **Hint line**: list를 drive하는 key는 screen의 hint line에 둔다. hint line은 그 action들을
  지정한(여럿이면 Additional Input Actions) Reci Mapped Key Text이며, 현재 device용이다. hint line도
  같은 순서로 key를 뺀다. action은 그것을 map한 focus path 위의 등록된 screen으로 가고, 없으면
  (click, 그리고 Reci가 Reci UI Input Data를 따르는 동안의 navigation은) focus된 widget으로, 그것도
  아니면 CommonUI binding으로 간다. active root 밖에서는 아무것도 보이지 않는다. Key Capture Hint
  line은 capture하는 동안 capture가 받는 key를 보이고, 그 밖에는 아무것도 보이지 않는다. key-binding
  row는 device를 고정하고 모든 key를 보인다.
- **Guide line**: CommonUI의 Bound Action Bar. Reci button의 Triggering Input Action은 거기에
  action의 description을 이름으로 하여 보인다(Show Triggering Action In Action Bar). bar의 button
  안에 있는 key text는 CommonUI binding보다 먼저 가져가는 key를 뺀다. in-place hint와 bar는 서로
  독립이다.
- **그리기**: 표시되는 device와 gamepad type에 맞는 CommonUI controller data의 glyph, 없으면
  이름(Key Name Overrides, 없으면 내장 짧은 이름). 터치는 key-binding row의 hint를 뺀 모든 hint를
  숨긴다. reserved width는 빠진 key까지 세며 모든 device에서 같다. button의 click key는 focus가
  없어도 자리를 유지하므로, focus가 layout을 움직이지 않는다. deactivated screen의 hint는
  페이드아웃하는 동안 key를 유지한다.

## Two-level list

`ReciTwoLevelListBase`는 row마다 다른 axis의 child list를 담은 parent list(카테고리와 그 item들)에
대해 데이터, focus, selection, confirm, 입력을 연결한다. asset은 바꾸지 않는다.

- **Bases**: list의 Blueprint는 이것에서 파생되며 `ParentList`라는 이름의 Reci List나 Reci Advanced
  List를 담는다. row의 Blueprint는 `ReciTwoLevelItemBase`(ReciFocusStyleButtonBase이며 parent의
  focus button)에서 파생되며 `ChildList`라는 이름의 list 하나를 담는다. 둘 다 BindWidget이다.
  ParentList의 normal entry class와 main entry class는 row base에서 파생된다. child entry는
  CommonButtonBase button을 담고 `ReciTwoLevelEntry`를 구현한다. child button은 click과 selection을
  받지만 keyboard focus는 절대 받지 않는다.
- **Placement**: 각 list는 Reci Vertical Box나 Reci Horizontal Box의 직접 child이고, 그 box는
  선택적으로 Reci Scroll Box나 Carousel Scroll Box의 유일한 child가 되며, 모두 이름을 붙이지 않는다.
  그 바깥은 자유다. 각 list의 box가 axis를 정한다. axis가 같거나 Tile Box이면 two-level navigation과
  confirm을 막는 configuration error다. list의 Blueprint는 ParentList의 placement와 entry class,
  그리고 ChildList가 다른 axis를 쓰는지를 검사하고, row의 Blueprint는 ChildList의 placement와 entry
  class를 검사한다. native tree나 런타임에 바뀐 class는 list가 build될 때 class마다 한 번 로그에
  남고, Is Configuration Valid가 false가 된다. base는 box를 추가하지 않으며 root, slot, size,
  alignment, padding을 교체하지 않는다. 간격은 각 box의 Item Spacing이고, motion은 scroll box에서
  설정하며, normal design과 main design은 각 Advanced List에 둔다(별도의 main design에 있는
  ChildList도 bind된다). stock Scroll Box의 gesture는 통합되지 않는다.
- **데이터**: Set Items는 parent object들과 child count provider, 즉 읽기 전용 Blueprint
  function(Parent 입력, count 출력)을 받는다. child object는 없다. row는 Set List Item Object에서
  parent를 받고(Parent Item으로도 얻는다), child entry는 Set Child Item(Parent, Child Index)에서
  child를 읽는다. normal presentation과 main presentation은 같은 데이터를 받는다. null parent와 중복
  parent는 빠지며, count가 0 이하면 empty row다. Refresh Items는 count를 다시 읽고 rebuild하며,
  Refresh Presentation은 binding을 다시 검사하고 rebuild한다. navigation과 motion은 절대 rebuild하지
  않는다.
- **입력**: parent axis는 parent focus를 옮기고, child axis는 child를 select한다. parent를 click하면
  그 parent에 focus하고, child를 click하면 그 parent에 focus하고 그 child를 select한다. key와 drag도
  같은 axis를 따른다. wrap하지 않는 끝에서, 또는 configuration error일 때 direction은 Slate로 간다.
  focus가 list를 떠날 수는 있지만(안에서 누른 key는 끝에서 멈춘다) 절대 row로 들어가지는 않는다.
  Wrap Navigation이나 Stop rule은 focus를 안에 둔다. drag는 그 axis의 viewport로 가고, viewport가
  없으면 swipe마다 item 하나씩 옮긴다. inactive row의 inertia는 choice를 유지한다. wheel은 parent
  viewport나 list로 간다. Confirm 입력이나 Confirm Selection만 confirm하며, click이나 drag는 절대
  confirm하지 않고, empty row에서도 confirm하지 않는다. On Focused Parent Changed는 parent와 index를
  주고, On Selection Changed와 On Confirmed는 child index를 더한다.
- **Child selection**은 row마다 따로다. Synchronize Child Selection(런타임에는 Set Synchronize Child
  Selection)은 index 하나를 공유하며, 짧은 row에서는 마지막 child로, empty row에서는 -1로 clamp된다.
  짧은 row에 focus해도 shared index는 낮아지지 않는다. Set Items를 다시 호출하면 parent identity와
  child index로 state를 복원한다. child index는 위치이므로, reorder 뒤에는 게임이 새 index로 Set
  Selected Child Index를 호출한다.
- **Presentation**: ReciTwoLevelEntry의 선택적 Update Two Level State는 parent focused 여부, child
  selected 여부, 두 index를 바뀌었을 때만 준다. row base도 이를 구현한다. emphasis는
  ReciListEntryEmphasis를 통해 도착한다.
- **bind된 동안** base는 box와 viewport의 입력을 coordinator를 통해 한 번씩 routing하고 그것들
  자체의 입력은 끄며, Selection Mode(parent None, child Single), Clear Selection On Focus
  Return(꺼짐), Allow Pointer Hover(base의 값), Advanced List의 Main Presentation Follows Focus와
  Confirm Main Item On Click(꺼짐)을 override한다. Confirm On Item Click, Selection Follows
  Navigation, Looping Entries는 적용되지 않는다. unbind하면 이것들을 복원한다. override되거나
  무시되는, 작성자가 기본값과 다르게 둔 값은 compile 시와 런타임에 한 번 경고한다. bind된 동안 바뀐
  Allow Pointer Hover는 한 번 경고하고 재설정된다.

## Motion

Slate의 animated scroll(FInterpTo)과 inertia(FInertialScrollManager, explicit Euler)는 프레임
레이트에 의존하므로, Reci의 viewport는 ReciMotion을 통해서만 움직이며 Slate는 애니메이션 없이만
scroll시킨다. drag는 content를 1:1로 움직인다. drag 뒤의 glide는 drag의 smoothed velocity로부터 어떤
프레임 레이트에서든 같은 거리를 가며, 이 velocity는 pointer가 멈춰 있는 동안 감쇠한다.

motion은 player의 scale(`UReciMotionSubsystem` Set Motion Scale)을 따르고, 없으면 Default Motion
Scale을 따르며, owning player가 없을 때도 이것을 쓴다. 1은 full motion이다. 0은 viewport와
carousel을 glide 없이 곧바로 target으로 보낸다. 그 사이 값에서는 duration, time constant, glide가 그
비율만큼 줄어든다. drag는 1:1을 유지한다. 플러그인은 아무것도 저장하지 않는다. Set Display Scale은
절대 애니메이션하지 않는다.

## 새 위젯이 들어갈 곳

- **Core**: interaction model이나 input layer가 그것 없이는 불완전할 때. list semantics, layout
  host, plain viewport, routing, hint, popup과 dropdown, motion, extension point(emphasis protocol,
  loop hook, viewport hook).
- **ReciCommonUIWidgets**: list나 screen이 그것 없이도 완결되는 하나의 presentation form일 때.
  carousel과 그 looping, advanced list, two-level list, marquee text, counting number. 그 외형은
  여전히 Blueprint의 것이다(원칙 7).
- **게임의 Widget Blueprint**: 그것이 외형일 때. style preset은 widget이 아니다.

core가 widget을 동작시키기 위해 그 widget을 알아야 한다면, 그 지식이 아니라 빠진 extension point를
추가한다.

## 새 입력 기능이 들어갈 곳

- **button 자신의 key** (Apply, Reset): CommonUI의 Triggering Enhanced Input Action. CommonUI는
  focus된 widget이 그 key를 처리하지 않고 남긴 뒤, button이 입력을 받을 수 있고, 그 layer가
  active이며, input mode가 button의 Input Mode Override(기본값 Menu)와 맞는 동안 button을 click한다.
  이것은 screen action이 아니다. screen의 Input Mapping에도 그 action이 있으면, screen action이 먼저
  실행되므로 `HandleReciScreenAction`은 그 action에 대해 false를 반환한다.
- **screen이 button 대신 해석하는 key** (자기 Back handler인 screen의 Back, 측정이 되는 press):
  screen이 CommonUI의 Back이나 screen action을 통해 처리하고, button은 아무것도 bind하지 않는 Hint
  Input Action으로 그 key를 보인다. button을 click하기만 하는 key는 절대 Hint Input Action과 screen
  branch의 조합으로 만들지 않는다.
- **focus가 screen 안 어디에 있든 screen이 필요로 하는 key**: screen action. stock routing이 전달할
  수 없는 것(다른 widget이 focus를 가진 동안 list를 drive하는 direction, key-down 시점으로 시간을
  재는 press)에만 쓴다.
  - screen은 `IReciScreenInput`을 구현하고, activation 때 `RegisterScreenInput`을, deactivation 때
    `UnregisterScreenInput`을 호출한다. interface가 없는 widget은 무시되고 보고된다. interface를
    구현했지만 activation 다음 tick까지 등록하지 않은 screen은 보고된다(development build).
  - screen Input Mapping의 action들(screen이 active인 동안 CommonUI가 적용하는 mapping context)은
    mapping 순서대로, focus된 navigation보다 먼저, key-down마다 그리고 screen이 받은 press의
    repeat마다, key-down time과 함께 `HandleReciScreenAction`에 도착한다. key는 player에게 적용된
    mapping이다. trigger와 modifier는 적용되지 않으며, 등록은 그것을 가진 mapping이나 action마다 한
    번 경고한다.
  - `IsReciScreenActionSuspended`는 action을 당분간 거절한다. 그 action에는 key가 제공되지 않으므로
    key는 focus와 CommonUI로 계속 가고, hint는 그 action을 세지 않으며, event-time resolver는
    실행되지 않는다. hint가 나머지 모든 action을 가져간 것으로 세므로, `HandleReciScreenAction`은 그
    모든 action에 true를 반환한다.
  - screen은 activated 상태이고, 보이고, enabled 상태이며, 자신이나 자신 안에 focus를 쥐고 있고,
    focus된 widget이 CommonUI active root 안에 있으며, input mode가 Menu나 All인 동안 입력을 받는다.
    그 user의 popup이 열려 있거나 raw-input widget이 focus를 가진 동안에는 절대 받지 않는다.
  - focus를 쥔 nested 등록된 screen들은, CommonUI가 child의 binding을 먼저 제공하듯, 각 key를 가장
    안쪽부터(capture, 그다음 action, 그다음 바깥 screen) 제공받는다. focus에서 바깥으로 가며 처음
    만나는 Is Modal activatable가 이 chain을 끝낸다.
  - 처리된 key는 Reci preprocessor에서 멈춘다. 그 key의 Enhanced Input event는 발동하지 않고 Bound
    Action Bar도 그 key를 보이지 않으므로, 그 hint는 hint line에 둔다. 원칙 13이 적용된다.
  - **Event time**: `FReciInputFocusCoordinator::SetEventTimeResolver`는 device key time을 공급하며,
    screen action에 제공되는 key-down이나 repeat마다 한 번 호출된다. 그 header는 언제 호출되는지와
    Reci가 호출 없이 무엇을 consume하는지를 나열한다.
- **Raw key capture** (key-binding editor): `HandleReciScreenKey`는 screen의 action과 focus된
  navigation보다 먼저 각 key를 보며, `IsCapturingReciScreenKeys`가 true인 동안에만 key를 받는다.
  hint는 capture 중인 screen을 모든 key를 받는 것으로 다룬다. stick은 focus를 옮기지 않으며, 그
  direction key는 capture에 닿는다. popup의 Back은 여전히 먼저다. capture가 언제 끝날지는 screen이
  정한다. capture layer(screen 안에 nested되어 active인 동안 등록하는 activatable)를 쓰면 그 수명이
  정하며, capture layer는 가장 안쪽이므로 key를 먼저 받는다. `IsKeyMappedToAction`은 key가 그
  player의 어떤 action에 속하는지를 알려 준다.
- **dialog가 열린 동안 멈춰야 하는 것**: CommonUI의 layer. dialog를 자기 stack에 push하거나 Is
  Modal로 nest한다. 그러면 CommonUI가 screen의 binding을 멈춘다. Reci에는 dialog modality가 없으며,
  Reci의 popup layer는 non-activatable popup용이다. screen input은 CommonUI의 binding처럼 Is Modal을
  따른다. focus가 active root(그 player의 입력을 받는 유일한 root) 밖에 있는 동안에는 없고, nested
  Is Modal widget 안에서 누른 key에 대해서도 없다. Is Modal이 아닌 nested dialog는 자신이 거절한
  key를 screen에 남긴다. 새 root는 paint되고 나서 active가 되므로, 자신이 root인 screen은 첫 프레임
  전에는 screen input을 받지 않는다. Set Is Activatable Tree Enabled가 꺼져 있으면 active root도
  screen input도 없다.
- **focus가 놓이는 곳** (screen이 activate되거나 위의 layer가 닫힐 때): 가장 leaf 쪽의 active
  activatable가 주는 Get Desired Focus Target. CommonUI는 input method가 바뀔 때마다(게임패드 뒤의
  마우스 이동도 포함) 묻기도 하므로, 각 widget은 `Keep Focus Or Target`(self, authored target)을
  반환한다. 이 함수는 그 widget의(nested activatable의 것이 아닌) 보이고 enabled인 control이 focus를
  쥐고 있는 동안에는 None을 반환해 focus를 그대로 두고, 그 밖에는 authored target을 반환한다. Reci
  list라면 그 target은 Get Hosted Focus Target이다. CommonUI는 None일 때 Desired Focus Widget에
  focus하므로 Desired Focus Widget은 설정하지 않는다. focus를 설정하는 graph는 authored target을
  쓴다. Auto Restore Focus가 켜져 있으면 CommonUI가 cache한 target이 먼저다. layer는 choice를
  유지하고, retained screen은 Blueprint lifecycle을 유지한다.
- **모든 dialog 아래에서도 동작하는 key** (볼륨): persistent CommonUI binding(button의 Is Persistent
  Binding, 또는 `bIsPersistent`를 준 `RegisterUIActionBinding`). 이 binding은 activation을
  무시하지만, action bar에 표시되지 않는 한(`bDisplayInActionBar`: `FBindUIActionArgs`에서는 켜짐,
  button에서는 꺼짐) 자기 widget과 그 위의 모든 widget이 보이고, enabled이며, 각각 자기 switcher의
  active child인 동안에만 발동한다. 어떤 dialog도 가리지 않는 layout에 둔다. 이 binding은 Reci와
  focus된 widget이 거절한 뒤에야 key를 본다. 어떤 screen도 소유하지 않는 mapping context로는
  충분하지 않다. Menu에서 CommonUI는 자기 binding이 처리하지 않은 key를 처리된 것으로 다루므로,
  Enhanced Input은 그 key를 절대 보지 못한다.

## 보고

Reci가 처리할 수 없는 구성은 그것을 보는 가장 이른 곳에서 한 번 보고된다. 그곳은 Blueprint
compiler가 검사하는 binding, validity query, 또는 `LogReciCommonUI`에 한 번 남기는
경고(`ReciReport.h`)다. 보고는 shipping build를 절대 크래시시키지 않으며, 그런 구성은 절대 조용히
맞춰지거나 무시되지 않는다.

## 목표가 아닌 것

- 웹 ARIA나 WCAG 준수, 또는 screen reader 지원. screen reader 지원은 별도의 감사가 필요하다.
- bundled key icon. controller data가 없으면 CommonUI는 게임패드를 구별할 수 없으므로, 게임패드
  이름은 XInput의 것이다.
- Reci가 아닌 button에 대해 CommonUI가 고르는 hint에서 key를 빼는 것(그 hint는 hover 때 click
  action을 보이며, 거기 놓인 Reci Mapped Key Text는 받은 key를 모두 보인다).
- button hint에 대한 Reci의 opt-out. CommonUI의 flag가 적용된다.
- style preset이나 theme. 데모의 style(테마는 Material 3)은 Reci 코드가 읽지 않는 예시다.
- CommonUI의 screen lifecycle을 대체하는 것, 또는 Widget Blueprint를 생성하거나 다시 쓰는 것.
- virtualized list.
- 첫 릴리스 전에 제거된 API를 위한 shim.

## Known deviations

각 deviation은 고쳐야 하며, 고치면 이 목록에서 뺀다.

**Public surface (원칙 15)**
- coordinator의 pointer와 wheel routing(hit-path target, hover isolation, click을 취소하는 drag나
  capture loss, control 위의 wheel)에는 automation test가 없다. Slate가 harness의 virtual window를
  hit-test하지 않으므로 이것들은 수동 확인이다.

## 데모

`Content/Demo/DemoMap`과 `DemoInputData`(Reci UI Input Data). tab으로 나뉜 page 여섯 개:

- **Lists**: -/+ button이 있고 Left, Right, 옆 방향 drag로도 value가 바뀌는 entry, 그리고 자기 key를
  가진 button.
- **Choices**: text와 enum에서 만든 dropdown, 그리고 device마다 고정된 key-binding row.
- **Carousel**: selected card가 커지는 looping carousel.
- **Grid**: tile을 Accept나 click으로 toggle하는 Multi selection Tile Box.
- **Modal**: 자기 Back handler인 Is Modal dialog(`WBP_DemoConfirmModal`)를 여는 button. dialog는
  Composer가 push한다.
- **Two-Level**: child carousel을 가진 카테고리. On Confirmed가 그 쌍을 보인다.
