#pragma once
#include "ClassAutoReg.h"
#include "ClassBuilder.h"

// ─── Declaration macro (header, inside class body) ────────────────────────
//
//   class Sprite : public UIRenderer {
//       CC_CLASS_DECL(Sprite, UIRenderer)
//   public:
//       ...
//   private:
//       SpriteFrame *_spriteFrame{nullptr};
//       Color _color;
//   };
//
#define CC_CLASS_DECL(ClassName, BaseName)                             \
public:                                                                \
    using ThisClass = ClassName;                                       \
    using BaseClass = BaseName;                                        \
    static const ::cc::reflection::ClassMeta *getStaticClass();        \
    virtual const ::cc::reflection::ClassMeta *getClass() const {      \
        return getStaticClass();                                       \
    }                                                                  \
                                                                       \
private:

// ─── Implementation macros (paired; in .cpp) ──────────────────────────────
//
//   CC_IMPLEMENT_CLASS(Sprite, "cc.Sprite", UIRenderer)
//       .property("spriteFrame", &Sprite::_spriteFrame)
//       .property("color",       &Sprite::_color, Color{255,255,255,255})
//   CC_END_CLASS(Sprite);
//
// For classes with no reflected base (root of a type tree), use
// CC_IMPLEMENT_ROOT_CLASS instead of CC_IMPLEMENT_CLASS.

#define CC_IMPLEMENT_CLASS(ClassName, NameStr, BaseName)               \
    const ::cc::reflection::ClassMeta *ClassName::getStaticClass() {   \
        static const ::cc::reflection::ClassMeta *meta =               \
            ::cc::reflection::ClassBuilder<ClassName>(                 \
                NameStr, BaseName::getStaticClass())

#define CC_IMPLEMENT_ROOT_CLASS(ClassName, NameStr)                    \
    const ::cc::reflection::ClassMeta *ClassName::getStaticClass() {   \
        static const ::cc::reflection::ClassMeta *meta =               \
            ::cc::reflection::ClassBuilder<ClassName>(NameStr, nullptr)

#define CC_END_CLASS(ClassName)                                        \
                .build();                                              \
        return meta;                                                   \
    }                                                                  \
    namespace {                                                        \
    static ::cc::reflection::ClassAutoReg                              \
        _cc_reg_##ClassName(&ClassName::getStaticClass);               \
    }
