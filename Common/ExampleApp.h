#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/GfxDevice.h>
#include <Rush/Platform.h>

#include <optional>
#include <string>

namespace Rush
{
class PrimitiveBatch;
class BitmapFontRenderer;
class CommandLine;

class ExampleApp : public Rush::Application
{
	RUSH_DISALLOW_COPY_AND_ASSIGN(ExampleApp);

public:
	ExampleApp();
	~ExampleApp();
	void update() final;
	// Adds the shared options to appArgs (may be null) and parses argv.
	// Returns the exit code when the app must not run.
	static std::optional<int> ParseArgs(AppConfig& cfg, int argc, char** argv, CommandLine* appArgs);

	bool isDesktop() const;

	template <typename T> static inline int Main(AppConfig cfg, int argc, char** argv, CommandLine* appArgs)
	{
		if (const std::optional<int> exitCode = ParseArgs(cfg, argc, argv, appArgs))
		{
			return *exitCode;
		}
		return Platform_Main<T>(cfg);
	}

protected:
	virtual void onUpdate() = 0;
	void renderMessage(const char* message);
	void renderMessage(const char* message, const ColorRGBA8& color, float scale);
	struct SamplerStates
	{
		GfxOwn<GfxSampler> pointClamp;
		GfxOwn<GfxSampler> linearClamp;
		GfxOwn<GfxSampler> linearWrap;
		GfxOwn<GfxSampler> anisotropicWrap;
	} m_samplerStates;

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
inline int Example_Main(AppConfig cfg, int argc, char** argv, CommandLine* appArgs = nullptr)
{
	return ExampleApp::Main<T>(cfg, argc, argv, appArgs);
}
}
