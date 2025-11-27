#include "ui_helpers.h"
#include <vector>
#include <iostream>

static GLuint g_pinTexPinned = 0;
static GLuint g_pinTexUnpinned = 0;

void ensurePinTextures() {
    if(g_pinTexPinned && g_pinTexUnpinned) return;
    const int W = 16, H = 16;
    std::vector<unsigned char> pixPinned(W*H*4, 0);
    std::vector<unsigned char> pixUnpinned(W*H*4, 0);
    auto setPix = [&](std::vector<unsigned char>& buf, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a){
        if(x<0||x>=W||y<0||y>=H) return;
        int idx = (y*W + x) * 4;
        buf[idx+0] = r; buf[idx+1] = g; buf[idx+2] = b; buf[idx+3] = a;
    };
    // Pinned icon: circular head + vertical stem (dark color)
    for(int y=0;y<H;++y){
        for(int x=0;x<W;++x){
            float dx = x - 8.0f; float dy = y - 5.0f;
            float d2 = dx*dx + dy*dy;
            if(d2 <= 4.5f*4.5f) {
                setPix(pixPinned, x, y, 40, 120, 180, 255);
            }
            // stem
            if(x >= 7 && x <= 9 && y >= 9 && y <= 13) setPix(pixPinned, x, y, 40,120,180,255);
        }
    }
    // Unpinned icon: same head but lighter and tilted stem
    for(int y=0;y<H;++y){
        for(int x=0;x<W;++x){
            float dx = x - 8.0f; float dy = y - 5.0f;
            float d2 = dx*dx + dy*dy;
            if(d2 <= 4.5f*4.5f) {
                setPix(pixUnpinned, x, y, 220,220,220,255);
            }
        }
    }
    // draw tilted stem pixels for unpinned
    int x0 = 10, y0 = 7; int x1 = 13, y1 = 13;
    int dx = abs(x1-x0), sx = x0<x1?1:-1;
    int dy = -abs(y1-y0), sy = y0<y1?1:-1;
    int err = dx + dy; int cx = x0, cy = y0;
    while(true){ setPix(pixUnpinned, cx, cy, 220,220,220,255); if(cx==x1 && cy==y1) break; int e2 = 2*err; if(e2>=dy){ err += dy; cx += sx;} if(e2<=dx){ err += dx; cy += sy;} }

    // Upload textures
    glGenTextures(1, &g_pinTexPinned);
    glBindTexture(GL_TEXTURE_2D, g_pinTexPinned);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixPinned.data());

    glGenTextures(1, &g_pinTexUnpinned);
    glBindTexture(GL_TEXTURE_2D, g_pinTexUnpinned);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixUnpinned.data());

    glBindTexture(GL_TEXTURE_2D, 0);
}

void ShowHeaderPin(const char* id, bool &pinned, float pin_w, float pin_h) {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    // Ensure textures exist
    ensurePinTextures();
    ImTextureID tex = (ImTextureID)(intptr_t)(pinned ? g_pinTexPinned : g_pinTexUnpinned);

    const float extraRightOffset = 6.0f;

    // If a menu bar is available, place the button into it so it aligns with titlebar controls
    if (ImGui::BeginMenuBar()) {
        // compute X inside window coordinates: align to right edge
        float posX = ImGui::GetWindowWidth() - pin_w - style.WindowPadding.x - extraRightOffset;
        ImGui::SetCursorPosX(posX);
        if (ImGui::ImageButton(id, tex, ImVec2(16,16), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0))) {
            pinned = !pinned;
        }
        // subtle hover outline handled by ImageButton internally
        ImGui::EndMenuBar();
        return;
    }

    // Fallback: draw in foreground and use invisible item for input (absolute coords)
    float tabH = ImGui::GetFontSize() + style.FramePadding.y * 2.0f;
    ImVec2 pinPos;
    pinPos.x = winPos.x + winSize.x - pin_w - style.WindowPadding.x - extraRightOffset;
    pinPos.y = winPos.y - tabH + (tabH - pin_h) * 0.5f;
    if (pinPos.y < 0.0f) pinPos.y = winPos.y + style.FramePadding.y;
    ImVec2 pinMax = ImVec2(pinPos.x + pin_w, pinPos.y + pin_h);

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImVec2 imgA = ImVec2(pinPos.x + (pin_w - 16.0f) * 0.5f, pinPos.y + (pin_h - 16.0f) * 0.5f);
    ImVec2 imgB = ImVec2(imgA.x + 16.0f, imgA.y + 16.0f);
    fg->AddImage(tex, imgA, imgB, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255));

    ImVec2 prevCursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(pinPos);
    ImGui::InvisibleButton(id, ImVec2(pin_w, pin_h));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) pinned = !pinned;
    if (hovered) fg->AddRect(pinPos, pinMax, IM_COL32(255,255,255,48), 2.0f);
    ImGui::SetCursorScreenPos(prevCursor);
}
