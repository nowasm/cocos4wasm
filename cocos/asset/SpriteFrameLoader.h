#pragma once

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "cocos/asset/SpriteFrame.h"

namespace cc {

// Loads a Cocos Creator 3.x `.json`-serialized SpriteFrame (Editor export
// pipeline emits `.sf.json` or plain `.json` with `__type__:"cc.SpriteFrame"`).
//
// Expected shape — flat object OR a top-level array whose first entry is
// a cc.SpriteFrame wrapper pointing at the payload:
//
//   {
//     "__type__": "cc.SpriteFrame",
//     "content": {                // OR fields inlined on the root
//       "name":         "frame_01",
//       "rect":         { "x": 0, "y": 0, "width": 128, "height": 128 },
//       "offset":       { "x": 0, "y": 0 },
//       "originalSize": { "width": 128, "height": 128 },
//       "rotated":      false,
//       "capInsets":    [0, 0, 0, 0],  // [left, top, right, bottom]
//       "texture":      "uuid...",     // Texture2D __uuid__ ref
//       "packable":     true,
//       "pivot":        { "x": 0.5, "y": 0.5 },
//       "pixelsToUnit": 100
//     }
//   }
//
// Texture resolution runs through AssetManager — the referenced uuid must
// be in the uuid-map. Missing texture is not fatal (a frame with only a
// rect is still valid — some minigame workflows build SpriteFrames from
// a shared atlas texture loaded separately).
class SpriteFrameLoader {
public:
    static SpriteFrame *loadFromFile(const ccstd::string &absPath);
};

}  // namespace cc
