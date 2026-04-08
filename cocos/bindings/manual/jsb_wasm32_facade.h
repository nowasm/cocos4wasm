#pragma once

#include "bindings/jswrapper/SeApi.h"

/// Register QuickJS-specific JSB binding patches (UIMeshBuffer setters etc.)
/// Always needed on Emscripten regardless of standalone vs full mode.
bool register_wasm32_binding_patches(se::Object *global);

/// Register the minimal JS facade that replaces the full cc module.
/// Only used in standalone WASM mode (CC_WASM_STANDALONE_FACADE=1).
bool register_all_wasm32_facade(se::Object *global);
