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
using emscripten::EM_VAL;

// EM_JS function defined in Object.cpp (C linkage)
extern "C" int browser_make_native_func(int funcId);

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

// Defined in Object.cpp (se namespace)
extern ccstd::vector<NativeFunctionPtr> &browser_getNativeFunctions();

bool Class::install() {
    val protoVal = val::object();

    // Set parent proto chain
    if (_parentProto != nullptr) {
        val::global("Object").call<void>("setPrototypeOf", protoVal, _parentProto->_getJSObject());
    }

    // Create constructor function
    val ctorFunc = val::undefined();
    if (_constructor != nullptr) {
        // Register constructor in the native function table (shared with Object.cpp)
        // We'll use Object::defineFunction's registry indirectly
        // For now, create the constructor using EM_JS
        EM_ASM({
            // Create a named constructor function dynamically
            var name = UTF8ToString($0);
            var funcId = $1;
            var ctor = function() {
                Module.__nativeCallThis = this;
                Module.__nativeCallArgs = Array.prototype.slice.call(arguments);
                Module.__nativeCallRet = undefined;
                Module._browser_do_native_call(funcId);
                // Call _ctor if defined on prototype
                if (this && this._ctor) {
                    this._ctor.apply(this, arguments);
                }
            };
            // Store for C++ to retrieve
            Module.__lastCreatedCtor = ctor;
        }, _name.c_str(), static_cast<int>(browser_getNativeFunctions().size()));
        browser_getNativeFunctions().push_back(_constructor);
        ctorFunc = val::module_property("__lastCreatedCtor");
    } else {
        // Abstract class - create a plain object as the "constructor"
        ctorFunc = val::object();
    }

    // Instance methods on prototype
    for (const auto &f : _funcs) {
        browser_getNativeFunctions().push_back(f.func);
        int funcId = static_cast<int>(browser_getNativeFunctions().size() - 1);
        val jsFunc = val::take_ownership(static_cast<EM_VAL>(
            reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(funcId)))));
        protoVal.set(f.name, jsFunc);
    }

    // Instance properties on prototype
    for (const auto &p : _props) {
        val desc = val::object();
        desc.set("configurable", true);
        if (p.getter) {
            browser_getNativeFunctions().push_back(p.getter);
            int gid = static_cast<int>(browser_getNativeFunctions().size() - 1);
            val gfn = val::take_ownership(static_cast<EM_VAL>(
                reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(gid)))));
            desc.set("get", gfn);
        }
        if (p.setter) {
            browser_getNativeFunctions().push_back(p.setter);
            int sid = static_cast<int>(browser_getNativeFunctions().size() - 1);
            val sfn = val::take_ownership(static_cast<EM_VAL>(
                reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(sid)))));
            desc.set("set", sfn);
        }
        val::global("Object").call<void>("defineProperty", protoVal, val(p.name), desc);
    }

    // Static functions on constructor
    for (const auto &sf : _staticFuncs) {
        browser_getNativeFunctions().push_back(sf.func);
        int funcId = static_cast<int>(browser_getNativeFunctions().size() - 1);
        val jsFunc = val::take_ownership(static_cast<EM_VAL>(
            reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(funcId)))));
        ctorFunc.set(sf.name, jsFunc);
    }

    // Static properties
    for (const auto &sp : _staticProps) {
        val desc = val::object();
        desc.set("configurable", true);
        if (sp.getter) {
            browser_getNativeFunctions().push_back(sp.getter);
            int gid = static_cast<int>(browser_getNativeFunctions().size() - 1);
            val gfn = val::take_ownership(static_cast<EM_VAL>(
                reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(gid)))));
            desc.set("get", gfn);
        }
        if (sp.setter) {
            browser_getNativeFunctions().push_back(sp.setter);
            int sid = static_cast<int>(browser_getNativeFunctions().size() - 1);
            val sfn = val::take_ownership(static_cast<EM_VAL>(
                reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(sid)))));
            desc.set("set", sfn);
        }
        val::global("Object").call<void>("defineProperty", ctorFunc, val(sp.name), desc);
    }

    // Static values
    for (const auto &sv : _staticValues) {
        ctorFunc.set(sv.first, internal::seToJsValue(sv.second));
    }

    // Set prototype ↔ constructor link
    ctorFunc.set("prototype", protoVal);
    protoVal.set("constructor", ctorFunc);

    // Register on parent namespace
    if (_parent != nullptr) {
        _parent->_getJSObject().set(_name, ctorFunc);
    }

    // Always create the proto wrapper — auto-generated bindings access
    // cls->getProto() unconditionally to register manual methods on it.
    _proto = Object::_createJSObject(this, protoVal);
    _proto->root();

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
