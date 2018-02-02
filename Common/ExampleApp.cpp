#include "ExampleApp.h"
#include "Utils.h"

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxCommon.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

namespace Rush
{

ExampleApp::ExampleApp() : m_window(Platform_GetWindow())
{
	m_window->retain();

	m_prim = new PrimitiveBatch();
	m_font = new BitmapFontRenderer(BitmapFontRenderer::createEmbeddedFont(true, 0, 1));

	// Depth stencil states

	{
		GfxDepthStencilDesc desc;
		desc.enable      = false;
		desc.writeEnable = false;
		desc.compareFunc = GfxCompareFunc::Always;
		m_depthStencilStates.disable.takeover(Gfx_CreateDepthStencilState(desc));
	}

	{
		GfxDepthStencilDesc desc;
		desc.enable      = true;
		desc.writeEnable = true;
		desc.compareFunc = GfxCompareFunc::LessEqual;
		m_depthStencilStates.writeLessEqual.takeover(Gfx_CreateDepthStencilState(desc));
	}

	// Blend states

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeOpaque();
		m_blendStates.opaque.takeover(Gfx_CreateBlendState(desc));
	}

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeLerp();
		m_blendStates.lerp.takeover(Gfx_CreateBlendState(desc));
	}

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeAdditive();
		m_blendStates.additive.takeover(Gfx_CreateBlendState(desc));
	}

	// Sampler states

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makePoint();
		desc.wrapU          = GfxTextureWrap::Clamp;
		desc.wrapV          = GfxTextureWrap::Clamp;
		desc.wrapW          = GfxTextureWrap::Clamp;
		m_samplerStates.pointClamp.takeover(Gfx_CreateSamplerState(desc));
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Clamp;
		desc.wrapV          = GfxTextureWrap::Clamp;
		desc.wrapW          = GfxTextureWrap::Clamp;
		m_samplerStates.linearClamp.takeover(Gfx_CreateSamplerState(desc));
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Wrap;
		desc.wrapV          = GfxTextureWrap::Wrap;
		desc.wrapW          = GfxTextureWrap::Wrap;
		m_samplerStates.linearWrap.takeover(Gfx_CreateSamplerState(desc));
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Wrap;
		desc.wrapV          = GfxTextureWrap::Wrap;
		desc.wrapW          = GfxTextureWrap::Wrap;
		desc.anisotropy     = 4.0f;
		m_samplerStates.anisotropicWrap.takeover(Gfx_CreateSamplerState(desc));
	}
}

ExampleApp::~ExampleApp()
{
	delete m_font;
	delete m_prim;

	m_window->release();
}

}