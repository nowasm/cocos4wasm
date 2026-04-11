/****************************************************************************
 Copyright (c) 2025 Xiamen Yaji Software Co., Ltd.

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

#include "ArrayBuffer.h"
#include <cstdlib>
#include <cstring>

namespace cc {

ArrayBuffer::ArrayBuffer(uint32_t length) : _byteLength{length} {
    _data = static_cast<uint8_t *>(malloc(length));
    memset(_data, 0x00, _byteLength);
}

ArrayBuffer::ArrayBuffer(const uint8_t *data, uint32_t length) {
    reset(data, length);
}

ArrayBuffer::ArrayBuffer() = default;

ArrayBuffer::~ArrayBuffer() {
    if (_data) {
        free(_data);
        _data = nullptr;
    }
}

ArrayBuffer::Ptr ArrayBuffer::slice(uint32_t begin, uint32_t end) {
    CC_ASSERT_GT(end, begin);
    CC_ASSERT_LT(begin, _byteLength);
    CC_ASSERT_LE(end, _byteLength);
    uint32_t newBufByteLength = (end - begin);
    Ptr buffer = ccnew ArrayBuffer(newBufByteLength);
    memcpy(buffer->getData(), _data + begin, newBufByteLength);
    return buffer;
}

void ArrayBuffer::reset(const uint8_t *data, uint32_t length) {
    if (_data != nullptr) {
        free(_data);
    }
    _byteLength = length;
    _data = static_cast<uint8_t *>(malloc(length));
    if (data) {
        memcpy(_data, data, length);
    } else {
        memset(_data, 0x00, length);
    }
}

} // namespace cc
