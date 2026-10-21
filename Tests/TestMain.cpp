#include "TestFramework.h"

#include <Common/CommandLine.h>

#include <Rush/Platform.h>

using namespace Test;

struct RunnerContext
{
	TestRunner* runner = nullptr;
	TestOptions options;
	int         exitCode = 0;
};

int main(int argc, char** argv)
{
	RunnerContext context;

	std::vector<std::string> testPatterns;
	std::vector<std::string> categoryPatterns;

	CommandLine args;
	args.addSwitch("list", 0, &context.options.list, "List available tests");
	args.addSwitch("list-categories", 0, &context.options.listCategories, "List available categories");
	args.addList("test", 't', &testPatterns, "PATTERN", "Run tests matching pattern (supports *), repeatable");
	args.addList("category", 'c', &categoryPatterns, "PATTERN", "Run categories matching pattern (supports *), repeatable");
	args.addSwitch("no-gfx", 0, &context.options.noGraphics, "Skip tests that require graphics");
	args.addSwitch("cpu-only", 0, &context.options.noGraphics, "Alias for --no-gfx");

	if (const std::optional<int> exitCode = args.parse(argc, argv))
	{
		return *exitCode;
	}

	for (const std::string& pattern : testPatterns)
	{
		context.options.testPatterns.push_back(String(pattern.c_str()));
	}
	for (const std::string& pattern : categoryPatterns)
	{
		context.options.categoryPatterns.push_back(String(pattern.c_str()));
	}

	if (context.options.noGraphics)
	{
		TestRunner runner(nullptr, context.options);
		return runner.runCpuOnly();
	}

	Rush::AppConfig cfg;
	cfg.name      = "Rush Tests (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 256;
	cfg.height    = 256;
	cfg.resizable = false;
	cfg.argc      = argc;
	cfg.argv      = argv;
	cfg.userData  = &context;

	cfg.onStartup = [](void* userData)
	{
		auto* ctx = reinterpret_cast<RunnerContext*>(userData);
		GfxContext* initCtx = Platform_GetGfxContext();
		ctx->runner = new TestRunner(initCtx, ctx->options);
		if (ctx->runner->isFinished())
		{
			ctx->exitCode = ctx->runner->exitCode();
		}
	};

	cfg.onUpdate = [](void* userData)
	{
		auto* ctx = reinterpret_cast<RunnerContext*>(userData);
		ctx->runner->update();
		if (ctx->runner->isFinished())
		{
			ctx->exitCode = ctx->runner->exitCode();
		}
	};

	cfg.onShutdown = [](void* userData)
	{
		auto* ctx = reinterpret_cast<RunnerContext*>(userData);
		delete ctx->runner;
		ctx->runner = nullptr;
	};

#ifdef RUSH_DEBUG
	cfg.debug = true;
#endif

	Rush::Platform_Main(cfg);
	return context.exitCode;
}
