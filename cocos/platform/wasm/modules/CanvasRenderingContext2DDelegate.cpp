/****************************************************************************
 Copyright (c) 2022-2023 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include "platform/wasm/modules/CanvasRenderingContext2DDelegate.h"
#include "base/Log.h"

#include <emscripten.h>
#include <cstring>

/**
 * This implementation uses an offscreen HTML5 Canvas element via EM_ASM to perform
 * 2D text rendering and measurement. The rendered pixels are read back into the
 * _imageData buffer (RGBA format) for use as a texture by the engine.
 *
 * A persistent offscreen canvas is created in Module._canvas2d and reused across calls.
 */

namespace {

// Helper: ensure the offscreen canvas and context exist in JS
static void ensureCanvas(int w, int h) {
    EM_ASM({
        if (!Module._canvas2d || Module._canvas2dW !== $0 || Module._canvas2dH !== $1) {
            Module._canvas2d = document.createElement('canvas');
            Module._canvas2d.width = $0;
            Module._canvas2d.height = $1;
            Module._canvas2dCtx = Module._canvas2d.getContext('2d');
            Module._canvas2dW = $0;
            Module._canvas2dH = $1;
        }
    }, w, h);
}

} // namespace

namespace cc {

CanvasRenderingContext2DDelegate::CanvasRenderingContext2DDelegate() {
}

CanvasRenderingContext2DDelegate::~CanvasRenderingContext2DDelegate() {
}

void CanvasRenderingContext2DDelegate::recreateBuffer(float w, float h) {
    int iw = static_cast<int>(w);
    int ih = static_cast<int>(h);
    if (iw < 1) iw = 1;
    if (ih < 1) ih = 1;

    ensureCanvas(iw, ih);

    // Clear the canvas
    EM_ASM({
        var ctx = Module._canvas2dCtx;
        ctx.clearRect(0, 0, $0, $1);
    }, iw, ih);

    // Allocate image data buffer (RGBA)
    _imageData.resize(static_cast<uint32_t>(iw * ih * 4));
    memset(_imageData.getBytes(), 0, _imageData.getSize());
}

void CanvasRenderingContext2DDelegate::beginPath() {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.beginPath();
    });
}

void CanvasRenderingContext2DDelegate::closePath() {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.closePath();
    });
}

void CanvasRenderingContext2DDelegate::moveTo(float x, float y) {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.moveTo($0, $1);
    }, static_cast<double>(x), static_cast<double>(y));
}

void CanvasRenderingContext2DDelegate::lineTo(float x, float y) {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.lineTo($0, $1);
    }, static_cast<double>(x), static_cast<double>(y));
}

void CanvasRenderingContext2DDelegate::stroke() {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.stroke();
    });
}

void CanvasRenderingContext2DDelegate::saveContext() {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.save();
    });
}

void CanvasRenderingContext2DDelegate::restoreContext() {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.restore();
    });
}

void CanvasRenderingContext2DDelegate::clearRect(float x, float y, float w, float h) {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.clearRect($0, $1, $2, $3);
    }, static_cast<double>(x), static_cast<double>(y), static_cast<double>(w), static_cast<double>(h));
}

void CanvasRenderingContext2DDelegate::fillRect(float x, float y, float w, float h) {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.fillRect($0, $1, $2, $3);
    }, static_cast<double>(x), static_cast<double>(y), static_cast<double>(w), static_cast<double>(h));
}

void CanvasRenderingContext2DDelegate::fillText(const ccstd::string &text, float x, float y, float /*maxWidth*/) {
    EM_ASM({
        if (Module._canvas2dCtx) {
            Module._canvas2dCtx.fillText(UTF8ToString($0), $1, $2);
        }
    }, text.c_str(), static_cast<double>(x), static_cast<double>(y));
}

void CanvasRenderingContext2DDelegate::strokeText(const ccstd::string &text, float x, float y, float /*maxWidth*/) const {
    EM_ASM({
        if (Module._canvas2dCtx) {
            Module._canvas2dCtx.strokeText(UTF8ToString($0), $1, $2);
        }
    }, text.c_str(), static_cast<double>(x), static_cast<double>(y));
}

CanvasRenderingContext2DDelegate::Size CanvasRenderingContext2DDelegate::measureText(const ccstd::string &text) {
    double width = EM_ASM_DOUBLE({
        if (Module._canvas2dCtx) {
            var metrics = Module._canvas2dCtx.measureText(UTF8ToString($0));
            return metrics.width;
        }
        return 0.0;
    }, text.c_str());

    // Height estimation: use font size (stored by updateFont in JS)
    double height = EM_ASM_DOUBLE({
        if (Module._canvas2dFontSize) return Module._canvas2dFontSize;
        return 12.0;
    });

    return Size{static_cast<float>(width), static_cast<float>(height)};
}

void CanvasRenderingContext2DDelegate::updateFont(const ccstd::string &fontName,
                                                  float fontSize,
                                                  bool bold,
                                                  bool italic,
                                                  bool oblique,
                                                  bool /* smallCaps */) {
    EM_ASM({
        if (!Module._canvas2dCtx) return;
        var style = '';
        if ($3) style += 'italic ';
        else if ($4) style += 'oblique ';
        if ($2) style += 'bold ';
        style += $1 + 'px ';
        style += UTF8ToString($0) || 'sans-serif';
        Module._canvas2dCtx.font = style;
        Module._canvas2dFontSize = $1;
    }, fontName.c_str(), static_cast<double>(fontSize), bold, italic, oblique);
}

void CanvasRenderingContext2DDelegate::setTextAlign(TextAlign align) {
    const char *alignStr = "left";
    switch (align) {
        case TextAlign::LEFT: alignStr = "left"; break;
        case TextAlign::CENTER: alignStr = "center"; break;
        case TextAlign::RIGHT: alignStr = "right"; break;
    }
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.textAlign = UTF8ToString($0);
    }, alignStr);
}

void CanvasRenderingContext2DDelegate::setTextBaseline(TextBaseline baseline) {
    const char *baselineStr = "alphabetic";
    switch (baseline) {
        case TextBaseline::TOP: baselineStr = "top"; break;
        case TextBaseline::MIDDLE: baselineStr = "middle"; break;
        case TextBaseline::BOTTOM: baselineStr = "bottom"; break;
        case TextBaseline::ALPHABETIC: baselineStr = "alphabetic"; break;
    }
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.textBaseline = UTF8ToString($0);
    }, baselineStr);
}

void CanvasRenderingContext2DDelegate::setFillStyle(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    EM_ASM({
        if (Module._canvas2dCtx) {
            Module._canvas2dCtx.fillStyle = 'rgba(' + $0 + ',' + $1 + ',' + $2 + ',' + ($3 / 255.0) + ')';
        }
    }, r, g, b, a);
}

void CanvasRenderingContext2DDelegate::setStrokeStyle(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    EM_ASM({
        if (Module._canvas2dCtx) {
            Module._canvas2dCtx.strokeStyle = 'rgba(' + $0 + ',' + $1 + ',' + $2 + ',' + ($3 / 255.0) + ')';
        }
    }, r, g, b, a);
}

void CanvasRenderingContext2DDelegate::setLineWidth(float lineWidth) {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.lineWidth = $0;
    }, static_cast<double>(lineWidth));
}

const cc::Data &CanvasRenderingContext2DDelegate::getDataRef() const {
    // Read back the pixel data from the offscreen canvas into _imageData
    if (_imageData.getSize() > 0) {
        EM_ASM({
            if (!Module._canvas2dCtx) return;
            var w = Module._canvas2dW || 1;
            var h = Module._canvas2dH || 1;
            var imgData = Module._canvas2dCtx.getImageData(0, 0, w, h);
            var src = imgData.data;
            var dst = $0;
            var len = Math.min(src.length, $1);
            HEAPU8.set(src.subarray(0, len), dst);
        }, _imageData.getBytes(), _imageData.getSize());
    }
    return _imageData;
}

void CanvasRenderingContext2DDelegate::fill() {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.fill();
    });
}

void CanvasRenderingContext2DDelegate::setLineCap(const ccstd::string &lineCap) {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.lineCap = UTF8ToString($0);
    }, lineCap.c_str());
}

void CanvasRenderingContext2DDelegate::setLineJoin(const ccstd::string &lineJoin) {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.lineJoin = UTF8ToString($0);
    }, lineJoin.c_str());
}

void CanvasRenderingContext2DDelegate::fillImageData(const Data &imageData,
                                                     float imageWidth,
                                                     float imageHeight,
                                                     float offsetX,
                                                     float offsetY) {
    if (imageData.getSize() == 0) return;

    EM_ASM({
        if (!Module._canvas2dCtx) return;
        var w = $2;
        var h = $3;
        var imgData = Module._canvas2dCtx.createImageData(w, h);
        var src = $0;
        var len = Math.min($1, imgData.data.length);
        imgData.data.set(HEAPU8.subarray(src, src + len));
        Module._canvas2dCtx.putImageData(imgData, $4, $5);
    }, imageData.getBytes(), imageData.getSize(),
       static_cast<int>(imageWidth), static_cast<int>(imageHeight),
       static_cast<double>(offsetX), static_cast<double>(offsetY));
}

void CanvasRenderingContext2DDelegate::strokeText(const ccstd::string &text,
                                                  float x,
                                                  float y,
                                                  float /* maxWidth */) {
    EM_ASM({
        if (Module._canvas2dCtx) {
            Module._canvas2dCtx.strokeText(UTF8ToString($0), $1, $2);
        }
    }, text.c_str(), static_cast<double>(x), static_cast<double>(y));
}

void CanvasRenderingContext2DDelegate::rect(float x,
                                            float y,
                                            float w,
                                            float h) {
    EM_ASM({
        if (Module._canvas2dCtx) Module._canvas2dCtx.rect($0, $1, $2, $3);
    }, static_cast<double>(x), static_cast<double>(y), static_cast<double>(w), static_cast<double>(h));
}

} // namespace cc
