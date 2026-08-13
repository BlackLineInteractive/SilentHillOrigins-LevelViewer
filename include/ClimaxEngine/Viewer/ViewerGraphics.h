#pragma once

#include "ClimaxEngine/Core/Types.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

namespace ClimaxEngine {
namespace Viewer {

class ViewerGraphics {
public:
    ViewerGraphics() = default;
    ~ViewerGraphics() = default;

    bool Init();
    void RenderFrame(int fbW, int fbH, int winW, int winH, const glm::mat4& mvp, const glm::vec3& eye, const glm::vec3& viewDir, size_t totalMeshes);
    void Shutdown();

public:
    GLuint p = 0;
    GLuint uiP = 0;
    GLuint solidP = 0;
    GLuint skyProg = 0;
    GLuint collProg = 0;
    GLuint lineVao = 0, lineVbo = 0;
    GLuint markerVao = 0, markerVbo = 0;
    GLuint skyVao = 0;
};

} // namespace Viewer
} // namespace ClimaxEngine
