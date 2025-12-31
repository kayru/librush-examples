#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/UtilArray.h>
#include <Rush/UtilColor.h>
#include <Rush/UtilString.h>
#include <Rush/UtilTuple.h>

#include <chrono>
#include <memory>
#include <type_traits>

namespace Rush
{
class GfxContext;
}

namespace Test
{

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
	virtual void        render(GfxContext*) {}
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
};

// GPU test that explicitly requires screenshot capture.
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
		AwaitScreenshot,
		Validate,
		Done,
	};

	void beginNextTest(GfxContext* ctx);
	void renderCurrent(GfxContext* ctx);
	void finishCurrent(GfxContext* ctx);
	void requestScreenshot();
	void updateInternal(GfxContext* ctx);
	void configureFromArgs(int argc, char** argv);
	void listTests() const;
	void listCategories() const;
	bool shouldRunTest(const TestEntry& entry, const char* testName) const;
	void printSummaryAndExit();
	static double elapsedMs(TimePoint start, TimePoint end);

	static void screenshotCallback(const ColorRGBA8* pixels, Tuple2u size, void* userData);

	State                     m_state = State::Init;
	bool                      m_anyFailed = false;
	bool                      m_exitRequested = false;
	size_t                    m_testIndex = 0;
	size_t                    m_skippedCount = 0;
	std::unique_ptr<TestCase> m_current;
	TestConfig                m_currentConfig;
	TestImage                 m_screenshot;
	bool                      m_screenshotReady = false;
	DynamicArray<TestEntry>   m_selectedTests;
	DynamicArray<String>      m_testPatterns;
	DynamicArray<String>      m_categoryPatterns;
	bool                      m_forceNoGraphics = false;
	GfxContext*               m_initContext = nullptr;
	TimePoint                 m_runStart;
	TimePoint                 m_testStart;
	int                       m_exitCode = 0;
};

} // namespace Test
