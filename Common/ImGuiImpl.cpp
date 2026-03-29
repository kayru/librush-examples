#include "ImGuiImpl.h"

#include <Rush/Window.h>
#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>

#include <imgui.h>
#include <stdio.h>

namespace Rush
{
namespace
{

PrimitiveBatch* s_prim = nullptr;
GfxContext* s_context = nullptr;
Window* s_window = nullptr;
ImGuiContext* s_guiContext = nullptr;

static GfxTexture imTextureIdToGfxTexture(ImTextureID id)
{
	return GfxTexture(UntypedResourceHandle((u16)(uintptr_t)id));
}

static ImTextureID gfxTextureToImTextureId(GfxTexture tex)
{
	return (ImTextureID)(uintptr_t)tex.index();
}

static void ImGuiImpl_UpdateTexture(ImTextureData* tex)
{
	if (tex->Status == ImTextureStatus_WantCreate)
	{
		RUSH_ASSERT(tex->TexID == ImTextureID_Invalid);
		RUSH_ASSERT(tex->Format == ImTextureFormat_RGBA32);

		const void* pixels = tex->GetPixels();
		GfxOwn<GfxTexture> gfxTex = Gfx_CreateTexture(
			GfxTextureDesc::make2D(tex->Width, tex->Height, GfxFormat_RGBA8_Unorm), pixels);

		tex->SetTexID(gfxTextureToImTextureId(gfxTex.detach()));
		tex->SetStatus(ImTextureStatus_OK);
	}
	else if (tex->Status == ImTextureStatus_WantUpdates)
	{
		// Rush doesn't support partial texture updates via PrimitiveBatch,
		// so recreate the texture from scratch.
		GfxTexture old = imTextureIdToGfxTexture(tex->TexID);
		if (old.valid())
		{
			Gfx_Release(old);
		}

		const void* pixels = tex->GetPixels();
		GfxOwn<GfxTexture> gfxTex = Gfx_CreateTexture(
			GfxTextureDesc::make2D(tex->Width, tex->Height, GfxFormat_RGBA8_Unorm), pixels);

		tex->SetTexID(gfxTextureToImTextureId(gfxTex.detach()));
		tex->SetStatus(ImTextureStatus_OK);
	}
	else if (tex->Status == ImTextureStatus_WantDestroy)
	{
		GfxTexture gfxTex = imTextureIdToGfxTexture(tex->TexID);
		if (gfxTex.valid())
		{
			Gfx_Release(gfxTex);
		}
		tex->SetTexID(ImTextureID_Invalid);
		tex->SetStatus(ImTextureStatus_Destroyed);
	}
}

static ImGuiKey rushKeyToImGuiKey(Key key)
{
	switch (key)
	{
	case Key_Tab:          return ImGuiKey_Tab;
	case Key_Left:         return ImGuiKey_LeftArrow;
	case Key_Right:        return ImGuiKey_RightArrow;
	case Key_Up:           return ImGuiKey_UpArrow;
	case Key_Down:         return ImGuiKey_DownArrow;
	case Key_Home:         return ImGuiKey_Home;
	case Key_End:          return ImGuiKey_End;
	case Key_Delete:       return ImGuiKey_Delete;
	case Key_Backspace:    return ImGuiKey_Backspace;
	case Key_Enter:        return ImGuiKey_Enter;
	case Key_Escape:       return ImGuiKey_Escape;
	case Key_Space:        return ImGuiKey_Space;
	case Key_A:            return ImGuiKey_A;
	case Key_C:            return ImGuiKey_C;
	case Key_V:            return ImGuiKey_V;
	case Key_X:            return ImGuiKey_X;
	case Key_Y:            return ImGuiKey_Y;
	case Key_Z:            return ImGuiKey_Z;
	case Key_LeftControl:  return ImGuiKey_LeftCtrl;
	case Key_RightControl: return ImGuiKey_RightCtrl;
	case Key_LeftShift:    return ImGuiKey_LeftShift;
	case Key_RightShift:   return ImGuiKey_RightShift;
	case Key_LeftAlt:      return ImGuiKey_LeftAlt;
	case Key_RightAlt:     return ImGuiKey_RightAlt;
	case Key_LeftSuper:    return ImGuiKey_LeftSuper;
	case Key_RightSuper:   return ImGuiKey_RightSuper;
	default:               return ImGuiKey_None;
	}
}

class GuiWMI : public WindowMessageInterceptor
{
public:

	virtual bool processEvent(const WindowEvent& e) override
	{
		ImGuiIO& io = ImGui::GetIO();

		switch (e.type)
		{
		case WindowEventType_Char:
			io.AddInputCharacter(e.character);
			return io.WantCaptureKeyboard;
		case WindowEventType_KeyDown:
		{
			ImGuiKey key = rushKeyToImGuiKey((Key)e.code);
			if (key != ImGuiKey_None)
				io.AddKeyEvent(key, true);
			return io.WantCaptureKeyboard;
		}
		case WindowEventType_KeyUp:
		{
			ImGuiKey key = rushKeyToImGuiKey((Key)e.code);
			if (key != ImGuiKey_None)
				io.AddKeyEvent(key, false);
			return io.WantCaptureKeyboard;
		}
		case WindowEventType_MouseDown:
			if (e.button < 5)
				io.AddMouseButtonEvent(e.button, true);
			return io.WantCaptureMouse;

		case WindowEventType_MouseUp:
			if (e.button < 5)
				io.AddMouseButtonEvent(e.button, false);
			return io.WantCaptureMouse;

		case WindowEventType_MouseMove:
			io.AddMousePosEvent(e.pos.x, e.pos.y);
			return io.WantCaptureMouse;

		case WindowEventType_Scroll:
			io.AddMouseWheelEvent(0.0f, e.scroll.y);
			return io.WantCaptureMouse;
		default:
			return false;
		}
	}
};

GuiWMI s_messageInterceptor;

}

static void ImGuiImpl_RenderDrawData(ImDrawData* drawData)
{
	if (drawData->Textures != nullptr)
	{
		for (ImTextureData* tex : *drawData->Textures)
		{
			ImGuiImpl_UpdateTexture(tex);
		}
	}

	ImVec2 displaySize = drawData->DisplaySize;
	if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
	{
		const Tuple2i windowSize = s_window->getSize();
		displaySize = ImVec2((float)windowSize.x, (float)windowSize.y);
	}

	s_prim->begin2D(displaySize.x, displaySize.y);
	s_prim->setSampler(PrimitiveBatch::SamplerState_Point);

	const ImVec2 clipOffset = drawData->DisplayPos;
	const ImVec2 clipScale = drawData->FramebufferScale;

	for (const ImDrawList* cmdList : drawData->CmdLists)
	{
		const auto& ib = cmdList->IdxBuffer;
		const auto& vb = cmdList->VtxBuffer;

		u32 indexOffset = 0;
		for (const auto& cmd : cmdList->CmdBuffer)
		{
			const GfxTexture tex = imTextureIdToGfxTexture(cmd.GetTexID());
			s_prim->setTexture(tex);

			auto verts = s_prim->drawVertices(GfxPrimitive::TriangleList, cmd.ElemCount);
			for (u32 i = 0; i < cmd.ElemCount; ++i)
			{
				const u32 index = ib[i + indexOffset];
				const auto& v = vb[index];

				verts[i].pos.x = v.pos.x;
				verts[i].pos.y = v.pos.y;
				verts[i].pos.z = 1;

				verts[i].tex.x = v.uv.x;
				verts[i].tex.y = v.uv.y;

				verts[i].col = ColorRGBA8(v.col);
			}

			const float clipMinX = (cmd.ClipRect.x - clipOffset.x) * clipScale.x;
			const float clipMinY = (cmd.ClipRect.y - clipOffset.y) * clipScale.y;
			const float clipMaxX = (cmd.ClipRect.z - clipOffset.x) * clipScale.x;
			const float clipMaxY = (cmd.ClipRect.w - clipOffset.y) * clipScale.y;

			GfxRect scissor;
			scissor.top = (int)clipMinY;
			scissor.bottom = (int)clipMaxY;
			scissor.left = (int)clipMinX;
			scissor.right = (int)clipMaxX;

			Gfx_SetScissorRect(s_context, scissor);
			s_prim->flush();
			indexOffset += cmd.ElemCount;
		}
	}

	s_prim->end2D();
}

void ImGuiImpl_Startup(Window* window)
{
	RUSH_ASSERT(s_window == nullptr);

	s_guiContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(s_guiContext);

	ImGuiIO& io = ImGui::GetIO();

	// Write config file next to the current binary instead of working directory
	static char imguiConfigPath[1024];
	const char* exeDir = Platform_GetExecutableDirectory();
	snprintf(imguiConfigPath, sizeof(imguiConfigPath), "%s/imgui.ini", exeDir);
	io.IniFilename = imguiConfigPath;

	io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

	s_window = window;
	s_window->setMessageInterceptor(&s_messageInterceptor);
}

void ImGuiImpl_Update(float dt)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = dt;

	io.AddMousePosEvent(s_window->getMouseState().pos.x, s_window->getMouseState().pos.y);

	io.AddMouseButtonEvent(0, s_window->getMouseState().buttons[0]);
	io.AddMouseButtonEvent(1, s_window->getMouseState().buttons[1]);

	io.DisplayFramebufferScale.x = s_window->getResolutionScale().x;
	io.DisplayFramebufferScale.y = s_window->getResolutionScale().y;

	const Tuple2i framebufferSize = s_window->getFramebufferSize();
	const float scaleX = (io.DisplayFramebufferScale.x > 0.0f) ? io.DisplayFramebufferScale.x : 1.0f;
	const float scaleY = (io.DisplayFramebufferScale.y > 0.0f) ? io.DisplayFramebufferScale.y : 1.0f;
	if (framebufferSize.x > 0)
		io.DisplaySize.x = (float)framebufferSize.x / scaleX;
	if (framebufferSize.y > 0)
		io.DisplaySize.y = (float)framebufferSize.y / scaleY;

	ImGui::NewFrame();
}


void ImGuiImpl_Shutdown()
{
	for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
	{
		if (tex->RefCount == 1)
		{
			tex->SetStatus(ImTextureStatus_WantDestroy);
			ImGuiImpl_UpdateTexture(tex);
		}
	}

	ImGui::DestroyContext(s_guiContext);

	delete s_prim;
	s_prim = nullptr;

	s_window = nullptr;
}

void ImGuiImpl_Render(GfxContext* context, PrimitiveBatch* prim)
{
	s_prim = prim;
	s_context = context;

	ImGui::Render();
	ImGuiImpl_RenderDrawData(ImGui::GetDrawData());

	s_prim = nullptr;
	s_context = nullptr;
}

}
