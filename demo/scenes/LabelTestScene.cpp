#include "SceneRegistry.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "base/Log.h"
#include "cocos/2d/components/Label.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/2d/renderer/UIBatcher2d.h"
#include "cocos/2d/text/BmfFont.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "engine/EngineEvents.h"

// ─── Label feature + performance probe ───────────────────────────────────
//
// Five test pages covering what our Label currently does (colour /
// multi-line / transforms), what it doesn't (outline / shadow — faked here
// via multi-label tricks to document the workaround), and the draw-call
// throughput ceiling (stress grid). Pages share a top strip (FPS / batch
// count / page name) and a bottom hint line; switching page just toggles
// node.setActive on that page's subtree.
//
//   key 1  Basics       — feature showcase + live counter + chat scroll
//   key 2  Colours      — 4×4 palette, alpha gradient, rainbow animation
//   key 3  Transforms   — node scale / rotation / spin / translate
//   key 4  Effects      — pseudo-outline, pseudo-shadow (multi-Label hack),
//                         typing animation, pulse, fade
//   key 5  Perf         — N×M stress grid
//                         − / = resize grid, T dynamic text, C colour cycle
//
// Intentionally NOT implemented features (Label only exposes setText /
// setFont / setColor today) are either demonstrated via multi-Label
// workarounds (outline, shadow) or flagged in place with "(NOT IMPL)"
// markers so the gap is obvious by just looking at the screen.

namespace {

using cc::Label;
using cc::Node;

constexpr float kCanvasW = 1280.f;
constexpr float kCanvasH =  720.f;

// Top strip / bottom strip Y (centred canvas with anchor 0.5).
constexpr float kTopStripY    =  330.f;
constexpr float kBottomStripY = -330.f;

// Per-page content window — leaves room for the two strips.
constexpr float kContentTopY  =  280.f;
constexpr float kContentBotY  = -280.f;

// Perf grid density presets, keyed to − (idx--) / = (idx++).
struct GridSize { int cols; int rows; };
constexpr GridSize kGridSizes[] = {
    {10,  5},   // 50
    {20, 10},   // 200
    {40, 20},   // 800
    {80, 40},   // 3200
};
constexpr int kDefaultGridIdx = 1;

// Build a Label node in one call — every label in the scene goes through
// this helper so the font + default attach pattern is uniform.
Node *mkLabel(cc::BmfFont *font, const char *text, cc::Vec3 pos,
              cc::Color color = cc::Color(220, 220, 220, 255),
              float scale = 1.f) {
    auto *n = ccnew Node();
    n->addComponent<cc::UITransform>();
    auto *lbl = n->addComponent<Label>();
    lbl->setFont(font);
    lbl->setColor(color);
    lbl->setText(text);
    n->setPosition(pos);
    if (scale != 1.f) n->setScale(cc::Vec3{scale, scale, 1.f});
    return n;
}

// Named colour helper — same named palette used across the colours page.
struct NamedColor { const char *name; cc::Color rgba; };
const NamedColor kPalette[] = {
    {"white",    cc::Color(255, 255, 255, 255)},
    {"silver",   cc::Color(200, 200, 200, 255)},
    {"red",      cc::Color(230,  70,  70, 255)},
    {"green",    cc::Color( 80, 200, 100, 255)},
    {"blue",     cc::Color( 80, 130, 230, 255)},
    {"cyan",     cc::Color( 80, 220, 220, 255)},
    {"magenta",  cc::Color(220,  80, 220, 255)},
    {"yellow",   cc::Color(230, 220,  90, 255)},
    {"orange",   cc::Color(240, 150,  50, 255)},
    {"pink",     cc::Color(240, 150, 200, 255)},
    {"purple",   cc::Color(170, 110, 230, 255)},
    {"lime",     cc::Color(170, 230,  70, 255)},
    {"teal",     cc::Color( 70, 180, 170, 255)},
    {"navy",     cc::Color( 60,  90, 180, 255)},
    {"olive",    cc::Color(170, 150,  70, 255)},
    {"brown",    cc::Color(170, 110,  70, 255)},
};

}  // namespace

class LabelTestScene : public DemoScene {
public:
    const char *name() const override { return "Label — feature probe + perf"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew Node("label-test-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color(22, 24, 32, 255));

        auto *rootUI = _root->addComponent<cc::UITransform>();
        rootUI->setContentSize(kCanvasW, kCanvasH);
        rootUI->setAnchorPoint(0.5f, 0.5f);

        _font = std::make_unique<cc::BmfFont>();
        if (!_font->load("default_fonts/builtin-bitmap/OpenSans-Regular.fnt")) {
            CC_LOG_ERROR("[LabelTest] font load failed — scene will be blank");
            cc::NodeActivator::get().activateNode(_root, true);
            return;
        }

        buildTopStrip();
        buildBottomStrip();

        buildPageBasics();
        buildPageColors();
        buildPageTransforms();
        buildPageEffects();
        buildPagePerf();

        bindKeyboard();

        switchToPage(1);  // start on Basics

        cc::NodeActivator::get().activateNode(_root, true);
        CC_LOG_INFO("[LabelTest] ready — keys 1-5 pages, see bottom strip for per-page hints");
    }

    void onUpdate(float dt) override {
        if (!_root) return;
        ++_frameCount;
        _time += dt;

        // FPS EMA — smooth enough to read, fast enough to react within
        // ~30 frames when page / grid switches change the cost.
        const float fpsInstant = dt > 0.f ? (1.f / dt) : 60.f;
        _fpsEma = 0.95f * _fpsEma + 0.05f * fpsInstant;

        refreshTopStrip();
        refreshBottomStrip();

        switch (_page) {
            case 1: tickBasics(dt);    break;
            case 2: tickColors(dt);    break;
            case 3: tickTransforms(dt);break;
            case 4: tickEffects(dt);   break;
            case 5: tickPerf(dt);      break;
            default: break;
        }
    }

    void onExit() override {
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
        _pageRoots.fill(nullptr);
        _topStats = nullptr;
        _bottomHint = nullptr;
        _basicsCounter = nullptr;
        _basicsChat.clear();
        _colorsRainbow.clear();
        _transformsSpin = nullptr;
        _transformsSlide = nullptr;
        _effectsTyping = nullptr;
        _effectsTypingFull.clear();
        _effectsPulse = nullptr;
        _effectsFade = nullptr;
        _perfLabels.clear();
        _perfRoot = nullptr;
        _font.reset();
    }

private:
    // ── Page switching ──────────────────────────────────────────────────
    void switchToPage(int p) {
        if (p == _page) return;
        _page = p;
        for (int i = 1; i <= 5; ++i) {
            if (_pageRoots[i]) _pageRoots[i]->setActive(i == _page);
        }
        CC_LOG_INFO("[LabelTest] switched to page %d", _page);
    }

    // ── Top + bottom strips (always visible) ────────────────────────────
    void buildTopStrip() {
        auto *n = mkLabel(_font.get(), "FPS: --",
                           cc::Vec3{0.f, kTopStripY, 0.f},
                           cc::Color(255, 255, 170, 255));
        _root->addChild(n);
        _topStats = n->getComponent<Label>();
    }

    void refreshTopStrip() {
        if (!_topStats) return;
        const size_t batchCount = cc::UIBatcher2d::get().getLastBatchCount();
        static const char *pageNames[6] = {
            "-", "1 Basics", "2 Colours", "3 Transforms", "4 Effects", "5 Perf"
        };
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "FPS: %.1f  |  batches: %zu  |  page: %s",
                      _fpsEma, batchCount,
                      (_page >= 1 && _page <= 5) ? pageNames[_page] : "?");
        _topStats->setText(buf);
    }

    void buildBottomStrip() {
        auto *n = mkLabel(_font.get(), "",
                           cc::Vec3{0.f, kBottomStripY, 0.f},
                           cc::Color(150, 170, 200, 255));
        _root->addChild(n);
        _bottomHint = n->getComponent<Label>();
    }

    void refreshBottomStrip() {
        if (!_bottomHint) return;
        const char *hint = "1-5 pages";
        switch (_page) {
            case 1: hint = "[1-5] pages   Basics: static showcase + live counter + chat"; break;
            case 2: hint = "[1-5] pages   Colours: palette + alpha + animated rainbow"; break;
            case 3: hint = "[1-5] pages   Transforms: node scale / rotation / spin / slide"; break;
            case 4: hint = "[1-5] pages   Effects: pseudo-outline / shadow / typing / pulse / fade"; break;
            case 5: {
                char buf[256];
                const auto [cols, rows] = kGridSizes[_gridIdx];
                std::snprintf(buf, sizeof(buf),
                              "[1-5] pages   [- =] grid: %dx%d=%d   [T] dynamic: %s   [C] colour: %s",
                              cols, rows, cols * rows,
                              _perfDynamicOn ? "ON" : "off",
                              _perfColorOn   ? "ON" : "off");
                _bottomHint->setText(buf);
                return;  // buf handled above
            }
            default: break;
        }
        _bottomHint->setText(hint);
    }

    // ── Page 1: Basics ──────────────────────────────────────────────────
    void buildPageBasics() {
        auto *page = ccnew Node("page-basics");
        _root->addChild(page);
        _pageRoots[1] = page;

        // Header
        auto *title = mkLabel(_font.get(),
            "basics  —  what Label renders today",
            cc::Vec3{0.f, kContentTopY, 0.f},
            cc::Color(200, 230, 255, 255));
        page->addChild(title);

        // 3×3 showcase grid (same content as the original scene)
        struct Probe { const char *text; cc::Color color; };
        const Probe rows[3][3] = {
            {
                {"single-line ASCII",              cc::Color(230, 230, 230, 255)},
                {"two-line\nvia \\n",              cc::Color(230, 230, 230, 255)},
                {"unicode ?? (atlas ASCII-only)",  cc::Color(230, 200, 140, 255)},
            },
            {
                {"red tint",                       cc::Color(220,  70,  70, 255)},
                {"50% alpha",                      cc::Color(230, 230, 230, 128)},
                {"rgba(50,200,140)",               cc::Color( 50, 200, 140, 255)},
            },
            {
                {"no wrap: long line overflows ->",cc::Color(170, 170, 255, 255)},
                {"horiz-align (NOT IMPL)",         cc::Color(170, 170, 170, 255)},
                {"native outline (NOT IMPL)",      cc::Color(170, 170, 170, 255)},
            },
        };
        const float colXs[3] = {-420.f, 0.f, 420.f};
        const float rowYs[3] = { 200.f, 120.f, 40.f};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                page->addChild(mkLabel(_font.get(), rows[r][c].text,
                                         cc::Vec3{colXs[c], rowYs[r], 0.f},
                                         rows[r][c].color));
            }
        }

        // Live counter
        {
            auto *n = mkLabel(_font.get(), "Counter: 0",
                               cc::Vec3{-420.f, -60.f, 0.f},
                               cc::Color(150, 230, 255, 255));
            page->addChild(n);
            _basicsCounter = n->getComponent<Label>();
        }

        // Rolling chat (5 lines, oldest on top).
        for (int i = 0; i < 5; ++i) {
            auto *cn = mkLabel(_font.get(), "",
                                cc::Vec3{200.f, -40.f - i * 25.f, 0.f},
                                cc::Color(200, 200, 200, 255));
            page->addChild(cn);
            _basicsChat.push_back(cn->getComponent<Label>());
        }
    }

    void tickBasics(float dt) {
        if (_basicsCounter) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Counter: %d", _frameCount);
            _basicsCounter->setText(buf);
        }
        _basicsChatTimer += dt;
        if (_basicsChatTimer >= 0.5f && !_basicsChat.empty()) {
            _basicsChatTimer = 0.f;
            ++_basicsChatIdx;
            for (size_t i = 0; i + 1 < _basicsChat.size(); ++i) {
                _basicsChat[i]->setText(_basicsChat[i + 1]->getText());
            }
            char buf[64];
            std::snprintf(buf, sizeof(buf), "> msg %d", _basicsChatIdx);
            _basicsChat.back()->setText(buf);
        }
    }

    // ── Page 2: Colours ─────────────────────────────────────────────────
    void buildPageColors() {
        auto *page = ccnew Node("page-colors");
        _root->addChild(page);
        _pageRoots[2] = page;

        page->addChild(mkLabel(_font.get(),
            "colours  —  setColor full RGBA, palette + alpha + animated rainbow",
            cc::Vec3{0.f, kContentTopY, 0.f},
            cc::Color(200, 230, 255, 255)));

        // 4×4 palette — one label per named colour, showing its own name.
        constexpr int kCols = 4;
        constexpr int kRows = 4;
        const float gridW = 900.f;
        const float gridH = 200.f;
        const float cellW = gridW / kCols;
        const float cellH = gridH / kRows;
        const float gridTop = 200.f;
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                const int idx = r * kCols + c;
                const float x = -gridW * 0.5f + cellW * 0.5f + c * cellW;
                const float y = gridTop - cellH * 0.5f - r * cellH;
                page->addChild(mkLabel(_font.get(), kPalette[idx].name,
                                         cc::Vec3{x, y, 0.f},
                                         kPalette[idx].rgba));
            }
        }

        // Alpha gradient — 8 labels from 32..255.
        const float alphaY = -40.f;
        for (int i = 0; i < 8; ++i) {
            const uint8_t a = static_cast<uint8_t>(32 + i * 32);
            const float x = -420.f + i * 120.f;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "a=%u", a);
            page->addChild(mkLabel(_font.get(), buf,
                                     cc::Vec3{x, alphaY, 0.f},
                                     cc::Color(230, 230, 230, a)));
        }

        // Rainbow animation — 8 labels cycling hue, phase offset.
        const float rainbowY = -130.f;
        _colorsRainbow.reserve(8);
        for (int i = 0; i < 8; ++i) {
            const float x = -420.f + i * 120.f;
            auto *n = mkLabel(_font.get(), "RAINBOW",
                               cc::Vec3{x, rainbowY, 0.f},
                               cc::Color(255, 255, 255, 255));
            page->addChild(n);
            _colorsRainbow.push_back(n->getComponent<Label>());
        }
    }

    void tickColors(float /*dt*/) {
        // Cycle hue over time, each label offset by its index.
        for (size_t i = 0; i < _colorsRainbow.size(); ++i) {
            const float phase = _time * 2.f + i * 0.6f;
            const uint8_t r = static_cast<uint8_t>(127.f + 127.f * std::sin(phase));
            const uint8_t g = static_cast<uint8_t>(127.f + 127.f * std::sin(phase + 2.094f));
            const uint8_t b = static_cast<uint8_t>(127.f + 127.f * std::sin(phase + 4.189f));
            _colorsRainbow[i]->setColor(cc::Color(r, g, b, 255));
        }
    }

    // ── Page 3: Transforms ──────────────────────────────────────────────
    void buildPageTransforms() {
        auto *page = ccnew Node("page-transforms");
        _root->addChild(page);
        _pageRoots[3] = page;

        page->addChild(mkLabel(_font.get(),
            "transforms  —  node.scale / node.rotation (Label has no fontSize property yet)",
            cc::Vec3{0.f, kContentTopY, 0.f},
            cc::Color(200, 230, 255, 255)));

        // Scale row — 5 labels at 0.3 / 0.6 / 1.0 / 1.4 / 2.0
        const float scales[5] = {0.3f, 0.6f, 1.0f, 1.4f, 2.0f};
        const float scaleY = 190.f;
        for (int i = 0; i < 5; ++i) {
            const float x = -440.f + i * 220.f;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "scale %.1fx", scales[i]);
            page->addChild(mkLabel(_font.get(), buf, cc::Vec3{x, scaleY, 0.f},
                                     cc::Color(220, 230, 255, 255), scales[i]));
        }

        // Rotation row — static snapshots at assorted angles.
        const float angles[7] = {-60.f, -30.f, -15.f, 0.f, 15.f, 30.f, 60.f};
        const float rotY = 40.f;
        for (int i = 0; i < 7; ++i) {
            const float x = -510.f + i * 170.f;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%+.0f deg", angles[i]);
            auto *n = mkLabel(_font.get(), buf, cc::Vec3{x, rotY, 0.f},
                               cc::Color(230, 220, 160, 255));
            n->setRotationFromEuler(cc::Vec3{0.f, 0.f, angles[i]});
            page->addChild(n);
        }

        // Animated spinner — one label rotating over time.
        {
            auto *n = mkLabel(_font.get(), "SPIN",
                               cc::Vec3{-300.f, -140.f, 0.f},
                               cc::Color(170, 230, 200, 255), 1.5f);
            page->addChild(n);
            _transformsSpin = n;
        }

        // Animated slider — label moving left-right.
        {
            auto *n = mkLabel(_font.get(), "slide",
                               cc::Vec3{200.f, -140.f, 0.f},
                               cc::Color(230, 180, 200, 255));
            page->addChild(n);
            _transformsSlide = n;
        }
    }

    void tickTransforms(float /*dt*/) {
        if (_transformsSpin) {
            const float a = _time * 90.f;  // 90°/sec
            _transformsSpin->setRotationFromEuler(cc::Vec3{0.f, 0.f, a});
        }
        if (_transformsSlide) {
            const float x = 200.f + 150.f * std::sin(_time * 1.5f);
            const cc::Vec3 p = _transformsSlide->getPosition();
            _transformsSlide->setPosition(cc::Vec3{x, p.y, p.z});
        }
    }

    // ── Page 4: Effects workarounds ─────────────────────────────────────
    //
    // Demonstrates the multi-Label techniques teams use when the engine's
    // Label doesn't expose native outline / shadow. Each trick is labelled
    // so it's clear what's being approximated and what the cost is (extra
    // draw verts).
    void buildPageEffects() {
        auto *page = ccnew Node("page-effects");
        _root->addChild(page);
        _pageRoots[4] = page;

        page->addChild(mkLabel(_font.get(),
            "effects  —  pseudo-outline / shadow (multi-Label hacks) + animation",
            cc::Vec3{0.f, kContentTopY, 0.f},
            cc::Color(200, 230, 255, 255)));

        // ── Pseudo-outline ── four dark offset copies + main on top.
        // Cost: 5x Label cost for one visual label. Good for titles /
        // HUD labels; not for chat etc.
        page->addChild(mkLabel(_font.get(), "  pseudo-outline (5 Labels)",
                                 cc::Vec3{-400.f, 220.f, 0.f},
                                 cc::Color(140, 140, 140, 255)));
        buildPseudoOutline(page, cc::Vec3{-400.f, 180.f, 0.f},
                            "OUTLINED!", 2.f,
                            cc::Color(255, 230, 100, 255),   // fill
                            cc::Color(30, 30, 30, 255));      // outline
        // For contrast — same string, no outline.
        page->addChild(mkLabel(_font.get(), "  no outline (1 Label)",
                                 cc::Vec3{200.f, 220.f, 0.f},
                                 cc::Color(140, 140, 140, 255)));
        page->addChild(mkLabel(_font.get(), "OUTLINED!",
                                 cc::Vec3{200.f, 180.f, 0.f},
                                 cc::Color(255, 230, 100, 255), 2.f));

        // ── Pseudo-shadow ── one dark offset copy behind.
        page->addChild(mkLabel(_font.get(), "  pseudo-shadow (2 Labels)",
                                 cc::Vec3{-400.f, 100.f, 0.f},
                                 cc::Color(140, 140, 140, 255)));
        buildPseudoShadow(page, cc::Vec3{-400.f, 60.f, 0.f},
                           "SHADOWED", 2.f,
                           cc::Color(180, 230, 255, 255),   // fill
                           cc::Color(0, 0, 0, 180),         // shadow
                           cc::Vec2{4.f, -4.f});

        // ── Typing animation ── text grows char-by-char.
        {
            _effectsTypingFull = "the quick brown fox jumps over the lazy dog";
            auto *n = mkLabel(_font.get(), "",
                               cc::Vec3{0.f, -30.f, 0.f},
                               cc::Color(200, 230, 200, 255));
            page->addChild(n);
            _effectsTyping = n->getComponent<Label>();
        }

        // ── Pulse ── scale animates in/out.
        {
            auto *n = mkLabel(_font.get(), "PULSE",
                               cc::Vec3{-300.f, -130.f, 0.f},
                               cc::Color(230, 180, 230, 255), 2.f);
            page->addChild(n);
            _effectsPulse = n;
        }

        // ── Fade ── alpha loops 0..255.
        {
            auto *n = mkLabel(_font.get(), "FADE",
                               cc::Vec3{200.f, -130.f, 0.f},
                               cc::Color(230, 230, 100, 255), 2.f);
            page->addChild(n);
            _effectsFade = n->getComponent<Label>();
        }
    }

    // Helper: build 5 labels (4 outline copies + 1 fill) at `pos`.
    // Outline offset 1px on each axis; gives a sharp single-pixel ring.
    void buildPseudoOutline(Node *parent, cc::Vec3 pos, const char *text,
                              float scale, cc::Color fill, cc::Color outline) {
        const cc::Vec3 offsets[4] = {
            cc::Vec3{-1.f,  0.f, 0.f}, cc::Vec3{ 1.f,  0.f, 0.f},
            cc::Vec3{ 0.f, -1.f, 0.f}, cc::Vec3{ 0.f,  1.f, 0.f},
        };
        for (const auto &o : offsets) {
            parent->addChild(mkLabel(_font.get(), text,
                                       cc::Vec3{pos.x + o.x, pos.y + o.y, pos.z},
                                       outline, scale));
        }
        parent->addChild(mkLabel(_font.get(), text, pos, fill, scale));
    }

    // Helper: 2 labels — shadow drop-copy + fill on top.
    void buildPseudoShadow(Node *parent, cc::Vec3 pos, const char *text,
                             float scale, cc::Color fill, cc::Color shadow,
                             cc::Vec2 offset) {
        parent->addChild(mkLabel(_font.get(), text,
                                   cc::Vec3{pos.x + offset.x, pos.y + offset.y, pos.z},
                                   shadow, scale));
        parent->addChild(mkLabel(_font.get(), text, pos, fill, scale));
    }

    void tickEffects(float /*dt*/) {
        // Typing effect: grow one char every 60 ms; restart on overflow.
        if (_effectsTyping && !_effectsTypingFull.empty()) {
            const float period = 0.06f;
            const int totalSteps = static_cast<int>(_effectsTypingFull.size()) + 20;
            const int step = static_cast<int>(_time / period) % totalSteps;
            const int cut = std::min(step, static_cast<int>(_effectsTypingFull.size()));
            _effectsTyping->setText(_effectsTypingFull.substr(0, cut));
        }

        // Pulse: sinusoidal scale 1.5..2.5.
        if (_effectsPulse) {
            const float s = 2.f + 0.5f * std::sin(_time * 3.f);
            _effectsPulse->setScale(cc::Vec3{s, s, 1.f});
        }

        // Fade: alpha follows |sin|.
        if (_effectsFade) {
            const uint8_t a = static_cast<uint8_t>(
                std::abs(std::sin(_time * 1.5f)) * 255.f);
            _effectsFade->setColor(cc::Color(230, 230, 100, a));
        }
    }

    // ── Page 5: Perf ────────────────────────────────────────────────────
    void buildPagePerf() {
        auto *page = ccnew Node("page-perf");
        _root->addChild(page);
        _pageRoots[5] = page;

        page->addChild(mkLabel(_font.get(),
            "perf  —  stress grid.  [- =] density  [T] setText every frame  [C] colour cycle",
            cc::Vec3{0.f, kContentTopY, 0.f},
            cc::Color(200, 230, 255, 255)));

        _perfRoot = ccnew Node("perf-grid");
        page->addChild(_perfRoot);

        buildPerfGrid();
    }

    void buildPerfGrid() {
        if (!_perfRoot) return;
        // Tear down any previous grid.
        _perfRoot->removeAllChildren();
        _perfLabels.clear();

        const auto [cols, rows] = kGridSizes[_gridIdx];
        const float gridW = 1100.f;
        const float gridH =  450.f;
        const float cellW = gridW / cols;
        const float cellH = gridH / rows;
        const float startX = -gridW * 0.5f + cellW * 0.5f;
        const float startY =  gridH * 0.5f - cellH * 0.5f - 40.f;

        _perfLabels.reserve(static_cast<size_t>(cols) * rows);
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const int idx = row * cols + col;
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%d", idx);
                auto *n = mkLabel(_font.get(), buf,
                                   cc::Vec3{startX + col * cellW,
                                             startY - row * cellH, 0.f});
                _perfRoot->addChild(n);
                _perfLabels.push_back(n->getComponent<Label>());
            }
        }
        // Activation: when the page is already active, its new children
        // piggyback on the ambient activation. Otherwise they'll come
        // alive when the user switches to page 5.
        if (_page == 5) cc::NodeActivator::get().activateNode(_perfRoot, true);
        CC_LOG_INFO("[LabelTest] perf grid = %dx%d (%d labels)",
                    cols, rows, cols * rows);
    }

    void tickPerf(float /*dt*/) {
        if (_perfDynamicOn) {
            for (size_t i = 0; i < _perfLabels.size(); ++i) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%d",
                              (static_cast<int>(i) + _frameCount) % 99999);
                _perfLabels[i]->setText(buf);
            }
        }
        if (_perfColorOn) {
            const float t = _time * 0.8f;
            for (size_t i = 0; i < _perfLabels.size(); ++i) {
                const float phase = t + i * 0.04f;
                const uint8_t r = static_cast<uint8_t>(127.f + 127.f * std::sin(phase));
                const uint8_t g = static_cast<uint8_t>(127.f + 127.f * std::sin(phase + 2.094f));
                const uint8_t b = static_cast<uint8_t>(127.f + 127.f * std::sin(phase + 4.189f));
                _perfLabels[i]->setColor(cc::Color(r, g, b, 255));
            }
        }
    }

    // ── Keyboard ────────────────────────────────────────────────────────
    void bindKeyboard() {
        _keyboardL.bind([this](const cc::KeyboardEvent &ev) {
            if (ev.action != cc::KeyboardEvent::Action::PRESS) return;
            switch (ev.key) {
                case '1': case '2': case '3': case '4': case '5':
                    switchToPage(ev.key - '0');
                    break;
                // Perf-only keys — ignored on other pages so they don't
                // accidentally burn cycles. SDLHelper remaps `-` and `=`
                // through KeyCode::MINUS / EQUAL (Creator enum values
                // 189 / 187), not their ASCII codepoints — so these
                // `case` values must match the enum numbers, not the
                // character literals.
                case static_cast<int>(cc::KeyCode::MINUS):
                    if (_page == 5 && _gridIdx > 0) {
                        --_gridIdx; buildPerfGrid();
                    }
                    break;
                case static_cast<int>(cc::KeyCode::EQUAL):
                    if (_page == 5 && _gridIdx + 1 < static_cast<int>(
                            sizeof(kGridSizes) / sizeof(kGridSizes[0]))) {
                        ++_gridIdx; buildPerfGrid();
                    }
                    break;
                case 'T':
                    if (_page == 5) _perfDynamicOn = !_perfDynamicOn;
                    break;
                case 'C':
                    if (_page == 5) _perfColorOn = !_perfColorOn;
                    break;
                default: break;
            }
        });
    }

    // ── State ───────────────────────────────────────────────────────────
    cc::Node                       *_root{nullptr};
    std::unique_ptr<cc::BmfFont>    _font;
    std::array<cc::Node *, 6>       _pageRoots{};  // index 1..5 used

    Label  *_topStats{nullptr};
    Label  *_bottomHint{nullptr};

    int           _page{0};
    int           _frameCount{0};
    float         _time{0.f};
    float         _fpsEma{60.f};

    // Page 1 — Basics
    Label              *_basicsCounter{nullptr};
    std::vector<Label*> _basicsChat;
    float               _basicsChatTimer{0.f};
    int                 _basicsChatIdx{0};

    // Page 2 — Colours
    std::vector<Label*> _colorsRainbow;

    // Page 3 — Transforms
    cc::Node *_transformsSpin{nullptr};
    cc::Node *_transformsSlide{nullptr};

    // Page 4 — Effects
    Label         *_effectsTyping{nullptr};
    ccstd::string  _effectsTypingFull;
    cc::Node      *_effectsPulse{nullptr};
    Label         *_effectsFade{nullptr};

    // Page 5 — Perf
    cc::Node           *_perfRoot{nullptr};
    std::vector<Label*> _perfLabels;
    int                 _gridIdx{kDefaultGridIdx};
    bool                _perfDynamicOn{false};
    bool                _perfColorOn{false};

    // Value-typed member — Listener ctor self-registers with the
    // Engine bus, dtor unregisters.
    cc::events::Keyboard::Listener _keyboardL;
};

REGISTER_DEMO_SCENE("LabelTestScene", LabelTestScene);
