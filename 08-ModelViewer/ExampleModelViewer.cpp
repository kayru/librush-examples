#include "ExampleModelViewer.h"

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <Rush/MathTypes.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilHash.h>
#include <Rush/UtilLog.h>

#include <Common/SceneConfig.h>
#include <Common/Utils.h>


#include <chrono>
#include <stdio.h>

static AppConfig g_appCfg;

int main(int argc, char** argv)
{
	g_appCfg.name = "ModelViewer (" RUSH_RENDER_API_NAME ")";

	g_appCfg.width     = 1280;
	g_appCfg.height    = 720;
	g_appCfg.argc      = argc;
	g_appCfg.argv      = argv;
	g_appCfg.resizable = true;
	g_appCfg.timingLevel = GfxTimingLevel::Scopes;

#ifdef RUSH_DEBUG
	g_appCfg.debug = true;
	Log::breakOnError = true;
#endif

	return Example_Main<ExampleModelViewer>(g_appCfg, argc, argv);
}

ExampleModelViewer::ExampleModelViewer() : ExampleApp(), m_boundingBox(Vec3(0.0f), Vec3(0.0f))
{
	const GfxCapability& caps = Gfx_GetCapability();

	Gfx_SetPresentInterval(1);

	m_windowEvents.setOwner(m_window);

	const u32      whiteTexturePixels[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
	GfxTextureDesc textureDesc           = GfxTextureDesc::make2D(2, 2);
	m_defaultWhiteTexture                = Gfx_CreateTexture(textureDesc, whiteTexturePixels);

	{
		m_vs = Gfx_CreateVertexShader(loadShaderFromFile(RUSH_SHADER_NAME("ModelVS.hlsl")));
		m_ps = Gfx_CreatePixelShader(loadShaderFromFile(RUSH_SHADER_NAME("ModelPS.hlsl")));
	}

	{
		u32 sampleCountMask = (caps.colorSampleCounts & caps.depthSampleCounts);
		if (sampleCountMask == 0)
		{
			sampleCountMask = 1;
		}
		m_msaaQuality = 1u << (31 - bitScanReverse(sampleCountMask));
	}

	m_materialDescriptorSetDesc.constantBuffers = 1; // material constants
	m_materialDescriptorSetDesc.samplers = 1; // material sampler
	m_materialDescriptorSetDesc.textures = 1; // albedo texture
	m_materialDescriptorSetDesc.stageFlags = GfxStageFlags::VertexPixel;

	{
		GfxRenderPipelineDesc pipelineDesc;
		pipelineDesc.vs = m_vs.get();
		pipelineDesc.ps = m_ps.get();
		pipelineDesc.vertexFormat.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Position, 0);
		pipelineDesc.vertexFormat.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Normal, 0);
		pipelineDesc.vertexFormat.add(0, GfxVertexFormatDesc::DataType::Float2, GfxVertexFormatDesc::Semantic::Texcoord, 0);
		pipelineDesc.bindings.descriptorSets[0].constantBuffers = 1; // scene constants
		// Metal argument buffers expect sampler+texture to share a set for reliable pairing.
		pipelineDesc.bindings.descriptorSets[1] = m_materialDescriptorSetDesc;
		pipelineDesc.depthStencil = GfxDepthStencilDesc::makeWriteTest(GfxCompareFunc::GreaterEqual);
		pipelineDesc.rasterizer.cullMode = GfxCullMode::CW;
		pipelineDesc.setBlendState(GfxBlendStateDesc::makeOpaque());
		pipelineDesc.renderTarget.colorFormats[0] = GfxFormat_RGBA8_Unorm;
		pipelineDesc.renderTarget.depthFormat = GfxFormat_D32_Float;
		pipelineDesc.renderTarget.sampleCount = m_msaaQuality;
		m_pipeline = Gfx_CreateRenderPipeline(pipelineDesc);
	}

	GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(Constants));
	m_constantBuffer = Gfx_CreateBuffer(cbDesc);

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
		m_statusString = "Procedural scene (cube + plane)";
		m_useProceduralScene = true;
		m_valid = buildProceduralModel();
	}

	loadConfig();

	m_cameraMan = new CameraManipulator();

	u32 threadCount = std::thread::hardware_concurrency();
	for (u32 i = 0; i < threadCount; ++i)
	{
		m_loadingThreads.push_back(std::thread([this]() { this->loadingThreadFunction(); }));
	}
}

ExampleModelViewer::~ExampleModelViewer()
{
	m_loadingThreadShouldExit = true;
	for (auto& it : m_loadingThreads)
	{
		it.join();
	}

	for (const auto& it : m_textures)
	{
		it.second->albedoTexture.reset();
		delete it.second;
	}

	m_windowEvents.setOwner(nullptr);

	delete m_cameraMan;
}

void ExampleModelViewer::onUpdate()
{
	TimingScope timingScope(m_stats.cpuTotal);

	m_gpuTiming.update();
	Gfx_ResetStats();

	const float dt = (float)m_timer.time();
	m_timer.reset();

	for (const WindowEvent& e : m_windowEvents)
	{
		switch (e.type)
		{
		case WindowEventType_KeyDown:
			if (e.code == Key_F2)
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
			break;
		case WindowEventType_Scroll:
			if (e.scroll.y > 0)
			{
				m_settings.m_cameraScale *= 1.25f;
			}
			else
			{
				m_settings.m_cameraScale *= 0.9f;
			}
			RUSH_LOG("Camera scale: %f", m_settings.m_cameraScale);
			break;
		default: break;
		}
	}

	float clipNear = 0.25f * m_settings.m_cameraScale;
	m_camera.setClip(clipNear, m_camera.getFarPlane());
	m_camera.setAspect(m_window->getAspect());
	m_cameraMan->setMoveSpeed(20.0f * m_settings.m_cameraScale);

	m_cameraMan->update(&m_camera, dt, m_window->getKeyboardState(), m_window->getMouseState());

	if (!isDesktop())
	{
		m_virtualGamepad.updateFlyCamera(m_window, m_camera, dt, m_cameraMan->getMoveSpeed());
	}

	interpolateCamera(m_interpolatedCamera, m_camera, dt);

	m_windowEvents.clear();

	{
		TextureData* textureData = nullptr;
		m_loadingMutex.lock();
		if (!m_loadedTextures.empty())
		{
			textureData = m_loadedTextures.back();
			m_loadedTextures.pop_back();
		}
		m_loadingMutex.unlock();

		if (textureData)
		{
			GfxTextureData mipData[16] = {};
			for (u32 i = 0; i < textureData->desc.mips; ++i)
			{
				mipData[i].pixels = textureData->mips[i].data();
				mipData[i].mip = i;
			}

			textureData->albedoTexture = Gfx_CreateTexture(textureData->desc, mipData, textureData->desc.mips);
			for (u32 i : textureData->patchList)
			{
				m_materials[i].albedoTexture = textureData->albedoTexture.get();
				updateMaterialDescriptorSet(m_materials[i]);
			}
		}
	}

	render();

}

void ExampleModelViewer::createRenderTargets()
{
	Tuple2i framebufferSize = m_window->getFramebufferSize();
	if (m_colorTarget.valid())
	{
		GfxTextureDesc desc = Gfx_GetTextureDesc(m_colorTarget);
		if (desc.width == framebufferSize.x && desc.height == framebufferSize.y)
		{
			return;
		}
	}

	GfxTextureDesc desc = GfxTextureDesc::make2D(framebufferSize.x, framebufferSize.y);

	desc.format   = GfxFormat_D32_Float;
	desc.usage    = GfxUsageFlags::DepthStencil;
	desc.samples  = m_msaaQuality;
	m_depthTarget = Gfx_CreateTexture(desc);

	desc.format         = GfxFormat_RGBA8_Unorm;
	desc.usage          = GfxUsageFlags::RenderTarget | GfxUsageFlags::TransferSrc;
	desc.samples        = m_msaaQuality;
	m_colorTarget       = Gfx_CreateTexture(desc);

	desc.usage      = GfxUsageFlags::StorageImage | GfxUsageFlags::ShaderResource | GfxUsageFlags::TransferDst;
	desc.samples    = 1;
	m_resolveTarget = Gfx_CreateTexture(desc);
}

void ExampleModelViewer::render()
{
	const GfxCapability& caps = Gfx_GetCapability();

	createRenderTargets();

	Mat4 matView = m_interpolatedCamera.buildViewMatrix();
	Mat4 matProj = m_interpolatedCamera.buildProjMatrix(m_reverseZ);

	Constants constants;
	constants.matViewProj = (matView * matProj).transposed();
	constants.matWorld    = m_worldTransform.transposed();

	GfxContext* ctx = Platform_GetGfxContext();

	GfxMarkerScope markerFrame(ctx, "Frame");

	{
		TimingScope timingScope(m_stats.cpuUpdateConstantBuffer);
		Gfx_UpdateBuffer(ctx, m_constantBuffer, &constants, sizeof(constants));
	}

	{
		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		passDesc.clearDepth     = m_reverseZ ? 0.0f : 1.0f;
		passDesc.color[0]       = m_colorTarget.get();
		passDesc.depth          = m_depthTarget.get();
		passDesc.name           = "Scene";
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(m_window->getFramebufferSize()));
		Gfx_SetScissorRect(ctx, m_window->getFramebufferSize());

		if (m_valid)
		{
			GfxMarkerScope markerFrame(ctx, "Model");

			TimingScope timingScope(m_stats.cpuModel);

			Gfx_SetRenderPipeline(ctx, m_pipeline);
			Gfx_SetVertexStream(ctx, 0, m_vertexBuffer);
			Gfx_SetIndexStream(ctx, m_indexBuffer);
			Gfx_SetConstantBuffer(ctx, 0, m_constantBuffer); // scene constants
			for (const MeshSegment& segment : m_segments)
			{
				const Material& material =
					(segment.material == 0xFFFFFFFF) ? m_defaultMaterial : m_materials[segment.material];

				Gfx_SetDescriptors(ctx, 1, material.descriptorSet);
				Gfx_DrawIndexed(ctx, segment.indexCount, segment.indexOffset, 0, m_vertexCount);
			}
		}

		Gfx_EndPass(ctx);

		{
			GfxScope scope(ctx, "Resolve");
			Gfx_ResolveImage(ctx, m_colorTarget, m_resolveTarget);
			Gfx_AddImageBarrier(ctx, m_resolveTarget, GfxResourceState_ShaderRead);
		}
	}

	{
		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		passDesc.clearDepth     = m_reverseZ ? 0.0f : 1.0f;
		passDesc.name           = "UI";
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(m_window->getFramebufferSize()));
		Gfx_SetScissorRect(ctx, m_window->getFramebufferSize());

		TimingScope timingScope(m_stats.cpuUI);

		{
			const Tuple2i framebufferSize = m_window->getFramebufferSize();
			m_prim->begin2D(framebufferSize);
			TexturedQuad2D q;
			q.pos[0] = Vec2(0.0f, 0.0f);
			q.pos[1] = Vec2((float)framebufferSize.x, 0.0f);
			q.pos[2] = Vec2((float)framebufferSize.x, (float)framebufferSize.y);
			q.pos[3] = Vec2(0.0f, (float)framebufferSize.y);
			q.tex[0] = Vec2(0.0f, 0.0f);
			q.tex[1] = Vec2(1.0f, 0.0);
			q.tex[2] = Vec2(1.0f, 1.0f);
			q.tex[3] = Vec2(0.0f, 1.0f);
			m_prim->setTexture(m_resolveTarget);
			m_prim->drawTexturedQuad(&q);
			m_prim->end2D();
		}

		m_prim->begin2D(m_window->getSize());

		const Vec2 safeOrigin = m_window->getSafeArea().m_min;

		m_font->setScale(2.0f);
		m_font->draw(m_prim, safeOrigin + Vec2(10.0f), m_statusString.c_str());

		m_font->setScale(1.0f);
		char            timingString[1024];
		const GfxStats& stats = Gfx_Stats();
		snprintf(timingString, sizeof(timingString),
		    "Draw calls: %d\n"
		    "Vertices: %d\n"
		    "GPU time: %.2f ms\n"
		    "%s"
		    "CPU time: %.2f ms\n"
		    "> Update CB: %.2f ms\n"
		    "> Model: %.2f ms\n"
		    "> UI: %.2f ms",
		    stats.drawCalls,
		    stats.vertices,
		    m_gpuTiming.busySeconds() * 1000.0,
		    m_gpuTiming.format().c_str(),
		    m_stats.cpuTotal.get() * 1000.0f,
		    m_stats.cpuUpdateConstantBuffer.get() * 1000.0f,
		    m_stats.cpuModel.get() * 1000.0f,
		    m_stats.cpuUI.get() * 1000.0f);
		m_font->draw(m_prim, safeOrigin + Vec2(10.0f, 30.0f), timingString);

		if (!isDesktop())
		{
			m_virtualGamepad.draw(m_prim, m_font, m_window->getSizeFloat());
		}

		m_prim->end2D();

		Gfx_EndPass(ctx);
	}
}

void ExampleModelViewer::loadingThreadFunction()
{
	while (!m_loadingThreadShouldExit)
	{
		TextureData* pendingLoad = nullptr;

		m_loadingMutex.lock();
		if (!m_pendingTextures.empty())
		{
			pendingLoad = m_pendingTextures.back();
			m_pendingTextures.pop_back();
		}
		m_loadingMutex.unlock();

		if (pendingLoad)
		{
			RUSH_LOG("Loading texture '%s'", pendingLoad->filename.c_str());

			if (loadImageWithMips(pendingLoad->filename.c_str(), GfxFormat_RGBA8_Unorm, pendingLoad->desc, pendingLoad->mips))
			{
				m_loadingMutex.lock();
				m_loadedTextures.push_back(pendingLoad);
				m_loadingMutex.unlock();
			}
			else
			{
				RUSH_LOG("Failed to load texture '%s'", pendingLoad->filename.c_str());
			}
		}
		else
		{
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(100ms);
		}
	}
}

void ExampleModelViewer::enqueueLoadTexture(const std::string& filename, u32 materialId)
{
	auto it = m_textures.find(filename);

	if (it == m_textures.end())
	{
		TextureData* textureData = new TextureData;
		textureData->filename    = filename;
		textureData->patchList.push_back(materialId);

		m_textures[filename] = textureData;

		m_pendingTextures.push_back(textureData);
	}
	else
	{
		it->second->patchList.push_back(materialId);
	}
}

void ExampleModelViewer::updateMaterialDescriptorSet(Material& material)
{
	if (!material.descriptorSet.valid())
	{
		material.descriptorSet = Gfx_CreateDescriptorSet(m_materialDescriptorSetDesc);
	}
	GfxSampler sampler = m_samplerStates.anisotropicWrap.get();
	Gfx_UpdateDescriptorSet(material.descriptorSet,
		&material.constantBuffer,
		&sampler,
		&material.albedoTexture,
		nullptr, // storage images
		nullptr  // storage buffers
	);
}

bool ExampleModelViewer::loadSceneData(const ProceduralSceneData& data)
{
	m_materials.clear();
	m_segments.clear();
	m_materialConstantBuffers.clear();

	const GfxBufferDesc materialCbDesc(GfxBufferFlags::Constant, GfxFormat_Unknown, 1, sizeof(MaterialConstants));
	for (const auto& mat : data.materials)
	{
		MaterialConstants constants;
		constants.baseColor = mat.baseColor;

		const u32 materialId = u32(m_materials.size());

		Material material;
		if (!mat.diffuseTextureName.empty())
		{
			enqueueLoadTexture(mat.diffuseTextureName, materialId);
		}

		material.albedoTexture = m_defaultWhiteTexture.get();

		{
			u64  constantHash = hashFnv1a64(&constants, sizeof(constants));
			auto it           = m_materialConstantBuffers.find(constantHash);
			if (it == m_materialConstantBuffers.end())
			{
				GfxOwn<GfxBuffer> cb = Gfx_CreateBuffer(materialCbDesc, &constants);
				material.constantBuffer = cb.get();
				m_materialConstantBuffers[constantHash] = std::move(cb);
			}
			else
			{
				material.constantBuffer = it->second.get();
			}
		}

		updateMaterialDescriptorSet(material);

		m_materials.push_back(std::move(material));
	}

	{
		MaterialConstants constants;
		constants.baseColor = Vec4(1.0f);
		m_defaultConstantBuffer = Gfx_CreateBuffer(materialCbDesc, &constants);
		m_defaultMaterial.constantBuffer = m_defaultConstantBuffer.get();
		m_defaultMaterial.albedoTexture = m_defaultWhiteTexture.get();
		updateMaterialDescriptorSet(m_defaultMaterial);
	}

	RUSH_LOG("Converting mesh");

	std::vector<Vertex> vertices;
	vertices.reserve(data.vertices.size());
	for (const auto& v : data.vertices)
	{
		Vertex dst;
		dst.position = v.position;
		dst.normal   = v.normal;
		dst.texcoord = v.texcoord;
		vertices.push_back(dst);
	}

	// Raw material ids preserved; 0xFFFFFFFF falls back to the default material at draw time.
	m_segments = data.segments;

	m_boundingBox = data.bounds;
	m_vertexCount = (u32)vertices.size();
	m_indexCount  = (u32)data.indices.size();

	RUSH_LOG("Uploading mesh to GPU");

	GfxBufferDesc vbDesc(GfxBufferFlags::Vertex, GfxFormat_Unknown, m_vertexCount, sizeof(Vertex));
	m_vertexBuffer = Gfx_CreateBuffer(vbDesc, vertices.data());

	GfxBufferDesc ibDesc(GfxBufferFlags::Index, GfxFormat_R32_Uint, m_indexCount, 4);
	m_indexBuffer = Gfx_CreateBuffer(ibDesc, data.indices.data());

	return m_vertexBuffer.valid() && m_indexBuffer.valid();
}

bool ExampleModelViewer::buildProceduralModel()
{
	ProceduralSceneData data;
	buildProceduralScene(data, ProceduralScene::BoxOnPlane);
	if (data.vertices.empty() || data.indices.empty())
	{
		return false;
	}

	return loadSceneData(data);
}

// Adding/removing/reordering Settings fields needs no bump (tagged format).
// Bump only on incompatible semantic changes or a Camera blob layout change.
static constexpr u32 kConfigVersion = 1;

const char* ExampleModelViewer::configModelName() const
{
	return (m_useProceduralScene || m_modelFilename.empty()) ? nullptr : m_modelFilename.c_str();
}

void ExampleModelViewer::saveConfig()
{
	saveSceneConfig("modelviewer", configModelName(), kConfigVersion, m_camera, m_settings);
}

void ExampleModelViewer::loadConfig()
{
	resetCamera(); // default framing; the file overwrites whatever it carries
	loadSceneConfig("modelviewer", configModelName(), kConfigVersion, m_camera, m_settings);
	m_interpolatedCamera = m_camera;
}

void ExampleModelViewer::resetCamera()
{
	m_camera = makeFramedCamera(m_boundingBox, m_window->getAspect());
	m_interpolatedCamera = m_camera;
}

bool ExampleModelViewer::loadModel(const char* filename)
{
	RUSH_LOG("Loading model '%s'", filename);

	ProceduralSceneData data;
	if (!loadSceneFromFile(filename, data))
	{
		return false;
	}

	return loadSceneData(data);
}
