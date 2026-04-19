#include "cocos/2d/text/TextFont.h"

namespace cc {

// Out-of-line "key function" — anchors the TextFont vtable so it's
// emitted in this TU only, rather than one copy per including
// translation unit. Without the anchor the linker sees duplicate
// `TextFont::~TextFont` definitions.
TextFont::~TextFont() = default;

}  // namespace cc
