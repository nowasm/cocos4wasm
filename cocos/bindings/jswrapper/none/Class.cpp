#include "Class.h"
#include "base/memory/Memory.h"

namespace se {

Class::Class() = default;

Class::~Class() {
    destroy();
}

void Class::destroy() {
    if (_proto != nullptr) {
        _proto->decRef();
        _proto = nullptr;
    }
}

bool Class::init(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void * /*data*/) {
    _name = clsName;
    _parent = parent;
    _parentProto = parentProto;
    _constructor = ctor;
    _proto = Object::createPlainObject();
    if (_proto != nullptr) {
        _proto->incRef();
    }
    return true;
}

Class *Class::create(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data) {
    auto *c = ccnew Class();
    if (!c->init(clsName, parent, parentProto, ctor, data)) {
        delete c;
        return nullptr;
    }
    return c;
}

Class *Class::create(const std::initializer_list<const char *> &classPath, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data) {
    ccstd::string name;
    for (const char *p : classPath) {
        if (!name.empty()) {
            name += ".";
        }
        name += p;
    }
    return create(name, parent, parentProto, ctor, data);
}

bool Class::defineFunction(const char *, NativeFunctionPtr, void *) {
    return true;
}

bool Class::defineProperty(const char *, NativeFunctionPtr, NativeFunctionPtr, void *) {
    return true;
}

bool Class::defineProperty(const std::initializer_list<const char *> &, NativeFunctionPtr, NativeFunctionPtr, void *) {
    return true;
}

bool Class::defineStaticFunction(const char *, NativeFunctionPtr, void *) {
    return true;
}

bool Class::defineStaticProperty(const char *, NativeFunctionPtr, NativeFunctionPtr, void *) {
    return true;
}

bool Class::defineStaticProperty(const char *, const Value &, PropertyAttribute) {
    return true;
}

bool Class::defineFinalizeFunction(V8FinalizeFunc func) {
    _finalizeFunc = func;
    return true;
}

bool Class::install() {
    return true;
}

Object *Class::getProto() const {
    return _proto;
}

V8FinalizeFunc Class::_getFinalizeFunction() const {
    return _finalizeFunc;
}

void Class::_setCtor(Object *obj) {
    _ctor = obj;
}

Object *Class::_createJSObjectWithClass(Class *cls) {
    if (cls == nullptr || cls->_proto == nullptr) {
        return nullptr;
    }
    return Object::createPlainObject();
}

} // namespace se
