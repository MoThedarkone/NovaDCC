#pragma once
#include <string>

class Scene;

namespace AssetLoader {
    // Load model at path and append to scene. Returns true on success.
    bool loadModel(const std::string& path, Scene& scene);
}
