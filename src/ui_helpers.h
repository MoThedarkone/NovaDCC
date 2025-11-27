#pragma once

#include <glad/glad.h>
#include "imgui.h"

void ensurePinTextures();
void ShowHeaderPin(const char* id, bool &pinned, float pin_w = 18.0f, float pin_h = 18.0f);
