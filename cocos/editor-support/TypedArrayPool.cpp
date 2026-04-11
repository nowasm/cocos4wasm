#include "TypedArrayPool.h"
#include "MiddlewareMacro.h"

MIDDLEWARE_BEGIN

TypedArrayPool *TypedArrayPool::instance = nullptr;

TypedArrayPool::TypedArrayPool() = default;
TypedArrayPool::~TypedArrayPool() { clearPool(); }

void TypedArrayPool::clearPool() {
    _pool.clear();
}

void TypedArrayPool::dump() {}

void *TypedArrayPool::pop(arrayType /*type*/, std::size_t /*size*/) {
    return nullptr;
}

TypedArrayPool::objPool *TypedArrayPool::getObjPool(arrayType type, std::size_t fitSize) {
    return nullptr;
}

void TypedArrayPool::push(arrayType /*type*/, std::size_t /*arrayCapacity*/, void * /*object*/) {
}

MIDDLEWARE_END
