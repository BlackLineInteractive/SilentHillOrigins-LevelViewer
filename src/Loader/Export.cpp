#include "ClimaxEngine/Loader/Export.h"
#include "ClimaxEngine/Core/Common.h"
#include "ClimaxEngine/SG/SceneObject.h"
#include "ClimaxEngine/Viewer/ViewerDraw.h"
#include <GL/glew.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>
#include <zlib.h>

namespace fs = std::filesystem;
using ClimaxEngine::Viewer::EvalUVAnim;

namespace {

// ── Minimal PNG writer ──────────────────────────────────────────────────────
// Only RGBA8, no interlacing, filter 0 on every scanline. zlib is already a
// dependency (the archive payloads use it), so this avoids pulling in stb.

void PutBE32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24)); out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));  out.push_back((uint8_t)v);
}

void PngChunk(std::vector<uint8_t>& out, const char tag[4],
              const uint8_t* data, size_t len) {
    PutBE32(out, (uint32_t)len);
    const size_t crcStart = out.size();
    out.insert(out.end(), tag, tag + 4);
    if (len && data) {
        out.insert(out.end(), data, data + len);
    }
    const uLong c = crc32(0, out.data() + crcStart, (uInt)(4 + len));
    PutBE32(out, (uint32_t)c);
}

std::vector<uint8_t> EncodePNG(const uint8_t* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return {};
    std::vector<uint8_t> png;
    static const uint8_t SIG[8] = {0x89,'P','N','G','\r','\n',0x1A,'\n'};
    png.insert(png.end(), SIG, SIG + 8);

    uint8_t ihdr[13];
    ihdr[0] = (uint8_t)(w >> 24); ihdr[1] = (uint8_t)(w >> 16);
    ihdr[2] = (uint8_t)(w >> 8);  ihdr[3] = (uint8_t)w;
    ihdr[4] = (uint8_t)(h >> 24); ihdr[5] = (uint8_t)(h >> 16);
    ihdr[6] = (uint8_t)(h >> 8);  ihdr[7] = (uint8_t)h;
    ihdr[8] = 8;    // bit depth
    ihdr[9] = 6;    // colour type: RGBA
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    PngChunk(png, "IHDR", ihdr, sizeof(ihdr));

    // Raw scanlines, each prefixed with filter byte 0.
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * ((size_t)w * 4 + 1));
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + (size_t)y * w * 4, rgba + (size_t)(y + 1) * w * 4);
    }

    uLongf bound = compressBound((uLong)raw.size());
    std::vector<uint8_t> comp(bound);
    if (compress2(comp.data(), &bound, raw.data(), (uLong)raw.size(), 6) != Z_OK)
        return {};
    comp.resize(bound);
    PngChunk(png, "IDAT", comp.data(), comp.size());
    PngChunk(png, "IEND", nullptr, 0);
    return png;
}

// ── glTF buffer accumulation ────────────────────────────────────────────────

struct Blob {
    std::vector<uint8_t> data;

    // Appends `len` bytes 4-byte aligned and returns the starting offset.
    size_t Append(const void* src, size_t len) {
        while (data.size() % 4) data.push_back(0);
        const size_t off = data.size();
        const uint8_t* p = (const uint8_t*)src;
        if (len && p) {
            data.insert(data.end(), p, p + len);
        }
        return off;
    }
};

std::string JsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) { /* drop control bytes */ }
                else o += c;
        }
    }
    return o;
}

// Reads a GL texture back into RGBA8.
bool ReadTexture(GLuint id, int w, int h, std::vector<uint8_t>& out) {
    if (!id || w <= 0 || h <= 0) return false;
    out.assign((size_t)w * h * 4, 0);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    return glGetError() == GL_NO_ERROR;
}

const TexPreviewInfo* FindTexInfo(const std::string& name) {
    if (name.empty() || name == "NULL") return nullptr;
    auto it = g_TexInfo.find(name);
    if (it != g_TexInfo.end()) return &it->second;
    std::string up = name;
    for (auto& c : up) c = (char)toupper((unsigned char)c);
    it = g_TexInfo.find(up);
    if (it != g_TexInfo.end()) return &it->second;
    std::string lo = name;
    for (auto& c : lo) c = (char)tolower((unsigned char)c);
    it = g_TexInfo.find(lo);
    if (it != g_TexInfo.end()) return &it->second;
    return nullptr;
}

uint32_t PackColor(const glm::vec4& c) {
    uint8_t r = (uint8_t)std::clamp(c.r * 255.0f + 0.5f, 0.0f, 255.0f);
    uint8_t g = (uint8_t)std::clamp(c.g * 255.0f + 0.5f, 0.0f, 255.0f);
    uint8_t b = (uint8_t)std::clamp(c.b * 255.0f + 0.5f, 0.0f, 255.0f);
    uint8_t a = (uint8_t)std::clamp(c.a * 255.0f + 0.5f, 0.0f, 255.0f);
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

// Material key distinguishing distinct shader & blending properties
struct MaterialKey {
    std::string texName;
    std::string uvAnimName;
    uint32_t blendMode = 0;
    bool untextured = false;
    bool unlitGeometry = false;
    bool additive = false;
    uint32_t colorRgba = 0xFFFFFFFF;

    bool operator<(const MaterialKey& o) const {
        return std::tie(texName, uvAnimName, blendMode, untextured, unlitGeometry, additive, colorRgba) <
               std::tie(o.texName, o.uvAnimName, o.blendMode, o.untextured, o.unlitGeometry, o.additive, o.colorRgba);
    }
};

enum class GltfAlphaMode {
    Opaque,
    Mask,
    Blend
};

GltfAlphaMode ClassifyAlphaMode(const MaterialKey& mat, const GlbExportOptions& opt) {
    if (!opt.accurateAlpha) return GltfAlphaMode::Mask;

    const uint32_t blend = mat.blendMode & 0xFFFF;
    // Blend mode 3 (NONE) ignores alpha channel completely (opaque pass in viewer)
    if (blend == 3) return GltfAlphaMode::Opaque;

    // Blend mode 1 (Additive) and 2 (Subtractive) require transparency blending
    if (blend == 1 || blend == 2 || mat.additive) return GltfAlphaMode::Blend;

    if (mat.untextured) {
        if (((mat.colorRgba >> 24) & 0xFF) < 250) return GltfAlphaMode::Blend;
        return GltfAlphaMode::Opaque;
    }

    const std::string& tn = mat.texName;
    auto itG = g_TexGradient.find(tn);
    if (itG != g_TexGradient.end() && itG->second) return GltfAlphaMode::Blend;

    auto itO = g_TexOpaque.find(tn);
    if (itO != g_TexOpaque.end() && itO->second) return GltfAlphaMode::Opaque;

    // By default, textured surfaces with binary punchthrough transparency use MASK
    return GltfAlphaMode::Mask;
}

} // namespace

bool ExportGLB(const std::string& path, const GlbExportOptions& opt, std::string& error) {
    error.clear();
    
    auto& registrar = ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance();
    const auto& objects = registrar.GetObjects();
    if (objects.empty()) { error = "nothing loaded"; return false; }

    // ── 1. Group geometry by full MaterialKey ────────────────────────────────
    struct Group {
        MaterialKey matKey;
        std::vector<float> pos, uv;
        std::vector<uint8_t> col;
        float bbMin[3] = { 1e30f,  1e30f,  1e30f};
        float bbMax[3] = {-1e30f, -1e30f, -1e30f};
    };
    std::map<MaterialKey, Group> groups;

    for (auto& obj : objects) {
        glm::mat4 xform = obj->GetTransform();
        for (MeshChunk* chunk : obj->GetMeshes()) {
            if (!chunk || chunk->vertices.empty()) continue;

            MaterialKey mk;
            mk.texName = chunk->texName.empty() ? "NULL" : chunk->texName;
            mk.uvAnimName = chunk->uvAnimName;
            mk.blendMode = chunk->blendMode;
            mk.untextured = chunk->untextured || mk.texName == "NULL";
            mk.unlitGeometry = chunk->unlitGeometry;
            mk.additive = chunk->additive;
            mk.colorRgba = PackColor(chunk->matColor);

            Group& g = groups[mk];
            g.matKey = mk;

            // Optional UV animation evaluation for vertex baking
            glm::vec4 bakedUvXform(1.0f, 1.0f, 0.0f, 0.0f);
            const bool doBakeUv = opt.bakeCurrentUvAnim && !chunk->uvAnimName.empty();
            if (doBakeUv) {
                auto itA = g_UVAnims.find(chunk->uvAnimName);
                if (itA != g_UVAnims.end()) {
                    bakedUvXform = EvalUVAnim(itA->second, 0, opt.uvAnimTime);
                }
            }

            for (const auto& v : chunk->vertices) {
                const glm::vec4 wp = xform * glm::vec4(v.pos, 1.0f);
                float vx = wp.x, vy = wp.y, vz = wp.z;
                g.pos.push_back(vx); g.pos.push_back(vy); g.pos.push_back(vz);

                if (doBakeUv) {
                    float tu = v.uv.x * bakedUvXform.x + bakedUvXform.z;
                    float tv = v.uv.y * bakedUvXform.y + bakedUvXform.w;
                    g.uv.push_back(tu); g.uv.push_back(tv);
                } else {
                    g.uv.push_back(v.uv.x); g.uv.push_back(v.uv.y);
                }

                for (int c = 0; c < 4; c++) {
                    float f = v.color[c];
                    f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
                    g.col.push_back((uint8_t)(f * 255.0f + 0.5f));
                }

                const float p3[3] = { wp.x, wp.y, wp.z };
                for (int c = 0; c < 3; c++) {
                    if (p3[c] < g.bbMin[c]) g.bbMin[c] = p3[c];
                    if (p3[c] > g.bbMax[c]) g.bbMax[c] = p3[c];
                }
            }
        }
    }

    for (auto it = groups.begin(); it != groups.end(); ) {
        if (it->second.pos.empty()) it = groups.erase(it);
        else ++it;
    }
    if (groups.empty()) { error = "no geometry to export"; return false; }

    // ── 2. Build the binary buffer + JSON arrays ────────────────────────────
    Blob bin;
    std::ostringstream jViews, jAccessors, jMeshes, jNodes, jMaterials;
    std::ostringstream jImages, jTextures, jSamplers, jLights, jSceneNodes, jAnimations;
    int nViews = 0, nAcc = 0, nMesh = 0, nNode = 0, nMat = 0, nImg = 0, nTex = 0, nAnim = 0;

    auto comma = [](std::ostringstream& s, int n) { if (n) s << ","; };

    // One sampler: repeat wrapping, linear filtering.
    jSamplers << R"({"magFilter":9729,"minFilter":9729,"wrapS":10497,"wrapT":10497})";

    // Collect all unique textures needed by materials
    std::map<std::string, int> texIndex;
    if (opt.embedTextures) {
        for (const auto& [mk, g] : groups) {
            if (mk.untextured || mk.texName.empty() || mk.texName == "NULL") continue;
            if (texIndex.count(mk.texName)) continue;

            const TexPreviewInfo* ti = FindTexInfo(mk.texName);
            if (!ti) continue;
            std::vector<uint8_t> rgba;
            if (!ReadTexture(ti->glID, ti->width, ti->height, rgba)) continue;
            std::vector<uint8_t> png = EncodePNG(rgba.data(), ti->width, ti->height);
            if (png.empty()) continue;

            const size_t off = bin.Append(png.data(), png.size());
            comma(jViews, nViews);
            jViews << "{\"buffer\":0,\"byteOffset\":" << off
                   << ",\"byteLength\":" << png.size() << "}";
            const int viewIdx = nViews++;

            comma(jImages, nImg);
            jImages << "{\"bufferView\":" << viewIdx << ",\"mimeType\":\"image/png\""
                    << ",\"name\":\"" << JsonEscape(mk.texName) << "\"}";
            comma(jTextures, nTex);
            jTextures << "{\"sampler\":0,\"source\":" << nImg << "}";
            texIndex[mk.texName] = nTex;
            nImg++; nTex++;
        }
    }

    bool usedUnlit = false;
    bool usedTexTransform = false;
    bool usedAnimPointer = false;

    // Track materials with UV animations to generate glTF animations
    struct MatUvAnimTarget {
        int matIndex = 0;
        std::string uvAnimName;
    };
    std::vector<MatUvAnimTarget> animTargets;

    // Meshes and materials: one per unique MaterialKey group.
    for (const auto& [mk, g] : groups) {
        const size_t count = g.pos.size() / 3;

        const size_t offP = bin.Append(g.pos.data(), g.pos.size() * 4);
        comma(jViews, nViews);
        jViews << "{\"buffer\":0,\"byteOffset\":" << offP
               << ",\"byteLength\":" << g.pos.size() * 4 << ",\"target\":34962}";
        const int vP = nViews++;

        const size_t offT = bin.Append(g.uv.data(), g.uv.size() * 4);
        comma(jViews, nViews);
        jViews << "{\"buffer\":0,\"byteOffset\":" << offT
               << ",\"byteLength\":" << g.uv.size() * 4 << ",\"target\":34962}";
        const int vT = nViews++;

        int vC = -1;
        if (opt.includeVertexColors) {
            const size_t offC = bin.Append(g.col.data(), g.col.size());
            comma(jViews, nViews);
            jViews << "{\"buffer\":0,\"byteOffset\":" << offC
                   << ",\"byteLength\":" << g.col.size() << ",\"target\":34962}";
            vC = nViews++;
        }

        comma(jAccessors, nAcc);
        jAccessors << "{\"bufferView\":" << vP << ",\"componentType\":5126,\"count\":" << count
                   << ",\"type\":\"VEC3\",\"min\":[" << g.bbMin[0] << "," << g.bbMin[1] << ","
                   << g.bbMin[2] << "],\"max\":[" << g.bbMax[0] << "," << g.bbMax[1] << ","
                   << g.bbMax[2] << "]}";
        const int aP = nAcc++;

        comma(jAccessors, nAcc);
        jAccessors << "{\"bufferView\":" << vT << ",\"componentType\":5126,\"count\":" << count
                   << ",\"type\":\"VEC2\"}";
        const int aT = nAcc++;

        int aC = -1;
        if (vC >= 0) {
            comma(jAccessors, nAcc);
            jAccessors << "{\"bufferView\":" << vC << ",\"componentType\":5121"
                       << ",\"normalized\":true,\"count\":" << count << ",\"type\":\"VEC4\"}";
            aC = nAcc++;
        }

        // ── Material definition ─────────────────────────────────────────────
        const int matIdx = nMat++;
        const GltfAlphaMode alphaMode = ClassifyAlphaMode(mk, opt);

        std::string matLabel = mk.texName;
        if (matLabel == "NULL" || matLabel.empty()) matLabel = "untextured";
        if (mk.blendMode == 1 || mk.additive) matLabel += "_add";
        else if (mk.blendMode == 2) matLabel += "_sub";
        else if (mk.blendMode == 3) matLabel += "_opaque";
        if (!mk.uvAnimName.empty()) matLabel += "_" + mk.uvAnimName;

        comma(jMaterials, matIdx);
        jMaterials << "{\"name\":\"" << JsonEscape(matLabel) << "\",\"doubleSided\":true";

        if (alphaMode == GltfAlphaMode::Mask) {
            jMaterials << ",\"alphaMode\":\"MASK\",\"alphaCutoff\":0.02";
        } else if (alphaMode == GltfAlphaMode::Blend) {
            jMaterials << ",\"alphaMode\":\"BLEND\"";
        } else {
            jMaterials << ",\"alphaMode\":\"OPAQUE\"";
        }

        // Material extensions (e.g. KHR_materials_unlit)
        const bool isUnlit = opt.unlitMaterials &&
            (mk.unlitGeometry || mk.blendMode == 1 || mk.additive);
        if (isUnlit) {
            usedUnlit = true;
            jMaterials << ",\"extensions\":{\"KHR_materials_unlit\":{}}";
        }

        jMaterials << ",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1";

        // Base color factor
        if (mk.untextured || mk.colorRgba != 0xFFFFFFFF) {
            float r = ((mk.colorRgba >> 0)  & 0xFF) / 255.0f;
            float g = ((mk.colorRgba >> 8)  & 0xFF) / 255.0f;
            float b = ((mk.colorRgba >> 16) & 0xFF) / 255.0f;
            float a = ((mk.colorRgba >> 24) & 0xFF) / 255.0f;
            jMaterials << ",\"baseColorFactor\":[" << r << "," << g << "," << b << "," << a << "]";
        }

        auto ti = texIndex.find(mk.texName);
        if (!mk.untextured && ti != texIndex.end()) {
            jMaterials << ",\"baseColorTexture\":{\"index\":" << ti->second;

            // KHR_texture_transform for UV animations
            if (opt.exportUvAnimations && !mk.uvAnimName.empty() && g_UVAnims.count(mk.uvAnimName)) {
                usedTexTransform = true;
                glm::vec4 initUv = EvalUVAnim(g_UVAnims[mk.uvAnimName], 0, 0.0f);
                jMaterials << ",\"extensions\":{\"KHR_texture_transform\":{"
                           << "\"offset\":[" << initUv.z << "," << initUv.w << "],"
                           << "\"scale\":[" << initUv.x << "," << initUv.y << "]"
                           << "}}";
                animTargets.push_back({ matIdx, mk.uvAnimName });
            }
            jMaterials << "}";
        }
        jMaterials << "}}";

        // ── Mesh & Node ─────────────────────────────────────────────────────
        comma(jMeshes, nMesh);
        jMeshes << "{\"name\":\"" << JsonEscape(matLabel) << "\",\"primitives\":[{\"attributes\":{"
                << "\"POSITION\":" << aP << ",\"TEXCOORD_0\":" << aT;
        if (aC >= 0) jMeshes << ",\"COLOR_0\":" << aC;
        jMeshes << "},\"material\":" << matIdx << ",\"mode\":4}]}";

        comma(jNodes, nNode);
        jNodes << "{\"name\":\"" << JsonEscape(matLabel) << "\",\"mesh\":" << nMesh << "}";
        comma(jSceneNodes, nNode);
        jSceneNodes << nNode;
        nMesh++; nNode++;
    }

    // ── 3. UV Animations via KHR_animation_pointer ──────────────────────────
    if (opt.exportUvAnimations && !animTargets.empty()) {
        for (const auto& target : animTargets) {
            auto itA = g_UVAnims.find(target.uvAnimName);
            if (itA == g_UVAnims.end() || itA->second.layers.empty()) continue;
            const auto& keys = itA->second.layers[0];
            if (keys.empty()) continue;

            const size_t numKeys = keys.size();
            std::vector<float> times;
            std::vector<float> offsets; // vec2 uOff, vOff
            std::vector<float> scales;  // vec2 uScale, vScale

            for (const auto& k : keys) {
                times.push_back(k.time);
                offsets.push_back(k.uOff);
                offsets.push_back(k.vOff);
                scales.push_back(k.uScale);
                scales.push_back(k.vScale);
            }

            const float minT = times.front();
            const float maxT = times.back();

            const size_t offTimes = bin.Append(times.data(), times.size() * sizeof(float));
            comma(jViews, nViews);
            jViews << "{\"buffer\":0,\"byteOffset\":" << offTimes
                   << ",\"byteLength\":" << times.size() * sizeof(float) << "}";
            const int vTimes = nViews++;

            const size_t offOffs = bin.Append(offsets.data(), offsets.size() * sizeof(float));
            comma(jViews, nViews);
            jViews << "{\"buffer\":0,\"byteOffset\":" << offOffs
                   << ",\"byteLength\":" << offsets.size() * sizeof(float) << "}";
            const int vOffs = nViews++;

            const size_t offScls = bin.Append(scales.data(), scales.size() * sizeof(float));
            comma(jViews, nViews);
            jViews << "{\"buffer\":0,\"byteOffset\":" << offScls
                   << ",\"byteLength\":" << scales.size() * sizeof(float) << "}";
            const int vScls = nViews++;

            comma(jAccessors, nAcc);
            jAccessors << "{\"bufferView\":" << vTimes << ",\"componentType\":5126,\"count\":" << numKeys
                       << ",\"type\":\"SCALAR\",\"min\":[" << minT << "],\"max\":[" << maxT << "]}";
            const int aTimes = nAcc++;

            comma(jAccessors, nAcc);
            jAccessors << "{\"bufferView\":" << vOffs << ",\"componentType\":5126,\"count\":" << numKeys
                       << ",\"type\":\"VEC2\"}";
            const int aOffs = nAcc++;

            comma(jAccessors, nAcc);
            jAccessors << "{\"bufferView\":" << vScls << ",\"componentType\":5126,\"count\":" << numKeys
                       << ",\"type\":\"VEC2\"}";
            const int aScls = nAcc++;

            usedAnimPointer = true;
            comma(jAnimations, nAnim);
            jAnimations << "{\"name\":\"" << JsonEscape("UVAnim_" + target.uvAnimName) << "\",\"samplers\":["
                        << "{\"input\":" << aTimes << ",\"interpolation\":\"LINEAR\",\"output\":" << aOffs << "},"
                        << "{\"input\":" << aTimes << ",\"interpolation\":\"LINEAR\",\"output\":" << aScls << "}"
                        << "],\"channels\":["
                        << "{\"sampler\":0,\"target\":{\"extensions\":{\"KHR_animation_pointer\":{\"pointer\":"
                        << "\"/materials/" << target.matIndex << "/pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/offset\"}}}},"
                        << "{\"sampler\":1,\"target\":{\"extensions\":{\"KHR_animation_pointer\":{\"pointer\":"
                        << "\"/materials/" << target.matIndex << "/pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/scale\"}}}}"
                        << "]}";
            nAnim++;
        }
    }

    // ── 4. Lights, as KHR_lights_punctual ───────────────────────────────────
    int nLights = 0;
    if (opt.includeLights) {
        for (const auto& go : g_GameObjects) {
            if (!go.isLight) continue;
            const bool spot = go.lightAngle > 0.0f && go.lightAngle < 180.0f;
            const float range = go.lightRange > 0.0f ? go.lightRange : 10.0f;
            const float cr = std::clamp(go.lightColor.r, 0.0f, 1.0f);
            const float cg = std::clamp(go.lightColor.g, 0.0f, 1.0f);
            const float cb = std::clamp(go.lightColor.b, 0.0f, 1.0f);

            comma(jLights, nLights);
            jLights << "{\"type\":\"" << (spot ? "spot" : "point") << "\",\"color\":["
                    << cr << "," << cg << "," << cb
                    << "],\"intensity\":1,\"range\":" << range;
            if (spot) {
                const double half = go.lightAngle * 0.5 * 3.14159265358979 / 180.0;
                jLights << ",\"spot\":{\"innerConeAngle\":0,\"outerConeAngle\":" << half << "}";
            }
            jLights << ",\"name\":\"" << JsonEscape(go.label) << "\"}";

            const glm::mat4& m = go.transform;
            comma(jNodes, nNode);
            jNodes << "{\"name\":\"" << JsonEscape(go.label) << "\",\"matrix\":[";
            for (int c = 0; c < 4; c++)
                for (int r = 0; r < 4; r++)
                    jNodes << (c || r ? "," : "") << m[c][r];
            jNodes << "],\"extensions\":{\"KHR_lights_punctual\":{\"light\":" << nLights << "}}}";
            comma(jSceneNodes, nNode);
            jSceneNodes << nNode;
            nNode++; nLights++;
        }
    }

    // ── 5. Assemble the JSON chunk ──────────────────────────────────────────
    std::ostringstream j;
    j << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"ClimaxGameEngineToolkit\"}";

    std::vector<std::string> extsUsed;
    if (nLights) extsUsed.push_back("\"KHR_lights_punctual\"");
    if (usedUnlit) extsUsed.push_back("\"KHR_materials_unlit\"");
    if (usedTexTransform) extsUsed.push_back("\"KHR_texture_transform\"");
    if (usedAnimPointer) extsUsed.push_back("\"KHR_animation_pointer\"");

    if (!extsUsed.empty()) {
        j << ",\"extensionsUsed\":[";
        for (size_t k = 0; k < extsUsed.size(); k++) {
            if (k) j << ",";
            j << extsUsed[k];
        }
        j << "]";
    }

    if (nLights) {
        j << ",\"extensions\":{\"KHR_lights_punctual\":{\"lights\":[" << jLights.str() << "]}}";
    }

    j << ",\"scene\":0,\"scenes\":[{\"nodes\":[" << jSceneNodes.str() << "]}]"
      << ",\"nodes\":["     << jNodes.str()     << "]"
      << ",\"meshes\":["    << jMeshes.str()    << "]"
      << ",\"materials\":[" << jMaterials.str() << "]";
    if (nImg) {
        j << ",\"images\":["   << jImages.str()   << "]"
          << ",\"samplers\":[" << jSamplers.str() << "]"
          << ",\"textures\":[" << jTextures.str() << "]";
    }
    if (nAnim) {
        j << ",\"animations\":[" << jAnimations.str() << "]";
    }
    j << ",\"accessors\":["   << jAccessors.str() << "]"
      << ",\"bufferViews\":[" << jViews.str()     << "]"
      << ",\"buffers\":[{\"byteLength\":" << bin.data.size() << "}]}";

    std::string json = j.str();
    while (json.size() % 4) json += ' ';
    while (bin.data.size() % 4) bin.data.push_back(0);

    // ── 6. Write the GLB container safely ───────────────────────────────────
    std::string writePath = path;
    if (writePath.empty()) writePath = "scene.glb";

    std::ofstream f(writePath, std::ios::binary);
    if (!f) {
        // If relative write failed, try writing to user's desktop as fallback
#if defined(_WIN32)
        const char* home = std::getenv("USERPROFILE");
        std::string fb = home ? std::string(home) + "\\Desktop\\" : "";
#else
        const char* home = std::getenv("HOME");
        std::string fb = home ? std::string(home) + "/Desktop/" : "";
#endif
        if (!fb.empty()) {
            std::string alt = fb + fs::path(writePath).filename().string();
            f.open(alt, std::ios::binary);
            if (f) {
                std::cout << "[export] note: wrote to " << alt << " (fallback from " << writePath << ")\n";
                writePath = alt;
            }
        }
    }

    if (!f) {
        error = "cannot open file for writing: " + writePath;
        return false;
    }

    const uint32_t total = 12 + 8 + (uint32_t)json.size() + 8 + (uint32_t)bin.data.size();
    auto w32 = [&](uint32_t v) { f.write((const char*)&v, 4); };

    f.write("glTF", 4); w32(2); w32(total);
    w32((uint32_t)json.size()); w32(0x4E4F534A);      // 'JSON'
    f.write(json.data(), (std::streamsize)json.size());
    w32((uint32_t)bin.data.size()); w32(0x004E4942);  // 'BIN\0'
    f.write((const char*)bin.data.data(), (std::streamsize)bin.data.size());

    if (!f) { error = "write failed during stream transfer"; return false; }
    return true;
}
