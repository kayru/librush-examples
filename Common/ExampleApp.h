#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/Platform.h>

#include <string>

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
	void update() final;
	static void SetupScreenshot(const AppConfig& cfg, int argc, char** argv);
	template <typename T> static inline int Main(AppConfig cfg, int argc, char** argv)
	{
		SetupScreenshot(cfg, argc, argv);
		return Platform_Main<T>(cfg);
	}

protected:
	virtual void onUpdate() = 0;
	struct DepthStencilStates
	{
		GfxOwn<GfxDepthStencilState> writeLessEqual;
		GfxOwn<GfxDepthStencilState> writeGreaterEqual;
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
	u32 m_frameIndex = 0;
	u32 m_frameLimit = 0;
	u32 m_maxScreenshotDim = 0;
	bool m_exitAfterFrames = false;
	bool m_screenshotRequested = false;
	bool m_screenshotInFlight = false;
	bool m_screenshotComplete = false;
	std::string m_screenshotPath;
	std::string m_screenshotAppName;

private:
	void requestScreenshot();
	void writeScreenshot(const ColorRGBA8* pixels, Tuple2u size);
	static void onScreenshot(const ColorRGBA8* pixels, Tuple2u size, void* userData);
};

template <typename T>
inline int Example_Main(AppConfig cfg, int argc, char** argv)
{
	return ExampleApp::Main<T>(cfg, argc, argv);
}
}
