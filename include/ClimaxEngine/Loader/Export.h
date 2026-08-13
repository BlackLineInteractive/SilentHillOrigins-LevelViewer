#pragma once
#include <string>

struct GlbExportOptions {
    bool embedTextures       = true;   // write each texture into the .glb as a PNG
    bool includeLights       = true;   // emit CColorLight objects via KHR_lights_punctual
    bool includeVertexColors = true;
    bool bakeInstances       = true;   // duplicate model geometry per placement
    bool accurateAlpha       = true;   // match viewer alpha (OPAQUE / MASK 0.02 / BLEND / additive)
    bool exportUvAnimations  = true;   // emit UV animation tracks & KHR_texture_transform
    bool bakeCurrentUvAnim   = false;  // bake active UV animation directly into vertex UV coordinates
    float uvAnimTime         = 0.0f;   // time used when baking UV animation
    bool unlitMaterials      = true;   // emit KHR_materials_unlit for unlit geometry & additive effects
};

// Writes the loaded scene to a binary glTF 2.0 file.
//
// Geometry is grouped by texture name: every mesh chunk that uses "sofa" ends up
// in one glTF mesh called "sofa" with one material. That is what makes the export
// usable in a DCC tool — one object per material instead of 1800 loose pieces.
//
// Returns false and fills `error` on failure.
bool ExportGLB(const std::string& path, const GlbExportOptions& opt, std::string& error);
