#include "cocos/asset/MaterialLoader.h"

#include <cstring>

#include "base/Log.h"
#include "base/memory/Memory.h"
#include "cocos/asset/AssetManager.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/TextureBase.h"
#include "math/Color.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "math/Vec4.h"
#include "platform/FileUtils.h"
#include "rapidjson/document.h"

namespace cc {

namespace {

using JsonValue = rapidjson::Value;

bool hasStringUuid(const JsonValue &v) {
    return v.IsObject() && v.HasMember("__uuid__") && v["__uuid__"].IsString();
}

// Cocos Creator emits `_effectAsset` either as `{"__uuid__":"..."}` (authored
// projects) or as a raw name string like "builtin-unlit" (hand-crafted test
// fixtures, before a UUID has been assigned). Support both for ergonomics.
IntrusivePtr<EffectAsset> resolveEffectAsset(const JsonValue &v) {
    if (hasStringUuid(v)) {
        auto ip = AssetManager::get().load<EffectAsset>(v["__uuid__"].GetString());
        return ip;
    }
    if (v.IsString()) {
        EffectAsset *raw = EffectAsset::get(v.GetString());
        return IntrusivePtr<EffectAsset>(raw);
    }
    return nullptr;
}

bool decodeColor(const JsonValue &v, Color &out) {
    if (!v.IsObject()) return false;
    auto asU8 = [](const JsonValue &m, uint8_t fallback) -> uint8_t {
        if (m.IsInt())    return static_cast<uint8_t>(m.GetInt());
        if (m.IsNumber()) return static_cast<uint8_t>(m.GetDouble());
        return fallback;
    };
    out.r = v.HasMember("r") ? asU8(v["r"], 255) : 255;
    out.g = v.HasMember("g") ? asU8(v["g"], 255) : 255;
    out.b = v.HasMember("b") ? asU8(v["b"], 255) : 255;
    out.a = v.HasMember("a") ? asU8(v["a"], 255) : 255;
    return true;
}

float asFloat(const JsonValue &v, float fallback = 0.0f) {
    return v.IsNumber() ? v.GetFloat() : fallback;
}

// Apply a single authored property to the material. Supported value shapes
// (matching what Creator emits for standard shader params):
//   number                       → float / int
//   bool                         → int (0/1) — defines route, not props
//   { x,y[,z[,w]] }              → Vec2/Vec3/Vec4
//   { r,g,b,a }                  → Color
//   { __uuid__ }                 → texture (resolved via AssetManager)
void applyProp(Material *mat, const ccstd::string &name, const JsonValue &v, int passIdx) {
    if (v.IsBool()) {
        mat->setPropertyInt32(name, v.GetBool() ? 1 : 0, passIdx);
        return;
    }
    if (v.IsInt()) {
        mat->setPropertyInt32(name, v.GetInt(), passIdx);
        return;
    }
    if (v.IsNumber()) {
        mat->setPropertyFloat32(name, v.GetFloat(), passIdx);
        return;
    }
    if (v.IsObject()) {
        if (hasStringUuid(v)) {
            auto tex = AssetManager::get().load<TextureBase>(v["__uuid__"].GetString());
            if (tex) mat->setPropertyTextureBase(name, tex.get(), passIdx);
            return;
        }
        // Heuristic: r/g/b/a keys ⇒ Color; x/y[/z[/w]] ⇒ Vec*.
        if (v.HasMember("r") && v.HasMember("g") && v.HasMember("b")) {
            Color c;
            decodeColor(v, c);
            mat->setPropertyColor(name, c, passIdx);
            return;
        }
        const bool hx = v.HasMember("x"), hy = v.HasMember("y");
        const bool hz = v.HasMember("z"), hw = v.HasMember("w");
        if (hx && hy && hz && hw) {
            mat->setPropertyVec4(name,
                Vec4{asFloat(v["x"]), asFloat(v["y"]), asFloat(v["z"]), asFloat(v["w"])},
                passIdx);
            return;
        }
        if (hx && hy && hz) {
            mat->setPropertyVec3(name,
                Vec3{asFloat(v["x"]), asFloat(v["y"]), asFloat(v["z"])}, passIdx);
            return;
        }
        if (hx && hy) {
            mat->setPropertyVec2(name, Vec2{asFloat(v["x"]), asFloat(v["y"])}, passIdx);
            return;
        }
    }
    CC_LOG_WARNING("[MaterialLoader] unsupported prop shape for '%s'", name.c_str());
}

// Parse one macro record: { "KEY": true | 1 | "VALUE" } → MacroRecord.
MacroRecord parseDefines(const JsonValue &obj) {
    MacroRecord out;
    if (!obj.IsObject()) return out;
    for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
        const char *k = it->name.GetString();
        const JsonValue &v = it->value;
        if (v.IsBool())        out[k] = v.GetBool();
        else if (v.IsInt())    out[k] = v.GetInt();
        else if (v.IsString()) out[k] = ccstd::string(v.GetString());
        else if (v.IsNumber()) out[k] = static_cast<int32_t>(v.GetDouble());
    }
    return out;
}

}  // namespace

Material *MaterialLoader::loadFromFile(const ccstd::string &absPath) {
    auto *fu = FileUtils::getInstance();
    ccstd::string text = fu->getStringFromFile(absPath);
    if (text.empty()) {
        CC_LOG_ERROR("[MaterialLoader] empty / missing file: %s", absPath.c_str());
        return nullptr;
    }

    rapidjson::Document doc;
    doc.Parse(text.c_str(), text.size());
    if (doc.HasParseError()) {
        CC_LOG_ERROR("[MaterialLoader] JSON parse error in %s at offset %zu",
                     absPath.c_str(), (size_t)doc.GetErrorOffset());
        return nullptr;
    }

    // Locate the cc.Material entry (top-level may be an array in authored
    // exports or a single object in hand-crafted fixtures).
    const JsonValue *matObj = nullptr;
    if (doc.IsArray()) {
        for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
            const JsonValue &e = doc[i];
            if (e.IsObject() && e.HasMember("__type__") &&
                e["__type__"].IsString() &&
                std::strcmp(e["__type__"].GetString(), "cc.Material") == 0) {
                matObj = &e;
                break;
            }
        }
    } else if (doc.IsObject()) {
        matObj = &doc;
    }
    if (!matObj) {
        CC_LOG_ERROR("[MaterialLoader] no cc.Material entry in %s", absPath.c_str());
        return nullptr;
    }

    // ── Effect + technique ────────────────────────────────────────────────
    IMaterialInfo info;
    if (matObj->HasMember("_effectAsset")) {
        auto effect = resolveEffectAsset((*matObj)["_effectAsset"]);
        if (!effect) {
            CC_LOG_ERROR("[MaterialLoader] could not resolve _effectAsset in %s",
                         absPath.c_str());
            return nullptr;
        }
        info.effectAsset = effect.get();
    } else if (matObj->HasMember("_effectName") &&
               (*matObj)["_effectName"].IsString()) {
        info.effectName = (*matObj)["_effectName"].GetString();
    } else {
        CC_LOG_ERROR("[MaterialLoader] no effect specified in %s", absPath.c_str());
        return nullptr;
    }

    if (matObj->HasMember("_techIdx") && (*matObj)["_techIdx"].IsInt()) {
        info.technique = static_cast<uint32_t>((*matObj)["_techIdx"].GetInt());
    }

    // ── Defines ────────────────────────────────────────────────────────────
    // Creator stores `_defines` as `[{...}, {...}]` (one record per pass).
    // We accept both that and a single top-level object for convenience.
    if (matObj->HasMember("_defines")) {
        const JsonValue &defs = (*matObj)["_defines"];
        if (defs.IsArray()) {
            if (defs.Size() == 1) {
                info.defines = IMaterialInfo::DefinesType{parseDefines(defs[0])};
            } else if (defs.Size() > 1) {
                ccstd::vector<MacroRecord> vec;
                vec.reserve(defs.Size());
                for (rapidjson::SizeType i = 0; i < defs.Size(); ++i) {
                    vec.emplace_back(parseDefines(defs[i]));
                }
                info.defines = IMaterialInfo::DefinesType{vec};
            }
        } else if (defs.IsObject()) {
            info.defines = IMaterialInfo::DefinesType{parseDefines(defs)};
        }
    }

    auto *mat = ccnew Material();
    mat->initialize(info);

    // ── Props (post-initialize) ───────────────────────────────────────────
    // `_props` is always an array, one entry per pass. CC_INVALID_INDEX means
    // "apply to all passes" which is what we want when size == 1.
    if (matObj->HasMember("_props") && (*matObj)["_props"].IsArray()) {
        const JsonValue &propsArr = (*matObj)["_props"];
        for (rapidjson::SizeType pi = 0; pi < propsArr.Size(); ++pi) {
            const JsonValue &pass = propsArr[pi];
            if (!pass.IsObject()) continue;
            const int passIdx = (propsArr.Size() == 1)
                                    ? static_cast<int>(CC_INVALID_INDEX)
                                    : static_cast<int>(pi);
            for (auto it = pass.MemberBegin(); it != pass.MemberEnd(); ++it) {
                applyProp(mat, it->name.GetString(), it->value, passIdx);
            }
        }
    }

    return mat;
}

}  // namespace cc
