#pragma once

#include "base/std/container/vector.h"
#include "cocos/2d/framework/UIRenderer.h"
#include "math/Vec2.h"

namespace cc {

// Stencil-based clipping mask. As an ancestor of other UIRenderers it
// restricts their visible region to the mask's shape. Works via the
// Cocos-style bit-per-level stencil model — see UIRenderer.cpp
// buildStencilDssInfo for the precise bit layout.
//
// Scope (P2f-a MVP): RECT shape only.
// P2f-b will add ELLIPSE and GRAPHICS (custom path).
// P2f-c turns the nested bit-mask flow on end-to-end.
//
// Runtime enable/disable: Mask writes its shape to stencil on every frame
// as long as the scene::Model is attached. If you re-parent UIRenderers
// under a newly-activated Mask you'll need to re-create their models so
// they pick up the ancestor count — easiest: toggle their node active.
class Mask : public UIRenderer {
    CC_CLASS_DECL(Mask, UIRenderer)
public:
    enum class Type : uint8_t {
        RECT = 0,
        ELLIPSE,
        GRAPHICS,     // user-supplied polygon path (CONVEX only)
    };

    Mask();
    ~Mask() override;

    // Shape selection. For RECT and ELLIPSE the size property drives the
    // geometry; for GRAPHICS the user builds the path via moveTo/lineTo.
    void setType(Type t);
    Type getType() const { return _type; }

    void setSize(float w, float h);
    const Vec2 &getSize() const { return _size; }

    // Segment count for ELLIPSE tessellation — more = smoother.
    void setEllipseSegments(int n);
    int  getEllipseSegments() const { return _ellipseSegments; }

    // GRAPHICS path builders. Only single convex subpath is supported —
    // triangle-fan tessellation assumes convex. moveTo resets the path.
    void graphicsClear();
    void graphicsMoveTo(float x, float y);
    void graphicsLineTo(float x, float y);

    // Inverted mode — when true the visible region becomes the COMPLEMENT
    // of the shape. Implemented via StencilStage ENTER_LEVEL_INVERTED
    // (func=NEVER, failOp=ZERO instead of REPLACE).
    void setInverted(bool v);
    bool isInverted() const { return _inverted; }

    bool isMask() const override { return true; }

protected:
    void updateGeometry() override;
    IntrusivePtr<Material> resolveMaterial() override;
    ccstd::vector<gfx::Attribute> vertexAttributes() const override;

private:
    void buildRectPath();
    void buildEllipsePath();
    void emitPolygonFan();

    Type _type{Type::RECT};
    Vec2 _size{200.0f, 200.0f};
    int  _ellipseSegments{32};
    bool _inverted{false};

    ccstd::vector<Vec2> _graphicsPath;   // user path when Type::GRAPHICS
    ccstd::vector<Vec2> _path;           // actual emit source (rebuilt from type)
};

}  // namespace cc
