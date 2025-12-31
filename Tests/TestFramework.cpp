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

void TestRegistry::add(TestFactory factory, const char* category)
{
	TestEntry entry;
	entry.factory = factory;
	entry.category = category;
	registryStorage().push_back(entry);
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
	m_initContext = ctx;
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
	bool rendered = false;

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
		if (m_currentConfig.requiresGraphics)
		{
			renderCurrent(ctx);
			rendered = true;
			m_state = m_currentConfig.captureScreenshot ? State::AwaitScreenshot : State::Validate;
		}
		else
		{
			m_state = State::Validate;
		}
		break;
	case State::AwaitScreenshot:
		if (m_screenshotReady)
		{
			m_state = State::Validate;
		}
		else
		{
			renderCurrent(ctx);
			rendered = true;
		}
		break;
	case State::Validate:
		finishCurrent(ctx);
		break;
	case State::Done:
	default:
		break;
	}

	if (!rendered && m_state != State::Done)
	{
		if (canRender)
		{
			GfxPassDesc passDesc;
			passDesc.flags = GfxPassFlags::None;
			Gfx_BeginPass(ctx, passDesc);
			Gfx_EndPass(ctx);
		}
	}
}

void TestRunner::beginNextTest(GfxContext* ctx)
{
	const bool canRender = ctx != nullptr;
	while (m_testIndex < m_selectedTests.size())
	{
		m_current.reset(m_selectedTests[m_testIndex++].factory(ctx));
		m_currentConfig   = m_current->config();
		m_screenshotReady = false;
		m_screenshot      = TestImage{};

		if ((m_forceNoGraphics || !canRender) && m_currentConfig.requiresGraphics)
		{
			RUSH_LOG("[Test] SKIP: %s (graphics unavailable)", m_current->name());
			m_current.reset();
			++m_skippedCount;
			continue;
		}

		RUSH_LOG("[Test] Begin: %s", m_current->name());
		m_testStart = Clock::now();
		m_state = State::Render;
		return;
	}

	m_state = State::Done;
}

void TestRunner::renderCurrent(GfxContext* ctx)
{
	const bool canRender = ctx != nullptr;
	if (!m_currentConfig.requiresGraphics)
	{
		return;
	}

	m_current->render(ctx);

	if (!m_currentConfig.captureScreenshot)
	{
		if (canRender)
		{
			GfxPassDesc passDesc;
			passDesc.flags = GfxPassFlags::None;
			Gfx_BeginPass(ctx, passDesc);
			Gfx_EndPass(ctx);
		}
	}

	if (m_currentConfig.captureScreenshot && canRender)
	{
		requestScreenshot();
	}
}

void TestRunner::finishCurrent(GfxContext* ctx)
{
	const TestImage* imagePtr = m_currentConfig.captureScreenshot ? &m_screenshot : nullptr;
	TestResult result         = m_current->validate(ctx, imagePtr);
	const double elapsed = elapsedMs(m_testStart, Clock::now());
	result.executionMs = elapsed;

	if (!result.passed)
	{
		m_anyFailed = true;
		RUSH_LOG_ERROR("[Test] FAIL: %s (%.2f ms) - %s", m_current->name(), result.executionMs,
		    result.message.length() == 0 ? "no message" : result.message.c_str());
	}
	else
	{
		RUSH_LOG("[Test] PASS: %s (%.2f ms)", m_current->name(), result.executionMs);
	}

	m_current.reset();
	m_state = State::Init;
}

void TestRunner::requestScreenshot()
{
	Gfx_RequestScreenshot(&TestRunner::screenshotCallback, this);
}

void TestRunner::screenshotCallback(const ColorRGBA8* pixels, Tuple2u size, void* userData)
{
	auto* runner = reinterpret_cast<TestRunner*>(userData);
	if (!runner)
	{
		return;
	}

	runner->m_screenshot.size = size;
	const size_t pixelCount = static_cast<size_t>(size.x) * size.y;
	runner->m_screenshot.pixels.resize(pixelCount);
	if (pixelCount)
	{
		std::memcpy(runner->m_screenshot.pixels.data(), pixels, pixelCount * sizeof(ColorRGBA8));
	}
	runner->m_screenshotReady = true;
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
		std::unique_ptr<TestCase> instance(entry.factory(m_initContext));
		if (!instance)
		{
			continue;
		}

		const char* name = instance->name();
		const TestConfig config = instance->config();
		if (m_forceNoGraphics && config.requiresGraphics)
		{
			continue;
		}

		if (shouldRunTest(entry, name))
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
		std::unique_ptr<TestCase> instance(entry.factory(m_initContext));
		if (!instance)
		{
			continue;
		}
		RUSH_LOG("%s [%s] - %s",
		    instance->name(),
		    entry.category ? entry.category : "uncategorized",
		    instance->description());
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

bool TestRunner::shouldRunTest(const TestEntry& entry, const char* testName) const
{
	const char* category = entry.category ? entry.category : "uncategorized";

	if (m_testPatterns.size() != 0)
	{
		bool matched = false;
		for (const auto& pattern : m_testPatterns)
		{
			if (matchesPattern(pattern.c_str(), testName))
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
