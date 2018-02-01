#include <Rush/GfxDevice.h>
#include <Rush/Platform.h>
#include <Rush/Rush.h>

#include <stdio.h>

using namespace Rush;

static void update(void* userData)
{
	GfxContext* gfxContext = Platform_GetGfxContext();

	GfxPassDescr passDescr;
	passDescr.flags          = GfxPassFlags::ClearAll;
	passDescr.clearColors[0] = ColorRGBA8(11, 22, 33);
	Gfx_BeginPass(gfxContext, passDescr);
	Gfx_EndPass(gfxContext);
}

int main()
{
	AppConfig cfg;

	cfg.onStartup  = [](void*) { printf("startup\n"); };
	cfg.onShutdown = [](void*) { printf("shutdown\n"); };
	cfg.onUpdate   = update;

	cfg.name = "Hello World";

	return Platform_Main(cfg);
}
