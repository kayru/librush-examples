#include "TestFramework.h"

#include <Rush/GfxDevice.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <regex>
#include <string>

namespace Test
{

void GfxTestCase::logSkipOnce()
{
	if (!m_loggedSkip && !m_skipReason.empty())
	{
		RUSH_LOG("[Test] SKIP: %s", m_skipReason.c_str());
		m_loggedSkip = true;
	}
}

void GfxScreenshotTestCase::logSkipOnce()
{
	if (!m_loggedSkip && !m_skipReason.empty())
	{
		RUSH_LOG("[Test] SKIP: %s", m_skipReason.c_str());
		m_loggedSkip = true;
	}
}

TestResult validateScreenshot(const TestImage* image)
{
	if (!image || image->pixels.empty())
	{
		return TestResult::fail("Missing screenshot data");
	}
	if (image->size.x == 0 || image->size.y == 0)
	{
		return TestResult::fail("Invalid screenshot size");
	}
	return TestResult::pass();
}

TestResult validateBufferU32(GfxBufferArg buf, const u32* expected, size_t count, const char* valueFmt)
{
	Gfx_Finish();

	const size_t expectedBytes = count * sizeof(u32);
	GfxMappedBuffer mapped = Gfx_MapBuffer(buf);
	if (!mapped.data || mapped.size < expectedBytes)
	{
		Gfx_UnmapBuffer(mapped);
		return TestResult::fail("Failed to map buffer for readback");
	}

	const u32* data = reinterpret_cast<const u32*>(mapped.data);
	for (size_t i = 0; i < count; ++i)
	{
		if (data[i] != expected[i])
		{
			// Build format string: "Mismatch at %zu: got <fmt> expected <fmt>"
			char fmt[128];
			std::snprintf(fmt, sizeof(fmt), "Mismatch at %%zu: got %s expected %s", valueFmt, valueFmt);
			Gfx_UnmapBuffer(mapped);
			return TestResult::fail(fmt, i, data[i], expected[i]);
		}
	}

	Gfx_UnmapBuffer(mapped);
	return TestResult::pass();
}

bool createSimpleTriangleScene(GfxContext* ctx, SimpleRTScene& scene,
    GfxBufferFlags extraBufferFlags, String& outError)
{
	const Vec3 vertices[] = {
		Vec3(-0.5f, -0.5f, 0.0f),
		Vec3( 0.5f, -0.5f, 0.0f),
		Vec3( 0.0f,  0.5f, 0.0f),
	};
	const u32 indices[] = { 0, 1, 2 };

	const GfxBufferFlags bufferFlags = GfxBufferFlags::RayTracing | extraBufferFlags;

	scene.vertexBuffer = Gfx_CreateBuffer(bufferFlags, GfxFormat::GfxFormat_RGB32_Float,
		3, u32(sizeof(Vec3)), vertices);
	scene.indexBuffer = Gfx_CreateBuffer(bufferFlags, GfxFormat::GfxFormat_R32_Uint,
		3, 4, indices);

	if (!scene.vertexBuffer.valid() || !scene.indexBuffer.valid())
	{
		outError = "Failed to create geometry buffers.";
		return false;
	}

	GfxRayTracingGeometryDesc geometryDesc;
	geometryDesc.indexBuffer  = scene.indexBuffer.get();
	geometryDesc.indexFormat  = GfxFormat::GfxFormat_R32_Uint;
	geometryDesc.indexCount   = 3;
	geometryDesc.vertexBuffer = scene.vertexBuffer.get();
	geometryDesc.vertexFormat = GfxFormat::GfxFormat_RGB32_Float;
	geometryDesc.vertexStride = u32(sizeof(Vec3));
	geometryDesc.vertexCount  = 3;
	geometryDesc.isOpaque     = true;

	GfxAccelerationStructureDesc blasDesc;
	blasDesc.type          = GfxAccelerationStructureType::BottomLevel;
	blasDesc.geometryCount = 1;
	blasDesc.geometries    = &geometryDesc;
	scene.blas = Gfx_CreateAccelerationStructure(blasDesc);

	GfxAccelerationStructureDesc tlasDesc;
	tlasDesc.type          = GfxAccelerationStructureType::TopLevel;
	tlasDesc.instanceCount = 1;
	scene.tlas = Gfx_CreateAccelerationStructure(tlasDesc);

	if (!scene.blas.valid() || !scene.tlas.valid())
	{
		outError = "Failed to create acceleration structures.";
		return false;
	}

	GfxOwn<GfxBuffer> instanceBuffer = Gfx_CreateBuffer(GfxBufferFlags::Transient | GfxBufferFlags::RayTracing);
	auto instanceData = Gfx_BeginUpdateBuffer<GfxRayTracingInstanceDesc>(ctx, instanceBuffer.get(), 1);
	instanceData[0].init();
	instanceData[0].accelerationStructureHandle = Gfx_GetAccelerationStructureHandle(scene.blas);
	Gfx_EndUpdateBuffer(ctx, instanceBuffer);

	Gfx_BuildAccelerationStructure(ctx, scene.blas);
	Gfx_AddFullPipelineBarrier(ctx);

	Gfx_BuildAccelerationStructure(ctx, scene.tlas, instanceBuffer);
	Gfx_AddFullPipelineBarrier(ctx);

	return true;
}

String deriveTestName(const char* typeName)
{
	// Strip common suffixes like "Test" or "TestCase" from a C++ type name.
	if (!typeName)
	{
		return String("UnnamedTest");
	}

	const char* suffixes[] = { "TestCase", "Test" };
	size_t length = std::strlen(typeName);

	for (const char* suffix : suffixes)
	{
		const size_t suffixLength = std::strlen(suffix);
		if (length >= suffixLength && std::strcmp(typeName + length - suffixLength, suffix) == 0)
		{
			length -= suffixLength;
			break;
		}
	}

	return String(typeName, length);
}

static DynamicArray<TestEntry>& registryStorage()
{
	static DynamicArray<TestEntry> entries;
	return entries;
}

void TestRegistry::add(TestEntry entry)
{
	registryStorage().push_back(std::move(entry));
}

const DynamicArray<TestEntry>& TestRegistry::all()
{
	return registryStorage();
}

static bool matchesPattern(const char* pattern, const char* value)
{
	if (!pattern || !value)
	{
		return false;
	}

	std::string regexPattern;
	regexPattern.reserve(std::strlen(pattern) * 2 + 2);
	regexPattern.push_back('^');

	for (const char* cur = pattern; *cur != 0; ++cur)
	{
		if (*cur == '*')
		{
			regexPattern.append(".*");
			continue;
		}

		switch (*cur)
		{
		case '.':
		case '^':
		case '$':
		case '+':
		case '?':
		case '(':
		case ')':
		case '[':
		case ']':
		case '{':
		case '}':
		case '|':
		case '\\':
			regexPattern.push_back('\\');
			break;
		default:
			break;
		}
		regexPattern.push_back(*cur);
	}

	regexPattern.push_back('$');
	try
	{
		const std::regex regex(regexPattern);
		return std::regex_match(value, regex);
	}
	catch (const std::regex_error&)
	{
		RUSH_LOG_ERROR("[Test] Invalid pattern: %s", pattern);
		return false;
	}
}

TestRunner::TestRunner(GfxContext* ctx, int argc, char** argv)
{
	if (ctx)
	{
		GfxTextureDesc texDesc = GfxTextureDesc::make2D(
		    kTestRenderWidth, kTestRenderHeight, kTestRenderFormat,
		    GfxUsageFlags::RenderTarget | GfxUsageFlags::TransferSrc);
		texDesc.debugName = "TestOffscreenTarget";
		m_offscreenTarget = Gfx_CreateTexture(texDesc);

		m_stagingCopyInfo = Gfx_GetImageCopyInfo(kTestRenderFormat,
		    {kTestRenderWidth, kTestRenderHeight, 1});

		GfxBufferDesc bufDesc;
		bufDesc.flags       = GfxBufferFlags::Storage;
		bufDesc.stride      = 1;
		bufDesc.count       = m_stagingCopyInfo.bytesPerRow * m_stagingCopyInfo.rowCount;
		bufDesc.hostVisible = true;
		bufDesc.debugName   = "TestStagingBuffer";
		m_stagingBuffer = Gfx_CreateBuffer(bufDesc);
	}

	configureFromArgs(argc, argv);
	m_runStart = Clock::now();
}

void TestRunner::update()
{
	updateInternal(Platform_GetGfxContext());
}

int TestRunner::runCpuOnly()
{
	while (!m_exitRequested)
	{
		updateInternal(nullptr);
	}

	return m_anyFailed ? 1 : 0;
}

void TestRunner::updateInternal(GfxContext* ctx)
{
	const bool canRender = ctx != nullptr;

	if (m_state == State::Done)
	{
		if (!m_exitRequested)
		{
			printSummaryAndExit();
		}
		return;
	}

	switch (m_state)
	{
	case State::Init:
		beginNextTest(ctx);
		break;
	case State::Render:
	{
		const TestConfig& cfg = currentEntry().config;
		if (cfg.requiresGraphics)
		{
			renderCurrent(ctx);
			if (cfg.captureScreenshot)
			{
				readbackOffscreenTarget(ctx);
			}
		}
		m_state = State::Validate;
		break;
	}
	case State::Validate:
		finishCurrent(ctx);
		break;
	case State::Done:
	default:
		break;
	}

	if (m_state != State::Done && canRender)
	{
		GfxPassDesc passDesc;
		passDesc.flags = GfxPassFlags::None;
		Gfx_BeginPass(ctx, passDesc);
		Gfx_EndPass(ctx);
	}
}

void TestRunner::beginNextTest(GfxContext* ctx)
{
	const bool canRender = ctx != nullptr;
	while (m_testIndex < m_selectedTests.size())
	{
		const TestEntry& entry = m_selectedTests[m_testIndex++];
		m_screenshot           = TestImage{};

		if ((m_forceNoGraphics || !canRender) && entry.config.requiresGraphics)
		{
			RUSH_LOG("[Test] SKIP: %s (graphics unavailable)", entry.name.c_str());
			++m_skippedCount;
			continue;
		}

		m_current.reset(entry.factory(ctx));
		RUSH_LOG("[Test] Begin: %s", entry.name.c_str());
		m_testStart = Clock::now();
		m_state = State::Render;
		return;
	}

	m_state = State::Done;
}

void TestRunner::renderCurrent(GfxContext* ctx)
{
	const TestEntry& entry = currentEntry();
	if (!entry.config.requiresGraphics)
	{
		return;
	}

	m_current->render(ctx, m_offscreenTarget.get());
}

void TestRunner::finishCurrent(GfxContext* ctx)
{
	const TestEntry& entry    = currentEntry();
	const TestImage* imagePtr = entry.config.captureScreenshot ? &m_screenshot : nullptr;
	TestResult result         = m_current->validate(ctx, imagePtr);
	const double elapsed      = elapsedMs(m_testStart, Clock::now());
	result.executionMs        = elapsed;

	if (!result.passed)
	{
		m_anyFailed = true;
		RUSH_LOG_ERROR("[Test] FAIL: %s (%.2f ms) - %s", entry.name.c_str(), result.executionMs,
		    result.message.length() == 0 ? "no message" : result.message.c_str());
	}
	else
	{
		RUSH_LOG("[Test] PASS: %s (%.2f ms)", entry.name.c_str(), result.executionMs);
	}

	m_current.reset();
	m_state = State::Init;
}

void TestRunner::readbackOffscreenTarget(GfxContext* ctx)
{
	Gfx_AddImageBarrier(ctx, m_offscreenTarget, GfxResourceState_TransferSrc);
	GfxImageRegion region;
	m_stagingCopyInfo = Gfx_CopyTextureToBuffer(ctx, m_offscreenTarget, region, m_stagingBuffer);
	Gfx_Finish();

	GfxMappedBuffer mapped = Gfx_MapBuffer(m_stagingBuffer);

	m_screenshot.size = {kTestRenderWidth, kTestRenderHeight};
	const u32 pixelCount = kTestRenderWidth * kTestRenderHeight;
	m_screenshot.pixels.resize(pixelCount);

	const u8* src = reinterpret_cast<const u8*>(mapped.data);
	for (u32 y = 0; y < kTestRenderHeight; ++y)
	{
		std::memcpy(&m_screenshot.pixels[y * kTestRenderWidth],
		    src + y * m_stagingCopyInfo.bytesPerRow,
		    kTestRenderWidth * sizeof(ColorRGBA8));
	}

	Gfx_UnmapBuffer(mapped);
}

void TestRunner::printSummaryAndExit()
{
	const double totalMs = elapsedMs(m_runStart, Clock::now());
	RUSH_LOG("[Test] Completed %zu test(s). Result: %s",
	    m_selectedTests.size(), m_anyFailed ? "FAIL" : "PASS");
	RUSH_LOG("[Test] Total time: %.2f ms", totalMs);
	if (m_skippedCount)
	{
		RUSH_LOG("[Test] Skipped %zu test(s).", m_skippedCount);
	}
	m_exitCode = m_anyFailed ? 1 : 0;
	if (Window* window = Platform_GetWindow())
	{
		window->close();
	}
	m_exitRequested = true;
}

double TestRunner::elapsedMs(TimePoint start, TimePoint end)
{
	const auto delta = end - start;
	return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(delta).count();
}

void TestRunner::configureFromArgs(int argc, char** argv)
{
	bool listOnly = false;
	bool listCategoriesOnly = false;

	for (int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		if (!arg)
		{
			continue;
		}

		if (std::strcmp(arg, "--list") == 0)
		{
			listOnly = true;
		}
		else if (std::strcmp(arg, "--list-categories") == 0)
		{
			listCategoriesOnly = true;
		}
		else if (std::strcmp(arg, "--test") == 0 || std::strcmp(arg, "-t") == 0)
		{
			if (i + 1 < argc)
			{
				m_testPatterns.push_back(String(argv[++i]));
			}
		}
		else if (std::strcmp(arg, "--category") == 0 || std::strcmp(arg, "-c") == 0)
		{
			if (i + 1 < argc)
			{
				m_categoryPatterns.push_back(String(argv[++i]));
			}
		}
		else if (std::strcmp(arg, "--no-gfx") == 0 || std::strcmp(arg, "--cpu-only") == 0)
		{
			m_forceNoGraphics = true;
		}
		else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0)
		{
			RUSH_LOG("Usage:");
			RUSH_LOG("  --list               List available tests");
			RUSH_LOG("  --list-categories    List available categories");
			RUSH_LOG("  --test, -t PATTERN   Run tests matching pattern (supports *)");
			RUSH_LOG("  --category, -c CAT   Run tests by category pattern (supports *)");
			RUSH_LOG("  --no-gfx             Skip tests that require graphics");
			RUSH_LOG("  --cpu-only           Alias for --no-gfx");
			m_exitRequested = true;
			m_state = State::Done;
			m_exitCode = 0;
			if (Window* window = Platform_GetWindow())
			{
				window->close();
			}
			return;
		}
	}

	for (const auto& entry : TestRegistry::all())
	{
		if (m_forceNoGraphics && entry.config.requiresGraphics)
		{
			continue;
		}

		if (shouldRunTest(entry))
		{
			m_selectedTests.push_back(entry);
		}
	}

	if (listOnly)
	{
		listTests();
		m_exitRequested = true;
		m_state = State::Done;
		m_exitCode = 0;
		if (Window* window = Platform_GetWindow())
		{
			window->close();
		}
		return;
	}

	if (listCategoriesOnly)
	{
		listCategories();
		m_exitRequested = true;
		m_state = State::Done;
		m_exitCode = 0;
		if (Window* window = Platform_GetWindow())
		{
			window->close();
		}
		return;
	}

	if (m_selectedTests.size() == 0)
	{
		RUSH_LOG_ERROR("[Test] No tests matched the specified filters.");
		m_exitRequested = true;
		m_state = State::Done;
		m_exitCode = 1;
		if (Window* window = Platform_GetWindow())
		{
			window->close();
		}
		return;
	}
}

void TestRunner::listTests() const
{
	for (const auto& entry : TestRegistry::all())
	{
		RUSH_LOG("%s [%s] - %s",
		    entry.name.c_str(),
		    entry.category ? entry.category : "uncategorized",
		    entry.description ? entry.description : "");
	}
}

void TestRunner::listCategories() const
{
	DynamicArray<String> categories;
	for (const auto& entry : TestRegistry::all())
	{
		const char* category = entry.category ? entry.category : "uncategorized";
		bool seen = false;
		for (const auto& existing : categories)
		{
			if (std::strcmp(existing.c_str(), category) == 0)
			{
				seen = true;
				break;
			}
		}
		if (!seen)
		{
			categories.push_back(String(category));
		}
	}

	for (const auto& category : categories)
	{
		RUSH_LOG("%s", category.c_str());
	}
}

bool TestRunner::shouldRunTest(const TestEntry& entry) const
{
	const char* category = entry.category ? entry.category : "uncategorized";

	if (m_testPatterns.size() != 0)
	{
		bool matched = false;
		for (const auto& pattern : m_testPatterns)
		{
			if (matchesPattern(pattern.c_str(), entry.name.c_str()))
			{
				matched = true;
				break;
			}
		}
		if (!matched)
		{
			return false;
		}
	}

	if (m_categoryPatterns.size() != 0)
	{
		bool matched = false;
		for (const auto& pattern : m_categoryPatterns)
		{
			if (matchesPattern(pattern.c_str(), category))
			{
				matched = true;
				break;
			}
		}
		if (!matched)
		{
			return false;
		}
	}

	return true;
}

TestResult TestResult::fail(const char* format, ...)
{
	TestResult res;
	res.passed = false;
	if (!format)
	{
		return res;
	}

	va_list args;
	va_start(args, format);

	va_list argsCopy;
	va_copy(argsCopy, args);
	const int needed = std::vsnprintf(nullptr, 0, format, argsCopy);
	va_end(argsCopy);

	if (needed <= 0)
	{
		va_end(args);
		res.message = format;
		return res;
	}

	const size_t size = static_cast<size_t>(needed);
	res.message.reset(size);
	char* buffer = res.message.data();
	std::vsnprintf(buffer, size + 1, format, args);
	va_end(args);
	return res;
}

} // namespace Test
