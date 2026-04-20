#include "cocos/2d/text/BmfFont.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "base/Log.h"
#include "game/TextureLoader.h"
#include "platform/FileUtils.h"

namespace cc {

namespace {

// Extracts a `key="value"` or `key=value` pair out of one line of the
// BMFont XML. BMFont's XML is flat and quoted-attribute-only, so a tiny
// text scanner is enough — avoids dragging tinyxml2 into the 2D layer.
bool readAttr(const ccstd::string &line, const char *key, ccstd::string &outValue) {
    auto pos = line.find(key);
    if (pos == ccstd::string::npos) return false;
    pos = line.find('=', pos);
    if (pos == ccstd::string::npos) return false;
    ++pos;
    if (pos < line.size() && line[pos] == '"') {
        auto endPos = line.find('"', pos + 1);
        if (endPos == ccstd::string::npos) return false;
        outValue.assign(line, pos + 1, endPos - pos - 1);
    } else {
        auto endPos = line.find_first_of(" />\n\r", pos);
        outValue.assign(line, pos, endPos - pos);
    }
    return true;
}

int readInt(const ccstd::string &line, const char *key, int fallback = 0) {
    ccstd::string v;
    if (!readAttr(line, key, v)) return fallback;
    return std::atoi(v.c_str());
}

// Pull the directory portion of a path (everything up to and including
// the last '/' or '\'). "a/b/file.fnt" -> "a/b/". Pure path used as
// non-root gets "".
ccstd::string dirnameOf(const ccstd::string &path) {
    auto pos = path.find_last_of("/\\");
    if (pos == ccstd::string::npos) return "";
    return path.substr(0, pos + 1);
}

}  // namespace

bool BmfFont::load(const ccstd::string &fntPath) {
    auto *fu = FileUtils::getInstance();
    ccstd::string fntResolved = fu->isFileExist(fntPath) ? fntPath : fu->fullPathForFilename(fntPath);
    if (fntResolved.empty() || !fu->isFileExist(fntResolved)) {
        CC_LOG_ERROR("[BmfFont] .fnt not found: %s", fntPath.c_str());
        return false;
    }

    ccstd::string text = fu->getStringFromFile(fntResolved);
    if (text.empty()) {
        CC_LOG_ERROR("[BmfFont] .fnt empty: %s", fntResolved.c_str());
        return false;
    }

    ccstd::string atlasFile;

    // Scan line by line. Each BMFont element (<info>, <common>, <page>,
    // <char>) lives on its own line in AngelCode's default output.
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == ccstd::string::npos) end = text.size();
        ccstd::string line(text, start, end - start);
        start = end + 1;

        if (line.find("<info") != ccstd::string::npos &&
            line.find("<infos") == ccstd::string::npos) {
            // <info face="..." size="N" ...> — N is the point size the
            // atlas was rasterised at. Non-fatal if missing (fallback
            // to lineHeight as reference in Label).
            _baseFontSize = static_cast<uint16_t>(readInt(line, "size"));
        } else if (line.find("<common") != ccstd::string::npos) {
            _atlasW     = static_cast<uint16_t>(readInt(line, "scaleW"));
            _atlasH     = static_cast<uint16_t>(readInt(line, "scaleH"));
            _lineHeight = static_cast<uint16_t>(readInt(line, "lineHeight"));
            // AngelCode BMFont's `base` = distance from line top to
            // baseline = our ascender metric (distance from baseline to
            // top of ascenders).
            _ascender   = static_cast<uint16_t>(readInt(line, "base"));
        } else if (line.find("<page") != ccstd::string::npos) {
            readAttr(line, "file", atlasFile);  // e.g. "OpenSans-Regular_0.png"
        } else if (line.find("<char") != ccstd::string::npos &&
                   line.find("<chars") == ccstd::string::npos) {
            FontLetterDef g;
            int id      = readInt(line, "id");
            g.x         = static_cast<uint16_t>(readInt(line, "x"));
            g.y         = static_cast<uint16_t>(readInt(line, "y"));
            g.w         = static_cast<uint16_t>(readInt(line, "width"));
            g.h         = static_cast<uint16_t>(readInt(line, "height"));
            g.xoffset   = static_cast<int16_t>(readInt(line, "xoffset"));
            g.yoffset   = static_cast<int16_t>(readInt(line, "yoffset"));
            g.xadvance  = static_cast<int16_t>(readInt(line, "xadvance"));
            _glyphs.emplace(static_cast<uint32_t>(id), g);
        }
    }

    if (_atlasW == 0 || _atlasH == 0 || atlasFile.empty() || _glyphs.empty()) {
        CC_LOG_ERROR("[BmfFont] incomplete parse: atlas=%dx%d glyphs=%zu page='%s'",
                     _atlasW, _atlasH, _glyphs.size(), atlasFile.c_str());
        return false;
    }

    ccstd::string atlasPath = dirnameOf(fntResolved) + atlasFile;
    // Font atlases are alpha masks — AngelCode's default export writes the
    // glyph shape as L8/LA8 pixel intensity. Asking TextureLoader for
    // ALPHA_MASK interpretation routes the intensity into the alpha channel
    // and leaves RGB white, which is what the alpha-blend shader expects.
    _atlas = game::TextureLoader::loadFromFile(
        atlasPath, game::TextureLoader::Interpret::ALPHA_MASK);
    if (!_atlas) {
        CC_LOG_ERROR("[BmfFont] atlas PNG load failed: %s", atlasPath.c_str());
        return false;
    }

    CC_LOG_INFO("[BmfFont] loaded '%s' — atlas %dx%d, %zu glyphs, "
                "lineHeight=%d baseSize=%d",
                fntResolved.c_str(), _atlasW, _atlasH, _glyphs.size(),
                _lineHeight, _baseFontSize);
    return true;
}

}  // namespace cc
