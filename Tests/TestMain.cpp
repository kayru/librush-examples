#include "TestFramework.h"

#include <Rush/Platform.h>

#include <cstring>

using namespace Test;

namespace
{
bool hasCpuOnlyFlag(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		if (!arg)
		{
			continue;
		}
		if (std::strcmp(arg, "--cpu-only") == 0 || std::strcmp(arg, "--no-gfx") == 0)
		{
			return true;
		}
	}
	return false;
}
}

struct RunnerContext
{
	TestRunner* runner = nullptr;
	int         exitCode = 0;
	int         argc = 0;
	char**      argv = nullptr;
};

int main(int argc, char** argv)
{
	Rush::AppConfig cfg;
	RunnerContext context;

	context.argc = argc;
	context.argv = argv;

	if (hasCpuOnlyFlag(argc, argv))
	{
		TestRunner runner(nullptr, argc, argv);
		return runner.runCpuOnly();
	}

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
		ctx->runner = new TestRunner(initCtx, ctx->argc, ctx->argv);
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
