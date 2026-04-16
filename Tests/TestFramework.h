#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/GfxDevice.h>
#include <Rush/MathCommon.h>
#include <Rush/UtilArray.h>
#include <Rush/UtilColor.h>
#include <Rush/UtilString.h>
#include <Rush/UtilTuple.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <type_traits>

namespace Rush
{
class GfxContext;
}

namespace Test
{

static constexpr u32 kTestRenderWidth  = 256;
static constexpr u32 kTestRenderHeight = 256;
static constexpr GfxFormat kTestRenderFormat = GfxFormat_RGBA8_Unorm;

struct TestImage
{
	Tuple2u size = {};
	DynamicArray<ColorRGBA8> pixels;
};

// Per-test configuration flags used by the runner.
struct TestConfig
{
	bool captureScreenshot = false;
	bool requiresGraphics = false;
};

// Result returned by a test validation step.
struct TestResult
{
	bool        passed = false;
	String      message;
	double      executionMs = 0.0;

	static TestResult pass()
	{
		TestResult res;
		res.passed = true;
		return res;
	}
	static TestResult fail(const char* format, ...);
};

// Base class for tests; implement name/description and validate at minimum.
class TestCase
{
public:
	virtual ~TestCase() = default;
	virtual const char* name() const { return m_name.length() ? m_name.c_str() : "UnnamedTest"; }
	virtual const char* description() const { return m_description.length() ? m_description.c_str() : ""; }
	virtual TestConfig  config() const { return {}; }
	virtual void        render(GfxContext*, GfxTexture renderTarget) {}
	virtual TestResult  validate(GfxContext*, const TestImage* image) = 0;

	void setMetadata(const char* name, const char* description)
	{
		m_name = name ? name : "";
		m_description = description ? description : "";
	}

private:
	String m_name;
	String m_description;
};

// GPU test with no screenshot capture by default.
// Provides optional skip infrastructure: set m_skipReason in the constructor
// to mark the test as skipped. The base render() logs the skip message once.
class GfxTestCase : public TestCase
{
public:
	TestConfig config() const override
	{
		TestConfig cfg;
		cfg.requiresGraphics = true;
		cfg.captureScreenshot = false;
		return cfg;
	}

protected:
	bool m_ready = false;
	String m_skipReason;

	void skip(const char* reason) { m_skipReason = reason; }
	bool isReady() const { return m_ready; }
	void logSkipOnce();

private:
	bool m_loggedSkip = false;
};

// GPU test that explicitly requires screenshot capture.
// Same skip infrastructure as GfxTestCase.
class GfxScreenshotTestCase : public TestCase
{
public:
	TestConfig config() const override
	{
		TestConfig cfg;
		cfg.requiresGraphics = true;
		cfg.captureScreenshot = true;
		return cfg;
	}

protected:
	bool m_ready = false;
	String m_skipReason;

	void skip(const char* reason) { m_skipReason = reason; }
	bool isReady() const { return m_ready; }
	void logSkipOnce();

private:
	bool m_loggedSkip = false;
};

// CPU-only test that never renders and skips screenshots.
class CpuTestCase : public TestCase
{
public:
	TestConfig config() const override
	{
		TestConfig cfg;
		cfg.captureScreenshot = false;
		cfg.requiresGraphics = false;
		return cfg;
	}
};

// Validate that a screenshot image is present and has non-zero dimensions.
// Returns a passing result on success, or a descriptive failure.
TestResult validateScreenshot(const TestImage* image);

// Map a buffer, compare its contents against expected u32 values, unmap, and return
// a pass/fail result. The format string controls how mismatched values are printed
// (e.g. "0x%08X" for hex, "%u" for decimal).
TestResult validateBufferU32(GfxBufferArg buf, const u32* expected, size_t count, const char* valueFmt = "0x%08X");

// Pack a ColorRGBA8 into a u32 using the same layout as static_cast<u32>(color).
inline u32 packColor(const ColorRGBA8& color)
{
	return static_cast<u32>(color);
}

// Simple triangle scene for ray tracing tests.
struct SimpleRTScene
{
	GfxOwn<GfxBuffer>                vertexBuffer;
	GfxOwn<GfxBuffer>                indexBuffer;
	GfxOwn<GfxAccelerationStructure> blas;
	GfxOwn<GfxAccelerationStructure> tlas;
};

// Create a single-triangle BLAS+TLAS scene. extraBufferFlags is OR'd into the
// vertex/index buffer flags (e.g. GfxBufferFlags::Storage when GPU reads are needed).
// On failure, outError is set and false is returned.
bool createSimpleTriangleScene(GfxContext* ctx, SimpleRTScene& scene,
    GfxBufferFlags extraBufferFlags, String& outError);

using TestFactory = TestCase* (*)(GfxContext*);

template <typename TestType>
TestType* CreateTestInstance(GfxContext* ctx)
{
	if constexpr (std::is_constructible_v<TestType, GfxContext*>)
	{
		return new TestType(ctx);
	}
	else
	{
		return new TestType();
	}
}

String deriveTestName(const char* typeName);

// Registry entry with factory and optional category tag.
struct TestEntry
{
	TestFactory factory = nullptr;
	const char* category = nullptr;
};

// Global test registry.
class TestRegistry
{
public:
	static void add(TestFactory factory, const char* category);
	static const DynamicArray<TestEntry>& all();
};

// Registers a test factory with the global registry.
#define RUSH_REGISTER_TEST(TestType, Category, DescriptionLiteral)                           \
	namespace                                                                                 \
	{                                                                                         \
	TestCase* Create##TestType(::Rush::GfxContext* ctx)                                       \
	{                                                                                         \
		TestCase* instance = ::Test::CreateTestInstance<TestType>(ctx);                       \
		if (instance)                                                                         \
		{                                                                                     \
			auto name = ::Test::deriveTestName(#TestType);                                    \
			instance->setMetadata(name.c_str(), DescriptionLiteral);                          \
		}                                                                                     \
		return instance;                                                                      \
	}                                                                                         \
	struct TestType##Registrar                                                              \
	{                                                                                        \
		TestType##Registrar() { ::Test::TestRegistry::add(&Create##TestType, Category); }     \
	};                                                                                       \
	static TestType##Registrar s_##TestType##Registrar;                                     \
	}

// Drives test execution and optional filtering.
class TestRunner
{
public:
	TestRunner(GfxContext* ctx, int argc, char** argv);
	void update();
	int  runCpuOnly();
	bool isFinished() const { return m_state == State::Done; }
	bool hasFailures() const { return m_anyFailed; }
	int  exitCode() const { return m_exitCode; }

private:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	enum class State
	{
		Init,
		Render,
		Validate,
		Done,
	};

	void beginNextTest(GfxContext* ctx);
	void renderCurrent(GfxContext* ctx);
	void finishCurrent(GfxContext* ctx);
	void readbackOffscreenTarget(GfxContext* ctx);
	void updateInternal(GfxContext* ctx);
	void configureFromArgs(int argc, char** argv);
	void listTests() const;
	void listCategories() const;
	bool shouldRunTest(const TestEntry& entry, const char* testName) const;
	void printSummaryAndExit();
	static double elapsedMs(TimePoint start, TimePoint end);

	State                     m_state = State::Init;
	bool                      m_anyFailed = false;
	bool                      m_exitRequested = false;
	size_t                    m_testIndex = 0;
	size_t                    m_skippedCount = 0;
	std::unique_ptr<TestCase> m_current;
	TestConfig                m_currentConfig;
	TestImage                 m_screenshot;
	DynamicArray<TestEntry>   m_selectedTests;
	DynamicArray<String>      m_testPatterns;
	DynamicArray<String>      m_categoryPatterns;
	bool                      m_forceNoGraphics = false;
	GfxContext*               m_initContext = nullptr;
	TimePoint                 m_runStart;
	TimePoint                 m_testStart;
	int                       m_exitCode = 0;
	GfxOwn<GfxTexture>        m_offscreenTarget;
	GfxOwn<GfxBuffer>         m_stagingBuffer;
	GfxImageCopyInfo          m_stagingCopyInfo = {};
};

} // namespace Test
