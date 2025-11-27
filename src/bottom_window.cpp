#include "bottom_window.h"

void DrawBottomWindow(bool& showBottomWindow, bool& pinBottom) {
    ImGuiWindowFlags bottomFlags = 0;
    bottomFlags |= ImGuiWindowFlags_MenuBar;
    if (pinBottom) bottomFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    ImGui::Begin("Bottom", &showBottomWindow, bottomFlags);
    ShowHeaderPin("pin_bottom", pinBottom);

    if(ImGui::BeginTabBar("Tabs")){
        if(ImGui::BeginTabItem("Asset Viewer")) {
            ImGui::TextWrapped("Preview will appear here.");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Console")) {
            // Console controls
            if(ImGui::Button("Clear")) {
                GuiConsole::instance().clear();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Console output captured from stdout/stderr");

            ImGui::Separator();
            ImGui::BeginChild("ConsoleChild", ImVec2(0, ImGui::GetContentRegionAvail().y - 10), true, ImGuiWindowFlags_HorizontalScrollbar);
            auto lines = GuiConsole::instance().lines();
            for (const auto &l : lines) {
                ImGui::TextUnformatted(l.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
