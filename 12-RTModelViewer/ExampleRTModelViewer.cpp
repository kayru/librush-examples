#include "ExampleRTModelViewer.h"

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

#include <stb_image.h>
#include <stb_image_resize.h>
#include <tiny_obj_loader.h>

#include <chrono>

#ifdef __GNUC__
#define sprintf_s sprintf // TODO: make a common cross-compiler/platform equivalent
#endif

static AppConfig g_appCfg;

int main(int argc, char** argv)
{
	g_appCfg.name = "RTModelViewer (" RUSH_RENDER_API_NAME ")";

	g_appCfg.width     = 1920;
	g_appCfg.height    = 1080;
	g_appCfg.argc      = argc;
	g_appCfg.argv      = argv;
	g_appCfg.resizable = true;

#ifdef RUSH_DEBUG
	g_appCfg.debug = true;
	Log::breakOnError = true;
#endif

	return Platform_Main<ExampleRTModelViewer>(g_appCfg);
}

struct TimingScope
{
	TimingScope(MovingAverage<double, 60>& output) : m_output(output) {}

	~TimingScope() { m_output.add(m_timer.time()); }

	MovingAverage<double, 60>& m_output;
	Timer                      m_timer;
};

ExampleRTModelViewer::ExampleRTModelViewer() : ExampleApp(), m_boundingBox(Vec3(0.0f), Vec3(0.0f))
{
	Gfx_SetPresentInterval(1);

	m_windowEvents.setOwner(m_window);

	const u32      whiteTexturePixels[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
	GfxTextureDesc textureDesc           = GfxTextureDesc::make2D(2, 2);

	m_defaultWhiteTextureId = u32(m_textureDescriptors.size());
	m_textureDescriptors.push_back(Gfx_CreateTexture(textureDesc, whiteTexturePixels));

	GfxDescriptorSetDesc materialDescriptorSetDesc;
	materialDescriptorSetDesc.stageFlags = GfxStageFlags::RayTracing;
	materialDescriptorSetDesc.textures = MaxTextures;
	m_materialDescriptorSet = Gfx_CreateDescriptorSet(materialDescriptorSetDesc);

	GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(SceneConstants));
	m_constantBuffer = Gfx_CreateBuffer(cbDesc);

	GfxRayTracingPipelineDesc pipelineDesc;
	pipelineDesc.rayGen = loadShaderFromFile(RUSH_SHADER_NAME("Primary.rgen"));
	pipelineDesc.miss = loadShaderFromFile(RUSH_SHADER_NAME("Primary.rmiss"));
	pipelineDesc.closestHit = loadShaderFromFile(RUSH_SHADER_NAME("Primary.rchit"));

	pipelineDesc.bindings.constantBuffers = 1; // scene constants
	pipelineDesc.bindings.samplers = 1; // default sampler
	pipelineDesc.bindings.rwImages = 1; // output image
	pipelineDesc.bindings.rwBuffers = 2; // IB + VB
	pipelineDesc.bindings.descriptorSets[1] = materialDescriptorSetDesc;
	pipelineDesc.bindings.accelerationStructures = 1; // TLAS

	m_rtPipeline = Gfx_CreateRayTracingPipeline(pipelineDesc);

	if (g_appCfg.argc >= 2)
	{
		const char* modelFilename = g_appCfg.argv[1];
		m_statusString            = std::string("Model: ") + modelFilename;
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
		m_statusString = "Usage: ExampleRTModelViewer <filename.obj>";
	}

	float aspect = m_window->getAspect();
	float fov    = 1.0f;

	m_camera = Camera(aspect, fov, 0.25f, 10000.0f);
	m_camera.lookAt(Vec3(m_boundingBox.m_max) + Vec3(2.0f), m_boundingBox.center());

	m_cameraMan = new CameraManipulator();
}

ExampleRTModelViewer::~ExampleRTModelViewer()
{
	for (const auto& it : m_textures)
	{
		delete it.second;
	}

	m_windowEvents.setOwner(nullptr);

	delete m_cameraMan;
}

void ExampleRTModelViewer::update()
{
	TimingScope timingScope(m_stats.cpuTotal);

	m_stats.gpuTotal.add(Gfx_Stats().lastFrameGpuTime);
	Gfx_ResetStats();

	const float dt = (float)m_timer.time();
	m_timer.reset();

	for (const WindowEvent& e : m_windowEvents)
	{
		switch (e.type)
		{
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

	Camera oldCamera = m_camera;

	m_camera.setAspect(m_window->getAspect());
	m_cameraMan->setMoveSpeed(20.0f * m_cameraScale);

	m_cameraMan->update(&m_camera, dt, m_window->getKeyboardState(), m_window->getMouseState());

	m_frameIndex++;

	if (m_camera.getPosition() != oldCamera.getPosition()
		|| m_camera.getForward() != oldCamera.getForward()
		|| m_camera.getAspect() != oldCamera.getAspect()
		|| m_camera.getFov() != oldCamera.getFov())
	{
		m_frameIndex = 0;
	}

	m_windowEvents.clear();

	render();
}

void ExampleRTModelViewer::createRayTracingScene(GfxContext* ctx)
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

void ExampleRTModelViewer::render()
{
	const GfxCapability& caps = Gfx_GetCapability();

	Mat4 matView = m_camera.buildViewMatrix();
	Mat4 matProj = m_camera.buildProjMatrix(caps.projectionFlags);

	SceneConstants constants = {};
	constants.matView = matView.transposed();
	constants.matProj = matProj.transposed();
	constants.matViewProj = (matView * matProj).transposed();
	constants.matViewProjInv = (matView * matProj).inverse().transposed();
	constants.cameraPosition = Vec4(m_camera.getPosition());
	constants.frameIndex = m_frameIndex;

	GfxContext* ctx = Platform_GetGfxContext();

	GfxTextureDesc outputImageDesc = Gfx_GetTextureDesc(m_outputImage);
	if (!m_outputImage.valid() || outputImageDesc.getSize2D() != m_window->getSize())
	{
		outputImageDesc = GfxTextureDesc::make2D(
			m_window->getSize(), GfxFormat_RGBA32_Float, GfxUsageFlags::StorageImage_ShaderResource);

		m_outputImage = Gfx_CreateTexture(outputImageDesc);
	}

	constants.outputSize = outputImageDesc.getSize2D();

	GfxMarkerScope markerFrame(ctx, "Frame");

	Gfx_UpdateBuffer(ctx, m_constantBuffer, &constants, sizeof(constants));

	if (m_valid)
	{
		GfxMarkerScope markerFrame(ctx, "Model");

		if (!m_tlas.valid())
		{
			createRayTracingScene(ctx);
		}

		GfxMarkerScope markerRT(ctx, "RT");
		Gfx_SetConstantBuffer(ctx, 0, m_constantBuffer);
		Gfx_SetSampler(ctx, 0, m_samplerStates.anisotropicWrap);
		Gfx_SetStorageImage(ctx, 0, m_outputImage);
		Gfx_SetStorageBuffer(ctx, 0, m_indexBuffer);
		Gfx_SetStorageBuffer(ctx, 1, m_vertexBuffer);
		Gfx_SetDescriptors(ctx, 1, m_materialDescriptorSet);
		Gfx_TraceRays(ctx, m_rtPipeline, m_tlas, m_sbtBuffer, outputImageDesc.width, outputImageDesc.height);

		Gfx_AddImageBarrier(ctx, m_outputImage, GfxResourceState_ShaderRead);
	}

	GfxPassDesc passDesc;
	passDesc.flags = GfxPassFlags::ClearAll;
	passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
	Gfx_BeginPass(ctx, passDesc);

	Gfx_SetViewport(ctx, GfxViewport(m_window->getSize()));
	Gfx_SetScissorRect(ctx, m_window->getSize());

	Gfx_SetDepthStencilState(ctx, m_depthStencilStates.writeLessEqual);
	Gfx_SetRasterizerState(ctx, m_rasterizerStates.solidCullCW);

	{
		GfxMarkerScope markerFrame(ctx, "UI");

		Gfx_SetBlendState(ctx, m_blendStates.lerp);
		Gfx_SetDepthStencilState(ctx, m_depthStencilStates.disable);

		m_prim->begin2D(m_window->getSize());

		m_prim->setTexture(m_outputImage);
		Box2 rect(Vec2(0.0f), m_window->getSizeFloat());
		m_prim->drawTexturedQuad(rect);

		m_font->setScale(2.0f);
		m_font->draw(m_prim, Vec2(10.0f), m_statusString.c_str());

		m_font->setScale(1.0f);
		char            timingString[1024];
		const GfxStats& stats = Gfx_Stats();
		sprintf_s(timingString,
		    "GPU time: %.2f ms\n"
		    "CPU time: %.2f ms\n",
		    m_stats.gpuTotal.get() * 1000.0f,
		    m_stats.cpuTotal.get() * 1000.0f);
		m_font->draw(m_prim, Vec2(10.0f, 30.0f), timingString);

		m_prim->end2D();
	}

	Gfx_EndPass(ctx);
}

void ExampleRTModelViewer::loadingThreadFunction()
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

				pendingLoad->desc      = GfxTextureDesc::make2D(w, h);
				pendingLoad->desc.mips = mipIndex;

				m_loadingMutex.lock();
				m_loadedTextures.push_back(pendingLoad);
				m_loadingMutex.unlock();
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

u32 ExampleRTModelViewer::enqueueLoadTexture(const std::string& filename)
{
	auto it = m_textures.find(filename);

	if (it == m_textures.end())
	{
		TextureData* textureData = new TextureData;

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
	return v == 0 ? 0.5 : v;
}

bool ExampleRTModelViewer::loadModelObj(const char* filename)
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

	const GfxBufferDesc materialCbDesc(GfxBufferFlags::Constant, GfxFormat_Unknown, 1, sizeof(MaterialConstants));
	for (auto& objMaterial : materials)
	{
		MaterialConstants constants;
		constants.baseColor.x = convertDiffuseColor(objMaterial.diffuse[0]);
		constants.baseColor.y = convertDiffuseColor(objMaterial.diffuse[1]);
		constants.baseColor.z = convertDiffuseColor(objMaterial.diffuse[2]);
		constants.baseColor.w = 1.0f;
		constants.albedoTextureId = m_defaultWhiteTextureId;

		u32 materialId = u32(m_materials.size());
		if (!objMaterial.diffuse_texname.empty())
		{
			std::string filename = directory + objMaterial.diffuse_texname;
			fixDirectorySeparatorsInplace(filename);
			constants.albedoTextureId = enqueueLoadTexture(filename);
		}

		m_materials.push_back(constants);
	}

	if (materials.empty())
	{
		MaterialConstants constants;
		constants.albedoTextureId = m_defaultWhiteTextureId;
		m_materials.push_back(constants);
	}

	RUSH_LOG("Converting mesh");

	std::vector<Vertex> vertices;
	std::vector<u32>    indices;

	m_boundingBox.expandInit();

	for (const auto& shape : shapes)
	{
		u32         firstVertex = (u32)vertices.size();
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

			vertices.push_back(v);
		}

		if (!haveNormals)
		{
			const u32 triangleCount = (u32)mesh.indices.size() / 3;
			for (u64 i = 0; i < triangleCount; ++i)
			{
				u32 idxA = firstVertex + mesh.indices[i * 3 + 0];
				u32 idxB = firstVertex + mesh.indices[i * 3 + 2];
				u32 idxC = firstVertex + mesh.indices[i * 3 + 1];

				Vec3 a = vertices[idxA].position;
				Vec3 b = vertices[idxB].position;
				Vec3 c = vertices[idxC].position;

				Vec3 normal = cross(b - a, c - b);

				normal = normalize(normal);

				vertices[idxA].normal += normal;
				vertices[idxB].normal += normal;
				vertices[idxC].normal += normal;
			}

			for (u32 i = firstVertex; i < (u32)vertices.size(); ++i)
			{
				vertices[i].normal = normalize(vertices[i].normal);
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
				m_segments.back().indexOffset = (u32)indices.size();
				m_segments.back().indexCount  = 0;
			}

			indices.push_back(mesh.indices[triangleIt * 3 + 0] + firstVertex);
			indices.push_back(mesh.indices[triangleIt * 3 + 2] + firstVertex);
			indices.push_back(mesh.indices[triangleIt * 3 + 1] + firstVertex);

			m_segments.back().indexCount += 3;
		}

		m_vertexCount = (u32)vertices.size();
		m_indexCount  = (u32)indices.size();
	}
	
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
	
	RUSH_LOG("Uploading mesh to GPU");

	GfxBufferDesc vbDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, m_vertexCount, sizeof(Vertex));
	m_vertexBuffer = Gfx_CreateBuffer(vbDesc, vertices.data());

	const u32 ibStride = 4;
	GfxBufferDesc ibDesc(GfxBufferFlags::Storage, GfxFormat_R32_Uint, m_indexCount, ibStride);
	m_indexBuffer = Gfx_CreateBuffer(ibDesc, indices.data());

	RUSH_LOG("Creating ray tracing data");

	DynamicArray<GfxRayTracingGeometryDesc> geometries;
	geometries.reserve(m_segments.size());

	const GfxCapability& caps = Gfx_GetCapability();
	const u32 shaderHandleSize = caps.rtShaderHandleSize;
	const u32 sbtRecordSize = alignCeiling(u32(shaderHandleSize + sizeof(MaterialConstants)), shaderHandleSize);

	DynamicArray<u8> sbtData;
	sbtData.resize(m_segments.size()* sbtRecordSize);

	const u8* hitGroupHandle = Gfx_GetRayTracingShaderHandle(m_rtPipeline, GfxRayTracingShaderType::HitGroup, 0);

	for (size_t i=0; i<m_segments.size(); ++i)
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

	return true;
}

bool ExampleRTModelViewer::loadModel(const char* filename)
{
	RUSH_LOG("Loading model '%s'", filename);

	if (endsWith(filename, ".obj"))
	{
		return loadModelObj(filename);
	}
	else
	{
		RUSH_LOG_ERROR("Unsupported model file extension.");
		return false;
	}
}
