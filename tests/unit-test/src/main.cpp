/****************************************************************************
Copyright (c) 2021 Xiamen Yaji Software Co., Ltd.
http://www.cocos2d-x.org

Pure-C++ mode unit-test entry. Historically this spun up Root + ScriptEngine
to host tests that poked at the JS bindings; those tests are gone along with
the script engine. Current tests exercise leaf utilities (math, reflection,
scheduler, memop, base) that don't need a live engine instance.
****************************************************************************/

#include "gtest/gtest.h"

// Satisfies UniversalPlatform::run which references cocos_main from the
// engine library; unused in test mode.
int cocos_main(int /*argc*/, const char** /*argv*/) {
    return 0;
}

int main(int argc, const char* argv[]) {
    ::testing::InitGoogleTest(&argc, const_cast<char**>(argv));
    return RUN_ALL_TESTS();
}
