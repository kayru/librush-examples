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

	void onUpdate() override;

private:

	void createScene(GfxContext* ctx);

	GfxOwn<GfxTexture> m_outputImage;
	GfxOwn<GfxBuffer> m_constantBuffer;

	GfxOwn<GfxComputeShader>         m_computeShader;
	GfxOwn<GfxTechnique>             m_technique;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;

	WindowEventListener m_windowEvents;
	std::string m_startupError;
};
