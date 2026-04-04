/****************************************************************************
 Copyright (c) 2024 Xiamen Yaji Software Co., Ltd.
 http://www.cocos.com
 ****************************************************************************/

#include "core/builtin/BuiltinEffectLoader.h"

#include "base/Log.h"
#include "core/assets/EffectAsset.h"
#include "platform/FileUtils.h"
#include "rapidjson/document.h"

namespace cc {

namespace {

// NOTE: do NOT use "using namespace rapidjson" here — cc::Value would conflict.
using JsonValue = rapidjson::Value;

// ---- helpers ----

ccstd::string getString(const JsonValue &v, const char *key, const char *def = "") {
    if (v.HasMember(key) && v[key].IsString()) return v[key].GetString();
    return def;
}

int32_t getInt(const JsonValue &v, const char *key, int32_t def = 0) {
    if (v.HasMember(key) && v[key].IsInt()) return v[key].GetInt();
    return def;
}

uint32_t getUint(const JsonValue &v, const char *key, uint32_t def = 0) {
    if (v.HasMember(key) && v[key].IsUint()) return v[key].GetUint();
    if (v.HasMember(key) && v[key].IsInt()) return static_cast<uint32_t>(v[key].GetInt());
    return def;
}

bool getBool(const JsonValue &v, const char *key, bool def = false) {
    if (v.HasMember(key) && v[key].IsBool()) return v[key].GetBool();
    return def;
}

ccstd::vector<ccstd::string> getStringArray(const JsonValue &v, const char *key) {
    ccstd::vector<ccstd::string> out;
    if (v.HasMember(key) && v[key].IsArray()) {
        for (auto &e : v[key].GetArray()) {
            if (e.IsString()) out.emplace_back(e.GetString());
        }
    }
    return out;
}

// ---- parse sub-structures ----

IShaderSource parseShaderSource(const JsonValue &v) {
    IShaderSource s;
    s.vert = getString(v, "vert");
    s.frag = getString(v, "frag");
    if (v.HasMember("compute") && v["compute"].IsString()) {
        s.compute = v["compute"].GetString();
    }
    return s;
}

IBuiltin parseBuiltin(const JsonValue &v) {
    IBuiltin b;
    b.name = getString(v, "name");
    b.defines = getStringArray(v, "defines");
    return b;
}

ccstd::vector<IBuiltin> parseBuiltinArray(const JsonValue &v, const char *key) {
    ccstd::vector<IBuiltin> out;
    if (v.HasMember(key) && v[key].IsArray()) {
        for (auto &e : v[key].GetArray()) {
            out.push_back(parseBuiltin(e));
        }
    }
    return out;
}

IBuiltinInfo parseBuiltinInfo(const JsonValue &v) {
    IBuiltinInfo info;
    info.blocks = parseBuiltinArray(v, "blocks");
    info.samplerTextures = parseBuiltinArray(v, "samplerTextures");
    info.buffers = parseBuiltinArray(v, "buffers");
    info.images = parseBuiltinArray(v, "images");
    return info;
}

IBuiltins parseBuiltins(const JsonValue &v) {
    IBuiltins b;
    if (v.HasMember("globals") && v["globals"].IsObject()) {
        b.globals = parseBuiltinInfo(v["globals"]);
    }
    if (v.HasMember("locals") && v["locals"].IsObject()) {
        b.locals = parseBuiltinInfo(v["locals"]);
    }
    if (v.HasMember("statistics") && v["statistics"].IsObject()) {
        for (auto &m : v["statistics"].GetObject()) {
            if (m.value.IsInt()) {
                b.statistics[m.name.GetString()] = m.value.GetInt();
            }
        }
    }
    return b;
}

IDefineInfo parseDefine(const JsonValue &v) {
    IDefineInfo d;
    d.name = getString(v, "name");
    d.type = getString(v, "type", "boolean");
    if (v.HasMember("range") && v["range"].IsArray()) {
        ccstd::vector<int32_t> range;
        for (auto &e : v["range"].GetArray()) {
            range.push_back(e.GetInt());
        }
        d.range = std::move(range);
    }
    if (v.HasMember("options") && v["options"].IsArray()) {
        ccstd::vector<ccstd::string> opts;
        for (auto &e : v["options"].GetArray()) {
            if (e.IsString()) opts.emplace_back(e.GetString());
        }
        d.options = std::move(opts);
    }
    if (v.HasMember("defaultVal")) {
        if (v["defaultVal"].IsString()) {
            d.defaultVal = v["defaultVal"].GetString();
        }
    }
    return d;
}

IAttributeInfo parseAttribute(const JsonValue &v) {
    IAttributeInfo a;
    a.name = getString(v, "name");
    a.format = static_cast<gfx::Format>(getUint(v, "format"));
    a.isNormalized = getBool(v, "isNormalized");
    a.stream = getUint(v, "stream");
    a.isInstanced = getBool(v, "isInstanced");
    a.location = getUint(v, "location");
    a.defines = getStringArray(v, "defines");
    return a;
}

gfx::Uniform parseUniform(const JsonValue &v) {
    gfx::Uniform u;
    u.name = getString(v, "name");
    u.type = static_cast<gfx::Type>(getUint(v, "type"));
    u.count = getUint(v, "count", 1);
    return u;
}

IBlockInfo parseBlock(const JsonValue &v) {
    IBlockInfo b;
    b.binding = getUint(v, "binding");
    b.name = getString(v, "name");
    b.stageFlags = static_cast<gfx::ShaderStageFlagBit>(getUint(v, "stageFlags", 1));
    b.defines = getStringArray(v, "defines");
    if (v.HasMember("members") && v["members"].IsArray()) {
        for (auto &m : v["members"].GetArray()) {
            b.members.push_back(parseUniform(m));
        }
    }
    return b;
}

ISamplerTextureInfo parseSamplerTexture(const JsonValue &v) {
    ISamplerTextureInfo st;
    st.binding = getUint(v, "binding");
    st.name = getString(v, "name");
    st.type = static_cast<gfx::Type>(getUint(v, "type"));
    st.count = getUint(v, "count", 1);
    st.stageFlags = static_cast<gfx::ShaderStageFlagBit>(getUint(v, "stageFlags", 16));
    st.defines = getStringArray(v, "defines");
    return st;
}

ISamplerInfo parseSampler(const JsonValue &v) {
    ISamplerInfo s;
    s.binding = getUint(v, "binding");
    s.name = getString(v, "name");
    s.count = getUint(v, "count", 1);
    s.stageFlags = static_cast<gfx::ShaderStageFlagBit>(getUint(v, "stageFlags", 16));
    return s;
}

ITextureInfo parseTexture(const JsonValue &v) {
    ITextureInfo t;
    t.binding = getUint(v, "binding");
    t.name = getString(v, "name");
    t.type = static_cast<gfx::Type>(getUint(v, "type"));
    t.count = getUint(v, "count", 1);
    t.stageFlags = static_cast<gfx::ShaderStageFlagBit>(getUint(v, "stageFlags", 16));
    return t;
}

IBufferInfo parseBuffer(const JsonValue &v) {
    IBufferInfo b;
    b.binding = getUint(v, "binding");
    b.name = getString(v, "name");
    b.stageFlags = static_cast<gfx::ShaderStageFlagBit>(getUint(v, "stageFlags"));
    return b;
}

IImageInfo parseImage(const JsonValue &v) {
    IImageInfo img;
    img.binding = getUint(v, "binding");
    img.name = getString(v, "name");
    img.type = static_cast<gfx::Type>(getUint(v, "type"));
    img.count = getUint(v, "count", 1);
    img.stageFlags = static_cast<gfx::ShaderStageFlagBit>(getUint(v, "stageFlags"));
    img.memoryAccess = static_cast<gfx::MemoryAccessBit>(getUint(v, "memoryAccess"));
    return img;
}

IInputAttachmentInfo parseSubpassInput(const JsonValue &v) {
    IInputAttachmentInfo si;
    si.binding = getUint(v, "binding");
    si.name = getString(v, "name");
    si.count = getUint(v, "count", 1);
    si.stageFlags = static_cast<gfx::ShaderStageFlagBit>(getUint(v, "stageFlags"));
    return si;
}

template <typename T, typename ParseFn>
ccstd::vector<T> parseArray(const JsonValue &parent, const char *key, ParseFn fn) {
    ccstd::vector<T> out;
    if (parent.HasMember(key) && parent[key].IsArray()) {
        for (auto &e : parent[key].GetArray()) {
            out.push_back(fn(e));
        }
    }
    return out;
}

IShaderInfo parseShader(const JsonValue &v) {
    IShaderInfo s;
    s.name = getString(v, "name");
    s.hash = static_cast<ccstd::hash_t>(getUint(v, "hash"));
    if (v.HasMember("glsl4") && v["glsl4"].IsObject()) s.glsl4 = parseShaderSource(v["glsl4"]);
    if (v.HasMember("glsl3") && v["glsl3"].IsObject()) s.glsl3 = parseShaderSource(v["glsl3"]);
    if (v.HasMember("glsl1") && v["glsl1"].IsObject()) s.glsl1 = parseShaderSource(v["glsl1"]);
    if (v.HasMember("builtins") && v["builtins"].IsObject()) s.builtins = parseBuiltins(v["builtins"]);
    s.defines = parseArray<IDefineInfo>(v, "defines", parseDefine);
    s.attributes = parseArray<IAttributeInfo>(v, "attributes", parseAttribute);
    s.blocks = parseArray<IBlockInfo>(v, "blocks", parseBlock);
    s.samplerTextures = parseArray<ISamplerTextureInfo>(v, "samplerTextures", parseSamplerTexture);
    s.samplers = parseArray<ISamplerInfo>(v, "samplers", parseSampler);
    s.textures = parseArray<ITextureInfo>(v, "textures", parseTexture);
    s.buffers = parseArray<IBufferInfo>(v, "buffers", parseBuffer);
    s.images = parseArray<IImageInfo>(v, "images", parseImage);
    s.subpassInputs = parseArray<IInputAttachmentInfo>(v, "subpassInputs", parseSubpassInput);
    return s;
}

// Parse pipeline state objects
RasterizerStateInfo parseRasterizerState(const JsonValue &v) {
    RasterizerStateInfo rs;
    if (v.HasMember("cullMode")) rs.cullMode = static_cast<gfx::CullMode>(v["cullMode"].GetInt());
    if (v.HasMember("polygonMode")) rs.polygonMode = static_cast<gfx::PolygonMode>(v["polygonMode"].GetInt());
    if (v.HasMember("isDiscard")) rs.isDiscard = v["isDiscard"].GetBool();
    if (v.HasMember("lineWidth")) rs.lineWidth = v["lineWidth"].GetFloat();
    return rs;
}

DepthStencilStateInfo parseDepthStencilState(const JsonValue &v) {
    DepthStencilStateInfo ds;
    if (v.HasMember("depthTest")) ds.depthTest = v["depthTest"].GetBool();
    if (v.HasMember("depthWrite")) ds.depthWrite = v["depthWrite"].GetBool();
    if (v.HasMember("depthFunc")) ds.depthFunc = static_cast<gfx::ComparisonFunc>(v["depthFunc"].GetInt());
    if (v.HasMember("stencilTestFront")) ds.stencilTestFront = v["stencilTestFront"].GetBool();
    if (v.HasMember("stencilTestBack")) ds.stencilTestBack = v["stencilTestBack"].GetBool();
    return ds;
}

BlendStateInfo parseBlendState(const JsonValue &v) {
    BlendStateInfo bs;
    if (v.HasMember("targets") && v["targets"].IsArray()) {
        ccstd::vector<BlendTargetInfo> targets;
        for (auto &t : v["targets"].GetArray()) {
            BlendTargetInfo bt;
            if (t.HasMember("blend")) bt.blend = t["blend"].GetBool();
            if (t.HasMember("blendSrc")) bt.blendSrc = static_cast<gfx::BlendFactor>(t["blendSrc"].GetInt());
            if (t.HasMember("blendDst")) bt.blendDst = static_cast<gfx::BlendFactor>(t["blendDst"].GetInt());
            if (t.HasMember("blendSrcAlpha")) bt.blendSrcAlpha = static_cast<gfx::BlendFactor>(t["blendSrcAlpha"].GetInt());
            if (t.HasMember("blendDstAlpha")) bt.blendDstAlpha = static_cast<gfx::BlendFactor>(t["blendDstAlpha"].GetInt());
            targets.push_back(bt);
        }
        bs.targets = std::move(targets);
    }
    return bs;
}

IPropertyInfo parseProperty(const JsonValue &v) {
    IPropertyInfo p;
    p.type = getInt(v, "type");
    if (v.HasMember("value")) {
        if (v["value"].IsArray()) {
            ccstd::vector<float> arr;
            for (auto &e : v["value"].GetArray()) {
                if (e.IsNumber()) arr.push_back(e.GetFloat());
            }
            p.value = std::move(arr);
        } else if (v["value"].IsString()) {
            p.value = ccstd::string(v["value"].GetString());
        }
    }
    if (v.HasMember("linear") && v["linear"].IsBool()) {
        p.linear = v["linear"].GetBool();
    }
    return p;
}

IPassInfoFull parsePass(const JsonValue &v) {
    IPassInfoFull pass;
    pass.program = getString(v, "program");
    if (v.HasMember("priority")) pass.priority = v["priority"].GetInt();
    if (v.HasMember("primitive")) pass.primitive = static_cast<gfx::PrimitiveMode>(v["primitive"].GetInt());
    if (v.HasMember("stage")) pass.stage = static_cast<pipeline::RenderPassStage>(v["stage"].GetInt());
    if (v.HasMember("phase") && v["phase"].IsString()) pass.phase = ccstd::string(v["phase"].GetString());
    if (v.HasMember("switch") && v["switch"].IsString()) pass.switch_ = ccstd::string(v["switch"].GetString());
    if (v.HasMember("rasterizerState") && v["rasterizerState"].IsObject()) {
        pass.rasterizerState = parseRasterizerState(v["rasterizerState"]);
    }
    if (v.HasMember("depthStencilState") && v["depthStencilState"].IsObject()) {
        pass.depthStencilState = parseDepthStencilState(v["depthStencilState"]);
    }
    if (v.HasMember("blendState") && v["blendState"].IsObject()) {
        pass.blendState = parseBlendState(v["blendState"]);
    }
    if (v.HasMember("dynamicStates") && v["dynamicStates"].IsInt()) {
        pass.dynamicStates = static_cast<gfx::DynamicStateFlagBit>(v["dynamicStates"].GetInt());
    }
    if (v.HasMember("properties") && v["properties"].IsObject()) {
        PassPropertyInfoMap props;
        for (auto &m : v["properties"].GetObject()) {
            if (m.value.IsObject()) {
                props[m.name.GetString()] = parseProperty(m.value);
            }
        }
        pass.properties = std::move(props);
    }
    return pass;
}

ITechniqueInfo parseTechnique(const JsonValue &v) {
    ITechniqueInfo tech;
    if (v.HasMember("name") && v["name"].IsString()) tech.name = v["name"].GetString();
    if (v.HasMember("passes") && v["passes"].IsArray()) {
        for (auto &p : v["passes"].GetArray()) {
            tech.passes.push_back(parsePass(p));
        }
    }
    return tech;
}

} // namespace

int loadBuiltinEffectsFromJson(const ccstd::string &jsonPath) {
    auto *fileUtils = FileUtils::getInstance();
    if (!fileUtils || !fileUtils->isFileExist(jsonPath)) {
        CC_LOG_WARNING("loadBuiltinEffectsFromJson: file not found: %s", jsonPath.c_str());
        return -1;
    }

    ccstd::string content = fileUtils->getStringFromFile(jsonPath);
    if (content.empty()) {
        CC_LOG_WARNING("loadBuiltinEffectsFromJson: empty file: %s", jsonPath.c_str());
        return -1;
    }

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError() || !doc.IsArray()) {
        CC_LOG_ERROR("loadBuiltinEffectsFromJson: JSON parse error in %s", jsonPath.c_str());
        return -1;
    }

    int count = 0;
    for (auto &effectVal : doc.GetArray()) {
        if (!effectVal.IsObject()) continue;

        auto *effect = ccnew EffectAsset();
        effect->setName(getString(effectVal, "name"));
        effect->setUuid(getString(effectVal, "name")); // use name as uuid for builtin effects

        // Parse shaders
        ccstd::vector<IShaderInfo> shaders;
        if (effectVal.HasMember("shaders") && effectVal["shaders"].IsArray()) {
            for (auto &s : effectVal["shaders"].GetArray()) {
                shaders.push_back(parseShader(s));
            }
        }
        effect->setShaders(std::move(shaders));

        // Parse techniques
        ccstd::vector<ITechniqueInfo> techniques;
        if (effectVal.HasMember("techniques") && effectVal["techniques"].IsArray()) {
            for (auto &t : effectVal["techniques"].GetArray()) {
                techniques.push_back(parseTechnique(t));
            }
        }
        effect->setTechniques(std::move(techniques));

        // Register the effect
        effect->onLoaded();
        ++count;
        CC_LOG_INFO("Loaded builtin effect: %s", effect->getName().c_str());
    }

    return count;
}

} // namespace cc
