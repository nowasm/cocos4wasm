#include "cocos/2d/components/Graphics.h"

#include <cmath>

#include "base/Log.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"

namespace cc {

CC_IMPLEMENT_CLASS(Graphics, "cc.Graphics", UIRenderer)
    .property("lineWidth",   &Graphics::_lineWidth,   2.0f)
    .property("strokeColor", &Graphics::_strokeColor, Color{255, 255, 255, 255})
    .property("fillColor",   &Graphics::_fillColor,   Color{255, 255, 255, 255})
CC_END_CLASS(Graphics);

Graphics::Graphics() {
    _vertexStrideFloats = 7;  // position(3) + color(4)
}

Graphics::~Graphics() = default;

ccstd::vector<gfx::Attribute> Graphics::vertexAttributes() const {
    return {
        gfx::Attribute{gfx::ATTR_NAME_POSITION, gfx::Format::RGB32F},
        gfx::Attribute{gfx::ATTR_NAME_COLOR,    gfx::Format::RGBA32F},
    };
}

void Graphics::updateGeometry() {
    _vertexStrideFloats = 7;
    // Geometry is accumulated eagerly by stroke()/fill(); just rely on
    // counts maintained in those emitters. Nothing else to do.
}

IntrusivePtr<Material> Graphics::resolveMaterial() {
    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) {
        CC_LOG_ERROR("[Graphics] builtin-unlit effect missing");
        return nullptr;
    }

    MacroRecord defines{{"USE_VERTEX_COLOR", true}};
    IMaterialInfo info;
    info.effectAsset = effect;
    info.technique   = 1u;  // transparent — uniform alpha blend for both strokes and fills
    info.defines     = IMaterialInfo::DefinesType{defines};

    auto *mat = ccnew Material();
    mat->initialize(info);
    // mainColor stays white (default) so vertex-colour survives unchanged;
    // fragment shader multiplies mainColor * v_color.
    return mat;
}

// ─── path building ─────────────────────────────────────────────────────────

void Graphics::moveTo(float x, float y) {
    _subpathStarts.push_back(_path.size());
    _path.push_back({x, y});
}

void Graphics::lineTo(float x, float y) {
    if (_path.empty()) _subpathStarts.push_back(0);
    _path.push_back({x, y});
}

void Graphics::rect(float x, float y, float w, float h) {
    moveTo(x,     y);
    lineTo(x + w, y);
    lineTo(x + w, y + h);
    lineTo(x,     y + h);
    lineTo(x,     y);  // close
}

void Graphics::circle(float cx, float cy, float r, int segments) {
    if (segments < 3) segments = 3;
    const float step = 6.28318530718f / static_cast<float>(segments);
    for (int i = 0; i <= segments; ++i) {
        const float a = step * static_cast<float>(i);
        const float x = cx + std::cos(a) * r;
        const float y = cy + std::sin(a) * r;
        if (i == 0) moveTo(x, y);
        else        lineTo(x, y);
    }
}

// ─── geometry emission ─────────────────────────────────────────────────────

void Graphics::pushVertex(float x, float y, const Color &c) {
    _vertexData.push_back(x);
    _vertexData.push_back(y);
    _vertexData.push_back(0.0f);
    _vertexData.push_back(static_cast<float>(c.r) / 255.0f);
    _vertexData.push_back(static_cast<float>(c.g) / 255.0f);
    _vertexData.push_back(static_cast<float>(c.b) / 255.0f);
    _vertexData.push_back(static_cast<float>(c.a) / 255.0f);
    ++_vertexCount;
}

void Graphics::pushTriangle(uint16_t i0, uint16_t i1, uint16_t i2) {
    _indexData.push_back(i0);
    _indexData.push_back(i1);
    _indexData.push_back(i2);
    _indexCount += 3;
}

void Graphics::emitLineQuad(float x0, float y0, float x1, float y1, const Color &c) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-5f) return;
    const float halfW = _lineWidth * 0.5f;
    const float nx = -dy / len * halfW;
    const float ny =  dx / len * halfW;

    const auto base = static_cast<uint16_t>(_vertexCount);
    pushVertex(x0 + nx, y0 + ny, c);   // 0
    pushVertex(x0 - nx, y0 - ny, c);   // 1
    pushVertex(x1 - nx, y1 - ny, c);   // 2
    pushVertex(x1 + nx, y1 + ny, c);   // 3
    pushTriangle(base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2));
    pushTriangle(base, static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3));
}

void Graphics::stroke() {
    // For each subpath emit line-quads between consecutive points.
    for (size_t sp = 0; sp < _subpathStarts.size(); ++sp) {
        const size_t start = _subpathStarts[sp];
        const size_t end   = (sp + 1 < _subpathStarts.size()) ? _subpathStarts[sp + 1] : _path.size();
        for (size_t i = start; i + 1 < end; ++i) {
            emitLineQuad(_path[i].x, _path[i].y,
                         _path[i + 1].x, _path[i + 1].y,
                         _strokeColor);
        }
    }
    _path.clear();
    _subpathStarts.clear();
    markDirty();
}

void Graphics::fill() {
    // Triangle fan per subpath — works for convex polygons; concave/holes
    // would need a real triangulator, deferred.
    for (size_t sp = 0; sp < _subpathStarts.size(); ++sp) {
        const size_t start = _subpathStarts[sp];
        const size_t end   = (sp + 1 < _subpathStarts.size()) ? _subpathStarts[sp + 1] : _path.size();
        if (end < start + 3) continue;

        const auto base = static_cast<uint16_t>(_vertexCount);
        for (size_t i = start; i < end; ++i) {
            pushVertex(_path[i].x, _path[i].y, _fillColor);
        }
        const auto n = static_cast<uint16_t>(end - start);
        for (uint16_t i = 1; i + 1 < n; ++i) {
            pushTriangle(base,
                         static_cast<uint16_t>(base + i),
                         static_cast<uint16_t>(base + i + 1));
        }
    }
    _path.clear();
    _subpathStarts.clear();
    markDirty();
}

void Graphics::clear() {
    _path.clear();
    _subpathStarts.clear();
    _vertexData.clear();
    _indexData.clear();
    _vertexCount = 0;
    _indexCount  = 0;
    markDirty();
}

}  // namespace cc
