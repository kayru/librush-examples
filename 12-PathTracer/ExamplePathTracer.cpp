#include "ExamplePathTracer.h"

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/UtilTimer.h>
#include <Rush/UtilRandom.h>
#include <Rush/Window.h>

#include <Rush/MathTypes.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilHash.h>
#include <Rush/UtilLog.h>

#include <stb_image.h>
#include <stb_image_write.h>
#include <cgltf.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdio.h>
#include <utility>

#include <Common/ImGuiImpl.h>
#include <Common/ImGuiExt.h>
#include <Common/SceneConfig.h>
#include <Common/Utils.h>
#include <imgui.h>

static AppConfig g_appCfg;

int main(int argc, char** argv)
{
	g_appCfg.name = "PathTracer (" RUSH_RENDER_API_NAME ")";

	g_appCfg.width     = 1920;
	g_appCfg.height    = 1080;
	g_appCfg.argc      = argc;
	g_appCfg.argv      = argv;
	g_appCfg.resizable = true;

	// --out=<png> renders offscreen and exits; run without a window or swapchain.
	std::string headlessOut;
	g_appCfg.headless = getArgString(argc, argv, "out", nullptr, headlessOut);

#ifdef RUSH_DEBUG
	g_appCfg.debug = true;
	Log::breakOnError = true;
#endif

	return Example_Main<ExamplePathTracer>(g_appCfg, argc, argv);
}

ExamplePathTracer::ExamplePathTracer() : ExampleApp(), m_boundingBox(Vec3(0.0f), Vec3(0.0f))
{
	Gfx_SetPresentInterval(0);

	if (m_window) // no window/UI in headless (--out) mode
	{
		ImGuiImpl_Startup(m_window);
		m_windowEvents.setOwner(m_window);
	}

	auto setError = [this](const char* message)
	{
		if (m_startupError.empty())
		{
			m_startupError = message;
		}
	};

	const GfxCapability& caps = Gfx_GetCapability();
	const bool rtAvailable = caps.rayTracingPipeline;

	const u32      whiteTexturePixels[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
	GfxTextureDesc textureDesc           = GfxTextureDesc::make2D(2, 2);

	m_defaultWhiteTextureId = u32(m_textureDescriptors.size());
	m_textureDescriptors.push_back(Gfx_CreateTexture(textureDesc, whiteTexturePixels));

	GfxDescriptorSetDesc materialDescriptorSetDesc;
	materialDescriptorSetDesc.flags = GfxDescriptorSetFlags::TextureArray;
	// Compute is needed by the ray-query pipeline; the set is shared with the RT pipeline.
	materialDescriptorSetDesc.stageFlags = GfxStageFlags::RayTracing | GfxStageFlags::Compute;
	materialDescriptorSetDesc.textures = MaxTextures;
	if (rtAvailable)
	{
		m_materialDescriptorSet = Gfx_CreateDescriptorSet(materialDescriptorSetDesc);
		if (!m_materialDescriptorSet.valid())
		{
			setError("Failed to create material descriptors.");
		}
	}
	else
	{
		setError("Ray tracing is not supported.");
	}

	{
		GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(SceneConstants));
		m_sceneConstantBuffer = Gfx_CreateBuffer(cbDesc);
	}

	{
		GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(TonemapConstants));
		m_tonemapConstantBuffer = Gfx_CreateBuffer(cbDesc);
	}

	{
		// click-to-focus feedback (shader writes the depth, CPU reads it back)
		GfxBufferDesc bd;
		bd.flags       = GfxBufferFlags::Storage;
		bd.hostVisible = true;
		bd.stride      = sizeof(float);
		bd.count       = 1;
		bd.debugName   = "FocusFeedback";
		m_focusFeedbackBuffer = Gfx_CreateBuffer(bd);
	}
	
	if (rtAvailable && m_startupError.empty())
	{
		GfxRayTracingPipelineDesc pipelineDesc;
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		// TODO: Move Metal path tracing to raygen/miss/chit stages once RT pipeline support matures.
		pipelineDesc.rayGen = loadShaderFromFile(RUSH_SHADER_NAME("PathTracer.metal"));
		if (pipelineDesc.rayGen.empty())
		{
			setError("Failed to load Metal path tracer shader.");
		}
#else
		pipelineDesc.rayGen = loadShaderFromFile(RUSH_SHADER_NAME("PathTracer.rgen"));
		pipelineDesc.miss = loadShaderFromFile(RUSH_SHADER_NAME("PathTracer.rmiss"));
		pipelineDesc.closestHit = loadShaderFromFile(RUSH_SHADER_NAME("PathTracer.rchit"));

		if (pipelineDesc.rayGen.empty() || pipelineDesc.miss.empty() || pipelineDesc.closestHit.empty())
		{
			setError("Failed to load ray tracing shaders.");
		}
#endif

		pipelineDesc.bindings.descriptorSets[0].constantBuffers = 1; // scene constants
		pipelineDesc.bindings.descriptorSets[0].samplers = 1; // default sampler
		pipelineDesc.bindings.descriptorSets[0].textures = 1; // envmap
		pipelineDesc.bindings.descriptorSets[0].rwImages = 1; // output image
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
	// Metal argument buffer layout is sequential; extra material buffers shift later bindings.
	// set=0 bindings: 0 cb,1 sampler,2 envmap,3 output,4 ib,5 vb,6 envmap dist,7 material,8 material index,9 focus feedback,10 TLAS.
	pipelineDesc.bindings.descriptorSets[0].rwBuffers = 6; // IB + VB + envmap distribution + materials + material indices + focus feedback
#else
	pipelineDesc.bindings.descriptorSets[0].rwBuffers = 4; // IB + VB + envmap distribution + focus feedback
#endif
		pipelineDesc.bindings.descriptorSets[0].accelerationStructures = 1; // TLAS
		pipelineDesc.bindings.descriptorSets[1] = materialDescriptorSetDesc;

		if (m_startupError.empty())
		{
			m_rtPipeline = Gfx_CreateRayTracingPipeline(pipelineDesc);
			if (!m_rtPipeline.valid())
			{
				setError("Failed to create ray tracing pipeline.");
			}
		}

#if RUSH_RENDER_API != RUSH_RENDER_API_MTL
		// Inline ray-query path: same shading run from a compute shader. Bindings mirror PT_CONFIG_RAYQUERY.
		if (m_startupError.empty() && caps.rayTracingInline)
		{
			GfxOwn<GfxComputeShader> cs = Gfx_CreateComputeShader(loadShaderFromFile(RUSH_SHADER_NAME("PathTracer.comp")));
			if (cs.valid())
			{
				GfxComputePipelineDesc rqDesc;
				rqDesc.cs = cs.get();
				rqDesc.workGroupSize = {8, 8, 1};
				rqDesc.bindings.descriptorSets[0].constantBuffers = 1; // scene constants
				rqDesc.bindings.descriptorSets[0].samplers = 1; // default sampler
				rqDesc.bindings.descriptorSets[0].textures = 1; // envmap
				rqDesc.bindings.descriptorSets[0].rwImages = 1; // output image
				// IB + VB + envmap distribution + materials + material indices + focus feedback
				rqDesc.bindings.descriptorSets[0].rwBuffers = 6;
				rqDesc.bindings.descriptorSets[0].accelerationStructures = 1; // TLAS
				rqDesc.bindings.descriptorSets[1] = materialDescriptorSetDesc;
				m_rayQueryPipeline = Gfx_CreateComputePipeline(rqDesc);
			}
			if (!m_rayQueryPipeline.valid())
			{
				RUSH_LOG("Ray query pipeline unavailable; falling back to RT pipeline only");
			}
		}
#endif
	}

	// The tonemap blit targets the swapchain, so it is only needed for on-screen display.
	if (m_startupError.empty() && m_window)
	{
		GfxShaderSource vsSource = loadShaderFromFile(RUSH_SHADER_NAME("Blit.hlsl"));
		GfxShaderSource psSource = loadShaderFromFile(RUSH_SHADER_NAME("BlitTonemap.hlsl"));
		if (vsSource.empty() || psSource.empty())
		{
			setError("Failed to load tonemap shaders.");
		}
		else
		{
			auto vs = Gfx_CreateVertexShader(vsSource);
			auto ps = Gfx_CreatePixelShader(psSource);

			if (vs.valid() && ps.valid())
			{
				GfxRenderPipelineDesc desc;
				desc.vs = vs.get();
				desc.ps = ps.get();
				desc.bindings.descriptorSets[0].constantBuffers = 1;
				desc.bindings.descriptorSets[0].samplers = 1; // linear sampler
				desc.bindings.descriptorSets[0].textures = 1; // input texture
				desc.setBlendState(GfxBlendStateDesc::makeOpaque());
				desc.renderTarget = Gfx_GetCapability().backBufferDesc;
				m_blitTonemap = Gfx_CreateRenderPipeline(desc);
			}

			if (!m_blitTonemap.valid())
			{
				setError("Failed to create tonemap pipeline.");
			}
		}
	}

	const char* modelFilename = nullptr;
	if (getPositionalArg(g_appCfg.argc, g_appCfg.argv, 0, modelFilename))
	{
		m_statusString            = std::string("Model: ") + modelFilename;
		m_modelFilename           = modelFilename;
		m_valid                   = loadModel(modelFilename);

		if (!m_valid)
		{
			RUSH_LOG("Could not load model from '%s'\n", modelFilename);
		}


		std::string envFilename = std::string(Platform_GetExecutableDirectory()) + "/envmap.hdr";
		std::string envArg;
		if (getArgString(g_appCfg.argc, g_appCfg.argv, "env", nullptr, envArg))
		{
			envFilename = envArg;
		}

		loadEnvmap(envFilename.c_str());

		Vec3  center       = m_boundingBox.center();
		Vec3  dimensions   = m_boundingBox.dimensions();
		float longest_side = dimensions.reduceMax();

		m_boundingBox.m_min = m_worldTransform * m_boundingBox.m_min;
		m_boundingBox.m_max = m_worldTransform * m_boundingBox.m_max;
	}
	else
	{
		ProceduralSceneData procedural;
		buildProceduralScene(procedural);

		m_vertices.clear();
		m_vertices.reserve(procedural.vertices.size());
		for (const auto& v : procedural.vertices)
		{
			Vertex dst;
			dst.position = v.position;
			dst.normal = v.normal;
			dst.texcoord = v.texcoord;
			dst.tangent = Vec4(v.tangent, 1.0f);
			m_vertices.push_back(dst);
		}

		m_indices = procedural.indices;
		m_segments.clear();
		m_segments.reserve(procedural.segments.size());
		for (const auto& seg : procedural.segments)
		{
			MeshSegment outSeg;
			outSeg.material = seg.material;
			outSeg.indexOffset = seg.indexOffset;
			outSeg.indexCount = seg.indexCount;
			m_segments.push_back(outSeg);
		}

		m_materials.clear();
		m_materials.reserve(procedural.materials.size());
		for (const auto& mat : procedural.materials)
		{
			MaterialConstants constants;
			constants.albedoFactor = mat.baseColor;
			constants.albedoTextureId = m_defaultWhiteTextureId;
			constants.specularTextureId = m_defaultWhiteTextureId;
			constants.normalTextureId = 0;
			constants.metallicFactor = 0.0f;
			constants.roughnessFactor = 1.0f;
			constants.reflectance = 0.08f;
			constants.materialMode = MaterialMode::MetallicRoughness;
			m_materials.push_back(constants);
		}

		m_boundingBox = procedural.bounds;
		m_vertexCount = u32(m_vertices.size());
		m_indexCount = u32(m_indices.size());
		m_haveNormals = true;
		m_haveTexcoords = true;
		m_haveTangents = true;
		m_haveNormalMaps = false;
		m_valid = true;
		m_useProceduralScene = true;
		m_statusString = "Procedural scene (cube + plane)";

		std::string envFilename = std::string(Platform_GetExecutableDirectory()) + "/envmap.hdr";
		loadEnvmap(envFilename.c_str());
		createGpuScene();
	}

	loadConfig();

	// Headless render-to-PNG (applied after loadConfig so command-line wins over the saved config):
	//   --out=<png> [--spp=N] [--tracing=rayquery|pipeline] [--w=W] [--h=H]
	if (getArgString(g_appCfg.argc, g_appCfg.argv, "out", nullptr, m_headlessOutPath))
	{
		u32 v = 0;
		if (getArgU32(g_appCfg.argc, g_appCfg.argv, "spp", nullptr, v) && v > 0) { m_headlessSpp = v; }
		if (getArgU32(g_appCfg.argc, g_appCfg.argv, "w", nullptr, v) && v > 0) { m_headlessSize.x = int(v); }
		if (getArgU32(g_appCfg.argc, g_appCfg.argv, "h", nullptr, v) && v > 0) { m_headlessSize.y = int(v); }
		// Default to the RT pipeline unless --tracing=rayquery is given (ignore any saved config mode).
		std::string mode;
		getArgString(g_appCfg.argc, g_appCfg.argv, "tracing", nullptr, mode);
		const bool rq = (mode == "rayquery" || mode == "rq");
		m_settings.m_tracingMode = int(rq ? TracingMode::RayQuery : TracingMode::RayTracingPipeline);
	}

	m_cameraMan = new CameraManipulator();
}

ExamplePathTracer::~ExamplePathTracer()
{
	if (m_window)
	{
		ImGuiImpl_Shutdown();
	}

	for (const auto& it : m_textures)
	{
		delete it.second;
	}

	m_windowEvents.setOwner(nullptr);

	delete m_cameraMan;
}

inline float focalLengthToFov(float focalLength, float sensorSize)
{
	return 2.0f * atanf((sensorSize / 2.0f) / focalLength);
}

struct SensorPreset
{
	const char* name;
	Vec2        sizeMM;
};

// Custom must be last.
static const SensorPreset g_sensorPresets[] = {
	{"Full Frame (36x24)", Vec2(36.0f, 24.0f)},
	{"APS-C (23.6x15.7)", Vec2(23.6f, 15.7f)},
	{"APS-C Canon (22.3x14.9)", Vec2(22.3f, 14.9f)},
	{"Super 35 (24.9x18.7)", Vec2(24.89f, 18.66f)},
	{"Micro 4/3 (17.3x13)", Vec2(17.3f, 13.0f)},
	{"1 inch (13.2x8.8)", Vec2(13.2f, 8.8f)},
	{"Medium Format (44x33)", Vec2(44.0f, 33.0f)},
	{"Custom", Vec2(36.0f, 24.0f)},
};
static constexpr int g_sensorCustomIndex = int(RUSH_COUNTOF(g_sensorPresets)) - 1;

struct FocalLengthPreset
{
	const char* name;
	float       mm;
};

// Custom must be last.
static const FocalLengthPreset g_focalLengthPresets[] = {
	{"14 mm", 14.0f},
	{"20 mm", 20.0f},
	{"24 mm", 24.0f},
	{"28 mm", 28.0f},
	{"35 mm", 35.0f},
	{"50 mm", 50.0f},
	{"70 mm", 70.0f},
	{"85 mm", 85.0f},
	{"105 mm", 105.0f},
	{"135 mm", 135.0f},
	{"200 mm", 200.0f},
	{"300 mm", 300.0f},
	{"400 mm", 400.0f},
	{"600 mm", 600.0f},
	{"Custom", 50.0f},
};
static constexpr int g_focalLengthCustomIndex = int(RUSH_COUNTOF(g_focalLengthPresets)) - 1;

void ExamplePathTracer::onUpdate()
{
	if (!m_headlessOutPath.empty())
	{
		if (m_startupError.empty())
		{
			renderHeadless(Platform_GetGfxContext());
		}
		else
		{
			RUSH_LOG_ERROR("HEADLESS: %s", m_startupError.c_str());
		}
		Platform_RequestExit();
		return;
	}

	if (!m_startupError.empty())
	{
		renderMessage(m_startupError.c_str());
		return;
	}

	TimingScope timingScope(m_stats.cpuTotal);

	m_stats.gpuTotal.add(Gfx_Stats().lastFrameGpuTime);
	m_totalGpuRenderTime += Gfx_Stats().lastFrameGpuTime;

	Gfx_ResetStats();

	const float dt = (float)m_timer.time();
	m_timer.reset();

	if (m_showUI)
	{
		ImGuiImpl_Update(dt);


		ImGui::Begin("Menu");
	bool renderSettingsChanged = false;
	renderSettingsChanged |= ImGui::Checkbox("Use envmap", &m_settings.m_useEnvmap);
	renderSettingsChanged |= ImGui::Checkbox("Neutral background", &m_settings.m_useNeutralBackground);
	renderSettingsChanged |= ImGui::Checkbox("Depth of Field", &m_settings.m_useDepthOfField);
		renderSettingsChanged |= ImGui::Checkbox("Normal mapping", &m_settings.m_useNormalMapping);
		{
			const char* sensorNames[RUSH_COUNTOF(g_sensorPresets)];
			for (int i = 0; i < int(RUSH_COUNTOF(g_sensorPresets)); ++i)
			{
				sensorNames[i] = g_sensorPresets[i].name;
			}
			if (ImGuiExt::Combo("Sensor size", &m_settings.m_sensorPreset, sensorNames, int(RUSH_COUNTOF(g_sensorPresets))))
			{
				if (m_settings.m_sensorPreset != g_sensorCustomIndex)
				{
					m_settings.m_cameraSensorSizeMM = g_sensorPresets[m_settings.m_sensorPreset].sizeMM;
				}
				renderSettingsChanged = true;
			}
			if (m_settings.m_sensorPreset == g_sensorCustomIndex)
			{
				renderSettingsChanged |= ImGuiExt::DragFloat2("Sensor size (mm)", &m_settings.m_cameraSensorSizeMM.x, 0.1f, 1.0f, 200.0f);
			}
		}
		{
			const char* focalNames[RUSH_COUNTOF(g_focalLengthPresets)];
			for (int i = 0; i < int(RUSH_COUNTOF(g_focalLengthPresets)); ++i)
			{
				focalNames[i] = g_focalLengthPresets[i].name;
			}
			if (ImGuiExt::Combo("Focal length", &m_settings.m_focalLengthPreset, focalNames, int(RUSH_COUNTOF(g_focalLengthPresets))))
			{
				if (m_settings.m_focalLengthPreset != g_focalLengthCustomIndex)
				{
					m_settings.m_focalLengthMM = g_focalLengthPresets[m_settings.m_focalLengthPreset].mm;
				}
				renderSettingsChanged = true;
			}
			if (m_settings.m_focalLengthPreset == g_focalLengthCustomIndex)
			{
				renderSettingsChanged |= ImGuiExt::SliderFloat("Focal length (mm)", &m_settings.m_focalLengthMM, 1.0f, 800.0f, ImGuiExt::LabelMode::Above, "%.1f", ImGuiSliderFlags_Logarithmic);
			}
		}
		renderSettingsChanged |= ImGuiExt::SliderFloat("Aperture (f-stop)", &m_settings.m_apertureFStop, 1.0f, 32.0f, ImGuiExt::LabelMode::Above, "f/%.1f", ImGuiSliderFlags_Logarithmic);
		renderSettingsChanged |= ImGuiExt::SliderFloat("Focus distance", &m_settings.m_focusDistance, 0.0f, 1000.0f, ImGuiExt::LabelMode::Above, "%.3f", ImGuiSliderFlags_Logarithmic);
		renderSettingsChanged |= ImGui::Checkbox("Focus assist", &m_settings.m_showFocusAssist);
		if (m_settings.m_showFocusAssist)
		{
			renderSettingsChanged |= ImGuiExt::SliderFloat("Focus assist falloff (px)", &m_settings.m_focusAssistFalloffPx, 0.5f, 64.0f, ImGuiExt::LabelMode::Above, "%.1f", ImGuiSliderFlags_Logarithmic);
		}
		renderSettingsChanged |= ImGuiExt::SliderFloat("Envmap rotation (deg)", &m_settings.m_envmapRotationDegrees, 0.0f, 360.0f);
		ImGuiExt::SliderFloat("Exposure EV100", &m_settings.m_exposureEV100, -10.0f, 10.0f);
		ImGuiExt::SliderFloat("Gamma", &m_settings.m_gamma, 0.25f, 3.0f);
		Vec3 camPos = m_camera.getPosition();
		if (ImGuiExt::DragFloat3("Camera position", &camPos.x, m_cameraScale))
		{
			m_camera.lookAt(camPos, camPos + m_camera.getForward());
			renderSettingsChanged = true;
		}
		if (ImGui::CollapsingHeader("Debug"))
		{
			renderSettingsChanged |= ImGui::Checkbox("Simple shading", &m_settings.m_debugSimpleShading);
			renderSettingsChanged |= ImGui::Checkbox("Disable accumulation", &m_settings.m_debugDisableAccumulation);
			renderSettingsChanged |= ImGui::Checkbox("Hit mask", &m_settings.m_debugHitMask);
			{
				const char* debugVisItems[] = {
					"None",
					"Albedo",
					"Geo normal",
					"Shading normal",
					"Normal mapped",
					"Tangent",
					"Bitangent",
					"Metalness",
					"Roughness",
					"UV",
				};
				int debugVisMode = m_settings.m_debugVisMode;
				if (ImGuiExt::Combo("Visualization", &debugVisMode, debugVisItems, (int)RUSH_COUNTOF(debugVisItems)))
				{
					m_settings.m_debugVisMode = debugVisMode;
					renderSettingsChanged = true;
				}
			}
#if RUSH_RENDER_API != RUSH_RENDER_API_MTL
			if (m_rayQueryPipeline.valid())
			{
				const char* tracingModeItems[] = {"RT pipeline", "Ray query"};
				int tracingMode = m_settings.m_tracingMode;
				if (ImGuiExt::Combo("Tracing mode", &tracingMode, tracingModeItems, (int)RUSH_COUNTOF(tracingModeItems)))
				{
					m_settings.m_tracingMode = tracingMode;
					rebuildAccelerationStructures();
					renderSettingsChanged = true;
				}
			}
#endif
			if (ImGui::Button("Reset accumulation"))
			{
				m_outputImage.reset();
				m_frameIndex = 0;
			}
		}
		ImGui::End();

		if (renderSettingsChanged)
		{
			m_frameIndex = 0;
		}
	}

	Camera oldCamera = m_camera;

	m_camera.setFov(focalLengthToFov(m_settings.m_focalLengthMM, m_settings.m_cameraSensorSizeMM.x));
	m_camera.setAspect(m_window->getAspect());
	m_cameraMan->setMoveSpeed(20.0f * m_cameraScale);

	for (const WindowEvent& e : m_windowEvents)
	{
		switch (e.type)
		{
		case WindowEventType_KeyDown:
			if (e.code == Key_F1)
			{
				m_showUI = !m_showUI;
			}
			else if (e.code == Key_F2)
			{
				saveConfig();
			}
			else if (e.code == Key_F3)
			{
				loadConfig();
			}
			else if (e.code == Key_F4)
			{
				resetCamera();
			}
			else if (e.code == Key_F)
			{
				focusOnCursor();
			}
			else if (e.code == Key_1)
			{
				m_settings.m_useEnvmap = !m_settings.m_useEnvmap;
				m_frameIndex = 0;
			}
			else if (e.code == Key_2)
			{
				m_settings.m_useNeutralBackground = !m_settings.m_useNeutralBackground;
				m_frameIndex = 0;
			}
			else if (e.code == Key_3)
			{
				m_settings.m_debugSimpleShading = !m_settings.m_debugSimpleShading;
				m_frameIndex = 0;
			}
			else if (e.code == Key_4)
			{
				m_settings.m_debugDisableAccumulation = !m_settings.m_debugDisableAccumulation;
				m_frameIndex = 0;
			}
			break;
		case WindowEventType_Scroll:
			if (e.scroll.y > 0)
			{
				m_cameraScale *= 1.25f;
			}
			else
			{
				m_cameraScale *= 0.9f;
			}
			RUSH_LOG("Camera scale: %f", m_cameraScale);
			break;
		default: break;
		}
	}

	if (!m_showUI || (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse))
	{
		m_cameraMan->update(&m_camera, dt, m_window->getKeyboardState(), m_window->getMouseState());
	}

	if (!isDesktop())
	{
		m_virtualGamepad.updateFlyCamera(m_window, m_camera, dt, m_cameraMan->getMoveSpeed());
	}

	if (m_camera.getPosition() != oldCamera.getPosition()
		|| m_camera.getForward() != oldCamera.getForward()
		|| m_camera.getAspect() != oldCamera.getAspect()
		|| m_camera.getFov() != oldCamera.getFov())
	{
		m_frameIndex = 0;
		m_totalGpuRenderTime = 0;
	}
	if (m_settings.m_debugDisableAccumulation)
	{
		m_frameIndex = 0;
	}

	m_windowEvents.clear();

	render();

	m_frameIndex++;

}

void ExamplePathTracer::createRayTracingScene(GfxContext* ctx)
{
	GfxAccelerationStructureDesc tlasDesc;
	tlasDesc.type = GfxAccelerationStructureType::TopLevel;
	tlasDesc.instanceCount = 1;
	m_tlas = Gfx_CreateAccelerationStructure(tlasDesc);

	if (!m_rtInstanceBuffer.valid())
	{
		m_rtInstanceBuffer = Gfx_CreateBuffer(GfxBufferFlags::Transient | GfxBufferFlags::Storage,
		    tlasDesc.instanceCount, (u32)sizeof(GfxRayTracingInstanceDesc));
	}
	{
		Mat4 transform = m_worldTransform.transposed();
		auto instanceData = Gfx_BeginUpdateBuffer<GfxRayTracingInstanceDesc>(ctx, m_rtInstanceBuffer.get(), tlasDesc.instanceCount);
		instanceData[0].init();
		memcpy(instanceData[0].transform, &transform, sizeof(float) * 12);
		instanceData[0].accelerationStructureHandle = Gfx_GetAccelerationStructureHandle(m_blas);
		Gfx_EndUpdateBuffer(ctx, m_rtInstanceBuffer);
	}

	Gfx_BuildAccelerationStructure(ctx, m_blas);
	Gfx_AddFullPipelineBarrier(ctx);

	Gfx_BuildAccelerationStructure(ctx, m_tlas, m_rtInstanceBuffer);
	Gfx_AddFullPipelineBarrier(ctx);
}

float ExamplePathTracer::computeExposure() const
{
	return 1.0f / (1.2f * powf(2.0f, -m_settings.m_exposureEV100));
}

float ExamplePathTracer::outputAspect() const
{
	return m_window ? m_window->getAspect() : (float(m_headlessSize.x) / float(m_headlessSize.y));
}

ExamplePathTracer::SceneConstants ExamplePathTracer::makeSceneConstants(Tuple2i outputSize, u32 frameIndex) const
{
	Mat4 matView = m_camera.buildViewMatrix();
	Mat4 matProj = m_camera.buildProjMatrix();

	SceneConstants constants = {};
	constants.matView = matView.transposed();
	constants.matProj = matProj.transposed();
	constants.matViewProj = (matView * matProj).transposed();
	constants.matViewProjInv = (matView * matProj).inverse().transposed();
	constants.matEnvmapTransform = Mat4::rotationY(toRadians(m_settings.m_envmapRotationDegrees)).transposed();
	constants.cameraPosition = Vec4(m_camera.getPosition());
	constants.frameIndex = frameIndex;
	constants.flags = 0;
	constants.flags |= m_settings.m_useEnvmap ? PT_FLAG_USE_ENVMAP: 0;
	constants.flags |= m_settings.m_useNeutralBackground ? PT_FLAG_USE_NEUTRAL_BACKGROUND : 0;
	constants.flags |= m_settings.m_useDepthOfField ? PT_FLAG_USE_DEPTH_OF_FIELD : 0;
	constants.flags |= m_settings.m_useNormalMapping && m_haveNormals && m_haveTangents && m_haveNormalMaps ? PT_FLAG_USE_NORMAL_MAPPING : 0;
	constants.flags |= m_settings.m_debugSimpleShading ? PT_FLAG_DEBUG_SIMPLE_SHADING : 0;
	constants.flags |= m_settings.m_debugDisableAccumulation ? PT_FLAG_DEBUG_DISABLE_ACCUMULATION : 0;
	constants.flags |= m_settings.m_debugHitMask ? PT_FLAG_DEBUG_HIT_MASK : 0;
	constants.flags |= m_settings.m_showFocusAssist ? PT_FLAG_DEBUG_FOCAL_PLANE : 0;
	constants.debugVisMode = (u32)m_settings.m_debugVisMode;
	constants.focusPickPixel = m_focusPickRequested ? m_focusPickPixel : Tuple2i{-1, -1};
	constants.outputSize = outputSize;
	constants.envmapSize = Gfx_GetTextureDesc(m_envmap).getSize2D();
	constants.cameraSensorSize = m_settings.m_cameraSensorSizeMM / 1000.0f;
	constants.focalLength = m_settings.m_focalLengthMM / 1000.0f;
	constants.focusDistance = m_settings.m_focusDistance;
	// Aperture diameter = focal length / f-number.
	const float apertureDiameterMM = m_settings.m_focalLengthMM / m_settings.m_apertureFStop;
	constants.apertureSize = apertureDiameterMM / 1000.0f;
	constants.focalPlaneFalloffPx = m_settings.m_focusAssistFalloffPx;
	return constants;
}

// CPU port of BlitTonemap.hlsl (Stachowiak neutral tonemap) so the headless PNG matches the viewport.
static float tonemapCurve(float v)
{
	const float c = v + v * v + 0.5f * v * v * v;
	return c / (1.0f + c);
}

static Vec3 neutralTonemap(Vec3 col)
{
	const float yX = 0.2126f * col.x + 0.7152f * col.y + 0.0722f * col.z;
	const float yY = -0.1146f * col.x - 0.3854f * col.y + 0.5f * col.z;
	const float yZ = 0.5f * col.x - 0.4542f * col.y - 0.0458f * col.z;

	const float bt = tonemapCurve(sqrtf(yY * yY + yZ * yZ) * 2.4f);

	float desat = max(0.0f, (bt - 0.7f) * 0.8f);
	desat *= desat;

	const Vec3 desatCol = col + (Vec3(yX) - col) * desat;

	const float tmLuma = tonemapCurve(yX);
	const Vec3  tm0    = col * max(0.0f, tmLuma / max(1e-5f, yX));
	const Vec3  tm1    = Vec3(tonemapCurve(desatCol.x), tonemapCurve(desatCol.y), tonemapCurve(desatCol.z));

	return (tm0 + (tm1 - tm0) * (bt * bt)) * 0.97f;
}

void ExamplePathTracer::renderHeadless(GfxContext* ctx)
{
	if (!m_valid || !m_materialDescriptorSet.valid())
	{
		RUSH_LOG_ERROR("HEADLESS: scene is not ready");
		return;
	}

	const bool rayQuery = useInlineScene();
	if (m_settings.m_tracingMode == int(TracingMode::RayQuery) && !rayQuery)
	{
		RUSH_LOG_ERROR("HEADLESS: ray query requested but unavailable; using RT pipeline");
	}

	const Tuple2i size   = m_headlessSize;
	const u32     width  = u32(size.x);
	const u32     height = u32(size.y);

	m_camera.setFov(focalLengthToFov(m_settings.m_focalLengthMM, m_settings.m_cameraSensorSizeMM.x));
	m_camera.setAspect(float(width) / float(height));

	GfxTextureDesc texDesc = GfxTextureDesc::make2D(size, GfxFormat_RGBA32_Float, GfxUsageFlags::StorageImage_ShaderResource);
	m_outputImage = Gfx_CreateTexture(texDesc);

	rebuildAccelerationStructures();
	createRayTracingScene(ctx);

	Timer renderTimer;
	for (u32 frame = 0; frame < m_headlessSpp; ++frame)
	{
		SceneConstants constants = makeSceneConstants(size, frame);
		Gfx_UpdateBuffer(ctx, m_sceneConstantBuffer, &constants, sizeof(constants));

		Gfx_SetConstantBuffer(ctx, 0, m_sceneConstantBuffer);
		Gfx_SetSampler(ctx, 0, m_samplerStates.anisotropicWrap);
		Gfx_SetTexture(ctx, 0, m_envmap);
		Gfx_SetStorageImage(ctx, 0, m_outputImage);
		Gfx_SetStorageBuffer(ctx, 0, m_indexBuffer);
		Gfx_SetStorageBuffer(ctx, 1, m_vertexBuffer);
		Gfx_SetStorageBuffer(ctx, 2, m_envmapDistribution);
		if (rayQuery)
		{
			if (m_materialBuffer.valid()) { Gfx_SetStorageBuffer(ctx, 3, m_materialBuffer); }
			if (m_materialIndexBuffer.valid()) { Gfx_SetStorageBuffer(ctx, 4, m_materialIndexBuffer); }
			Gfx_SetStorageBuffer(ctx, 5, m_focusFeedbackBuffer);
		}
		else
		{
			Gfx_SetStorageBuffer(ctx, 3, m_focusFeedbackBuffer);
		}
		Gfx_SetDescriptors(ctx, 1, m_materialDescriptorSet);
		Gfx_SetAccelerationStructure(ctx, 0, m_tlas);

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		Gfx_TraceRays(ctx, m_rtPipeline, m_sbtBuffer, width, height);
#else
		if (rayQuery)
		{
			Gfx_SetComputePipeline(ctx, m_rayQueryPipeline);
			Gfx_Dispatch(ctx, (width + 7u) / 8u, (height + 7u) / 8u, 1u);
		}
		else
		{
			Gfx_TraceRays(ctx, m_rtPipeline, m_sbtBuffer, width, height);
		}
#endif

		Gfx_AddFullPipelineBarrier(ctx); // accumulation reads the previous frame's writes
	}

	const GfxImageCopyInfo copyInfo = Gfx_GetImageCopyInfo(GfxFormat_RGBA32_Float, {width, height, 1});
	GfxBufferDesc stagingDesc;
	stagingDesc.flags       = GfxBufferFlags::Storage;
	stagingDesc.stride      = 1;
	stagingDesc.count       = copyInfo.bytesPerRow * copyInfo.rowCount;
	stagingDesc.hostVisible = true;
	GfxOwn<GfxBuffer> staging = Gfx_CreateBuffer(stagingDesc);

	GfxImageRegion fullRegion;
	Gfx_AddImageBarrier(ctx, m_outputImage, GfxResourceState_TransferSrc);
	Gfx_CopyTextureToBuffer(ctx, m_outputImage, fullRegion, staging);
	Gfx_Finish(); // blocks until the GPU has finished all traced frames

	const double renderSeconds = renderTimer.time();

	GfxMappedBuffer mapped = Gfx_MapBuffer(staging);
	if (!mapped.data)
	{
		RUSH_LOG_ERROR("HEADLESS: failed to map readback buffer");
		return;
	}

	std::vector<u8> rgba(size_t(width) * height * 4);
	const float exposure = computeExposure();
	const float invGamma = 1.0f / m_settings.m_gamma;
	for (u32 y = 0; y < height; ++y)
	{
		const float* row = reinterpret_cast<const float*>(reinterpret_cast<const u8*>(mapped.data) + size_t(y) * copyInfo.bytesPerRow);
		// The traced image has row 0 at the bottom (matches the display blit); flip for the PNG.
		u8* dst = rgba.data() + size_t(height - 1 - y) * width * 4;
		for (u32 x = 0; x < width; ++x)
		{
			const Vec3 tonemapped = neutralTonemap(Vec3(row[x * 4 + 0], row[x * 4 + 1], row[x * 4 + 2]) * exposure);
			const float channels[3] = {tonemapped.x, tonemapped.y, tonemapped.z};
			for (u32 c = 0; c < 3; ++c)
			{
				float v = powf(channels[c] < 0.0f ? 0.0f : channels[c], invGamma);
				v = v > 1.0f ? 1.0f : v;
				dst[x * 4 + c] = u8(v * 255.0f + 0.5f);
			}
			dst[x * 4 + 3] = 255;
		}
	}
	Gfx_UnmapBuffer(mapped);

	stbi_write_png(m_headlessOutPath.c_str(), int(width), int(height), 4, rgba.data(), int(width * 4));
	RUSH_LOG("HEADLESS: wrote %s (%s, %ux%u, %u spp)", m_headlessOutPath.c_str(),
		rayQuery ? "rayquery" : "pipeline", width, height, m_headlessSpp);
	RUSH_LOG("HEADLESS: render took %.3f s (%.3f ms/spp)", renderSeconds,
		m_headlessSpp ? (renderSeconds * 1000.0 / m_headlessSpp) : 0.0);
}

void ExamplePathTracer::render()
{
	GfxContext* ctx = Platform_GetGfxContext();

	GfxTextureDesc outputImageDesc = Gfx_GetTextureDesc(m_outputImage);
	const Tuple2i framebufferSize = m_window->getFramebufferSize();
	if (!m_outputImage.valid() || outputImageDesc.getSize2D() != framebufferSize)
	{
		outputImageDesc = GfxTextureDesc::make2D(
			framebufferSize, GfxFormat_RGBA32_Float, GfxUsageFlags::StorageImage_ShaderResource);

		m_outputImage = Gfx_CreateTexture(outputImageDesc);
		m_frameIndex = 0;
	}

	SceneConstants constants = makeSceneConstants(outputImageDesc.getSize2D(), m_frameIndex);

	GfxMarkerScope markerFrame(ctx, "Frame");

	Gfx_UpdateBuffer(ctx, m_sceneConstantBuffer, &constants, sizeof(constants));

	const bool rtReady = (m_rtPipeline.valid() || m_rayQueryPipeline.valid()) && m_materialDescriptorSet.valid();
	if (m_valid && rtReady)
	{
		GfxMarkerScope markerFrame(ctx, "Model");

		if (!m_tlas.valid())
		{
			createRayTracingScene(ctx);
		}

		const bool inlineScene = useInlineScene();

		GfxMarkerScope markerRT(ctx, inlineScene ? "RayQuery" : "RT");
		Gfx_SetConstantBuffer(ctx, 0, m_sceneConstantBuffer);
		Gfx_SetSampler(ctx, 0, m_samplerStates.anisotropicWrap);
		Gfx_SetTexture(ctx, 0, m_envmap);
		Gfx_SetStorageImage(ctx, 0, m_outputImage);
		Gfx_SetStorageBuffer(ctx, 0, m_indexBuffer);
		Gfx_SetStorageBuffer(ctx, 1, m_vertexBuffer);
		Gfx_SetStorageBuffer(ctx, 2, m_envmapDistribution);
		if (inlineScene)
		{
			if (m_materialBuffer.valid())
			{
				Gfx_SetStorageBuffer(ctx, 3, m_materialBuffer);
			}
			if (m_materialIndexBuffer.valid())
			{
				Gfx_SetStorageBuffer(ctx, 4, m_materialIndexBuffer);
			}
			Gfx_SetStorageBuffer(ctx, 5, m_focusFeedbackBuffer);
		}
		else
		{
			Gfx_SetStorageBuffer(ctx, 3, m_focusFeedbackBuffer);
		}
		Gfx_SetDescriptors(ctx, 1, m_materialDescriptorSet);
		Gfx_SetAccelerationStructure(ctx, 0, m_tlas);

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		Gfx_TraceRays(ctx, m_rtPipeline, m_sbtBuffer, outputImageDesc.width, outputImageDesc.height);
#else
		if (inlineScene)
		{
			Gfx_SetComputePipeline(ctx, m_rayQueryPipeline);
			Gfx_Dispatch(ctx, (outputImageDesc.width + 7u) / 8u, (outputImageDesc.height + 7u) / 8u, 1u);
		}
		else
		{
			Gfx_TraceRays(ctx, m_rtPipeline, m_sbtBuffer, outputImageDesc.width, outputImageDesc.height);
		}
#endif

		if (m_focusPickRequested)
		{
			m_focusPickRequested = false;
			Gfx_Finish(); // ensure the shader's feedback write completed
			GfxMappedBuffer mapped = Gfx_MapBuffer(m_focusFeedbackBuffer);
			if (mapped.data)
			{
				const float depth = *reinterpret_cast<const float*>(mapped.data);
				if (depth > 0.0f) // <= 0 = background
				{
					m_settings.m_focusDistance = depth;
					m_frameIndex = 0;
				}
			}
			Gfx_UnmapBuffer(mapped);
		}
	}

	Gfx_AddImageBarrier(ctx, m_outputImage, GfxResourceState_ShaderRead);

	GfxPassDesc passDesc;
	passDesc.flags = GfxPassFlags::ClearAll;
	passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
	Gfx_BeginPass(ctx, passDesc);

	Gfx_SetViewport(ctx, GfxViewport(m_window->getFramebufferSize()));
	Gfx_SetScissorRect(ctx, m_window->getFramebufferSize());

	{
		GfxMarkerScope markerFrame(ctx, "Tonemap");

		TonemapConstants constants = {};
		constants.exposure = computeExposure();
		constants.gamma = m_settings.m_gamma;
		Gfx_UpdateBuffer(ctx, m_tonemapConstantBuffer, &constants, sizeof(constants));

		if (m_blitTonemap.valid())
		{
			Gfx_SetRenderPipeline(ctx, m_blitTonemap);
			Gfx_SetConstantBuffer(ctx, 0, m_tonemapConstantBuffer);
			Gfx_SetSampler(ctx, 0, m_samplerStates.linearClamp);
			Gfx_SetTexture(ctx, 0, m_outputImage);
			Gfx_Draw(ctx, 0, 3);
		}
	}

	if (m_showUI)
	{
		GfxMarkerScope markerFrame(ctx, "UI");

		m_prim->begin2D(m_window->getSize());

		const Vec2 safeOrigin = m_window->getSafeArea().m_min;

		m_font->setScale(1.0f);
		m_font->draw(m_prim, safeOrigin + Vec2(10.0f), m_statusString.c_str());

		char            timingString[1024];
		const GfxStats& stats = Gfx_Stats();
		snprintf(timingString, sizeof(timingString),
		    "GPU time: %.2f ms\n"
		    "CPU time: %.2f ms\n"
		    "Total render time: %.2f sec\n"
		    "Samples per pixel: %d\n",
		    m_stats.gpuTotal.get() * 1000.0f,
		    m_stats.cpuTotal.get() * 1000.0f,
		    m_totalGpuRenderTime,
		    m_frameIndex);

		m_font->draw(m_prim, safeOrigin + Vec2(10.0f, 30.0f), timingString);

		if (!isDesktop())
		{
			m_virtualGamepad.draw(m_prim, m_font, m_window->getSizeFloat());
		}

		m_prim->end2D();

		ImGuiImpl_Render(ctx, m_prim);
	}

	Gfx_EndPass(ctx);
}

void ExamplePathTracer::loadingThreadFunction()
{
	bool hasWork = true;
	while(hasWork)
	{
		TextureData* pendingLoad = nullptr;
		
		m_loadingMutex.lock();
		if (!m_pendingTextures.empty())
		{
			pendingLoad = m_pendingTextures.back();
			m_pendingTextures.pop_back();
		}
		hasWork = !m_pendingTextures.empty() || !m_loadingThreadShouldExit;
		m_loadingMutex.unlock();

		if (pendingLoad)
		{
			m_loadingMutex.lock();
			RUSH_LOG("Loading texture '%s'", pendingLoad->filename.c_str());
			m_loadingMutex.unlock();

			const bool loaded = loadImageWithMips(pendingLoad->filename.c_str(), pendingLoad->desc.format,
			    pendingLoad->desc, pendingLoad->mips);

			m_loadingMutex.lock();
			if (loaded)
			{
				m_loadedTextures.push_back(pendingLoad);
			}
			else
			{
				RUSH_LOG("Failed to load texture '%s'", pendingLoad->filename.c_str());
				m_loadedTextures.push_back(nullptr);
			}
			m_loadingMutex.unlock();
		}
		else
		{
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(100ms);
		}
	}
}

u32 ExamplePathTracer::enqueueLoadTexture(const std::string& filename, GfxFormat format)
{
	auto it = m_textures.find(filename);

	if (it == m_textures.end())
	{
		TextureData* textureData = new TextureData;

		textureData->desc.format = format;
		textureData->filename    = filename;
		textureData->descriptorIndex = u32(m_textureDescriptors.size());
		m_textureDescriptors.push_back(InvalidResourceHandle());

		RUSH_ASSERT(m_textureDescriptors.size() < MaxTextures);

		m_textures[filename] = textureData;

		m_loadingMutex.lock();
		m_pendingTextures.push_back(textureData);
		m_loadingMutex.unlock();

		return textureData->descriptorIndex;
	}
	else
	{
		return it->second->descriptorIndex;
	}

}

template <typename T>
const T* getDataPtr(const cgltf_accessor* attr)
{
	const char* buffer = (const char*)(attr->buffer_view->buffer->data);
	return reinterpret_cast<const T*>(&buffer[attr->offset + attr->buffer_view->offset]);
}

inline Vec3 mulPosition(Vec3 v, float transform[16])
{
	Vec3 r;
	r.x = v.x * transform[0] + v.y * transform[4] + v.z * transform[8] + transform[12];
	r.y = v.x * transform[1] + v.y * transform[5] + v.z * transform[9] + transform[13];
	r.z = v.x * transform[2] + v.y * transform[6] + v.z * transform[10] + transform[14];
	return r;
}

inline Vec3 mulNormal(Vec3 v, float transform[16])
{
	Vec3 r;
	r.x = v.x * transform[0] + v.y * transform[4] + v.z * transform[8];
	r.y = v.x * transform[1] + v.y * transform[5] + v.z * transform[9];
	r.z = v.x * transform[2] + v.y * transform[6] + v.z * transform[10];
	return r;
}

static const char* toString(cgltf_result v)
{
	switch (v)
	{
		case cgltf_result_success: return "success";
		case cgltf_result_data_too_short: return "data_too_short";
		case cgltf_result_unknown_format: return "unknown_format";
		case cgltf_result_invalid_json: return "invalid_json";
		case cgltf_result_invalid_gltf: return "invalid_gltf";
		case cgltf_result_invalid_options: return "invalid_options";
		case cgltf_result_file_not_found: return "file_not_found";
		case cgltf_result_io_error: return "io_error";
		case cgltf_result_out_of_memory: return "out_of_memory";
		default: return "[unknown]";
	}
}

bool ExamplePathTracer::loadModelGLTF(const char* filename)
{
	std::string directory = directoryFromFilename(filename);

	cgltf_options options = {};
	cgltf_data* data = nullptr;
	cgltf_result result = cgltf_parse_file(&options, filename, &data);
	if (result != cgltf_result_success)
	{
		RUSH_LOG_ERROR("GLTF loader error: %s (%d)", toString(result), (int)result);
		cgltf_free(data);
		return false;
	}

	result = cgltf_load_buffers(&options, data, filename);
	if (result != cgltf_result_success)
	{
		RUSH_LOG_ERROR("GLTF loader error: %s (%d)", toString(result), (int)result);
		cgltf_free(data);
		return false;
	}

	RUSH_LOG("Converting mesh from GLTF");

	std::unordered_map<const void*, u32> materialMap;

	{
		MaterialConstants constants;
		constants.albedoTextureId = m_defaultWhiteTextureId;
		constants.albedoFactor = Vec4(1.0f);
		constants.specularFactor = Vec4(1.0f);
		m_materials.push_back(constants);
	}

	for (u32 i=0; i<data->materials_count; ++i)
	{
		const cgltf_material& inMaterial = data->materials[i];

		MaterialConstants constants;
		constants.albedoTextureId = m_defaultWhiteTextureId;

		switch (inMaterial.alpha_mode)
		{
		default:
		case cgltf_alpha_mode_opaque:
			constants.alphaMode = AlphaMode::Opaque;
			break;
		case cgltf_alpha_mode_blend:
			constants.alphaMode = AlphaMode::Blend;
			break;
		case cgltf_alpha_mode_mask:
			constants.alphaMode = AlphaMode::Mask;
			break;
		}

		if (inMaterial.name && !strcmp(inMaterial.name, "outline"))
		{
			// hack to skip outline rendering
			constants.alphaMode = AlphaMode::Blend;
		}

		if (inMaterial.has_pbr_metallic_roughness)
		{
			constants.materialMode = MaterialMode::MetallicRoughness;

			constants.albedoFactor[0] = inMaterial.pbr_metallic_roughness.base_color_factor[0];
			constants.albedoFactor[1] = inMaterial.pbr_metallic_roughness.base_color_factor[1];
			constants.albedoFactor[2] = inMaterial.pbr_metallic_roughness.base_color_factor[2];
			constants.albedoFactor[3] = 1.0;

			constants.metallicFactor = inMaterial.pbr_metallic_roughness.metallic_factor;
			constants.roughnessFactor = inMaterial.pbr_metallic_roughness.roughness_factor;

			if (auto texture = inMaterial.pbr_metallic_roughness.base_color_texture.texture)
			{
				if (texture->image && texture->image->uri)
				{
					std::string filename = directory + std::string(texture->image->uri);
					fixDirectorySeparatorsInplace(filename);
					constants.albedoTextureId = enqueueLoadTexture(filename, GfxFormat::GfxFormat_RGBA8_sRGB);
				}
			}

			if (auto texture = inMaterial.pbr_metallic_roughness.metallic_roughness_texture.texture)
			{
				if (texture->image && texture->image->uri)
				{
					std::string filename = directory + std::string(texture->image->uri);
					fixDirectorySeparatorsInplace(filename);
					constants.specularTextureId = enqueueLoadTexture(filename, GfxFormat::GfxFormat_RGBA8_Unorm);
				}
			}
		}
		else if (inMaterial.has_pbr_specular_glossiness)
		{
			constants.materialMode = MaterialMode::SpecularGlossiness;

			constants.albedoFactor[0] = inMaterial.pbr_specular_glossiness.diffuse_factor[0];
			constants.albedoFactor[1] = inMaterial.pbr_specular_glossiness.diffuse_factor[1];
			constants.albedoFactor[2] = inMaterial.pbr_specular_glossiness.diffuse_factor[2];

			constants.specularFactor[0] = inMaterial.pbr_specular_glossiness.specular_factor[0];
			constants.specularFactor[1] = inMaterial.pbr_specular_glossiness.specular_factor[1];
			constants.specularFactor[2] = inMaterial.pbr_specular_glossiness.specular_factor[2];

			constants.roughnessFactor = inMaterial.pbr_specular_glossiness.glossiness_factor;

			if (auto texture = inMaterial.pbr_specular_glossiness.diffuse_texture.texture)
			{
				if (texture->image && texture->image->uri)
				{
					std::string filename = directory + std::string(texture->image->uri);
					fixDirectorySeparatorsInplace(filename);
					constants.albedoTextureId = enqueueLoadTexture(filename, GfxFormat::GfxFormat_RGBA8_sRGB);
				}
			}

			if (auto texture = inMaterial.pbr_specular_glossiness.specular_glossiness_texture.texture)
			{
				if (texture->image && texture->image->uri)
				{
					std::string filename = directory + std::string(texture->image->uri);
					fixDirectorySeparatorsInplace(filename);
					constants.specularTextureId = enqueueLoadTexture(filename, GfxFormat::GfxFormat_RGBA8_sRGB);
				}
			}
		}

		if (auto texture = inMaterial.normal_texture.texture)
		{
			if (texture->image && texture->image->uri)
			{
				m_haveNormalMaps = true;
				std::string filename = directory + std::string(texture->image->uri);
				fixDirectorySeparatorsInplace(filename);
				constants.normalTextureId = enqueueLoadTexture(filename, GfxFormat::GfxFormat_RGBA8_Unorm);
			}
		}

		materialMap[&inMaterial] = u32(m_materials.size());

		m_materials.push_back(constants);
	}

	m_boundingBox.expandInit();

	for (u32 ni = 0; ni < data->nodes_count; ++ni)
	{
		const cgltf_node& node = data->nodes[ni];
		if (!node.mesh)
		{
			continue;
		}

		float transform[16];
		cgltf_node_transform_world(&node, transform);

		const cgltf_mesh& mesh = *node.mesh;

		for (u32 pi = 0; pi < mesh.primitives_count; ++pi)
		{
			const cgltf_primitive& prim = mesh.primitives[pi];
			const cgltf_accessor* aidx = prim.indices;
			const cgltf_accessor* apos = nullptr;
			const cgltf_accessor* anor = nullptr;
			const cgltf_accessor* atex = nullptr;
			const cgltf_accessor* atan = nullptr;

			for (u32 ai = 0; ai < prim.attributes_count; ++ai)
			{
				const cgltf_attribute& attr = prim.attributes[ai];
				if (attr.type == cgltf_attribute_type_position && attr.index == 0)
				{
					apos = attr.data;
				}
				else if (attr.type == cgltf_attribute_type_normal && attr.index == 0)
				{
					anor = attr.data;
				}
				else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0)
				{
					atex = attr.data;
				}
				else if (attr.type == cgltf_attribute_type_tangent && attr.index == 0)
				{
					atan = attr.data;
				}
			}

			const u32 firstIndex = u32(m_indices.size());
			const u32 firstVertex = u32(m_vertices.size());

			MeshSegment seg;
			seg.material = materialMap[prim.material];
			seg.indexOffset = firstIndex;
			seg.indexCount = u32(aidx->count);

			if (m_materials[seg.material].alphaMode == AlphaMode::Blend)
			{
				continue; // transparent materials not implemented
			}

			m_segments.push_back(seg);

			if (!aidx || !apos)
			{
				continue;
			}

			if (aidx->component_type == cgltf_component_type_r_32u)
			{
				auto ptr = getDataPtr<u32>(aidx);
				for (u32 i = 0; i < aidx->count; ++i)
				{
					m_indices.push_back(firstVertex + ptr[i]);
				}
			}
			else
			{
				auto ptr = getDataPtr<u16>(aidx);
				for (u32 i = 0; i < aidx->count; ++i)
				{
					m_indices.push_back(firstVertex + ptr[i]);
				}
			}

			// convert winding due to coordinate system difference
			const u64 triCount = (m_indices.size() - firstIndex) / 3;
			for (u64 i = 0; i < triCount; ++i)
			{
				std::swap(m_indices[firstIndex + i * 3 + 1], m_indices[firstIndex + i * 3 + 2]);
			}

			// positions

			if (apos)
			{
				auto ptr = getDataPtr<float>(apos);
				for (u32 i = 0; i < apos->count; ++i)
				{
					Vertex v = {};
					v.position = mulPosition(Vec3(ptr), transform);
					v.position.x = -v.position.x;
					m_vertices.push_back(v);
					ptr += apos->stride / 4;
					m_boundingBox.expand(v.position);
				}
			}

			// normals

			if (anor && anor->count == apos->count)
			{
				m_haveNormals = true;
				auto ptr = getDataPtr<float>(anor);
				for (u32 i = 0; i < anor->count; ++i)
				{
					m_vertices[firstVertex + i].normal = mulNormal(Vec3(ptr), transform);
					m_vertices[firstVertex + i].normal.x = -m_vertices[firstVertex + i].normal.x;
					ptr += anor->stride / 4;
				}
			}

			// tangents

			if (atan && apos && atan->count == apos->count)
			{
				m_haveTangents = true;
				auto ptr     = getDataPtr<float>(atan);
				for (u32 i = 0; i < atan->count; ++i)
				{
					m_vertices[firstVertex + i].tangent = Vec4(ptr);
					ptr += atan->stride / 4;
				}
			}

			// texcoords

			if (atex && atex->count == apos->count)
			{
				m_haveTexcoords = true;
				auto ptr = getDataPtr<float>(atex);
				for (u32 i = 0; i < atex->count; ++i)
				{
					m_vertices[firstVertex + i].texcoord = Vec2(ptr);
					ptr += atex->stride / 4;
				}
			}

			if (!m_haveNormals)
			{
				// TODO: compute face normals
			}
		}
	}

	cgltf_free(data);

	m_vertexCount = (u32)m_vertices.size();
	m_indexCount = (u32)m_indices.size();

	createGpuScene();

	return true;
}

bool ExamplePathTracer::loadModelObj(const char* filename)
{
	ProceduralSceneData data;
	if (!loadObjScene(filename, data))
	{
		return false;
	}

	RUSH_LOG("Converting mesh from OBJ");

	for (const auto& mat : data.materials)
	{
		MaterialConstants constants;
		constants.albedoFactor    = mat.baseColor;
		constants.albedoTextureId = mat.diffuseTextureName.empty()
		    ? m_defaultWhiteTextureId
		    : enqueueLoadTexture(mat.diffuseTextureName, GfxFormat::GfxFormat_RGBA8_sRGB);
		m_materials.push_back(constants);
	}

	if (data.materials.empty())
	{
		MaterialConstants constants;
		constants.albedoTextureId = m_defaultWhiteTextureId;
		m_materials.push_back(constants);
	}

	m_vertices.reserve(data.vertices.size());
	for (const auto& v : data.vertices)
	{
		Vertex dst;
		dst.position = v.position;
		dst.normal   = v.normal;
		dst.texcoord = v.texcoord;
		dst.tangent  = Vec4(v.tangent, 0.0f);
		m_vertices.push_back(dst);
	}

	m_indices = data.indices;

	m_segments.reserve(data.segments.size());
	for (const auto& seg : data.segments)
	{
		MeshSegment outSeg;
		outSeg.material    = u32(max(0, int(seg.material))); // untextured runs (raw -1) map to material 0
		outSeg.indexOffset = seg.indexOffset;
		outSeg.indexCount  = seg.indexCount;
		m_segments.push_back(outSeg);
	}

	m_boundingBox = data.bounds;
	m_vertexCount = (u32)m_vertices.size();
	m_indexCount  = (u32)m_indices.size();

	createGpuScene();

	return true;
}

void ExamplePathTracer::createGpuScene()
{
	RUSH_LOG("Uploading mesh to GPU");

	GfxBufferDesc vbDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, m_vertexCount, sizeof(Vertex));
	m_vertexBuffer = Gfx_CreateBuffer(vbDesc, m_vertices.data());

	const u32 ibStride = 4;
	GfxBufferDesc ibDesc(GfxBufferFlags::Storage, GfxFormat_R32_Uint, m_indexCount, ibStride);
	m_indexBuffer = Gfx_CreateBuffer(ibDesc, m_indices.data());

	if (!m_pendingTextures.empty())
	{
		const u32 textureCount = u32(m_pendingTextures.size());
		RUSH_LOG("Loading %d textures", textureCount);

		std::vector<std::thread> loadingThreads;

		u32 threadCount = std::thread::hardware_concurrency();
		for (u32 i = 0; i < threadCount; ++i)
		{
			loadingThreads.push_back(std::thread([this]() { this->loadingThreadFunction(); }));
		}

		m_loadingThreadShouldExit = true;
		loadingThreadFunction();
		for (auto& it : loadingThreads)
		{
			it.join();
		}

		RUSH_LOG("Uploading textures to GPU");

		while (!m_loadedTextures.empty())
		{
			TextureData* textureData = nullptr;

			textureData = m_loadedTextures.back();
			m_loadedTextures.pop_back();

			if (textureData)
			{
				GfxTextureData mipData[16] = {};
				for (u32 i = 0; i < textureData->desc.mips; ++i)
				{
					mipData[i].pixels = textureData->mips[i].data();
					mipData[i].mip = i;
				}

				u32 descriptorIndex = textureData->descriptorIndex;
				auto texture = Gfx_CreateTexture(textureData->desc, mipData, textureData->desc.mips);
				if (!texture.valid())
				{
					RUSH_LOG_ERROR("Failed to create texture '%s' (format %u)",
					    textureData->filename.c_str(), u32(textureData->desc.format));
					continue;
				}
				m_textureDescriptors[descriptorIndex] = std::move(texture);
			}
		}
	}

	std::vector<GfxTexture> textureDescriptors;
	textureDescriptors.resize(MaxTextures, m_textureDescriptors[m_defaultWhiteTextureId].get());

	for (size_t i = 0; i < m_textureDescriptors.size(); ++i)
	{
		if (m_textureDescriptors[i].get().valid())
		{
			textureDescriptors[i] = m_textureDescriptors[i].get();
		}
	}

	if (m_materialDescriptorSet.valid())
	{
		Gfx_UpdateDescriptorSet(m_materialDescriptorSet,
			nullptr, // constant buffers
			nullptr, // samplers
			textureDescriptors.data(),
			nullptr, // storage images
			nullptr  // storage buffers
		);
	}

	if (!m_materials.empty())
	{
		GfxBufferDesc materialDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, u32(m_materials.size()), sizeof(MaterialConstants));
		m_materialBuffer = Gfx_CreateBuffer(materialDesc, m_materials.data());
	}

	const u32 triangleCount = m_indexCount / 3;
	if (triangleCount > 0)
	{
		std::vector<u32> materialIndices(triangleCount, 0);
		for (const auto& segment : m_segments)
		{
			const u32 start = segment.indexOffset / 3;
			const u32 count = segment.indexCount / 3;
			const u32 end = std::min(start + count, triangleCount);
			for (u32 tri = start; tri < end; ++tri)
			{
				materialIndices[tri] = segment.material;
			}
		}

		GfxBufferDesc indexDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, triangleCount, sizeof(u32));
		m_materialIndexBuffer = Gfx_CreateBuffer(indexDesc, materialIndices.data());
	}

	createBottomLevelAccelerationStructure();
}

bool ExamplePathTracer::useInlineScene() const
{
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
	return true;
#else
	return m_settings.m_tracingMode == int(TracingMode::RayQuery) && m_rayQueryPipeline.valid();
#endif
}

void ExamplePathTracer::createBottomLevelAccelerationStructure()
{
	const bool rtReady = (m_rtPipeline.valid() || m_rayQueryPipeline.valid()) && m_materialDescriptorSet.valid();
	if (!rtReady)
	{
		return;
	}

	RUSH_LOG("Creating ray tracing data");

	const bool      inlineScene = useInlineScene();
	const u32       ibStride    = 4;
	const GfxFormat ibFormat    = GfxFormat_R32_Uint;

	DynamicArray<GfxRayTracingGeometryDesc> geometries;

	if (inlineScene)
	{
		// Single geometry over the whole mesh: primId is a global triangle index and the
		// per-triangle material index buffer resolves the material inline (matches Metal).
		geometries.reserve(1);

		GfxRayTracingGeometryDesc geometryDesc;
		geometryDesc.indexBuffer       = m_indexBuffer.get();
		geometryDesc.indexFormat       = ibFormat;
		geometryDesc.indexCount        = m_indexCount;
		geometryDesc.indexBufferOffset = 0;
		geometryDesc.vertexBuffer      = m_vertexBuffer.get();
		geometryDesc.vertexFormat      = GfxFormat::GfxFormat_RGB32_Float;
		geometryDesc.vertexStride      = sizeof(Vertex);
		geometryDesc.vertexCount       = m_vertexCount;
		geometries.push_back(geometryDesc);
	}
	else
	{
		// One geometry per segment; the material for each is baked into its SBT hit-group record.
		geometries.reserve(m_segments.size());

		const GfxCapability& caps             = Gfx_GetCapability();
		const u32            shaderHandleSize = caps.rtShaderHandleSize;
		const u32 sbtRecordSize = alignCeiling(u32(shaderHandleSize + sizeof(MaterialConstants)), shaderHandleSize);

		DynamicArray<u8> sbtData;
		sbtData.resize(m_segments.size() * sbtRecordSize);

		const u8* hitGroupHandle = Gfx_GetRayTracingShaderHandle(m_rtPipeline, GfxRayTracingShaderType::HitGroup, 0);

		for (size_t i = 0; i < m_segments.size(); ++i)
		{
			const auto& segment = m_segments[i];

			GfxRayTracingGeometryDesc geometryDesc;
			geometryDesc.indexBuffer       = m_indexBuffer.get();
			geometryDesc.indexFormat       = ibFormat;
			geometryDesc.indexCount        = segment.indexCount;
			geometryDesc.indexBufferOffset = segment.indexOffset * ibStride;
			geometryDesc.vertexBuffer      = m_vertexBuffer.get();
			geometryDesc.vertexFormat      = GfxFormat::GfxFormat_RGB32_Float;
			geometryDesc.vertexStride      = sizeof(Vertex);
			geometryDesc.vertexCount       = m_vertexCount;
			geometries.push_back(geometryDesc);

			u8* sbtRecord          = &sbtData[i * sbtRecordSize];
			u8* sbtRecordConstants = sbtRecord + shaderHandleSize;

			MaterialConstants materialConstants = m_materials[segment.material];
			materialConstants.firstIndex        = segment.indexOffset;

			memcpy(sbtRecord, hitGroupHandle, shaderHandleSize);
			memcpy(sbtRecordConstants, &materialConstants, sizeof(materialConstants));
		}

		m_sbtBuffer = Gfx_CreateBuffer(
		    GfxBufferFlags::Storage | GfxBufferFlags::RayTracing, u32(sbtData.size() / sbtRecordSize), sbtRecordSize, sbtData.data());
	}

	GfxAccelerationStructureDesc blasDesc;
	blasDesc.type          = GfxAccelerationStructureType::BottomLevel;
	blasDesc.geometryCount = u32(geometries.size());
	blasDesc.geometries    = geometries.data();
	m_blas                 = Gfx_CreateAccelerationStructure(blasDesc);

	m_blasIsInline = inlineScene;
}

void ExamplePathTracer::rebuildAccelerationStructures()
{
	// Mode switch changes the BLAS layout (single vs per-segment); rebuild BLAS now, TLAS lazily in render().
	m_tlas      = {};
	m_sbtBuffer = {};
	m_blas      = {};
	createBottomLevelAccelerationStructure();
	m_frameIndex = 0;
}

void ExamplePathTracer::resetCamera()
{
	m_camera = makeFramedCamera(m_boundingBox, outputAspect());
	m_frameIndex = 0;
}

void ExamplePathTracer::focusOnCursor()
{
	if (m_showUI && ImGui::GetIO().WantCaptureMouse)
	{
		return;
	}

	// feedback is only written in the normal render path, not the debug views
	if (m_settings.m_debugSimpleShading || m_settings.m_debugHitMask || m_settings.m_debugVisMode != 0)
	{
		return;
	}

	const Vec2 windowSize = m_window->getSizeFloat();
	if (windowSize.x <= 0.0f || windowSize.y <= 0.0f)
	{
		return;
	}

	// flip Y: the blit displays the traced image upside down (row 0 at the bottom)
	const Vec2 mousePos = m_window->getMouseState().pos;
	if (mousePos.x < 0.0f || mousePos.y < 0.0f || mousePos.x >= windowSize.x || mousePos.y >= windowSize.y)
	{
		return;
	}

	const Tuple2i fbSize = m_window->getFramebufferSize();
	const int px = int((mousePos.x / windowSize.x) * float(fbSize.x));
	const int py = int((1.0f - mousePos.y / windowSize.y) * float(fbSize.y));

	m_focusPickPixel = Tuple2i{px < fbSize.x ? px : fbSize.x - 1, py < fbSize.y ? py : fbSize.y - 1};
	m_focusPickRequested = true;
}

// Adding/removing/reordering Settings fields needs no bump (tagged format).
// Bump only on incompatible semantic changes or a Camera blob layout change.
static constexpr u32 kConfigVersion = 1;

const char* ExamplePathTracer::configModelName() const
{
	return (m_useProceduralScene || m_modelFilename.empty()) ? nullptr : m_modelFilename.c_str();
}

void ExamplePathTracer::saveConfig()
{
	saveSceneConfig("pathtracer", configModelName(), kConfigVersion, m_camera, m_settings);
}

void ExamplePathTracer::loadConfig()
{
	// Establish scene defaults first; the file overwrites whatever it carries.
	if (m_useProceduralScene)
	{
		const float aspect = outputAspect();
		const float fov    = 1.0f;
		m_camera = Camera(aspect, fov, 0.25f);
		const Vec3 center = m_boundingBox.center();
		m_camera.lookAt(center + Vec3(2.0f, 2.5f, 2.0f), center);
	}
	else
	{
		resetCamera();
	}

	loadSceneConfig("pathtracer", configModelName(), kConfigVersion, m_camera, m_settings);

	// Validate the loaded tracing mode against this device/build, then bring the acceleration
	// structures in line with it (createGpuScene built them for the pre-load default mode).
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
	m_settings.m_tracingMode = int(TracingMode::RayQuery);
#else
	if (m_settings.m_tracingMode == int(TracingMode::RayQuery) && !m_rayQueryPipeline.valid())
	{
		m_settings.m_tracingMode = int(TracingMode::RayTracingPipeline);
	}
#endif
	if (m_blas.valid() && useInlineScene() != m_blasIsInline)
	{
		rebuildAccelerationStructures();
	}

	m_frameIndex = 0;
}

bool ExamplePathTracer::loadModel(const char* filename)
{
	RUSH_LOG("Loading model '%s'", filename);

	if (endsWith(filename, ".obj"))
	{
		return loadModelObj(filename);
	}
	if (endsWith(filename, ".gltf"))
	{
		return loadModelGLTF(filename);
	}
	else
	{
		RUSH_LOG_ERROR("Unsupported model file extension.");
		return false;
	}
}

// Discrete probability distribution sampling based on alias method
// http://www.keithschwarz.com/darts-dice-coins
template <typename T>
struct DiscreteDistribution
{
	typedef std::pair<T, size_t> Cell;

	DiscreteDistribution(const T* weights, size_t count, T weightSum)
	{
		std::vector<Cell> large;
		std::vector<Cell> small;
		for (size_t i = 0; i < count; ++i)
		{
			T p = weights[i] * count / weightSum;
			if (p < T(1)) small.push_back({ p, i });
			else large.push_back({ p, i });
		}

		m_cells.resize(count, { T(0), 0 });

		while (large.size() && small.size())
		{
			auto l = small.back(); small.pop_back();
			auto g = large.back(); large.pop_back();
			m_cells[l.second].first = l.first;
			m_cells[l.second].second = g.second;
			g.first = (l.first + g.first) - T(1);
			if (g.first < T(1))
			{
				small.push_back(g);
			}
			else
			{
				large.push_back(g);
			}
		}

		while (large.size())
		{
			auto g = large.back(); large.pop_back();
			m_cells[g.second].first = T(1);
		}

		while (small.size())
		{
			auto l = small.back(); small.pop_back();
			m_cells[l.second].first = T(1);
		}
	}

	std::vector<Cell> m_cells;
};

inline double latLongTexelArea(Vec2 pos, Vec2 imageSize)
{
	Vec2 uv0 = pos / imageSize;
	Vec2 uv1 = (pos + Vec2(1.0f)) / imageSize;

	double theta0 = Pi * (uv0.x * 2.0 - 1.0);
	double theta1 = Pi * (uv1.x * 2.0 - 1.0);

	double phi0 = Pi * (uv0.y - 0.5);
	double phi1 = Pi * (uv1.y - 0.5);

	return abs(theta1 - theta0) * abs(sin(phi1) - sin(phi0));
}

void ExamplePathTracer::loadEnvmap(const char* filename)
{
	struct EnvmapCell
	{
		float p;
		u32 i;
	};

	FileIn f(filename);
	if (f.valid())
	{
		RUSH_LOG("Loading envmap '%s'", filename);

		int width, height, comp;
		Vec4* img = (Vec4*)stbi_loadf(filename, &width, &height, &comp, 4);

		const Vec2 imageSize = Vec2(float(width), float(height));

		std::vector<double> weights;
		std::vector<double> areas;

		const u64 pixelCount = u64(width) * height;
		weights.reserve(pixelCount);
		areas.reserve(pixelCount);

		double weightSum = 0;
		for (u64 i = 0; i < pixelCount; ++i)
		{
			//img[i] = Vec4(1.0);

			u32 x = u32(i % width);
			u32 y = u32(i / width);
			Vec2 pixelPos = Vec2(float(x), float(y));

			double pixelIntensity = double(img[i].xyz().reduceMax());
			double pixelArea = latLongTexelArea(pixelPos, imageSize);
			double weight = pixelArea * pixelIntensity;

			weights.push_back(weight);
			areas.push_back(pixelArea);
			weightSum += weight;
		}

		for (u64 i = 0; i < pixelCount; ++i)
		{
			double pdf = (weights[i] / weightSum) / areas[i];
			img[i].w = float(pdf);
		}

		DiscreteDistribution<double> distribution(weights.data(), weights.size(), weightSum);

		std::vector<EnvmapCell> envmapDistributionBuffer;
		envmapDistributionBuffer.reserve(pixelCount);
		for (u64 i = 0; i < pixelCount; ++i)
		{
			EnvmapCell cell;
			cell.p = float(distribution.m_cells[i].first);
			cell.i = u32(distribution.m_cells[i].second);
			envmapDistributionBuffer.push_back(cell);
		}

		GfxTextureDesc desc = GfxTextureDesc::make2D(width, height, GfxFormat_RGBA32_Float);
		m_envmap = Gfx_CreateTexture(desc, img);
		m_envmapDistribution = Gfx_CreateBuffer(GfxBufferFlags::Storage, u32(pixelCount), sizeof(EnvmapCell), envmapDistributionBuffer.data());

		free(img);

		m_settings.m_useEnvmap = true;
	}
	else
	{
		Vec4 img = Vec4(0, 0, 0, 1);
		GfxTextureDesc desc = GfxTextureDesc::make2D(1, 1, GfxFormat_RGBA32_Float);
		m_envmap = Gfx_CreateTexture(desc, &img);

		EnvmapCell envmapCell = {};
		m_envmapDistribution = Gfx_CreateBuffer(GfxBufferFlags::Storage, 1, sizeof(EnvmapCell), &envmapCell);
	}
}
