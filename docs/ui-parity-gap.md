# UI Parity Gap Audit — Cocos Creator TS → cocos4wasm C++

Generated: 2026-04-19
Baseline: P5.5 UI-alignment work

## Executive Summary

Most UI components are structurally present but property registration and event handler serialization vary widely. Batch commit cd9a96b (9 components) introduced stubs lacking full CC_PROPERTY coverage.

**Key gaps:**
- Event handler arrays (ComponentEventHandler[] → std::function) won't deserialize from JSON
- Missing properties: ScrollView.content, Slider.slideEvents, ProgressBar.reverse, PageViewIndicator.pageView
- Missing methods: PageView pagination (gotoPage), ToggleContainer.toggleItems getter
- Platform divergence: EditBox uses SDL native instead of DOM

## Summary Table

Component | __type__ | Props | Methods | Events | Enums | Verdict
---|---|---|---|---|---|---
Button | ✅ cc.Button | 11/12 | ✅ | ⚠️ Handler differs | ✅ | Good
Widget | ✅ cc.Widget | 16/16 | ✅ | ✅ | ✅ | Excellent
Layout | ✅ cc.Layout | 14/14 | ✅ | ✅ | ✅ | Excellent
ScrollView | ✅ cc.ScrollView | 8/12 | ⚠️ Partial | ⚠️ Handler | ✅ | Partial
ScrollBar | ✅ cc.ScrollBar | 4/4 | ⚠️ Basic | ✅ | ✅ | Minimal
Slider | ✅ cc.Slider | 3/4 | ⚠️ | ⚠️ Missing slideEvents | ✅ | Stub
ProgressBar | ✅ cc.ProgressBar | 4/5 | ⚠️ | ✅ | ✅ | Stub
Toggle | ✅ cc.Toggle | 4/4 | ✅ | ⚠️ Handler | ✅ | Good
ToggleContainer | ✅ cc.ToggleContainer | 2/3 | ⚠️ Missing toggleItems | ✅ | N/A | Minimal
PageView | ✅ cc.PageView | 5/7 | ⚠️ Stub | ✅ | ✅ | Stub
PageViewIndicator | ✅ cc.PageViewIndicator | 4/5 | ⚠️ Stub | ✅ | ✅ | Stub
SafeArea | ✅ cc.SafeArea | 1/1 | ✅ | ✅ | N/A | Good
BlockInputEvents | ✅ cc.BlockInputEvents | 0/0 | ✅ | ✅ | N/A | Excellent
EditBox | ✅ cc.EditBox | 9/10 | ✅ | ✅ | ✅ | Partial

## Critical Issues

### 1. Event Handler Serialization (System-level blocker)
Upstream uses ComponentEventHandler[] JSON format:
  { target: nodeUUID, component: Button, handler: onButtonClick }

C++ uses std::function vectors which cannot deserialize from JSON. Editor will silently drop handlers.

Affected components: Button, Slider, Toggle, ToggleContainer, PageView, ScrollView, EditBox

Fix: Reflection layer deserialization bridge (1-2 days)

### 2. Missing Properties (Component-level)
- ScrollView: content (Node ref for programmatic scroll)
- Slider: slideEvents (property registration)
- ProgressBar: reverse (boolean for RTL bars)
- Toggle: checkEvents (property registration)
- ToggleContainer: toggleItems (read-only getter)
- PageViewIndicator: pageView (binding)
- EditBox: backgroundImage (direct SpriteFrame property)

### 3. Type Mismatch: Texture2D vs SpriteFrame
Affects: Button (4 sprites), ProgressBar, PageViewIndicator
Pending: SpriteFrame asset wrapper landing
Fix: Deserialization UUID→Texture2D* bridge (0.5 days)

### 4. Platform Divergence: EditBox
- TS: DOM <input> backend
- C++: SDL native backend (EditBoxImpl)
Input behavior (spell-check, autocomplete) differs; underlying enum/property match.

### 5. Missing Pagination Logic
PageView has properties but no gotoPage(), getCurrentPageIndex() methods or page-turn implementation.
Fix: Implement pagination state machine (1-2 days)

## Per-Component Risk Assessment

Button
- ✅ All state properties present
- ✅ Transition enums match
- ⚠️ clickEvents handler array won't deserialize
Risk: Medium (visual state ok; click handlers lost)

Widget
- ✅ All 16 properties registered
- ✅ All enum values match
- ✅ updateAlignment() method present
Risk: None (excellent parity)

Layout
- ✅ All layout-type enums present
- ✅ All padding/spacing/direction properties registered
- ✅ updateLayout() method present
Risk: None (excellent parity)

ScrollView
- ✅ Scroll properties present (horizontal, vertical flags)
- ❌ content (Node ref) missing — critical for programmatic scroll
- ⚠️ scrollEvents handler differs in serialization
Risk: High (content null → crash on scroll; handlers lost)

ScrollBar
- ✅ Handle, direction, autoHide flags present
- ⚠️ Auto-hide timer integration incomplete
Risk: Medium (basic functionality ok; advanced features rough)

Slider
- ✅ Handle, direction, progress present
- ❌ slideEvents not property-registered
Risk: High (handlers lost on deserialization)

ProgressBar
- ✅ barSprite, mode, totalLength, progress
- ❌ reverse property missing
Risk: Medium (RTL bars won't work)

Toggle
- ✅ isChecked, checkMark present (inherits from Button)
- ⚠️ checkEvents handler differs
Risk: Medium (toggle state ok; handlers lost)

ToggleContainer
- ✅ allowSwitchOff present
- ⚠️ toggleItems no getter/property
- ⚠️ checkEvents handler differs
Risk: Medium (basic functionality ok; missing query API)

PageView
- ✅ sizeMode, direction, threshold, timing, indicator
- ❌ gotoPage(), getCurrentPageIndex() missing
- ❌ pageEvents not exposed
Risk: Critical (pagination won't work)

PageViewIndicator
- ✅ spriteFrame, direction, cellSize, spacing
- ❌ pageView binding missing
Risk: High (indicator won't auto-update)

SafeArea
- ✅ symmetric property
- ✅ updateArea() method
Risk: None (complete)

BlockInputEvents
- ✅ Event blocking on onEnable/onDisable
Risk: None (complete)

EditBox
- ✅ String, placeholder, labels, flags, modes
- ⚠️ backgroundImage property differs (uses Sprite ref)
- ⚠️ Event handlers differ in serialization
- ⚠️ Platform differences (SDL vs DOM)
Risk: Medium (basic input ok; handlers lost; platform gaps)

## Missing Components (no C++ counterpart)

View (view.ts)
- Purpose: viewport / resolution management
- Impact: UI coordinate system inconsistencies
- Effort if needed: 2-3 days

ViewGroup (view-group.ts)
- Purpose: multi-view coordination (split-screen)
- Impact: multi-view features unavailable
- Effort if needed: 3-5 days

UICoordinateTracker (ui-coordinate-tracker.ts)
- Purpose: world→screen coordinate mapping (damage numbers above enemy)
- Impact: world-space UI overlays unavailable
- Effort if needed: 1 day

## Effort Estimate

Component-level gaps: ~9 days
- Button (event serialization): 0.5 days
- ScrollView (content ref + events): 1 day
- ScrollBar (auto-hide timer): 1 day
- Slider (slideEvents prop): 1 day
- ProgressBar (reverse prop): 0.5 days
- Toggle (checkEvents prop): 0.5 days
- ToggleContainer (toggleItems + events): 0.5 days
- PageView (pagination + events): 2 days
- PageViewIndicator (pageView binding): 1 day
- EditBox (event serialization + bgImage): 1 day

System-level (event deserialization bridge): 1-2 days

Total for full parity: ~11 days

## Recommendations

Priority 1 (Blockers):
1. Event handler serialization bridge (system-level)
2. ScrollView.content property
3. PageView pagination API

Priority 2 (Quick wins):
1. Register missing event properties
2. Add missing simple properties
3. Auto-hide timer integration

Priority 3 (Nice-to-have):
1. SpriteFrame→Texture2D bridge
2. View/ViewGroup/UICoordinateTracker ports

## Validation Checklist

For each component:
- [ ] Load upstream .scene with component
- [ ] Verify no load errors
- [ ] Check all properties deserialize
- [ ] Confirm event handlers fire
- [ ] Visual layout matches upstream

