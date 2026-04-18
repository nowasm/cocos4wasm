#pragma once

#include "cocos/2d/framework/UIRenderer.h"
#include "math/Color.h"

namespace cc {

class BmfFont;

// Text component that renders one line via a BMFont atlas. Emits one
// textured quad per glyph, stitched together using the glyph metrics and
// the pen-advance model AngelCode .fnt describes.
//
// Scope (P2d MVP):
//   - Single-line, no wrap
//   - Horizontal alignment: centred on the node's origin
//   - Vertical alignment: baseline at origin Y
//   - BMFont atlas only (TTF via FreeType deferred; see memory/p2 note)
//
// The caller owns the BmfFont and keeps it alive for the Label's lifetime
// (scenes typically just hold it alongside the node). No reference
// counting on BmfFont — it's a value-typed resource, not a CCObject.
class Label : public UIRenderer {
    CC_CLASS_DECL(Label, UIRenderer)
public:
    Label();
    ~Label() override;

    void setFont(BmfFont *font);
    BmfFont *getFont() const { return _font; }

    void setText(const ccstd::string &text);
    const ccstd::string &getText() const { return _text; }

    void setColor(const Color &c);
    const Color &getColor() const { return _color; }

protected:
    void updateGeometry() override;
    IntrusivePtr<Material> resolveMaterial() override;
    ccstd::vector<gfx::Attribute> vertexAttributes() const override;

private:
    BmfFont      *_font{nullptr};
    ccstd::string _text;
    Color         _color{255, 255, 255, 255};
};

}  // namespace cc
