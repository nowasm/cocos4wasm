#include "cocos/2d/components/Mask.h"

#include <cmath>

#include "base/Log.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"

namespace cc {

CC_IMPLEMENT_CLASS(Mask, "cc.Mask", UIRenderer)
    .property("size",            &Mask::_size,             Vec2{200.0f, 200.0f})
    .property("ellipseSegments", &Mask::_ellipseSegments)
    .property("inverted",        &Mask::_inverted)
CC_END_CLASS(Mask);

Mask::Mask() {
    _vertexStrideFloats = 3;  // position-only; colour never reaches the framebuffer anyway
}

Mask::~Mask() = default;

void Mask::setType(Type t) {
    _type = t;
    markDirty();
}

void Mask::setSize(float w, float h) {
    _size.set(w, h);
    markDirty();
}

void Mask::setEllipseSegments(int n) {
    _ellipseSegments = n < 3 ? 3 : n;
    markDirty();
}

void Mask::setInverted(bool v) {
    _inverted = v;
    markDirty();
}

void Mask::graphicsClear() {
    _graphicsPath.clear();
    markDirty();
}

void Mask::graphicsMoveTo(float x, float y) {
    _graphicsPath.clear();
    _graphicsPath.push_back({x, y});
    markDirty();
}

void Mask::graphicsLineTo(float x, float y) {
    _graphicsPath.push_back({x, y});
    markDirty();
}

ccstd::vector<gfx::Attribute> Mask::vertexAttributes() const {
    return {
        gfx::Attribute{gfx::ATTR_NAME_POSITION, gfx::Format::RGB32F},
    };
}

void Mask::buildRectPath() {
    const float hx = _size.x * 0.5f;
    const float hy = _size.y * 0.5f;
    _path = {
        {-hx, -hy},
        { hx, -hy},
        { hx,  hy},
        {-hx,  hy},
    };
}

void Mask::buildEllipsePath() {
    _path.clear();
    _path.reserve(static_cast<size_t>(_ellipseSegments));
    const float hx = _size.x * 0.5f;
    const float hy = _size.y * 0.5f;
    const float twoPi = 6.28318530718f;
    for (int i = 0; i < _ellipseSegments; ++i) {
        const float a = (twoPi * static_cast<float>(i)) / static_cast<float>(_ellipseSegments);
        _path.push_back({std::cos(a) * hx, std::sin(a) * hy});
    }
}

void Mask::emitPolygonFan() {
    _vertexData.clear();
    _indexData.clear();
    _vertexCount = 0;
    _indexCount  = 0;

    if (_path.size() < 3) return;

    // Fan from vertex 0 — correct for convex polygons. Concave / self-
    // intersecting paths would need a real tessellator, not supported yet.
    for (const auto &p : _path) {
        _vertexData.push_back(p.x);
        _vertexData.push_back(p.y);
        _vertexData.push_back(0.0f);
    }
    const auto n = static_cast<uint16_t>(_path.size());
    _vertexCount = n;

    for (uint16_t i = 1; i + 1 < n; ++i) {
        _indexData.push_back(0);
        _indexData.push_back(i);
        _indexData.push_back(static_cast<uint16_t>(i + 1));
    }
    _indexCount = static_cast<uint32_t>(_indexData.size());
}

void Mask::updateGeometry() {
    _vertexStrideFloats = 3;
    switch (_type) {
        case Type::RECT:     buildRectPath();    break;
        case Type::ELLIPSE:  buildEllipsePath(); break;
        case Type::GRAPHICS: _path = _graphicsPath; break;
    }
    emitPolygonFan();
}

namespace {
// Shared across every Mask instance. Mask shape fragments never reach the
// colour buffer (ENTER_LEVEL stencil forces them to fail), so one Material
// with no per-instance state is plenty. The batcher's stencilCache then
// splits this base by (depth, isMask) — identical masks at the same depth
// collapse into one batch.
IntrusivePtr<Material> g_maskBaseMaterial;
}  // namespace

IntrusivePtr<Material> Mask::resolveMaterial() {
    if (g_maskBaseMaterial) return g_maskBaseMaterial;

    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) {
        CC_LOG_ERROR("[Mask] builtin-unlit effect missing");
        return nullptr;
    }

    MacroRecord defines{
        {"CC_USE_FOG", 4},  // disable scene fog on UI (see Label.cpp)
    };
    IMaterialInfo info;
    info.effectAsset = effect;
    info.technique   = 1u;  // transparent (blend state harmless since fragments are discarded)
    info.defines     = IMaterialInfo::DefinesType{defines};

    auto *mat = ccnew Material();
    mat->initialize(info);
    g_maskBaseMaterial = mat;
    return g_maskBaseMaterial;
}

}  // namespace cc
