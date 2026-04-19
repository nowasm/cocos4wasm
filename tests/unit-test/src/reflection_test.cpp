#include "cocos/core/reflection/Reflection.h"
#include "gtest/gtest.h"

// ─── Test fixtures ────────────────────────────────────────────────────────
// Test class names are prefixed "refl_test." to avoid clashing with real
// engine classes registered in the same process.

enum class Mood : int32_t {
    HAPPY = 0,
    SAD = 1,
    ANGRY = 2,
};

// Root of our test hierarchy — no reflected base.
class Widget {
    CC_CLASS_DECL(Widget, void)
public:
    Widget() = default;
    virtual ~Widget() = default;

    bool          _enabled{true};
    int32_t       _slot{0};
    float         _weight{1.0f};
    ccstd::string _label;
    cc::Vec3      _position;
    cc::Color     _tint;
    Mood          _mood{Mood::HAPPY};
    Widget       *_partner{nullptr};

    // Reflected methods — used by the method-invocation tests.
    void bump(int32_t n)                       { _slot += n; }
    void appendLabel(const ccstd::string &s)   { _label += s; }
    void touch()                               { _enabled = true; }
    void moveTo(Widget *other)                 { _partner = other; }
};

// The CC_CLASS_DECL macro injects a `virtual getClass()` that calls
// BaseClass::getStaticClass()->... — for the root case we fake a "BaseClass"
// typedef by declaring Widget as root and discarding the base.
// Workaround: since CC_CLASS_DECL takes (Class, Base) we pass `void` which
// never goes through getStaticClass() because we use CC_IMPLEMENT_ROOT_CLASS.

CC_IMPLEMENT_ROOT_CLASS(Widget, "refl_test.Widget")
    .property("enabled",  &Widget::_enabled)
    .property("slot",     &Widget::_slot, 7)
    .property("weight",   &Widget::_weight, 2.5f)
    .property("label",    &Widget::_label, ccstd::string("hello"))
    .property("position", &Widget::_position)
    .property("tint",     &Widget::_tint)
    .property("mood",     &Widget::_mood)
    .property("partner",  &Widget::_partner)
    .method("bump",        &Widget::bump)
    .method("appendLabel", &Widget::appendLabel)
    .method("touch",       &Widget::touch)
    .method("moveTo",      &Widget::moveTo)
CC_END_CLASS(Widget);

// Derived from Widget with one extra property and its own method.
class Button : public Widget {
    CC_CLASS_DECL(Button, Widget)
public:
    uint32_t _clickCount{0};

    void click(Widget *sender, const ccstd::string &data) {
        ++_clickCount;
        _label = data;
        _partner = sender;
    }
};

CC_IMPLEMENT_CLASS(Button, "refl_test.Button", Widget)
    .property("clickCount", &Button::_clickCount)
    .method("click", &Button::click)
CC_END_CLASS(Button);

// Second root-level class for ClassDB uniqueness tests.
class Panel {
    CC_CLASS_DECL(Panel, void)
public:
    Panel() = default;
    virtual ~Panel() = default;
    int32_t _id{0};
};

CC_IMPLEMENT_ROOT_CLASS(Panel, "refl_test.Panel")
    .property("id", &Panel::_id)
CC_END_CLASS(Panel);

using namespace cc::reflection;

// ─── Tests ────────────────────────────────────────────────────────────────

TEST(Reflection, ClassRegistration) {
    auto &db = ClassDB::get();
    EXPECT_NE(db.findByName("refl_test.Widget"), nullptr);
    EXPECT_NE(db.findByName("refl_test.Button"), nullptr);
    EXPECT_NE(db.findByName("refl_test.Panel"), nullptr);
    EXPECT_EQ(db.findByName("refl_test.DoesNotExist"), nullptr);
    EXPECT_EQ(db.findByName(nullptr), nullptr);
}

TEST(Reflection, ClassMetaBasics) {
    auto *meta = Widget::getStaticClass();
    ASSERT_NE(meta, nullptr);
    EXPECT_STREQ(meta->name, "refl_test.Widget");
    EXPECT_EQ(meta->base, nullptr);
    EXPECT_EQ(meta->size, sizeof(Widget));
    ASSERT_NE(meta->factory, nullptr);

    auto *derived = Button::getStaticClass();
    ASSERT_NE(derived, nullptr);
    EXPECT_STREQ(derived->name, "refl_test.Button");
    EXPECT_EQ(derived->base, meta);
    EXPECT_EQ(derived->size, sizeof(Button));
}

TEST(Reflection, CreateInstance) {
    auto &db = ClassDB::get();
    auto *w = db.createInstance<Widget>("refl_test.Widget");
    ASSERT_NE(w, nullptr);
    EXPECT_TRUE(w->_enabled);          // default from class definition
    EXPECT_EQ(w->_slot, 0);            // class default-init
    EXPECT_FLOAT_EQ(w->_weight, 1.0f);
    delete w;

    auto *b = db.createInstance<Button>("refl_test.Button");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->_clickCount, 0u);
    EXPECT_TRUE(b->_enabled);          // inherited ctor default
    delete b;

    EXPECT_EQ(db.createInstance("refl_test.Nope"), nullptr);
}

TEST(Reflection, PropertyEnumerationOwn) {
    auto *meta = Widget::getStaticClass();
    ASSERT_NE(meta, nullptr);
    ASSERT_EQ(meta->properties.size(), 8u);

    EXPECT_STREQ(meta->properties[0].name, "enabled");
    EXPECT_EQ(meta->properties[0].typeId, TypeId::BOOL);

    EXPECT_STREQ(meta->properties[1].name, "slot");
    EXPECT_EQ(meta->properties[1].typeId, TypeId::INT32);

    EXPECT_STREQ(meta->properties[2].name, "weight");
    EXPECT_EQ(meta->properties[2].typeId, TypeId::FLOAT);

    EXPECT_STREQ(meta->properties[3].name, "label");
    EXPECT_EQ(meta->properties[3].typeId, TypeId::STRING);

    EXPECT_STREQ(meta->properties[4].name, "position");
    EXPECT_EQ(meta->properties[4].typeId, TypeId::VEC3);

    EXPECT_STREQ(meta->properties[5].name, "tint");
    EXPECT_EQ(meta->properties[5].typeId, TypeId::COLOR);

    EXPECT_STREQ(meta->properties[6].name, "mood");
    EXPECT_EQ(meta->properties[6].typeId, TypeId::ENUM);

    EXPECT_STREQ(meta->properties[7].name, "partner");
    EXPECT_EQ(meta->properties[7].typeId, TypeId::POINTER);
}

TEST(Reflection, PropertyLookupWalksBase) {
    auto *meta = Button::getStaticClass();
    ASSERT_NE(meta, nullptr);

    // own property
    auto *p = meta->findProperty("clickCount");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->typeId, TypeId::UINT32);

    // inherited property
    auto *inherited = meta->findProperty("label");
    ASSERT_NE(inherited, nullptr);
    EXPECT_EQ(inherited->typeId, TypeId::STRING);

    EXPECT_EQ(meta->findProperty("ghost"), nullptr);
    EXPECT_EQ(meta->findProperty(nullptr), nullptr);
}

TEST(Reflection, SetGetBool) {
    Widget w;
    auto *p = Widget::getStaticClass()->findProperty("enabled");
    ASSERT_NE(p, nullptr);

    bool v = false;
    p->setter(&w, &v);
    EXPECT_FALSE(w._enabled);

    bool out = true;
    p->getter(&w, &out);
    EXPECT_FALSE(out);
}

TEST(Reflection, SetGetInt) {
    Widget w;
    auto *p = Widget::getStaticClass()->findProperty("slot");
    ASSERT_NE(p, nullptr);

    int32_t v = 42;
    p->setter(&w, &v);
    EXPECT_EQ(w._slot, 42);

    int32_t out = 0;
    p->getter(&w, &out);
    EXPECT_EQ(out, 42);
}

TEST(Reflection, SetGetFloat) {
    Widget w;
    auto *p = Widget::getStaticClass()->findProperty("weight");
    ASSERT_NE(p, nullptr);

    float v = 3.75f;
    p->setter(&w, &v);
    EXPECT_FLOAT_EQ(w._weight, 3.75f);
}

TEST(Reflection, SetGetString) {
    Widget w;
    auto *p = Widget::getStaticClass()->findProperty("label");
    ASSERT_NE(p, nullptr);

    ccstd::string v = "reflect!";
    p->setter(&w, &v);
    EXPECT_EQ(w._label, "reflect!");

    ccstd::string out;
    p->getter(&w, &out);
    EXPECT_EQ(out, "reflect!");
}

TEST(Reflection, SetGetVec3) {
    Widget w;
    auto *p = Widget::getStaticClass()->findProperty("position");
    ASSERT_NE(p, nullptr);

    cc::Vec3 v{1, 2, 3};
    p->setter(&w, &v);
    EXPECT_FLOAT_EQ(w._position.x, 1.0f);
    EXPECT_FLOAT_EQ(w._position.y, 2.0f);
    EXPECT_FLOAT_EQ(w._position.z, 3.0f);
}

TEST(Reflection, SetGetColor) {
    Widget w;
    auto *p = Widget::getStaticClass()->findProperty("tint");
    ASSERT_NE(p, nullptr);

    cc::Color c{200, 100, 50, 255};
    p->setter(&w, &c);
    EXPECT_EQ(w._tint.r, 200);
    EXPECT_EQ(w._tint.g, 100);
    EXPECT_EQ(w._tint.b, 50);
    EXPECT_EQ(w._tint.a, 255);
}

TEST(Reflection, SetGetEnum) {
    Widget w;
    auto *p = Widget::getStaticClass()->findProperty("mood");
    ASSERT_NE(p, nullptr);

    Mood v = Mood::ANGRY;
    p->setter(&w, &v);
    EXPECT_EQ(w._mood, Mood::ANGRY);
}

TEST(Reflection, SetGetPointer) {
    Widget w;
    Widget partner;
    auto *p = Widget::getStaticClass()->findProperty("partner");
    ASSERT_NE(p, nullptr);

    Widget *ptr = &partner;
    p->setter(&w, &ptr);
    EXPECT_EQ(w._partner, &partner);

    Widget *out = nullptr;
    p->getter(&w, &out);
    EXPECT_EQ(out, &partner);
}

TEST(Reflection, ApplyDefaultStampsProvidedValue) {
    Widget w;
    w._slot = 999;
    w._weight = 99.0f;
    w._label = "dirty";

    auto *meta = Widget::getStaticClass();
    meta->findProperty("slot")->applyDefault(&w);
    meta->findProperty("weight")->applyDefault(&w);
    meta->findProperty("label")->applyDefault(&w);

    EXPECT_EQ(w._slot, 7);            // provided default
    EXPECT_FLOAT_EQ(w._weight, 2.5f); // provided default
    EXPECT_EQ(w._label, "hello");     // provided default
}

TEST(Reflection, ApplyDefaultWithoutExplicitValue) {
    Widget w;
    w._enabled = false;
    w._position = cc::Vec3{9, 9, 9};

    auto *meta = Widget::getStaticClass();
    meta->findProperty("enabled")->applyDefault(&w);  // no default given → FieldT{}
    meta->findProperty("position")->applyDefault(&w);

    EXPECT_FALSE(w._enabled);  // bool{} == false
    EXPECT_FLOAT_EQ(w._position.x, 0.0f);
    EXPECT_FLOAT_EQ(w._position.y, 0.0f);
    EXPECT_FLOAT_EQ(w._position.z, 0.0f);
}

TEST(Reflection, IsDerivedFrom) {
    auto *widget = Widget::getStaticClass();
    auto *button = Button::getStaticClass();
    auto *panel  = Panel::getStaticClass();

    EXPECT_TRUE(button->isDerivedFrom(widget));
    EXPECT_TRUE(button->isDerivedFrom(button));  // reflexive
    EXPECT_TRUE(widget->isDerivedFrom(widget));

    EXPECT_FALSE(widget->isDerivedFrom(button));
    EXPECT_FALSE(button->isDerivedFrom(panel));
    EXPECT_FALSE(panel->isDerivedFrom(widget));
}

TEST(Reflection, VirtualGetClass) {
    Button b;
    Widget *asBase = &b;

    // getClass() is virtual — must return the most-derived meta.
    EXPECT_EQ(asBase->getClass(), Button::getStaticClass());
    EXPECT_EQ(asBase->getClass()->base, Widget::getStaticClass());

    Widget w;
    EXPECT_EQ(w.getClass(), Widget::getStaticClass());
}

// ─── Method reflection ────────────────────────────────────────────────────

TEST(Reflection, MethodRegistration) {
    auto *meta = Widget::getStaticClass();
    ASSERT_NE(meta, nullptr);
    ASSERT_EQ(meta->methods.size(), 4u);
    EXPECT_STREQ(meta->methods[0].name, "bump");
    EXPECT_EQ(meta->methods[0].arity, 1u);
    EXPECT_STREQ(meta->methods[1].name, "appendLabel");
    EXPECT_EQ(meta->methods[1].arity, 1u);
    EXPECT_STREQ(meta->methods[2].name, "touch");
    EXPECT_EQ(meta->methods[2].arity, 0u);
    EXPECT_STREQ(meta->methods[3].name, "moveTo");
    EXPECT_EQ(meta->methods[3].arity, 1u);
}

TEST(Reflection, MethodLookupWalksBase) {
    auto *btnMeta = Button::getStaticClass();
    ASSERT_NE(btnMeta, nullptr);

    // own method
    EXPECT_NE(btnMeta->findMethod("click"), nullptr);

    // inherited from Widget
    EXPECT_NE(btnMeta->findMethod("bump"), nullptr);
    EXPECT_NE(btnMeta->findMethod("appendLabel"), nullptr);

    EXPECT_EQ(btnMeta->findMethod("ghost"), nullptr);
    EXPECT_EQ(btnMeta->findMethod(nullptr), nullptr);
}

TEST(Reflection, InvokeNoArg) {
    Widget w;
    w._enabled = false;
    MethodArgs none;
    EXPECT_TRUE(Widget::getStaticClass()->invoke(&w, "touch", none));
    EXPECT_TRUE(w._enabled);
}

TEST(Reflection, InvokeIntArg) {
    Widget w;
    w._slot = 10;
    MethodArgs args;
    args.push_back(MethodArg::makeInt(5));
    EXPECT_TRUE(Widget::getStaticClass()->invoke(&w, "bump", args));
    EXPECT_EQ(w._slot, 15);
}

TEST(Reflection, InvokeStringArg) {
    Widget w;
    w._label = "foo";
    MethodArgs args;
    args.push_back(MethodArg::makeString("bar"));
    EXPECT_TRUE(Widget::getStaticClass()->invoke(&w, "appendLabel", args));
    EXPECT_EQ(w._label, "foobar");
}

TEST(Reflection, InvokePointerArg) {
    Widget a;
    Widget b;
    MethodArgs args;
    args.push_back(MethodArg::makePointer(&b));
    EXPECT_TRUE(Widget::getStaticClass()->invoke(&a, "moveTo", args));
    EXPECT_EQ(a._partner, &b);
}

TEST(Reflection, InvokeTwoArgsPointerAndString) {
    Button btn;
    Widget sender;
    MethodArgs args;
    args.push_back(MethodArg::makePointer(&sender));
    args.push_back(MethodArg::makeString("hello"));
    EXPECT_TRUE(Button::getStaticClass()->invoke(&btn, "click", args));
    EXPECT_EQ(btn._clickCount, 1u);
    EXPECT_EQ(btn._label, "hello");
    EXPECT_EQ(btn._partner, &sender);
}

TEST(Reflection, InvokeInheritedMethodOnDerived) {
    Button btn;
    btn._slot = 3;
    MethodArgs args;
    args.push_back(MethodArg::makeInt(2));
    EXPECT_TRUE(Button::getStaticClass()->invoke(&btn, "bump", args));
    EXPECT_EQ(btn._slot, 5);
}

TEST(Reflection, InvokeUnknownMethodReturnsFalse) {
    Widget w;
    MethodArgs none;
    EXPECT_FALSE(Widget::getStaticClass()->invoke(&w, "doesNotExist", none));
    EXPECT_FALSE(Widget::getStaticClass()->invoke(&w, nullptr, none));
}

TEST(Reflection, InvokeMissingArgsUsesDefaults) {
    // Handler expects one int — caller forgets to pass it. Should not crash,
    // and the int arg should default to 0 (no-op on _slot).
    Widget w;
    w._slot = 42;
    MethodArgs none;
    EXPECT_TRUE(Widget::getStaticClass()->invoke(&w, "bump", none));
    EXPECT_EQ(w._slot, 42);  // 42 + 0

    // Same for string
    w._label = "x";
    EXPECT_TRUE(Widget::getStaticClass()->invoke(&w, "appendLabel", none));
    EXPECT_EQ(w._label, "x");  // x + "" = x
}

TEST(Reflection, InvokeExtraArgsAreIgnored) {
    // JS semantics: extra positional args are silently dropped.
    Widget w;
    w._slot = 0;
    MethodArgs args;
    args.push_back(MethodArg::makeInt(7));
    args.push_back(MethodArg::makeInt(999));   // ignored
    args.push_back(MethodArg::makeString("noise"));  // ignored
    EXPECT_TRUE(Widget::getStaticClass()->invoke(&w, "bump", args));
    EXPECT_EQ(w._slot, 7);
}

TEST(Reflection, InvokeTypeMismatchFallsBackToDefault) {
    // Caller passes a string where an int is expected — coercion returns 0,
    // the callee runs harmlessly.
    Widget w;
    w._slot = 100;
    MethodArgs args;
    args.push_back(MethodArg::makeString("not-a-number"));
    EXPECT_TRUE(Widget::getStaticClass()->invoke(&w, "bump", args));
    EXPECT_EQ(w._slot, 100);  // +0
}

TEST(Reflection, TypeIdOfBasics) {
    EXPECT_EQ(typeIdOf<bool>(), TypeId::BOOL);
    EXPECT_EQ(typeIdOf<int>(), TypeId::INT32);
    EXPECT_EQ(typeIdOf<uint32_t>(), TypeId::UINT32);
    EXPECT_EQ(typeIdOf<float>(), TypeId::FLOAT);
    EXPECT_EQ(typeIdOf<double>(), TypeId::DOUBLE);
    EXPECT_EQ(typeIdOf<ccstd::string>(), TypeId::STRING);
    EXPECT_EQ(typeIdOf<cc::Vec2>(), TypeId::VEC2);
    EXPECT_EQ(typeIdOf<cc::Vec3>(), TypeId::VEC3);
    EXPECT_EQ(typeIdOf<cc::Color>(), TypeId::COLOR);
    EXPECT_EQ(typeIdOf<Mood>(), TypeId::ENUM);
    EXPECT_EQ(typeIdOf<Widget *>(), TypeId::POINTER);
    EXPECT_EQ(typeIdOf<cc::Mat4>(), TypeId::MAT4);
}
