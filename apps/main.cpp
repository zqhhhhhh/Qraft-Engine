#include "engine/Engine.h"
// #include "engine/Timer.h"

int main(int argc, char* argv[]) {
    std::string resources_root_hint;
    std::string scene_file_hint;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--resources-root" && i + 1 < argc)
        {
            resources_root_hint = argv[++i] ? argv[i] : "";
        }
        else if (arg == "--scene-file" && i + 1 < argc)
        {
            scene_file_hint = argv[++i] ? argv[i] : "";
        }
    }

    // Timer timer("Total Execution");
    Engine engine(resources_root_hint, scene_file_hint);
    engine.GameLoop();
    return 0;
}
