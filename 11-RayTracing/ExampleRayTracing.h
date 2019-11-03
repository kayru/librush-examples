#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/MathTypes.h>
#include <Rush/Platform.h>
#include <Rush/UtilCamera.h>
#include <Rush/Window.h>

#include <Common/ExampleApp.h>
#include <Common/Utils.h>

#include <memory>
#include <mutex>
#include <stdio.h>
#include <string>
#include <thread>
#include <unordered_map>

class ExampleRayTracing : public ExampleApp
{
public:

	ExampleRayTracing();
	~ExampleRayTracing();

	void update() override;

private:

	void createScene(GfxContext* ctx);

	GfxOwn<GfxTexture> m_outputImage;
	GfxOwn<GfxBuffer> m_constantBuffer;

	GfxOwn<GfxRayTracingPipeline>    m_rtPipeline;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;
	GfxOwn<GfxBuffer>                m_sbtBuffer;

	WindowEventListener m_windowEvents;
};
