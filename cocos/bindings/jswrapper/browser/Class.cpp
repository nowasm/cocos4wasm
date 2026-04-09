#include "Class.h"
#include "ScriptEngine.h"
#include "Utils.h"
#include "HelperMacros.h"
#include "../State.h"
#include "base/memory/Memory.h"

#include <emscripten.h>
#include <emscripten/val.h>

#if SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_BROWSER

using emscripten::val;

namespace se {

namespace {
ccstd::vector<Class *> __allClasses; // NOLINT
uint32_t __nextClassId = 1; // NOLINT
} // namespace

Class::Class() {
    __allClasses.push_back(this);
}

Class::~Class() = default;

Class *Class::create(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data) {
    auto *c = ccnew Class();
    if (!c->init(clsName, parent, parentProto, ctor, data)) {
        delete c;
        return nullptr;
    }
    return c;
}

Class *Class::create(const std::initializer_list<const char *> &classPath, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data) {
    se::AutoHandleScope scope;
    se::Value currentParent{parent};
    int i = 0;
    int last = static_cast<int>(classPath.size()) - 1;
    for (auto *p : classPath) {
        if (i < last) {
            se::Value tmp;
            currentParent.toObject()->getProperty(p, &tmp);
            currentParent = tmp;
        }
        ++i;
    }
    return create(*(classPath.end() - 1), currentParent.toObject(), parentProto, ctor, data);
}

bool Class::init(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void * /*data*/) {
    _name = clsName;
    _parent = parent;
    if (_parent) _parent->incRef();
    _parentProto = parentProto;
    if (_parentProto) _parentProto->incRef();
    _constructor = ctor;
    _classId = __nextClassId++;
    return true;
}

void Class::_setCtor(Object *obj) {
    _ctor = obj;
    if (obj != nullptr) {
        obj->root();
        obj->incRef();
    }
}

void Class::destroy() {
    if (_parent) { _parent->decRef(); _parent = nullptr; }
    if (_proto) { _proto->decRef(); _proto = nullptr; }
    if (_parentProto) { _parentProto->decRef(); _parentProto = nullptr; }
    if (_ctor.has_value()) {
        if (_ctor.value() != nullptr) {
            _ctor.value()->unroot();
            _ctor.value()->decRef();
        }
        _ctor.reset();
    }
}

void Class::cleanup() {
    for (auto *cls : __allClasses) {
        cls->destroy();
    }
    se::ScriptEngine::getInstance()->addAfterCleanupHook([]() {
        for (auto *cls : __allClasses) {
            delete cls;
        }
        __allClasses.clear();
    });
}

bool Class::defineFunction(const char *name, NativeFunctionPtr func, void * /*data*/) {
    _funcs.push_back({name, func});
    return true;
}

bool Class::defineProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter, void * /*data*/) {
    _props.push_back({name, getter, setter});
    return true;
}

bool Class::defineProperty(const std::initializer_list<const char *> &names, NativeFunctionPtr getter, NativeFunctionPtr setter, void *data) {
    for (const auto *n : names) defineProperty(n, getter, setter, data);
    return true;
}

bool Class::defineStaticFunction(const char *name, NativeFunctionPtr func, void * /*data*/) {
    _staticFuncs.push_back({name, func});
    return true;
}

bool Class::defineStaticProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter, void * /*data*/) {
    _staticProps.push_back({name, getter, setter});
    return true;
}

bool Class::defineStaticProperty(const char *name, const Value &value, PropertyAttribute /*attribute*/) {
    _staticValues.push_back({name, value});
    return true;
}

bool Class::defineFinalizeFunction(V8FinalizeFunc func) {
    _finalizeFunc = func;
    return true;
}

bool Class::install() {
    // TODO: Full implementation with native function trampolines.
    // For now, create a constructor function and prototype object
    // using browser JS directly via EM_ASM.

    val protoVal = val::object();

    // Set parent proto chain
    if (_parentProto != nullptr) {
        val::global("Object").call<void>("setPrototypeOf", protoVal, _parentProto->_getJSObject());
    }

    // For now, static values only (no native function trampolines yet)
    val ctorFunc = val::object(); // placeholder
    if (_constructor != nullptr) {
        // TODO: Create a real constructor trampoline
        ctorFunc = val::global("Object").call<val>("create", val::null());
    }

    // Set static values
    for (const auto &sv : _staticValues) {
        ctorFunc.set(sv.first, internal::seToJsValue(sv.second));
    }

    // Set prototype and constructor
    ctorFunc.set("prototype", protoVal);
    protoVal.set("constructor", ctorFunc);

    // Register on parent namespace
    if (_parent != nullptr) {
        _parent->_getJSObject().set(_name, ctorFunc);
    }

    // Create proto se::Object
    if (_createProto) {
        _proto = Object::_createJSObject(this, protoVal);
        _proto->root();
    }

    CC_LOG_INFO("Class::install('%s') - stub (no native trampolines yet)", _name.c_str());
    return true;
}

Object *Class::getProto() const { return _proto; }
V8FinalizeFunc Class::_getFinalizeFunction() const { return _finalizeFunc; }

Object *Class::_createJSObjectWithClass(Class *cls) {
    if (!cls) return nullptr;
    val obj = val::object();
    return Object::_createJSObject(cls, obj);
}

} // namespace se

#endif
