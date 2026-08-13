#include "ClimaxEngine/Viewer/ViewerGraphics.h"
#pragma once

#include <string>


#include <GL/glew.h>

namespace ClimaxEngine {
namespace Viewer {

class ViewerApp {
public:
    ViewerApp() = default;
    ~ViewerApp() = default;

    int Run(int argc, char* argv[]);

private:
    ViewerGraphics m_Graphics;
};


} // namespace Viewer
} // namespace ClimaxEngine
