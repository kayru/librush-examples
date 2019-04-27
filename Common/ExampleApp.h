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
		GfxOwn<GfxDepthStencilState> writeLessEqual;
		GfxOwn<GfxDepthStencilState> disable;
	} m_depthStencilStates;

	struct RasterizerStates
	{
		GfxOwn<GfxRasterizerState> solidCullCW;
		GfxOwn<GfxRasterizerState> solidCullCCW;
		GfxOwn<GfxRasterizerState> solidNoCull;
	} m_rasterizerStates;

	struct SamplerStates
	{
		GfxOwn<GfxSampler> pointClamp;
		GfxOwn<GfxSampler> linearClamp;
		GfxOwn<GfxSampler> linearWrap;
		GfxOwn<GfxSampler> anisotropicWrap;
	} m_samplerStates;

	struct BlendStates
	{
		GfxOwn<GfxBlendState> lerp;
		GfxOwn<GfxBlendState> opaque;
		GfxOwn<GfxBlendState> additive;
	} m_blendStates;

	Window*             m_window;
	PrimitiveBatch*     m_prim = nullptr;
	BitmapFontRenderer* m_font = nullptr;
};
}
