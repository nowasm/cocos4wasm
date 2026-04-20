#pragma once

#include <utility>

#include "base/std/container/string.h"
#include "base/std/container/vector.h"
#include "cocos/ui/editBox/EditBoxImplBase.h"
#include "cocos/ui/editBox/Types.h"
#include "math/Color.h"

namespace cc {

class EditBox;
class Sprite;

// Desktop / native backend for EditBox. Analogous to `EditBoxImpl` in
// `cocos/ui/editbox/edit-box-impl.ts`, but driven by SDL input events
// instead of DOM <input>/<textarea>.
//
// Responsibility split with the delegate EditBox:
//   EditBox        — owns serialisable state (string, placeholder, flags,
//                    labels, background) and the public component API.
//   EditBoxImpl    — owns focus, caret position, selection, IME
//                    composition buffer, and the caret/selection sprites.
//                    Binds the SDL listeners; receives text and keyboard
//                    events; writes back to the delegate via the
//                    documented `_editBox*` hooks.
//
// Why the visuals live here: the caret and selection rectangle are only
// meaningful during an active editing session. Putting them in the impl
// keeps EditBox rendering fully label-driven and makes a future
// read-only "null" impl possible.
class EditBoxImpl : public EditBoxImplBase {
public:
    EditBoxImpl() = default;
    ~EditBoxImpl() override;

    void init(EditBox *delegate) override;
    void onEnable() override;
    void beforeDraw() override;
    void onDisable() override;
    void clear() override;

    void setTabIndex(int index) override;
    void setSize(float width, float height) override;

    void beginEditing() override;
    void endEditing() override;

    // Per-frame tick from the delegate's update() — drives caret blink
    // and any deferred geometry sync.
    void tick(float dt);

    // Called by the delegate when its Node receives a MouseDown/TouchEnd
    // that hit-tested successfully. `localX` and `localY` are the
    // anchor-relative UI coordinates the dispatcher already computed.
    // Focuses the box if needed and positions the caret.
    // Pointer-down entry point. winX/winY are the raw window-pixel
    // coordinates; we do the world→box-local transform inside to reuse the
    // same hit-testing math as drag (windowPointToCaretIndex). Using the
    // bubbled NodeMouseEventArg's localX/Y directly doesn't work because
    // those land in whatever child node was hit (PLACEHOLDER_LABEL or
    // TEXT_LABEL), not the EditBox's own frame.
    void onDelegateNodeClick(float winX, float winY);

    // Caret / selection helpers — exposed so the delegate can forward
    // programmatic calls without friending this class.
    size_t getCaretIndex() const { return _caretIdx; }
    void   setCaretIndex(size_t i);
    bool   hasSelection() const { return _selAnchor != _caretIdx; }
    size_t getSelectionStart() const;
    size_t getSelectionEnd() const;
    void   clearSelection();

private:
    // ── Child-sprite management ──────────────────────────────────────────
    // Caret + selection live as children of the delegate's node. Created
    // lazily in init(); never destroyed (we just hide by alpha on blur
    // to sidestep the Label re-activation rebuild hazard documented in
    // the previous monolithic impl).
    void ensureVisualChildren();
    void setCaretVisible(bool on);
    void setSelectionVisible(bool on);

    // ── Listener lifecycle ───────────────────────────────────────────────
    // Bound at init, unbound at clear(). The global click-outside hook
    // is always on; it checks _editing internally to avoid cost when
    // the box isn't focused.
    void bindListeners();
    void unbindListeners();

    // ── Caret / selection geometry ───────────────────────────────────────
    void updateCaretPos();
    void updateSelectionHighlight();
    void resetCaretBlink();
    void deleteSelection();

    // ── Hit-test / pointer → caret index ─────────────────────────────────
    // `pointInsideBox` checks whether window-space (winX, winY) lands in
    // the delegate's UITransform. `windowPointToCaretIndex` converts
    // the same point into a codepoint-aligned byte index into the
    // delegate's string.
    bool   pointInsideBox(float winX, float winY) const;
    size_t windowPointToCaretIndex(float winX, float winY) const;

    // ── Multi-line helpers ───────────────────────────────────────────────
    // Thin utilities used by both caret movement (up/down) and click
    // positioning when inputMode is ANY.
    //
    // Line ranges are `pair<start, length>` rather than a named struct
    // so the types stay in scope inside lambdas (MSVC's class-scope
    // lookup through `this` capture doesn't surface nested types).
    using LineRange = std::pair<size_t, size_t>;  // (start, length)
    void rebuildLineIndex(ccstd::vector<LineRange> &out) const;
    size_t lineIndexOf(const ccstd::vector<LineRange> &lines, size_t byteIdx) const;
    float  columnAdvance(size_t lineStart, size_t byteIdx) const;
    size_t byteIdxAtColumn(size_t lineStart, size_t lineLen, float targetX) const;

    // ── Text insertion ───────────────────────────────────────────────────
    // Shared path for TextInput and Ctrl+V. Applies the input-mode
    // character filter, the max-length budget, and writes through to
    // the delegate. Emits TEXT_CHANGED on success.
    void insertText(const ccstd::string &chunk);

    // Only one impl may hold focus at a time. Mirrors
    // `_currentEditBoxImpl` in TS.
    static EditBoxImpl *s_current;

    // Opaque listener bundle (SDL bus subscriptions). Heap-allocated so
    // the header doesn't pull in EngineEvents.h.
    struct Listeners;
    Listeners *_listeners{nullptr};

    // Caret + selection visuals.
    Sprite *_caret{nullptr};
    Sprite *_selHighlight{nullptr};
    Color   _caretColor{230, 230, 230, 255};
    Color   _selectionColor{80, 140, 210, 180};

    // Caret / selection state.
    size_t _caretIdx{0};
    size_t _selAnchor{0};
    bool   _mouseDragging{false};
    bool   _ctrlDown{false};

    // Caret blink — driven by wall-clock so timing stays correct even if
    // the game-loop dt does not map to real time.
    bool     _caretVisible{true};
    uint64_t _caretLastBlinkMs{0};
    static constexpr uint64_t kCaretBlinkPeriodMs = 500;

    // Up/Down preferred column for multi-line navigation.
    float _preferredCaretX{0.f};
    bool  _preferredCaretXValid{false};

    // IME composition (live preview string from SDL TextEditing).
    ccstd::string _composition;

    // Pending size from `setSize` — stored so `beforeDraw` can reapply
    // after any parent layout change. Current SDL impl doesn't need
    // the per-frame sync (Label is scene-graph native) so this is
    // informational; carried through for parity with TS.
    float _width{0.f};
    float _height{0.f};
};

}  // namespace cc
