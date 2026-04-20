#pragma once

#include <cstdint>

namespace cc {

// Windows-specific keyboard bypass for global IME hooks.
//
// Most Chinese IMEs (Sogou / QQ / Baidu / even Microsoft Pinyin in some
// modes) install a WH_KEYBOARD_LL hook that swallows printable keys
// (letters, digits, symbols) before they reach any window's message
// pump. Detaching the IME context via ImmAssociateContext(hwnd, NULL)
// is *not* enough — the hook runs above that layer.
//
// Raw Input (WM_INPUT / RegisterRawInputDevices) reads keyboard data
// through a different kernel path that the hook can't touch. This
// module registers a keyboard raw-input device for the given HWND and
// subclasses its window proc; on every WM_INPUT it builds a KeyboardEvent
// and broadcasts on the existing `events::Keyboard` bus — so every
// listener in the engine sees it the same way as before.
//
// VK codes line up with our `cc::KeyCode` enum values 1:1 (the enum was
// designed around Win32 VK values), so no extra mapping table is needed.
// For letters and digits the VK also equals the ASCII code, which is
// what scene listeners that write `case '1'` / `case 'A'` expect.
namespace RawInputHook {

// Install raw-input forwarding on `hwndHandle`. Safe to call multiple
// times on the same handle (second+ calls are ignored).
//
// The handle is an `uintptr_t` rather than HWND so the header doesn't
// have to pull in Windows.h; callers cast from HWND at the call site.
void install(uintptr_t hwndHandle);

// True once `install()` has successfully subscribed to raw-input on at
// least one window. Consumers like SDLHelper use this to suppress their
// own SDL_KEYDOWN/SDL_KEYUP broadcasts, since raw-input already covers
// the same keys and handlers that are not idempotent (e.g. EditBox
// backspace) would otherwise fire twice per physical key press.
bool isInstalled();

}  // namespace RawInputHook

}  // namespace cc
