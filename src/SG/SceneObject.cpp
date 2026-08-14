#include "ClimaxEngine/SG/SceneObject.h"
#include "ClimaxEngine/Render/GPUMesh.h"
#include <GL/glew.h>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

namespace ClimaxEngine {
namespace SG {

std::vector<MeshChunk*> CMeshObject::GetMeshes() {
    std::vector<MeshChunk*> ptrs;
    for (auto& m : m_meshes) {
        ptrs.push_back(&m);
    }
    return ptrs;
}

CSceneObjectRegistrar& CSceneObjectRegistrar::GetInstance() {
    static CSceneObjectRegistrar instance;
    return instance;
}

void CSceneObjectRegistrar::RegisterObject(std::shared_ptr<CSceneObject> obj) {
    if (obj) m_objects.push_back(obj);
}

void CSceneObjectRegistrar::Clear() {
    m_objects.clear();
}

void CWorldObject::SetMatrixAndDraw(const RenderContext& ctx, MeshChunk* chunk) {
    // World space is drawn as is, no custom transformation needed unless we want to shift the whole world
    glUniformMatrix4fv(ctx.uM, 1, GL_FALSE, glm::value_ptr(ctx.viewProj));
    glUniformMatrix4fv(ctx.uModel, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniform1i(ctx.uUseSkinning, 0);

    GpuFor(*chunk).Draw();
}

// PS2 characters are segmented, not vertex-skinned.
//
// Measured, not assumed: the Skin PLG carries the inverse bind matrices and
// then stops -- its tail is 16 bytes of counts and an empty split-data header
// -- and every VIF packet in a character container uses the same four streams,
// positions, UVs, colours and normals, with no bone or weight stream anywhere.
// So each atomic hangs rigidly off one frame, and animating means moving whole
// pieces by their frame's matrix rather than blending vertices.
//
// The rest-pose matrix is already baked into the vertices at load time, so what
void CClumpObject::CrossfadeTo(const AnimClip& newClip, float duration) {
    if (animClip.tracks.empty() || blendWeight < 0.01f) {
        animClip = newClip;
        animTime = 0.0f;
        prevClip = AnimClip{};
        blendWeight = 1.0f;
        return;
    }

    prevClip = animClip;
    prevAnimTime = animTime;
    animClip = newClip;
    animTime = 0.0f;
    blendWeight = 0.0f;
    blendDuration = duration > 0.01f ? duration : 0.22f;
}

void CClumpObject::AdvanceTime(float dt) {
    animTime += dt;
    if (blendWeight < 1.0f) {
        prevAnimTime += dt;
        blendWeight += dt / blendDuration;
        if (blendWeight > 1.0f) blendWeight = 1.0f;
    }
}

static void SampleTrack(const AnimTrack& tr, float time, glm::vec3& outPos, glm::quat& outRot) {
    if (tr.times.empty()) return;
    if (time <= tr.times.front()) {
        outPos = tr.pos.front();
        outRot = tr.rot.front();
        return;
    }
    if (time >= tr.times.back()) {
        outPos = tr.pos.back();
        outRot = tr.rot.back();
        return;
    }
    auto it = std::lower_bound(tr.times.begin(), tr.times.end(), time);
    size_t i1 = (size_t)std::distance(tr.times.begin(), it);
    size_t i0 = i1 - 1;
    float span = tr.times[i1] - tr.times[i0];
    float f = span > 1e-6f ? (time - tr.times[i0]) / span : 0.0f;
    outPos = glm::mix(tr.pos[i0], tr.pos[i1], f);
    outRot = glm::slerp(tr.rot[i0], tr.rot[i1], f);
}

void CClumpObject::SetMatrixAndDraw(const RenderContext& ctx, MeshChunk* chunk) {
    glm::mat4 model = m_transform;
    bool skinned = false;

    const AnimClip* clip = nullptr;
    if (animClip.duration > 0.0f && !animClip.tracks.empty())
        clip = &animClip;
    else if (state.animClipIndex >= 0 &&
             state.animClipIndex < (int)g_AnimClips.size())
        clip = &g_AnimClips[(size_t)state.animClipIndex];

    const bool canSkin = chunk->hasWeights && state.animSkinning;
    const bool haveFrame = chunk->frameIndex >= 0 &&
                           chunk->frameIndex < (int)skeleton.bones.size();


    if (!skeleton.bones.empty() && (canSkin || haveFrame) &&
        (clip || state.animRestPose)) {

        const size_t n = skeleton.bones.size();
        const float dur = clip ? clip->duration : 0.0f;
        const float t = dur > 0.0f ? std::fmod(animTime, dur) : 0.0f;

        const float prevDur = prevClip.tracks.empty() ? 0.0f : prevClip.duration;
        const float prevT = prevDur > 0.0f ? std::fmod(prevAnimTime, prevDur) : 0.0f;

        std::vector<glm::mat4> rest(n), posed(n);
        for (size_t b = 0; b < n; ++b) {
            const Bone& bone = skeleton.bones[b];
            glm::mat4 local = bone.restLocal;

            if (clip && !state.animRestPose && bone.trackIndex >= 0 &&
                bone.trackIndex < (int)clip->tracks.size()) {
                const AnimTrack& tr = clip->tracks[(size_t)bone.trackIndex];
                if (!tr.times.empty()) {
                    glm::vec3 curPos(0.0f);
                    glm::quat curRot(1.0f, 0.0f, 0.0f, 0.0f);
                    SampleTrack(tr, t, curPos, curRot);

                    if (blendWeight < 1.0f && !prevClip.tracks.empty() &&
                        bone.trackIndex < (int)prevClip.tracks.size() &&
                        !prevClip.tracks[(size_t)bone.trackIndex].times.empty()) {
                        glm::vec3 prevPos(0.0f);
                        glm::quat prevRot(1.0f, 0.0f, 0.0f, 0.0f);
                        SampleTrack(prevClip.tracks[(size_t)bone.trackIndex], prevT, prevPos, prevRot);

                        glm::vec3 p = glm::mix(prevPos, curPos, blendWeight);
                        glm::quat q = glm::slerp(prevRot, curRot, blendWeight);
                        local = glm::translate(glm::mat4(1.0f), p) * glm::mat4_cast(q);
                    } else {
                        local = glm::translate(glm::mat4(1.0f), curPos) * glm::mat4_cast(curRot);
                    }
                }
            }

            const int parent = bone.parent;
            const bool ok = parent >= 0 && parent < (int)b;
            rest[b]  = ok ? rest[parent]  * skeleton.bones[b].restLocal : skeleton.bones[b].restLocal;
            posed[b] = ok ? posed[parent] * local : local;
        }

        currentBoneMats = posed;
        restBoneMats = rest;


        if (canSkin) {
            // The native data indexes bones by their place in the HAnim table,
            // so the uniform array has to be in that order, not frame order.
            // The rest pose is already baked into the vertices, so the skinning
            // matrix is the same delta the rigid path uses.
            std::vector<glm::mat4> pal(128, glm::mat4(1.0f));
            for (size_t b = 0; b < n; ++b) {
                // Indexed by the skin's own bone index, which is the second
                // field of the HAnim table entry -- the per-vertex slots refer
                // to that, not to the table's position.
                const Bone &bn = skeleton.bones[b];
                const int t = bn.skinIndex >= 0 ? bn.skinIndex : bn.trackIndex;
                if (t >= 0 && t < 128) pal[(size_t)t] = posed[b] * glm::inverse(rest[b]);
            }
            GLint prog = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
            if (prog) {
                const GLint loc = glGetUniformLocation(prog, "boneTransforms");
                if (loc >= 0) {
                    glUniformMatrix4fv(loc, 128, GL_FALSE, glm::value_ptr(pal[0]));
                    skinned = true;
                }
            }
        } else if (haveFrame) {
            const size_t f = (size_t)chunk->frameIndex;
            model = m_transform * posed[f] * glm::inverse(rest[f]);
        }
    }

    glUniformMatrix4fv(ctx.uM, 1, GL_FALSE, glm::value_ptr(ctx.viewProj * model));
    glUniformMatrix4fv(ctx.uModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(ctx.uUseSkinning, skinned ? 1 : 0);

    GpuFor(*chunk).Draw();
}

} // namespace SG
} // namespace ClimaxEngine
