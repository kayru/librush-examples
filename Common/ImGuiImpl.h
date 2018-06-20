#pragma once

#include <Rush/Rush.h>

namespace Rush
{

class Window;
class PrimitiveBatch;
class GfxContext;

void ImGuiImpl_Startup(Window* window);
void ImGuiImpl_Update(float dt);
void ImGuiImpl_Render(GfxContext* context, PrimitiveBatch* prim);
void ImGuiImpl_Shutdown();

}
