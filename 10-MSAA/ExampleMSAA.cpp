#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilTimer.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

#include <stdio.h>

#include <Common/ExampleApp.h>
#include <Common/ImGuiImpl.h>
#include <Common/Utils.h>
#include <imgui.h>

class ImGuiApp : public ExampleApp
{
public:
	ImGuiApp()
	{
		ImGuiImpl_Startup(m_window);

		m_blendPremult = Gfx_CreateBlendState(GfxBlendStateDesc::makeLerp());
		m_blendOpaque = Gfx_CreateBlendState(GfxBlendStateDesc::makeOpaque());

		{
			const GfxCapability& caps = Gfx_GetCapability();
			u32 mask = caps.colorSampleCounts;
			m_msaaModes.reserve(bitCount(mask));
			u32 sampleCount = 1;
			while (mask)
			{
				if (mask & sampleCount)
				{
					m_msaaModes.push(sampleCount);
				}
				mask &= ~sampleCount;
				sampleCount <<= 1;
			}
		}
	}

	~ImGuiApp()
	{
		ImGuiImpl_Shutdown();
	}

	void onUpdate() override
	{
		int desiredSampleCount = m_msaaModes[m_msaaQuality];
		if (m_currentSampleCount != desiredSampleCount || m_currentZoom != m_zoom)
		{
			const u32 resolution = max(1u, m_maxResolution >> m_zoom);

			{
				GfxTextureDesc desc = GfxTextureDesc::make2D(resolution, resolution);
				desc.usage = GfxUsageFlags::RenderTarget | GfxUsageFlags::ShaderResource | GfxUsageFlags::TransferDst;
				m_resolveTarget = Gfx_CreateTexture(desc);
			}

			{
				GfxTextureDesc desc = GfxTextureDesc::make2D(resolution, resolution);
				desc.samples = desiredSampleCount;
				desc.usage = GfxUsageFlags::RenderTarget | GfxUsageFlags::TransferSrc;
				m_msaaTarget = Gfx_CreateTexture(desc);
			}

			m_currentSampleCount = desiredSampleCount;
			m_currentZoom = m_zoom;
		}

		const float dt = float(m_deltaTime.time());
		ImGuiImpl_Update(dt);

		if (m_enableAnimation)
		{
			if (m_animationTime > 1.0)
			{
				m_animationTime = m_animationTime - floorf(m_animationTime);
			}

			m_animationTime += dt * 0.1f;
		}

		auto window = m_window;
		auto ctx    = Platform_GetGfxContext();

		// off-screen rendering

		ColorRGBA8 colors[] = {
			ColorRGBA8::Red(m_enableWireframe ? 100 : 255),
			ColorRGBA8::Green(m_enableWireframe ? 100 : 255),
			ColorRGBA8::Blue(m_enableWireframe ? 100 : 255),
			ColorRGBA8::White(m_enableWireframe ? 100 : 255),
		};

		Vec2 vertices[RUSH_COUNTOF(colors) * 3];

		{
			auto positionFromAngle = [](float a)
			{
				float sa = sinf(a);
				float ca = cosf(a);
				return Vec2(ca, sa);
			};
			u32 vertIdx = 0;
			float t = (m_animationTime * TwoPi) / 3.0f;
			for (ColorRGBA8 c : colors)
			{

				vertices[vertIdx++] = positionFromAngle(t + (0 * TwoPi / 3.0f));
				vertices[vertIdx++] = positionFromAngle(t + (1 * TwoPi / 3.0f));
				vertices[vertIdx++] = positionFromAngle(t + (2 * TwoPi / 3.0f));
				t += (TwoPi / RUSH_COUNTOF(colors));
			}
		}

		{
			const bool useMSAA = m_currentSampleCount > 1;

			GfxPassDesc passDesc;
			passDesc.flags = GfxPassFlags::ClearAll;
			passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
			passDesc.color[0] = useMSAA ? m_msaaTarget.get() : m_resolveTarget.get();
			Gfx_BeginPass(ctx, passDesc);
			Gfx_SetBlendState(ctx, m_blendPremult);
			m_prim->begin2D(Vec2(1.0f), Vec2(0.0f));

			u32 vertIdx = 0;
			for (ColorRGBA8 c : colors)
			{
				m_prim->drawTriangle(
					vertices[vertIdx+0],
					vertices[vertIdx+1],
					vertices[vertIdx+2],
					c);
				vertIdx += 3;
			}

			m_prim->end2D();

			Gfx_EndPass(ctx);

			if (useMSAA)
			{
				Gfx_ResolveImage(ctx, m_msaaTarget, m_resolveTarget);
			}

			Gfx_AddImageBarrier(ctx, m_resolveTarget, GfxResourceState_ShaderRead);
		}

		// render to back buffer
		{
			GfxPassDesc passDesc;
			passDesc.flags = GfxPassFlags::ClearAll;
			passDesc.clearColors[0] = ColorRGBA8(0, 0, 0);
			Gfx_BeginPass(ctx, passDesc);
			Gfx_SetBlendState(ctx, m_blendOpaque);

			Gfx_SetViewport(ctx, GfxViewport(window->getSize()));
			Gfx_SetScissorRect(ctx, window->getSize());

			m_prim->begin2D(Vec2(1.0f), Vec2(0.0f));

			TexturedQuad2D q = makeFullScreenQuad();

			m_prim->setTexture(m_resolveTarget);
			m_prim->drawTexturedQuad(&q);

			if (m_enableWireframe)
			{
				u32 vertexIdx = 0;
				for (ColorRGBA8 c : colors)
				{
					c.a = 255;
					m_prim->drawLine(vertices[vertexIdx + 0], vertices[vertexIdx + 1], c);
					m_prim->drawLine(vertices[vertexIdx + 1], vertices[vertexIdx + 2], c);
					m_prim->drawLine(vertices[vertexIdx + 2], vertices[vertexIdx + 0], c);
					vertexIdx += 3;
				}
			}

			m_prim->end2D();

			if (ImGui::Begin("Settings"))
			{
				ImGui::Checkbox("Enable Wireframe", &m_enableWireframe);
				ImGui::Checkbox("Enable Animation", &m_enableAnimation);
				ImGui::SliderFloat("Animation time", &m_animationTime, 0.0f, 1.0f);
				char msaaText[3] = {};
				snprintf(msaaText, sizeof(msaaText), "%d", m_msaaModes[m_msaaQuality]);
				ImGui::SliderInt("Sample count", &m_msaaQuality, 0, u32(m_msaaModes.size())-1, msaaText);
				ImGui::SliderInt("Zoom", &m_zoom, 0, bitScanForward(m_maxResolution));
				
			}
			ImGui::End();

			ImGuiImpl_Render(ctx, m_prim);

			Gfx_EndPass(ctx);
		}

		m_deltaTime.reset();
	}

private:
	bool m_enableWireframe = false;
	bool m_enableAnimation = false;
	int m_msaaQuality = 0;
	int m_zoom = 0;
	DynamicArray<u32> m_msaaModes;

	float m_animationTime = 0.0f;

	GfxOwn<GfxBlendState> m_blendPremult;
	GfxOwn<GfxBlendState> m_blendOpaque;
	GfxOwn<GfxTexture> m_resolveTarget;
	GfxOwn<GfxTexture> m_msaaTarget;
	u32 m_currentSampleCount = 0;
	u32 m_currentZoom = 0;

	Timer m_deltaTime;

	u32 m_maxResolution = 1024;
};

int main(int argc, char** argv)
{
	AppConfig cfg;

	cfg.name      = "MSAA (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 1024;
	cfg.height    = 1024;
	cfg.resizable = true;
	cfg.argc = argc;
	cfg.argv = argv;

#ifdef RUSH_DEBUG
	cfg.debug = true;
#endif

	return Example_Main<ImGuiApp>(cfg, argc, argv);
}
