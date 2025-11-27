#include "assets_window.h"

void DrawAssetsWindow(Scene& scene, bool& showAssetsWindow, bool& pinAssets) {
    ImGuiWindowFlags assetsFlags = 0;
    assetsFlags |= ImGuiWindowFlags_MenuBar;
    if (pinAssets) assetsFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    ImGui::Begin("Assets", &showAssetsWindow, assetsFlags);
    ShowHeaderPin("pin_assets", pinAssets);

    ImGui::Text("Asset Browser (placeholder)");
    if(ImGui::Button("Import...")) { /* TODO */ }
    ImGui::Separator();

    // Show entity list for selection
    ImGui::Text("Entities:");
    const auto& ents = scene.entities();
    for(size_t i = 0; i < ents.size(); ++i) {
        std::string label = "Entity " + std::to_string(ents[i].id);
        bool selected = (scene.getSelectedId() == ents[i].id);
        if(ImGui::Selectable(label.c_str(), selected)) {
            scene.selectEntity(ents[i].id);
        }
    }

    ImGui::End();
}
