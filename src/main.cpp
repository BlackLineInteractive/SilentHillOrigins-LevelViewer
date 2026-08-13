#include "ClimaxEngine/Viewer/ViewerApp.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        ClimaxEngine::Viewer::ViewerApp app;
        return app.Run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred." << std::endl;
        return EXIT_FAILURE;
    }
}
