#pragma once

#include "base/std/container/vector.h"
#include "cocos/2d/framework/UIRenderer.h"
#include "math/Color.h"

namespace cc {

// Immediate-mode 2D drawing component, HTML5-Canvas-ish API.
//
// Call sequence:
//   graphics->moveTo(x, y);          // start / break subpath
//   graphics->lineTo(x, y);          // extend current subpath
//   graphics->rect(x, y, w, h);      // closed rect subpath
//   graphics->stroke();              // emit line quads for all subpaths
//   graphics->fill();                // emit triangle-fan per subpath
//   graphics->clear();               // wipe everything, start over
//
// Strokes become thin triangle strips (width = _lineWidth) so a single
// scene::Model / TRIANGLE_LIST handles both strokes and fills. Miter
// joints between segments are NOT handled — segments are drawn
// independently and will show gaps at sharp angles; acceptable for the
// demo pass of P2e.
//
// Vertex format: position(3) + color(4). Material is builtin-unlit
// technique 1 (transparent) with USE_VERTEX_COLOR, so strokes and fills
// can carry different colours in one draw call.
class Graphics : public UIRenderer {
    CC_CLASS_DECL(Graphics, UIRenderer)
public:
    Graphics();
    ~Graphics() override;

    // ─── state ─────────────────────────────────────────────────────────
    void setStrokeColor(const Color &c) { _strokeColor = c; }
    void setFillColor(const Color &c)   { _fillColor = c; }
    void setColor(const Color &c)       { _strokeColor = _fillColor = c; }
    void setLineWidth(float w)          { _lineWidth = w; }

    // ─── path building ─────────────────────────────────────────────────
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void rect(float x, float y, float w, float h);
    void circle(float cx, float cy, float r, int segments = 32);

    // ─── emission ──────────────────────────────────────────────────────
    void stroke();
    void fill();
    void clear();  // wipes both path state and accumulated geometry

protected:
    void updateGeometry() override;      // no-op; geometry accumulated eagerly
    IntrusivePtr<Material> resolveMaterial() override;
    ccstd::vector<gfx::Attribute> vertexAttributes() const override;

private:
    struct PathPoint { float x, y; };

    void emitLineQuad(float x0, float y0, float x1, float y1, const Color &c);
    void pushVertex(float x, float y, const Color &c);
    void pushTriangle(uint16_t i0, uint16_t i1, uint16_t i2);

    ccstd::vector<PathPoint> _path;
    ccstd::vector<size_t>    _subpathStarts;  // indices into _path

    Color _strokeColor{255, 255, 255, 255};
    Color _fillColor{255, 255, 255, 255};
    float _lineWidth{2.0f};
};

}  // namespace cc
