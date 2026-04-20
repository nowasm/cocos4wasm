#include "platform/win32/RawInputHook.h"

#include <Windows.h>
#include <hidusage.h>

#include <unordered_set>

#include "base/Log.h"
#include "engine/EngineEvents.h"

// Force-link user32 (RegisterRawInputDevices, GetRawInputData) locally —
// same pattern as the imm32 pragma in SystemWindow.cpp.
#pragma comment(lib, "user32.lib")

namespace cc {
namespace RawInputHook {

namespace {

// Windows subclasses its window proc by storing the previous WNDPROC
// pointer via SetPropW / GetPropW so our subclassed proc can chain
// through. A single static set tracks which HWNDs we've already hooked
// so repeated `install` calls are no-ops.
constexpr const wchar_t *kPrevProcProp = L"cc_ri_prev_wndproc";
std::unordered_set<HWND> g_hookedWindows;

// Active modifier snapshot — Raw Input doesn't carry the mod state the
// way WM_KEYDOWN/GetKeyState do, so we query the async keyboard state
// per event. Cheaper and simpler than tracking presses ourselves.
bool isVkDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// Convert a RAWKEYBOARD + its VKey into the KeyCode integer our
// `events::Keyboard` bus expects. Our enum values were picked to match
// Win32 VK directly (ARROW_LEFT=37=VK_LEFT, `'1'`=49=VK_1 etc.), so the
// conversion is largely pass-through. A few cases need adjustment:
//   * Left/right variants (Ctrl / Shift / Alt) — RI flags tell us which.
//   * Numpad vs main-row digits — distinguished by RI_KEY_E0 flag.
//   * The Windows "meta" keys LWIN / RWIN map to our META_LEFT/RIGHT.
int mapVKToCocosKey(const RAWKEYBOARD &kb) {
    const UINT vk = kb.VKey;
    const bool extended = (kb.Flags & RI_KEY_E0) != 0;

    switch (vk) {
        case VK_SHIFT:
            // Raw Input reports VK_SHIFT unified; disambiguate via scan.
            return (MapVirtualKeyW(kb.MakeCode, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT)
                       ? static_cast<int>(KeyCode::SHIFT_RIGHT)
                       : static_cast<int>(KeyCode::SHIFT_LEFT);
        case VK_CONTROL:
            return extended ? static_cast<int>(KeyCode::CONTROL_RIGHT)
                            : static_cast<int>(KeyCode::CONTROL_LEFT);
        case VK_MENU:  // Alt
            return extended ? static_cast<int>(KeyCode::ALT_RIGHT)
                            : static_cast<int>(KeyCode::ALT_LEFT);
        case VK_LWIN:  return static_cast<int>(KeyCode::META_LEFT);
        case VK_RWIN:  return static_cast<int>(KeyCode::META_RIGHT);
        case VK_RETURN:
            return extended ? static_cast<int>(KeyCode::NUMPAD_ENTER)
                            : static_cast<int>(KeyCode::ENTER);
        default:
            break;
    }

    // Numpad 0-9: when NumLock is ON we get VK_NUMPAD0..9 directly.
    // Raw keys still VK_NUMPADx even without NumLock? They actually
    // come as VK_INSERT/VK_END/etc. We pass those through; NUMPAD_0..9
    // codes in our enum are used when NumLock+digit is pressed.
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        static constexpr int kNumpadCodes[10] = {
            static_cast<int>(KeyCode::NUMPAD_0),
            static_cast<int>(KeyCode::NUMPAD_1),
            static_cast<int>(KeyCode::NUMPAD_2),
            static_cast<int>(KeyCode::NUMPAD_3),
            static_cast<int>(KeyCode::NUMPAD_4),
            static_cast<int>(KeyCode::NUMPAD_5),
            static_cast<int>(KeyCode::NUMPAD_6),
            static_cast<int>(KeyCode::NUMPAD_7),
            static_cast<int>(KeyCode::NUMPAD_8),
            static_cast<int>(KeyCode::NUMPAD_9),
        };
        return kNumpadCodes[vk - VK_NUMPAD0];
    }

    // For all other VKs (digits '0'-'9' = 0x30..0x39, letters 'A'-'Z' =
    // 0x41..0x5A, arrows, Home/End/Ins/Del, OEM_MINUS=189, OEM_PLUS=187,
    // F1-F12, Esc, Backspace, Tab, Space, Delete), the VK number already
    // equals the `KeyCode` enum value, so we pass through unchanged.
    return static_cast<int>(vk);
}

// Parse a WM_INPUT message into a RAWINPUT buffer and forward the
// keyboard chunk (if any) to the `events::Keyboard` bus.
void handleRawInput(uint32_t windowId, HRAWINPUT rawHandle) {
    UINT size = 0;
    if (GetRawInputData(rawHandle, RID_INPUT, nullptr, &size,
                         sizeof(RAWINPUTHEADER)) == UINT_MAX) {
        return;
    }
    // The buffer is small enough (~40 bytes for a keyboard event) that a
    // stack buffer avoids a heap allocation on the hot path.
    alignas(RAWINPUT) unsigned char buf[64];
    if (size > sizeof(buf)) return;  // unexpected; skip
    if (GetRawInputData(rawHandle, RID_INPUT, buf, &size,
                         sizeof(RAWINPUTHEADER)) == UINT_MAX) {
        return;
    }
    const RAWINPUT *ri = reinterpret_cast<const RAWINPUT *>(buf);
    if (ri->header.dwType != RIM_TYPEKEYBOARD) return;

    const RAWKEYBOARD &kb = ri->data.keyboard;

    // Microsoft documents a spurious VK=0xFF event for non-ASCII key-up
    // from some hardware; ignore.
    if (kb.VKey == 0xFF) return;

    KeyboardEvent event;
    event.windowId = windowId;
    event.key = mapVKToCocosKey(kb);
    const bool isBreak = (kb.Flags & RI_KEY_BREAK) != 0;
    event.action = isBreak ? KeyboardEvent::Action::RELEASE
                            : KeyboardEvent::Action::PRESS;
    event.shiftKeyActive = isVkDown(VK_SHIFT);
    event.ctrlKeyActive  = isVkDown(VK_CONTROL);
    event.altKeyActive   = isVkDown(VK_MENU);
    event.metaKeyActive  = isVkDown(VK_LWIN) || isVkDown(VK_RWIN);

    events::Keyboard::broadcast(event);
}

// Subclassed window proc — intercepts WM_INPUT, forwards everything
// else to the original (SDL's) window proc. Raw input is an *additive*
// channel: SDL still receives WM_KEYDOWN for keys the OS/IME doesn't
// swallow, so printable keys come exclusively via this path while
// arrow / modifier keys arrive via both. Our `events::Keyboard` bus
// already tolerates duplicate events (listeners are idempotent), and
// the double-broadcast risk is an acceptable cost for a ~10 line fix
// to an unfixable hook-layer issue.
LRESULT CALLBACK subclassedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUT) {
        handleRawInput(/*windowId=*/0,
                       reinterpret_cast<HRAWINPUT>(lParam));
        // Fall through to the original proc so SDL can drain the
        // message — some SDL versions expect WM_INPUT to be
        // processed/cleanup-called even if we don't route the result.
    }
    auto prev = reinterpret_cast<WNDPROC>(GetPropW(hwnd, kPrevProcProp));
    if (prev) return CallWindowProcW(prev, hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool registerKeyboardDevice(HWND hwnd) {
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid.usUsage     = HID_USAGE_GENERIC_KEYBOARD;
    // RIDEV_INPUTSINK: receive events even when the window isn't in the
    // foreground (useful for test fixtures). RIDEV_NOLEGACY is tempting
    // to reduce double-broadcast, but it also suppresses WM_KEYDOWN —
    // which would break any other subsystem reading GetKeyboardState.
    rid.dwFlags     = RIDEV_INPUTSINK;
    rid.hwndTarget  = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        CC_LOG_WARNING("[win32] RegisterRawInputDevices failed (err=%lu)",
                       GetLastError());
        return false;
    }
    return true;
}

}  // namespace

void install(uintptr_t hwndHandle) {
    HWND hwnd = reinterpret_cast<HWND>(hwndHandle);
    if (!hwnd) return;
    if (g_hookedWindows.count(hwnd)) return;  // idempotent
    g_hookedWindows.insert(hwnd);

    // Subclass first so we don't miss the first WM_INPUT that arrives
    // right after RegisterRawInputDevices.
    WNDPROC prev = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(subclassedWndProc)));
    SetPropW(hwnd, kPrevProcProp, prev);

    if (!registerKeyboardDevice(hwnd)) {
        // Roll back the subclass so we don't pay its cost for nothing.
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(prev));
        RemovePropW(hwnd, kPrevProcProp);
        g_hookedWindows.erase(hwnd);
        return;
    }

    CC_LOG_INFO("[win32] Raw Input keyboard hook installed on HWND=%p", hwnd);
}

bool isInstalled() {
    return !g_hookedWindows.empty();
}

}  // namespace RawInputHook
}  // namespace cc
