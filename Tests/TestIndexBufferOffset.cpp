#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/UtilLog.h>

using namespace Test;
using namespace Rush;

// Validates that Gfx_SetIndexStream with a non-zero byte offset
// correctly draws using indices at that offset.
class IndexBufferOffsetTest final : public GfxScreenshotTestCase
{
public:
	explicit IndexBufferOffsetTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		const GfxCapability& caps = Gfx_GetCapability();

		const char* vsName = RUSH_SHADER_NAME("TestIndexBufferOffsetVS.hlsl");
		const char* psName = RUSH_SHADER_NAME("TestIndexBufferOffsetPS.hlsl");

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		const bool shaderSupported = caps.shaderTypeSupported(GfxShaderSourceType_MSL_BIN);
#else
		const bool shaderSupported = caps.shaderTypeSupported(GfxShaderSourceType_SPV);
#endif

		if (!shaderSupported)
		{
			m_skipReason = "Required shader source type is not supported.";
			return;
		}

		GfxShaderSource vsSource = loadShaderFromFile(vsName);
		GfxShaderSource psSource = loadShaderFromFile(psName);
		if (vsSource.empty() || psSource.empty())
		{
			m_skipReason = "Failed to load shaders.";
			return;
		}

		m_vs = Gfx_CreateVertexShader(vsSource);
		m_ps = Gfx_CreatePixelShader(psSource);

		// Full-screen triangle
		const float vertices[] = {
			-1.0f, -1.0f,
			 3.0f, -1.0f,
			-1.0f,  3.0f,
		};
		m_vb = Gfx_CreateBuffer(GfxBufferFlags::Vertex, GfxFormat_Unknown, 3, sizeof(float) * 2, vertices);

		// Index buffer: 3 padding indices, then the real indices at byte offset 6
		const u16 indices[] = {
			0xFFFF, 0xFFFF, 0xFFFF, // padding (3 * 2 = 6 bytes)
			0, 1, 2,                 // real indices at byte offset 6
		};
		m_ib = Gfx_CreateBuffer(GfxBufferFlags::Index, GfxFormat_R16_Uint, 6, sizeof(u16), indices);
		m_ibOffset = 3 * sizeof(u16); // 6 bytes

		GfxRenderPipelineDesc pipelineDesc;
		pipelineDesc.ps = m_ps.get();
		pipelineDesc.vs = m_vs.get();
		pipelineDesc.vertexFormat.add(0, GfxVertexFormatDesc::DataType::Float2, GfxVertexFormatDesc::Semantic::Position, 0);
		pipelineDesc.bindings.descriptorSets[0].stageFlags = GfxStageFlags::Vertex | GfxStageFlags::Pixel;
		pipelineDesc.renderTarget = caps.backBufferDesc;

		m_pipeline = Gfx_CreateRenderPipeline(pipelineDesc);
		if (!m_pipeline.valid())
		{
			m_skipReason = "Failed to create technique.";
			return;
		}

		m_ready = true;
	}

	void render(GfxContext* ctx) override
	{
		if (!m_ready)
		{
			if (!m_loggedSkip && !m_skipReason.empty())
			{
				RUSH_LOG("[Test] SKIP: %s", m_skipReason.c_str());
				m_loggedSkip = true;
			}
			return;
		}

		GfxPassDesc passDesc;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(0, 0, 0, 255);

		Gfx_BeginPass(ctx, passDesc);
		Gfx_SetRenderPipeline(ctx, m_pipeline);
		Gfx_SetVertexStream(ctx, 0, m_vb);
		Gfx_SetIndexStream(ctx, m_ibOffset, GfxFormat_R16_Uint, m_ib);
		Gfx_DrawIndexed(ctx, 3, 0, 0, 3);
		Gfx_EndPass(ctx);
	}

	TestResult validate(GfxContext*, const TestImage* image) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		if (!image || image->pixels.empty())
		{
			return TestResult::fail("Missing screenshot data");
		}

		if (image->size.x == 0 || image->size.y == 0)
		{
			return TestResult::fail("Invalid screenshot size");
		}

		// The full-screen triangle should have painted the center pixel green.
		const u32 cx = image->size.x / 2;
		const u32 cy = image->size.y / 2;
		const ColorRGBA8 pixel = image->pixels[cy * image->size.x + cx];

		if (pixel.g < 200)
		{
			return TestResult::fail("Center pixel (%u,%u,%u) expected green", pixel.r, pixel.g, pixel.b);
		}

		return TestResult::pass();
	}

private:
	bool m_ready = false;
	bool m_loggedSkip = false;
	String m_skipReason;

	GfxOwn<GfxVertexShader> m_vs;
	GfxOwn<GfxPixelShader> m_ps;
	GfxOwn<GfxBuffer> m_vb;
	GfxOwn<GfxBuffer> m_ib;
	GfxOwn<GfxRenderPipeline> m_pipeline;
	u32 m_ibOffset = 0;
};

RUSH_REGISTER_TEST(IndexBufferOffsetTest, "gfx",
	"Validates index buffer with non-zero byte offset.");
