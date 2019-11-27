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
#include <stb_image_resize.h>
#include <tiny_obj_loader.h>
#include <cgltf.h>
#include <algorithm>

#include <Common/ImGuiImpl.h>
#include <imgui.h>

#ifdef __GNUC__
#define sprintf_s sprintf // TODO: make a common cross-compiler/platform equivalent
#endif

static AppConfig g_appCfg;

int main(int argc, char** argv)
{
	g_appCfg.name = "PathTracer (" RUSH_RENDER_API_NAME ")";

	g_appCfg.width     = 1920;
	g_appCfg.height    = 1080;
	g_appCfg.argc      = argc;
	g_appCfg.argv      = argv;
	g_appCfg.resizable = true;

#ifdef RUSH_DEBUG
	g_appCfg.debug = true;
	Log::breakOnError = true;
#endif

	return Platform_Main<ExamplePathTracer>(g_appCfg);
}

struct TimingScope
{
	TimingScope(MovingAverage<double, 60>& output) : m_output(output) {}

	~TimingScope() { m_output.add(m_timer.time()); }

	MovingAverage<double, 60>& m_output;
	Timer                      m_timer;
};

ExamplePathTracer::ExamplePathTracer() : ExampleApp(), m_boundingBox(Vec3(0.0f), Vec3(0.0f))
{
	Gfx_SetPresentInterval(0);

	ImGuiImpl_Startup(m_window);

	m_windowEvents.setOwner(m_window);

	const u32      whiteTexturePixels[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
	GfxTextureDesc textureDesc           = GfxTextureDesc::make2D(2, 2);

	m_defaultWhiteTextureId = u32(m_textureDescriptors.size());
	m_textureDescriptors.push_back(Gfx_CreateTexture(textureDesc, whiteTexturePixels));

	GfxDescriptorSetDesc materialDescriptorSetDesc;
	materialDescriptorSetDesc.flags = GfxDescriptorSetFlags::TextureArray;
	materialDescriptorSetDesc.stageFlags = GfxStageFlags::RayTracing;
	materialDescriptorSetDesc.textures = MaxTextures;
	m_materialDescriptorSet = Gfx_CreateDescriptorSet(materialDescriptorSetDesc);

	{
		GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(SceneConstants));
		m_sceneConstantBuffer = Gfx_CreateBuffer(cbDesc);
	}

	{
		GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(TonemapConstants));
		m_tonemapConstantBuffer = Gfx_CreateBuffer(cbDesc);
	}
	

	{
		GfxRayTracingPipelineDesc pipelineDesc;
		pipelineDesc.rayGen = loadShaderFromFile(RUSH_SHADER_NAME("PathTracer.rgen"));
		pipelineDesc.miss = loadShaderFromFile(RUSH_SHADER_NAME("PathTracer.rmiss"));
		pipelineDesc.closestHit = loadShaderFromFile(RUSH_SHADER_NAME("PathTracer.rchit"));

		pipelineDesc.bindings.constantBuffers = 1; // scene constants
		pipelineDesc.bindings.samplers = 1; // default sampler
		pipelineDesc.bindings.textures = 1; // envmap
		pipelineDesc.bindings.rwImages = 1; // output image
		pipelineDesc.bindings.rwBuffers = 3; // IB + VB + envmap distribution
		pipelineDesc.bindings.descriptorSets[1] = materialDescriptorSetDesc;
		pipelineDesc.bindings.accelerationStructures = 1; // TLAS

		m_rtPipeline = Gfx_CreateRayTracingPipeline(pipelineDesc);
	}

	{
		auto vs = Gfx_CreateVertexShader(loadShaderFromFile(RUSH_SHADER_NAME("Blit.vert")));
		auto ps = Gfx_CreatePixelShader(loadShaderFromFile(RUSH_SHADER_NAME("BlitTonemap.frag")));
		auto vf = Gfx_CreateVertexFormat({});

		GfxTechniqueDesc desc;
		desc.vs = vs.get();
		desc.ps = ps.get();
		desc.vf = vf.get();
		desc.bindings.constantBuffers = 1;
		desc.bindings.samplers = 1; // linear sampler
		desc.bindings.textures = 1; // input texture
		m_blitTonemap = Gfx_CreateTechnique(desc);
	}

	if (g_appCfg.argc >= 2)
	{
		const char* modelFilename = g_appCfg.argv[1];
		m_statusString            = std::string("Model: ") + modelFilename;
		m_valid                   = loadModel(modelFilename);

		if (!m_valid)
		{
			RUSH_LOG("Could not load model from '%s'\n", modelFilename);
		}

		const char* envFilename = "envmap.hdr";
		if (g_appCfg.argc >= 4)
		{
			if (!strcmp(g_appCfg.argv[2], "--env"))
			{
				envFilename = g_appCfg.argv[3];
			}
		}

		loadEnvmap(envFilename);

		Vec3  center       = m_boundingBox.center();
		Vec3  dimensions   = m_boundingBox.dimensions();
		float longest_side = dimensions.reduceMax();
		if (longest_side != 0)
		{
			float scale      = 100.0f / longest_side;
			m_worldTransform = Mat4::scaleTranslate(scale, -center * scale);
		}

		m_boundingBox.m_min = m_worldTransform * m_boundingBox.m_min;
		m_boundingBox.m_max = m_worldTransform * m_boundingBox.m_max;
	}
	else
	{
		m_statusString = "Usage: ExamplePathTracer <filename.obj>";
	}

	loadCamera();

	m_cameraMan = new CameraManipulator();
}

ExamplePathTracer::~ExamplePathTracer()
{
	ImGuiImpl_Shutdown();

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

void ExamplePathTracer::update()
{
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
		renderSettingsChanged |= ImGui::SliderFloat("Focal length (mm)", &m_settings.m_focalLengthMM, 1.0f, 250.0f);
		renderSettingsChanged |= ImGui::SliderFloat("Envmap rotation (deg)", &m_settings.m_envmapRotationDegrees, 0.0f, 360.0f);
		ImGui::SliderFloat("Exposure EV100", &m_settings.m_exposureEV100, -10.0f, 10.0f);
		ImGui::SliderFloat("Gamma", &m_settings.m_gamma, 0.25f, 3.0f);
		ImGui::End();

		if (renderSettingsChanged)
		{
			m_frameIndex = 0;
		}
	}

	Camera oldCamera = m_camera;

	m_camera.setFov(focalLengthToFov(m_settings.m_focalLengthMM, m_cameraSensorSizeMM.x));
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
				saveCamera();
			}
			else if (e.code == Key_F3)
			{
				loadCamera();
			}
			else if (e.code == Key_F4)
			{
				resetCamera();
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

	if (!m_showUI ||(!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse))
	{
		m_cameraMan->update(&m_camera, dt, m_window->getKeyboardState(), m_window->getMouseState());
	}

	if (m_camera.getPosition() != oldCamera.getPosition()
		|| m_camera.getForward() != oldCamera.getForward()
		|| m_camera.getAspect() != oldCamera.getAspect()
		|| m_camera.getFov() != oldCamera.getFov())
	{
		m_frameIndex = 0;
		m_totalGpuRenderTime = 0;
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

	GfxOwn<GfxBuffer> instanceBuffer = Gfx_CreateBuffer(GfxBufferFlags::Transient);
	{
		Mat4 transform = m_worldTransform.transposed();
		auto instanceData = Gfx_BeginUpdateBuffer<GfxRayTracingInstanceDesc>(ctx, instanceBuffer.get(), tlasDesc.instanceCount);
		instanceData[0].init();
		memcpy(instanceData[0].transform, &transform, sizeof(float) * 12);
		instanceData[0].accelerationStructureHandle = Gfx_GetAccelerationStructureHandle(m_blas);
		Gfx_EndUpdateBuffer(ctx, instanceBuffer);
	}

	Gfx_BuildAccelerationStructure(ctx, m_blas);
	Gfx_BuildAccelerationStructure(ctx, m_tlas, instanceBuffer);
}

void ExamplePathTracer::render()
{
	const GfxCapability& caps = Gfx_GetCapability();

	Mat4 matView = m_camera.buildViewMatrix();
	Mat4 matProj = m_camera.buildProjMatrix(caps.projectionFlags);

	SceneConstants constants = {};
	constants.matView = matView.transposed();
	constants.matProj = matProj.transposed();
	constants.matViewProj = (matView * matProj).transposed();
	constants.matViewProjInv = (matView * matProj).inverse().transposed();
	//constants.matEnvmapTransform = Mat4::rotationY(toRadians(m_settings.m_envmapRotationDegrees)).transposed();
	constants.cameraPosition = Vec4(m_camera.getPosition());
	constants.frameIndex = m_frameIndex;
	constants.flags = 0;
	constants.flags |= m_settings.m_useEnvmap ? PT_FLAG_USE_ENVMAP: 0;
	constants.flags |= m_settings.m_useNeutralBackground ? PT_FLAG_USE_NEUTRAL_BACKGROUND : 0;

	GfxContext* ctx = Platform_GetGfxContext();

	GfxTextureDesc outputImageDesc = Gfx_GetTextureDesc(m_outputImage);
	if (!m_outputImage.valid() || outputImageDesc.getSize2D() != m_window->getSize())
	{
		outputImageDesc = GfxTextureDesc::make2D(
			m_window->getSize(), GfxFormat_RGBA32_Float, GfxUsageFlags::StorageImage_ShaderResource);

		m_outputImage = Gfx_CreateTexture(outputImageDesc);
	}

	constants.outputSize = outputImageDesc.getSize2D();
	constants.envmapSize = Gfx_GetTextureDesc(m_envmap).getSize2D();

	GfxMarkerScope markerFrame(ctx, "Frame");

	Gfx_UpdateBuffer(ctx, m_sceneConstantBuffer, &constants, sizeof(constants));

	if (m_valid)
	{
		GfxMarkerScope markerFrame(ctx, "Model");

		if (!m_tlas.valid())
		{
			createRayTracingScene(ctx);
		}

		GfxMarkerScope markerRT(ctx, "RT");
		Gfx_SetConstantBuffer(ctx, 0, m_sceneConstantBuffer);
		Gfx_SetSampler(ctx, 0, m_samplerStates.anisotropicWrap);
		Gfx_SetTexture(ctx, 0, m_envmap);
		Gfx_SetStorageImage(ctx, 0, m_outputImage);
		Gfx_SetStorageBuffer(ctx, 0, m_indexBuffer);
		Gfx_SetStorageBuffer(ctx, 1, m_vertexBuffer);
		Gfx_SetStorageBuffer(ctx, 2, m_envmapDistribution);
		Gfx_SetDescriptors(ctx, 1, m_materialDescriptorSet);
		Gfx_TraceRays(ctx, m_rtPipeline, m_tlas, m_sbtBuffer, outputImageDesc.width, outputImageDesc.height);
	}

	Gfx_AddImageBarrier(ctx, m_outputImage, GfxResourceState_ShaderRead);

	GfxPassDesc passDesc;
	passDesc.flags = GfxPassFlags::ClearAll;
	passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
	Gfx_BeginPass(ctx, passDesc);

	Gfx_SetViewport(ctx, GfxViewport(m_window->getSize()));
	Gfx_SetScissorRect(ctx, m_window->getSize());

	Gfx_SetDepthStencilState(ctx, m_depthStencilStates.writeLessEqual);
	Gfx_SetRasterizerState(ctx, m_rasterizerStates.solidCullCW);

	{
		GfxMarkerScope markerFrame(ctx, "Tonemap");

		TonemapConstants constants = {};
		constants.exposure = 1.0f / (1.2f * powf(2.0f, -m_settings.m_exposureEV100));
		constants.gamma = m_settings.m_gamma;
		Gfx_UpdateBuffer(ctx, m_tonemapConstantBuffer, &constants, sizeof(constants));

		Gfx_SetDepthStencilState(ctx, m_depthStencilStates.disable);
		Gfx_SetRasterizerState(ctx, m_rasterizerStates.solidNoCull);
		Gfx_SetBlendState(ctx, m_blendStates.opaque);
		Gfx_SetTechnique(ctx, m_blitTonemap);
		Gfx_SetConstantBuffer(ctx, 0, m_tonemapConstantBuffer);
		Gfx_SetSampler(ctx, 0, m_samplerStates.linearClamp);
		Gfx_SetTexture(ctx, 0, m_outputImage);
		Gfx_Draw(ctx, 0, 3);
	}

	if (m_showUI)
	{
		GfxMarkerScope markerFrame(ctx, "UI");

		Gfx_SetBlendState(ctx, m_blendStates.lerp);
		Gfx_SetDepthStencilState(ctx, m_depthStencilStates.disable);

		m_prim->begin2D(m_window->getSize());

		m_font->draw(m_prim, Vec2(10.0f), m_statusString.c_str());

		char            timingString[1024];
		const GfxStats& stats = Gfx_Stats();
		sprintf_s(timingString,
		    "GPU time: %.2f ms\n"
		    "CPU time: %.2f ms\n"
		    "Total render time: %.2f sec\n"
		    "Samples per pixel: %d\n",
		    m_stats.gpuTotal.get() * 1000.0f,
		    m_stats.cpuTotal.get() * 1000.0f,
		    m_totalGpuRenderTime,
		    m_frameIndex);
		m_font->draw(m_prim, Vec2(10.0f, 30.0f), timingString);

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

			int w, h, comp;
			u8* pixels = stbi_load(pendingLoad->filename.c_str(), &w, &h, &comp, 4);

			if (pixels)
			{
				u32 mipIndex = 0;

				{
					u32 levelSize = w * h * 4;
					pendingLoad->mips[mipIndex].resize(levelSize);
					memcpy(pendingLoad->mips[mipIndex].data(), pixels, levelSize);
					mipIndex++;
				}

				u32 mipWidth  = w;
				u32 mipHeight = h;

				while (mipWidth != 1 && mipHeight != 1)
				{
					u32 nextMipWidth  = max<u32>(1, mipWidth / 2);
					u32 nextMipHeight = max<u32>(1, mipHeight / 2);

					u32 levelSize = nextMipWidth * nextMipHeight * 4;
					pendingLoad->mips[mipIndex].resize(levelSize);

					const u32 mipPitch     = mipWidth * 4;
					const u32 nextMipPitch = nextMipWidth * 4;

					int resizeResult = stbir_resize_uint8(pendingLoad->mips[mipIndex - 1].data(), mipWidth, mipHeight,
					    mipPitch, pendingLoad->mips[mipIndex].data(), nextMipWidth, nextMipHeight, nextMipPitch, 4);
					RUSH_ASSERT(resizeResult);

					mipIndex++;
					mipWidth  = nextMipWidth;
					mipHeight = nextMipHeight;
				}

				pendingLoad->desc      = GfxTextureDesc::make2D(w, h, pendingLoad->desc.format);
				pendingLoad->desc.mips = mipIndex;

				m_loadingMutex.lock();
				m_loadedTextures.push_back(pendingLoad);
				m_loadingMutex.unlock();

				free(pixels);
			}
			else
			{
				RUSH_LOG("Failed to load texture '%s'", pendingLoad->filename.c_str());
				m_loadedTextures.push_back(nullptr);
			}
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

inline float convertDiffuseColor(float v)
{
	return v == 0.0f ? 0.5f : v;
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

		constants.albedoFactor[0] = convertDiffuseColor(constants.albedoFactor[0]);
		constants.albedoFactor[1] = convertDiffuseColor(constants.albedoFactor[1]);
		constants.albedoFactor[2] = convertDiffuseColor(constants.albedoFactor[2]);

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

			bool haveNormals = false;
			if (anor && anor->count == apos->count)
			{
				haveNormals = true;
				auto ptr = getDataPtr<float>(anor);
				for (u32 i = 0; i < anor->count; ++i)
				{
					m_vertices[firstVertex + i].normal = mulNormal(Vec3(ptr), transform);
					m_vertices[firstVertex + i].normal.x = -m_vertices[firstVertex + i].normal.x;
					ptr += anor->stride / 4;
				}
			}

			// texcoords

			bool haveTexcoords = false;
			if (atex && atex->count == apos->count)
			{
				haveTexcoords = true;
				auto ptr = getDataPtr<float>(atex);
				for (u32 i = 0; i < atex->count; ++i)
				{
					m_vertices[firstVertex + i].texcoord = Vec2(ptr);
					ptr += atex->stride / 4;
				}
			}

			if (!haveNormals)
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
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      errors;

	std::string directory = directoryFromFilename(filename);

	bool loaded = tinyobj::LoadObj(shapes, materials, errors, filename, directory.c_str());
	if (!loaded)
	{
		RUSH_LOG_ERROR("OBJ loader error: %s", errors.c_str());
		return false;
	}

	RUSH_LOG("Converting mesh from OBJ");

	for (auto& objMaterial : materials)
	{
		MaterialConstants constants;
		constants.albedoFactor.x = convertDiffuseColor(objMaterial.diffuse[0]);
		constants.albedoFactor.y = convertDiffuseColor(objMaterial.diffuse[1]);
		constants.albedoFactor.z = convertDiffuseColor(objMaterial.diffuse[2]);
		constants.albedoFactor.w = 1.0f;
		constants.albedoTextureId = m_defaultWhiteTextureId;

		u32 materialId = u32(m_materials.size());
		if (!objMaterial.diffuse_texname.empty())
		{
			std::string filename = directory + objMaterial.diffuse_texname;
			fixDirectorySeparatorsInplace(filename);
			constants.albedoTextureId = enqueueLoadTexture(filename, GfxFormat::GfxFormat_RGBA8_sRGB);
		}

		m_materials.push_back(constants);
	}

	if (materials.empty())
	{
		MaterialConstants constants;
		constants.albedoTextureId = m_defaultWhiteTextureId;
		m_materials.push_back(constants);
	}

	m_boundingBox.expandInit();

	for (const auto& shape : shapes)
	{
		u32         firstVertex = (u32)m_vertices.size();
		const auto& mesh        = shape.mesh;

		const u32 vertexCount = (u32)mesh.positions.size() / 3;

		const bool haveTexcoords = !mesh.texcoords.empty();
		const bool haveNormals   = mesh.positions.size() == mesh.normals.size();

		for (u64 i = 0; i < vertexCount; ++i)
		{
			Vertex v;

			v.position.x = mesh.positions[i * 3 + 0];
			v.position.y = mesh.positions[i * 3 + 1];
			v.position.z = mesh.positions[i * 3 + 2];

			m_boundingBox.expand(v.position);

			if (haveTexcoords)
			{
				v.texcoord.x = mesh.texcoords[i * 2 + 0];
				v.texcoord.y = mesh.texcoords[i * 2 + 1];

				v.texcoord.y = 1.0f - v.texcoord.y;
			}
			else
			{
				v.texcoord = Vec2(0.0f);
			}

			if (haveNormals)
			{
				v.normal.x = mesh.normals[i * 3 + 0];
				v.normal.y = mesh.normals[i * 3 + 1];
				v.normal.z = mesh.normals[i * 3 + 2];
			}
			else
			{
				v.normal = Vec3(0.0);
			}

			v.position.x = -v.position.x;
			v.normal.x   = -v.normal.x;

			//v.normal = Vec3(1, 0, 0);

			m_vertices.push_back(v);
		}

		if (!haveNormals)
		{
			const u32 triangleCount = (u32)mesh.indices.size() / 3;
			for (u64 i = 0; i < triangleCount; ++i)
			{
				u32 idxA = firstVertex + mesh.indices[i * 3 + 0];
				u32 idxB = firstVertex + mesh.indices[i * 3 + 2];
				u32 idxC = firstVertex + mesh.indices[i * 3 + 1];

				Vec3 a = m_vertices[idxA].position;
				Vec3 b = m_vertices[idxB].position;
				Vec3 c = m_vertices[idxC].position;

				Vec3 normal = cross(b - a, c - b);

				normal = normalize(normal);

				m_vertices[idxA].normal += normal;
				m_vertices[idxB].normal += normal;
				m_vertices[idxC].normal += normal;
			}

			for (u32 i = firstVertex; i < (u32)m_vertices.size(); ++i)
			{
				m_vertices[i].normal = normalize(m_vertices[i].normal);
			}
		}

		int currentMaterialId = -1;

		const u32 triangleCount = (u32)mesh.indices.size() / 3;
		for (u64 triangleIt = 0; triangleIt < triangleCount; ++triangleIt)
		{
			if (mesh.material_ids[triangleIt] != currentMaterialId || m_segments.empty())
			{
				currentMaterialId = mesh.material_ids[triangleIt];
				m_segments.push_back(MeshSegment());
				m_segments.back().material    = max(0,currentMaterialId);
				m_segments.back().indexOffset = (u32)m_indices.size();
				m_segments.back().indexCount  = 0;
			}

			m_indices.push_back(mesh.indices[triangleIt * 3 + 0] + firstVertex);
			m_indices.push_back(mesh.indices[triangleIt * 3 + 2] + firstVertex);
			m_indices.push_back(mesh.indices[triangleIt * 3 + 1] + firstVertex);

			m_segments.back().indexCount += 3;
		}

		m_vertexCount = (u32)m_vertices.size();
		m_indexCount  = (u32)m_indices.size();
	}

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
				m_textureDescriptors[descriptorIndex] = Gfx_CreateTexture(textureData->desc, mipData, textureData->desc.mips);
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

	Gfx_UpdateDescriptorSet(m_materialDescriptorSet,
		nullptr, // constant buffers
		nullptr, // samplers
		textureDescriptors.data(),
		nullptr, // storage images
		nullptr  // storage buffers
	);


	RUSH_LOG("Creating ray tracing data");

	DynamicArray<GfxRayTracingGeometryDesc> geometries;
	geometries.reserve(m_segments.size());

	const GfxCapability& caps = Gfx_GetCapability();
	const u32 shaderHandleSize = caps.rtShaderHandleSize;
	const u32 sbtRecordSize = alignCeiling(u32(shaderHandleSize + sizeof(MaterialConstants)), shaderHandleSize);

	DynamicArray<u8> sbtData;
	sbtData.resize(m_segments.size() * sbtRecordSize);

	const u8* hitGroupHandle = Gfx_GetRayTracingShaderHandle(m_rtPipeline, GfxRayTracingShaderType::HitGroup, 0);

	for (size_t i = 0; i < m_segments.size(); ++i)
	{
		const auto& segment = m_segments[i];

		GfxRayTracingGeometryDesc geometryDesc;
		geometryDesc.indexBuffer = m_indexBuffer.get();
		geometryDesc.indexFormat = ibDesc.format;
		geometryDesc.indexCount = segment.indexCount;
		geometryDesc.indexBufferOffset = segment.indexOffset * ibStride;
		geometryDesc.vertexBuffer = m_vertexBuffer.get();
		geometryDesc.vertexFormat = GfxFormat::GfxFormat_RGB32_Float;
		geometryDesc.vertexStride = sizeof(Vertex);
		geometryDesc.vertexCount = m_vertexCount;
		geometries.push_back(geometryDesc);

		u8* sbtRecord = &sbtData[i * sbtRecordSize];
		u8* sbtRecordConstants = sbtRecord + shaderHandleSize;

		MaterialConstants materialConstants = m_materials[segment.material];
		materialConstants.firstIndex = segment.indexOffset;

		memcpy(sbtRecord, hitGroupHandle, sizeof(shaderHandleSize));
		memcpy(sbtRecordConstants, &materialConstants, sizeof(materialConstants));
	}

	m_sbtBuffer = Gfx_CreateBuffer(GfxBufferFlags::Storage, u32(sbtData.size() / sbtRecordSize), sbtRecordSize, sbtData.data());

	GfxAccelerationStructureDesc blasDesc;
	blasDesc.type = GfxAccelerationStructureType::BottomLevel;
	blasDesc.geometyCount = u32(geometries.size());
	blasDesc.geometries = geometries.data();
	m_blas = Gfx_CreateAccelerationStructure(blasDesc);
}

void ExamplePathTracer::resetCamera()
{
	float aspect = m_window->getAspect();
	float fov = 1.0f;
	m_camera = Camera(aspect, fov, 0.25f, 10000.0f);
	m_camera.lookAt(Vec3(m_boundingBox.m_max) + Vec3(2.0f), m_boundingBox.center());
	m_frameIndex = 0;
}

void ExamplePathTracer::saveCamera()
{
	FileOut f("camera.bin");
	if (f.valid())
	{
		f.writeT(m_camera);
		RUSH_LOG("Camera saved to file");
	}
}

void ExamplePathTracer::loadCamera()
{
	FileIn f("camera.bin");
	if (f.valid())
	{
		f.readT(m_camera);
		m_frameIndex = 0;
		RUSH_LOG("Camera loaded from file");
	}
	else
	{
		resetCamera();
	}
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
