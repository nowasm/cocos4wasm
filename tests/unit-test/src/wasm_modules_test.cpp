/****************************************************************************
 Copyright (c) 2024 Xiamen Yaji Software Co., Ltd.

 Tests for EMSCRIPTEN-restored modules:
 - JPEG/PNG/WebP format detection (Image)
 - Audio decoder manager lifecycle
 - PCMHeader default values
****************************************************************************/

#include "gtest/gtest.h"

// ── Image format detection ────────────────────────────────────────────────────
// Include order matters: define platform macros before pulling in GFX headers
#include "base/Config.h"
#include "cocos/platform/Image.h"

// Expose protected static helpers via a thin subclass
class TestImage : public cc::Image {
public:
    static cc::Image::Format detect(const unsigned char *data, uint32_t len) {
        return detectFormat(data, len);
    }
    static bool checkPng(const unsigned char *data, uint32_t len) { return isPng(data, len); }
    static bool checkJpg(const unsigned char *data, uint32_t len) { return isJpg(data, len); }
    static bool checkWebp(const unsigned char *data, uint32_t len) { return isWebp(data, len); }
};

// PNG magic: \x89 P N G \r \n \x1a \n
static const unsigned char kPngHeader[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
    0x00, 0x00, 0x00, 0x0d  // minimal extra bytes
};

// JPEG magic: FF D8 FF ...
static const unsigned char kJpgHeader[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10
};

// WebP magic: RIFF????WEBP
static const unsigned char kWebpHeader[] = {
    'R', 'I', 'F', 'F',
    0x24, 0x00, 0x00, 0x00,  // file size (placeholder)
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' '
};

// Random bytes that match none of the above
static const unsigned char kUnknownHeader[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d
};

TEST(WasmImageFormatTest, PngDetection) {
    EXPECT_TRUE(TestImage::checkPng(kPngHeader, sizeof(kPngHeader)));
    EXPECT_FALSE(TestImage::checkPng(kJpgHeader, sizeof(kJpgHeader)));
    EXPECT_FALSE(TestImage::checkPng(kWebpHeader, sizeof(kWebpHeader)));
    // Too short
    EXPECT_FALSE(TestImage::checkPng(kPngHeader, 4));
}

TEST(WasmImageFormatTest, JpgDetection) {
    EXPECT_TRUE(TestImage::checkJpg(kJpgHeader, sizeof(kJpgHeader)));
    EXPECT_FALSE(TestImage::checkJpg(kPngHeader, sizeof(kPngHeader)));
    EXPECT_FALSE(TestImage::checkJpg(kWebpHeader, sizeof(kWebpHeader)));
    // Too short
    EXPECT_FALSE(TestImage::checkJpg(kJpgHeader, 2));
}

TEST(WasmImageFormatTest, WebpDetection) {
    EXPECT_TRUE(TestImage::checkWebp(kWebpHeader, sizeof(kWebpHeader)));
    EXPECT_FALSE(TestImage::checkWebp(kPngHeader, sizeof(kPngHeader)));
    EXPECT_FALSE(TestImage::checkWebp(kJpgHeader, sizeof(kJpgHeader)));
    // Too short
    EXPECT_FALSE(TestImage::checkWebp(kWebpHeader, 8));
}

TEST(WasmImageFormatTest, DetectFormatPng) {
    EXPECT_EQ(TestImage::detect(kPngHeader, sizeof(kPngHeader)), cc::Image::Format::PNG);
}

TEST(WasmImageFormatTest, DetectFormatJpg) {
    EXPECT_EQ(TestImage::detect(kJpgHeader, sizeof(kJpgHeader)), cc::Image::Format::JPG);
}

TEST(WasmImageFormatTest, DetectFormatWebp) {
    EXPECT_EQ(TestImage::detect(kWebpHeader, sizeof(kWebpHeader)), cc::Image::Format::WEBP);
}

TEST(WasmImageFormatTest, DetectFormatUnknown) {
    EXPECT_EQ(TestImage::detect(kUnknownHeader, sizeof(kUnknownHeader)), cc::Image::Format::UNKNOWN);
}

// ── PCMHeader defaults ────────────────────────────────────────────────────────
#include "cocos/audio/include/AudioDef.h"

TEST(WasmAudioDefTest, PCMHeaderDefaults) {
    PCMHeader h;
    EXPECT_EQ(h.totalFrames, 0u);
    EXPECT_EQ(h.bytesPerFrame, 0u);
    EXPECT_EQ(h.sampleRate, 0u);
    EXPECT_EQ(h.channelCount, 0u);
    EXPECT_EQ(h.dataFormat, AudioDataFormat::UNKNOWN);
}

TEST(WasmAudioDefTest, PCMHeaderAssignment) {
    PCMHeader h;
    h.sampleRate   = 44100;
    h.channelCount = 2;
    h.bytesPerFrame = 4;
    h.dataFormat   = AudioDataFormat::SIGNED_16;
    EXPECT_EQ(h.sampleRate, 44100u);
    EXPECT_EQ(h.channelCount, 2u);
    EXPECT_EQ(h.bytesPerFrame, 4u);
    EXPECT_EQ(h.dataFormat, AudioDataFormat::SIGNED_16);
}

// ── AudioDecoderManager lifecycle ────────────────────────────────────────────
#if CC_USE_AUDIO
#include "cocos/audio/common/decoder/AudioDecoderManager.h"
#include "cocos/audio/common/decoder/AudioDecoder.h"

TEST(WasmAudioDecoderTest, InitDestroy) {
    EXPECT_TRUE(cc::AudioDecoderManager::init());
    cc::AudioDecoderManager::destroy();
    // Double-init should also succeed
    EXPECT_TRUE(cc::AudioDecoderManager::init());
    cc::AudioDecoderManager::destroy();
}

TEST(WasmAudioDecoderTest, CreateDecoderNonExistentFile) {
    cc::AudioDecoderManager::init();
    // A file that doesn't exist should return nullptr or a decoder that fails open
    cc::AudioDecoder *dec = cc::AudioDecoderManager::createDecoder("nonexistent_file.mp3");
    if (dec != nullptr) {
        // If a decoder object is returned, it should not be opened
        EXPECT_FALSE(dec->isOpened());
        cc::AudioDecoderManager::destroyDecoder(dec);
    } else {
        SUCCEED(); // nullptr is also acceptable
    }
    cc::AudioDecoderManager::destroy();
}
#endif // CC_USE_AUDIO
