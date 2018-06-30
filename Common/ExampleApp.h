#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/GfxRef.h>
#include <Rush/Platform.h>

namespace Rush
{
class PrimitiveBatch;
class BitmapFontRenderer;

class ExampleApp : public Rush::Application
{
	RUSH_DISALLOW_COPY_AND_ASSIGN(ExampleApp);

public:
	ExampleApp();
	~ExampleApp();

protected:
	struct DepthStencilStates
	{
		GfxDepthStencilStateRef writeLessEqual;
		GfxDepthStencilStateRef disable;
	} m_depthStencilStates;

	struct RasterizerStates
	{
		GfxRasterizerStateRef solidCullCW;
		GfxRasterizerStateRef solidCullCCW;
		GfxRasterizerStateRef solidNoCull;
	} m_rasterizerStates;

	struct SamplerStates
	{
		GfxSamplerRef pointClamp;
		GfxSamplerRef linearClamp;
		GfxSamplerRef linearWrap;
		GfxSamplerRef anisotropicWrap;
	} m_samplerStates;

	struct BlendStates
	{
		GfxBlendStateRef lerp;
		GfxBlendStateRef opaque;
		GfxBlendStateRef additive;
	} m_blendStates;

	Window*             m_window;
	PrimitiveBatch*     m_prim = nullptr;
	BitmapFontRenderer* m_font = nullptr;
};
}
