#include "IOTypedArray.h"
#include <cstdlib>
#include <cstring>

MIDDLEWARE_BEGIN

IOTypedArray::IOTypedArray(se::Object::TypedArrayType arrayType, std::size_t defaultSize, bool /*usePool*/) {
    _arrayType = arrayType;
    _bufferSize = defaultSize;
    _buffer = static_cast<uint8_t *>(malloc(defaultSize));
    memset(_buffer, 0, defaultSize);
}

IOTypedArray::~IOTypedArray() {
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
}

void IOTypedArray::resize(std::size_t newLen, bool needCopy) {
    if (_bufferSize >= newLen) return;

    auto *newBuffer = static_cast<uint8_t *>(malloc(newLen));
    if (needCopy && _buffer) {
        memcpy(newBuffer, _buffer, _bufferSize);
        memset(newBuffer + _bufferSize, 0, newLen - _bufferSize);
    } else {
        memset(newBuffer, 0, newLen);
    }

    free(_buffer);
    _buffer = newBuffer;
    _bufferSize = newLen;
    _outRange = false;
}

MIDDLEWARE_END
