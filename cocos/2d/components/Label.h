#pragma once

#include <utility>

#include "base/std/container/vector.h"
#include "cocos/2d/framework/UIRenderer.h"
#include "math/Color.h"

namespace cc {

class TextFont;

// Text component that renders text via a BMFont atlas. Emits one
// textured quad per glyph, stitched together using the glyph metrics
// and the pen-advance model AngelCode .fnt describes.
//
// Features:
//   • Multi-line (explicit '\n' splits; auto-wrap is deferred to a
//     later milestone, see LabelTestScene page 1)
//   • Horizontal alignment — LEFT / CENTER / RIGHT, measured against
//     the node's UITransform content rect
//   • Vertical alignment — TOP / CENTER / BOTTOM, same rect
//   • fontSize — independent of node.scale so text stays crisp at any
//     size (no bilinear blur from scaling the sampled atlas)
//   • BMFont atlas only (TTF via FreeType deferred)
//
// The caller owns the BmfFont and keeps it alive for the Label's
// lifetime (scenes typically just hold it alongside the node). No
// reference counting on BmfFont — it's a value-typed resource, not a
// CCObject.
class Label : public UIRenderer {
    CC_CLASS_DECL(Label, UIRenderer)
public:
    // Mirrors cc.Label.HorizontalAlign / VerticalAlign from Cocos
    // Creator TS — same numeric order so scene-file deserialisation
    // produces identical behaviour.
    enum class HorizontalAlign : uint32_t { LEFT = 0, CENTER = 1, RIGHT = 2 };
    enum class VerticalAlign   : uint32_t { TOP  = 0, CENTER = 1, BOTTOM = 2 };

    Label();
    ~Label() override;

    void setFont(TextFont *font);
    TextFont *getFont() const { return _font; }

    void setText(const ccstd::string &text);
    const ccstd::string &getText() const { return _text; }

    void setColor(const Color &c);
    const Color &getColor() const { return _color; }

    // Alignment against the node's UITransform content box. Default is
    // CENTER/CENTER — matches the MVP label's visual so existing
    // scenes render unchanged until they opt in.
    void setHorizontalAlign(HorizontalAlign v);
    HorizontalAlign getHorizontalAlign() const { return _hAlign; }

    void setVerticalAlign(VerticalAlign v);
    VerticalAlign getVerticalAlign() const { return _vAlign; }

    // Font size in logical pixels. `<= 0` (the default) means "use the
    // font's native point size" (from BmfFont's `<info size=...>`).
    // Independent from node.scale — scaling the node re-samples the
    // atlas (bilinear blur); fontSize scales the pen-advance math
    // so glyphs stay pixel-aligned at the atlas's native rate.
    void setFontSize(float size);
    float getFontSize() const { return _fontSize; }

    // Line height override in logical pixels. `<= 0` means "use the
    // font's native line height" (scaled by fontSize).
    void setLineHeight(float h);
    float getLineHeight() const { return _lineHeight; }

    // Word-wrap: when on, lines that exceed the UITransform's content-box
    // width break at the nearest whitespace (or codepoint when no word
    // boundary fits) so the text flows into additional visual rows.
    // Disabled by default so existing Labels keep their old single-line
    // layout; EditBox turns it on for InputMode::ANY.
    bool isWrapEnabled() const { return _wrap; }
    void setWrapEnabled(bool v);

    // Post-layout visual-line map, rebuilt each updateGeometry(). Each
    // pair is (startByte, byteLen) into `_text`. EditBox reads this so
    // caret / selection / hit-test track the Label's actual wrap output.
    using LineRange = std::pair<size_t, size_t>;
    const ccstd::vector<LineRange> &getVisualLines() const { return _visualLines; }

    // Runs the wrap algorithm against the current font / text / size and
    // updates `_visualLines` in place. EditBox calls this right after
    // setText so the caret can land on the fresh line without waiting
    // for the next render pass to refresh the map.
    void recomputeVisualLines();

    void onEnable() override;
    void onDisable() override;

protected:
    void updateGeometry() override;
    IntrusivePtr<Material> resolveMaterial() override;
    ccstd::vector<gfx::Attribute> vertexAttributes() const override;
    gfx::Texture *resolveBatchTexture() const override;

    // Subscription handle for Node::SizeChanged — rebuilds geometry
    // when the parent UITransform resizes (alignment math depends on
    // contentSize). Heap-allocated so the header stays free of the
    // EventTarget template machinery.
    struct SizeHook;
    SizeHook *_sizeHook{nullptr};

private:
    TextFont       *_font{nullptr};
    ccstd::string   _text;
    Color           _color{255, 255, 255, 255};
    HorizontalAlign _hAlign{HorizontalAlign::CENTER};
    VerticalAlign   _vAlign{VerticalAlign::CENTER};
    float           _fontSize{-1.f};    // <=0 → use font's native
    float           _lineHeight{-1.f};  // <=0 → use font's native
    bool            _wrap{false};

    // Refreshed every updateGeometry(); empty until then.
    ccstd::vector<LineRange> _visualLines;
};

}  // namespace cc
