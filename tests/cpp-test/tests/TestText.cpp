#include "../TestScene.h"
#include "profiler/DebugRenderer.h"

using namespace cc;
using namespace cc::game;

// ---- Basic Text Colors ----
class TestTextColors : public TestScene {
public:
    std::string name() override { return "Text Colors"; }
    std::string category() override { return "Text"; }
    void setup(scene::RenderScene *rs, Root *root) override {}
    void update(float dt) override {
        auto *dr = DebugRenderer::getInstance();
        if (!dr) return;

        struct TextLine { const char *text; gfx::Color color; };
        TextLine lines[] = {
            {"White Text (Default)",           {1.0f, 1.0f, 1.0f, 1.0f}},
            {"Red Text",                       {1.0f, 0.0f, 0.0f, 1.0f}},
            {"Green Text",                     {0.0f, 1.0f, 0.0f, 1.0f}},
            {"Blue Text",                      {0.0f, 0.0f, 1.0f, 1.0f}},
            {"Yellow Text",                    {1.0f, 1.0f, 0.0f, 1.0f}},
            {"Cyan Text",                      {0.0f, 1.0f, 1.0f, 1.0f}},
            {"Magenta Text",                   {1.0f, 0.0f, 1.0f, 1.0f}},
            {"Orange Text",                    {1.0f, 0.6f, 0.0f, 1.0f}},
            {"Semi-transparent White",         {1.0f, 1.0f, 1.0f, 0.4f}},
        };

        float y = 60.0f;
        for (auto &line : lines) {
            DebugTextInfo info;
            info.color = line.color;
            info.scale = 1.5f;
            dr->addText(line.text, Vec2(40.0f, y), info);
            y += 36.0f;
        }
    }
};
REGISTER_TEST("Text", "Text Colors", TestTextColors);

// ---- Font Styles ----
class TestFontStyles : public TestScene {
public:
    std::string name() override { return "Font Styles"; }
    std::string category() override { return "Text"; }
    void setup(scene::RenderScene *rs, Root *root) override {}
    void update(float dt) override {
        auto *dr = DebugRenderer::getInstance();
        if (!dr) return;

        float y = 60.0f;
        float x = 40.0f;

        // Normal
        {
            DebugTextInfo info;
            info.color = {1, 1, 1, 1};
            info.scale = 1.5f;
            dr->addText("Normal Text", Vec2(x, y), info);
            y += 40.0f;
        }
        // Bold
        {
            DebugTextInfo info;
            info.color = {1, 1, 1, 1};
            info.bold = true;
            info.scale = 1.5f;
            dr->addText("Bold Text", Vec2(x, y), info);
            y += 40.0f;
        }
        // Italic
        {
            DebugTextInfo info;
            info.color = {1, 1, 1, 1};
            info.italic = true;
            info.scale = 1.5f;
            dr->addText("Italic Text", Vec2(x, y), info);
            y += 40.0f;
        }
        // Bold + Italic
        {
            DebugTextInfo info;
            info.color = {1, 1, 1, 1};
            info.bold = true;
            info.italic = true;
            info.scale = 1.5f;
            dr->addText("Bold Italic Text", Vec2(x, y), info);
            y += 40.0f;
        }
        // Shadow
        {
            DebugTextInfo info;
            info.color = {1, 1, 0, 1};
            info.shadow = true;
            info.shadowThickness = 2;
            info.shadowColor = {0, 0, 0, 1};
            info.scale = 1.5f;
            dr->addText("Text with Shadow", Vec2(x, y), info);
            y += 40.0f;
        }
        // Large scale
        {
            DebugTextInfo info;
            info.color = {0.3f, 0.8f, 1.0f, 1.0f};
            info.scale = 3.0f;
            dr->addText("BIG TEXT", Vec2(x, y), info);
            y += 60.0f;
        }
        // Small scale
        {
            DebugTextInfo info;
            info.color = {0.8f, 0.8f, 0.8f, 1.0f};
            info.scale = 0.8f;
            dr->addText("Small text for fine details and notes", Vec2(x, y), info);
        }
    }
};
REGISTER_TEST("Text", "Font Styles", TestFontStyles);

// ---- Text Layout ----
class TestTextLayout : public TestScene {
    float _time{0};
public:
    std::string name() override { return "Text Layout"; }
    std::string category() override { return "Text"; }
    void setup(scene::RenderScene *rs, Root *root) override {}
    void update(float dt) override {
        _time += dt;
        auto *dr = DebugRenderer::getInstance();
        if (!dr) return;

        // Title
        {
            DebugTextInfo info;
            info.color = {1, 0.9f, 0.2f, 1};
            info.bold = true;
            info.scale = 2.0f;
            info.shadow = true;
            info.shadowThickness = 2;
            dr->addText("Cocos C++ Engine", Vec2(40, 50), info);
        }

        // Paragraph
        const char *lines[] = {
            "This is a pure C++ game engine built from Cocos Creator.",
            "All JavaScript bindings have been removed.",
            "Rendering powered by OpenGL ES 3.0 / GLES forward pipeline.",
            "Features: PBR materials, multiple lights, scene graph, input events.",
        };
        float y = 110.0f;
        for (auto *line : lines) {
            DebugTextInfo info;
            info.color = {0.85f, 0.85f, 0.85f, 1};
            info.scale = 1.2f;
            dr->addText(line, Vec2(40, y), info);
            y += 28.0f;
        }

        // Animated colored line
        {
            float r = 0.5f + 0.5f * sinf(_time * 2.0f);
            float g = 0.5f + 0.5f * sinf(_time * 2.0f + 2.1f);
            float b = 0.5f + 0.5f * sinf(_time * 2.0f + 4.2f);
            DebugTextInfo info;
            info.color = {r, g, b, 1};
            info.bold = true;
            info.scale = 1.8f;
            dr->addText("Rainbow animated text!", Vec2(40, y + 20), info);
        }

        // Stats at bottom
        {
            DebugTextInfo info;
            info.color = {0.5f, 1.0f, 0.5f, 1};
            info.scale = 1.0f;
            char buf[128];
            snprintf(buf, sizeof(buf), "Time: %.1fs | Press Left/Right to switch tests", _time);
            dr->addText(buf, Vec2(40, 650), info);
        }
    }
};
REGISTER_TEST("Text", "Text Layout", TestTextLayout);

// ---- Color Palette Text ----
class TestColorPalette : public TestScene {
public:
    std::string name() override { return "Color Palette"; }
    std::string category() override { return "Text"; }
    void setup(scene::RenderScene *rs, Root *root) override {}
    void update(float dt) override {
        auto *dr = DebugRenderer::getInstance();
        if (!dr) return;

        DebugTextInfo title;
        title.color = {1, 1, 1, 1};
        title.bold = true;
        title.scale = 1.8f;
        dr->addText("Font Color Palette", Vec2(40, 50), title);

        struct Swatch { const char *label; gfx::Color color; };
        static const Swatch swatches[] = {
            {"FF0000 Red",    {1.0f, 0.0f, 0.0f, 1.0f}},
            {"00FF00 Green",  {0.0f, 1.0f, 0.0f, 1.0f}},
            {"0000FF Blue",   {0.0f, 0.0f, 1.0f, 1.0f}},
            {"FFFF00 Yellow", {1.0f, 1.0f, 0.0f, 1.0f}},
            {"FF00FF Pink",   {1.0f, 0.0f, 1.0f, 1.0f}},
            {"00FFFF Cyan",   {0.0f, 1.0f, 1.0f, 1.0f}},
            {"FF8800 Orange", {1.0f, 0.53f, 0.0f, 1.0f}},
            {"8800FF Purple", {0.53f, 0.0f, 1.0f, 1.0f}},
            {"FFFFFF White",  {1.0f, 1.0f, 1.0f, 1.0f}},
            {"888888 Gray",   {0.53f, 0.53f, 0.53f, 1.0f}},
            {"00FF88 Mint",   {0.0f, 1.0f, 0.53f, 1.0f}},
            {"FF0088 Rose",   {1.0f, 0.0f, 0.53f, 1.0f}},
        };

        float startY = 110.0f;
        for (int i = 0; i < 12; ++i) {
            DebugTextInfo info;
            info.color = swatches[i].color;
            info.bold = true;
            info.scale = 1.5f;
            int col = i % 3;
            int row = i / 3;
            float x = 60.0f + col * 260.0f;
            float y = startY + row * 60.0f;
            dr->addText(swatches[i].label, Vec2(x, y), info);
        }
    }
};
REGISTER_TEST("Text", "Color Palette", TestColorPalette);
