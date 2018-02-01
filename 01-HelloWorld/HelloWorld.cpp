#include <Rush/Rush.h>
#include <Rush/Platform.h>
#include <Rush/GfxDevice.h>

#include <stdio.h>

using namespace Rush;

static void update()
{
	GfxContext* gfxContext = Platform_GetGfxContext();

	GfxPassDescr passDescr;
	passDescr.flags = GfxPassFlags::ClearAll;
	passDescr.clearColors[0] = ColorRGBA8(11, 22, 33);
	Gfx_BeginPass(gfxContext, passDescr);
	Gfx_EndPass(gfxContext);
}

int main()
{
	AppConfig cfg;

	cfg.onStartup = []() { printf("startup\n"); };
	cfg.onShutdown = []() { printf("shutdown\n"); };
	cfg.onUpdate = update;

	cfg.name = "Hello World";

	return Platform_Main(cfg);
}
