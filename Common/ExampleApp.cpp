#include "ExampleApp.h"
#include "Utils.h"

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxCommon.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

#include <stb_image_resize.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace Rush
{

namespace
{
	struct ExampleRunSettings
	{
		u32 frameLimit = 0;
		u32 maxScreenshotDim = 0;
		bool exitAfterFrames = false;
		bool screenshotRequested = false;
		std::string screenshotPath;
		std::string appName;
	};

	ExampleRunSettings g_runSettings;

	std::filesystem::path getScreenshotDirectory()
	{
		std::filesystem::path base = Platform_GetExecutableDirectory();
		return base / "Screenshots";
	}

	std::filesystem::path buildDefaultOutputPath(const std::string& appName, u32 targetFrame)
	{
		char filename[256];
		std::snprintf(filename, sizeof(filename), "%s_frame%04u.png", appName.c_str(), targetFrame);
		return getScreenshotDirectory() / filename;
	}

	std::string getExtensionLower(const std::filesystem::path& path)
	{
		std::string ext = path.extension().string();
		if (!ext.empty() && ext[0] == '.')
		{
			ext.erase(0, 1);
		}
		return toLower(ext);
	}

	bool ensureDirectoryForPath(const std::filesystem::path& path, const std::filesystem::path& fallbackDir)
	{
		std::error_code ec;
		std::filesystem::path dir = path.parent_path();
		if (dir.empty())
		{
			dir = fallbackDir;
		}

		if (dir.empty())
		{
			return true;
		}

		std::filesystem::create_directories(dir, ec);
		return !ec;
	}
}

ExampleApp::ExampleApp() : m_window(Platform_GetWindow())
{
	m_window->retain();

	m_prim = new PrimitiveBatch();
	m_font = new BitmapFontRenderer(BitmapFontRenderer::createEmbeddedFont(true, 0, 1));

	m_frameLimit = g_runSettings.frameLimit;
	m_exitAfterFrames = g_runSettings.exitAfterFrames;
	m_screenshotRequested = g_runSettings.screenshotRequested;
	m_screenshotPath = g_runSettings.screenshotPath;
	m_screenshotAppName = sanitizeFilename(g_runSettings.appName, "Screenshot");
	m_maxScreenshotDim = g_runSettings.maxScreenshotDim;

	// Depth stencil states

	{
		GfxDepthStencilDesc desc;
		desc.enable      = false;
		desc.writeEnable = false;
		desc.compareFunc = GfxCompareFunc::Always;
		m_depthStencilStates.disable = Gfx_CreateDepthStencilState(desc);
	}

	{
		GfxDepthStencilDesc desc;
		desc.enable      = true;
		desc.writeEnable = true;
		desc.compareFunc = GfxCompareFunc::LessEqual;
		m_depthStencilStates.writeLessEqual = Gfx_CreateDepthStencilState(desc);
	}

	{
		GfxDepthStencilDesc desc;
		desc.enable      = true;
		desc.writeEnable = true;
		desc.compareFunc = GfxCompareFunc::GreaterEqual;
		m_depthStencilStates.writeGreaterEqual = Gfx_CreateDepthStencilState(desc);
	}

	// Rasterizer states

	{
		GfxRasterizerDesc desc;
		desc.cullMode = GfxCullMode::CW;
		m_rasterizerStates.solidCullCW = Gfx_CreateRasterizerState(desc);
	}

	{
		GfxRasterizerDesc desc;
		desc.cullMode = GfxCullMode::CCW;
		m_rasterizerStates.solidCullCCW = Gfx_CreateRasterizerState(desc);
	}

	{
		GfxRasterizerDesc desc;
		desc.cullMode = GfxCullMode::None;
		m_rasterizerStates.solidNoCull = Gfx_CreateRasterizerState(desc);
	}

	// Blend states

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeOpaque();
		m_blendStates.opaque = Gfx_CreateBlendState(desc);
	}

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeLerp();
		m_blendStates.lerp = Gfx_CreateBlendState(desc);
	}

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeAdditive();
		m_blendStates.additive = Gfx_CreateBlendState(desc);
	}

	// Sampler states

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makePoint();
		desc.wrapU          = GfxTextureWrap::Clamp;
		desc.wrapV          = GfxTextureWrap::Clamp;
		desc.wrapW          = GfxTextureWrap::Clamp;
		m_samplerStates.pointClamp = Gfx_CreateSamplerState(desc);
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Clamp;
		desc.wrapV          = GfxTextureWrap::Clamp;
		desc.wrapW          = GfxTextureWrap::Clamp;
		m_samplerStates.linearClamp = Gfx_CreateSamplerState(desc);
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Wrap;
		desc.wrapV          = GfxTextureWrap::Wrap;
		desc.wrapW          = GfxTextureWrap::Wrap;
		m_samplerStates.linearWrap = Gfx_CreateSamplerState(desc);
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Wrap;
		desc.wrapV          = GfxTextureWrap::Wrap;
		desc.wrapW          = GfxTextureWrap::Wrap;
		desc.anisotropy     = 4.0f;
		m_samplerStates.anisotropicWrap = Gfx_CreateSamplerState(desc);
	}
}

ExampleApp::~ExampleApp()
{
	delete m_font;
	delete m_prim;

	m_window->release();
}

void ExampleApp::SetupScreenshot(const AppConfig& cfg, int argc, char** argv)
{
	g_runSettings = {};

	bool hasFrames = getArgU32(argc, argv, "frames", nullptr, g_runSettings.frameLimit);
	bool hasScreenshot = getArgString(argc, argv, "screenshot", nullptr, g_runSettings.screenshotPath);
	getArgU32(argc, argv, "screenshot-size", nullptr, g_runSettings.maxScreenshotDim);

	if (!hasFrames && !hasScreenshot)
	{
		// No opt-in flags: keep running without auto-exit or screenshots.
	}
	else if (hasFrames && !hasScreenshot)
	{
		g_runSettings.exitAfterFrames = true;
		g_runSettings.screenshotRequested = false;
	}
	else if (hasScreenshot)
	{
		if (!hasFrames)
		{
			g_runSettings.frameLimit = 50;
		}
		g_runSettings.screenshotRequested = true;
	}

	g_runSettings.appName = cfg.name ? cfg.name : std::string();
}

void ExampleApp::update()
{
	onUpdate();

	++m_frameIndex;

	bool done = false;
	
	if (m_frameLimit > 0 && m_frameIndex >= m_frameLimit)
	{
		if (m_screenshotRequested)
		{
			requestScreenshot();
		}
		else if (m_exitAfterFrames)
		{
			done = true;
		}
	}

	if (m_screenshotRequested && m_screenshotComplete)
	{
		done = true;
	}

	if (done)
	{
		m_window->close();
	}
}

void ExampleApp::requestScreenshot()
{
	if (m_screenshotInFlight || m_screenshotComplete)
	{
		return;
	}

	m_screenshotInFlight = true;
	Gfx_RequestScreenshot(&ExampleApp::onScreenshot, this);
}

void ExampleApp::onScreenshot(const ColorRGBA8* pixels, Tuple2u size, void* userData)
{
	if (!userData)
	{
		return;
	}

	auto* self = reinterpret_cast<ExampleApp*>(userData);
	self->writeScreenshot(pixels, size);
	self->m_screenshotComplete = true;
}

void ExampleApp::writeScreenshot(const ColorRGBA8* pixels, Tuple2u size)
{
	if (!pixels || size.x == 0 || size.y == 0)
	{
		RUSH_LOG_ERROR("Screenshot capture failed: empty buffer.");
		return;
	}

	const u32 srcWidth = size.x;
	const u32 srcHeight = size.y;

	const u32 maxSide = std::max(srcWidth, srcHeight);
	u32 dstWidth = srcWidth;
	u32 dstHeight = srcHeight;
	if (m_maxScreenshotDim > 0 && maxSide > m_maxScreenshotDim)
	{
		const float scale = float(m_maxScreenshotDim) / float(maxSide);
		dstWidth = std::max(1u, u32(srcWidth * scale + 0.5f));
		dstHeight = std::max(1u, u32(srcHeight * scale + 0.5f));
	}

	const size_t srcSize = size_t(srcWidth) * srcHeight * 4;
	std::vector<u8> srcPixels(srcSize);
	std::memcpy(srcPixels.data(), pixels, srcSize);

	const u8* outputPixels = srcPixels.data();
	std::vector<u8> resizedPixels;
	u8* writablePixels = srcPixels.data();

	if (dstWidth != srcWidth || dstHeight != srcHeight)
	{
		const size_t dstSize = size_t(dstWidth) * dstHeight * 4;
		resizedPixels.resize(dstSize);

		const int ok = stbir_resize_uint8(
		    srcPixels.data(), int(srcWidth), int(srcHeight), 0,
		    resizedPixels.data(), int(dstWidth), int(dstHeight), 0, 4);
		if (!ok)
		{
			RUSH_LOG_ERROR("Screenshot resize failed.");
			return;
		}

		outputPixels = resizedPixels.data();
		writablePixels = resizedPixels.data();
	}

	const size_t outputSize = size_t(dstWidth) * dstHeight * 4;
	for (size_t i = 3; i < outputSize; i += 4)
	{
		writablePixels[i] = 255;
	}

	std::filesystem::path outputPath;
	if (!m_screenshotPath.empty())
	{
		outputPath = std::filesystem::path(m_screenshotPath);
		if (outputPath.is_relative())
		{
			outputPath = getScreenshotDirectory() / outputPath;
		}
	}
	else
	{
		const u32 frameSuffix = (m_frameLimit > 0) ? m_frameLimit : 1;
		outputPath = buildDefaultOutputPath(m_screenshotAppName, frameSuffix);
	}

	if (!ensureDirectoryForPath(outputPath, getScreenshotDirectory()))
	{
		RUSH_LOG_ERROR("Failed to create screenshot directory: %s", outputPath.parent_path().string().c_str());
		return;
	}

	std::string format = getExtensionLower(outputPath);
	if (format.empty())
	{
		format = "png";
		outputPath = outputPath.string() + ".png";
	}
	if (format != "png" && format != "jpg" && format != "jpeg")
	{
		RUSH_LOG_ERROR("Unsupported screenshot format: %s", format.c_str());
		return;
	}

	int writeOk = 0;
	if (format == "png")
	{
		const int stride = int(dstWidth * 4);
		writeOk = stbi_write_png(outputPath.string().c_str(), int(dstWidth), int(dstHeight), 4, outputPixels, stride);
	}
	else
	{
		const int quality = 90;
		writeOk = stbi_write_jpg(outputPath.string().c_str(), int(dstWidth), int(dstHeight), 4, outputPixels, quality);
	}
	if (!writeOk)
	{
		RUSH_LOG_ERROR("Failed to write screenshot: %s", outputPath.string().c_str());
		return;
	}

	RUSH_LOG("Screenshot saved to %s", outputPath.string().c_str());
}

}
