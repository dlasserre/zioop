#include "zioop/gui/Renderer.hpp"

int main() {
    {
        gui::Renderer renderer(
        1280,
        720,
        "Zioop Arena"
    );
        renderer.drawWorld();
    }

    return EXIT_SUCCESS;
}
