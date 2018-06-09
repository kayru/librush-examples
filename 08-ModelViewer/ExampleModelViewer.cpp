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

#include <stb_image.h>
#include <stb_image_resize.h>
#include <tiny_obj_loader.h>

#ifdef __GNUC__
#define sprintf_s sprintf // TODO: make a common cross-compiler/platform equivalent
#endif

static AppConfig g_appCfg;

int main(int argc, char** argv)
{
	g_appCfg.name = "ModelViewer (" RUSH_RENDER_API_NAME ")";

	g_appCfg.width     = 1280;
	g_appCfg.height    = 720;
	g_appCfg.argc      = argc;
	g_appCfg.argv      = argv;
	g_appCfg.resizable = true;

#ifndef NDEBUG
	g_appCfg.debug = true;
#endif

	return Platform_Main<ExampleModelViewer>(g_appCfg);
}

struct TimingScope
{
	TimingScope(MovingAverage<double, 60>& output) : m_output(output) {}

	~TimingScope() { m_output.add(m_timer.time()); }

	MovingAverage<double, 60>& m_output;
	Timer                      m_timer;
};

ExampleModelViewer::ExampleModelViewer() : ExampleApp(), m_boundingBox(Vec3(0.0f), Vec3(0.0f))
{
	Gfx_SetPresentInterval(1);

	m_windowEvents.setOwner(m_window);

	const u32      whiteTexturePixels[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
	GfxTextureDesc textureDesc           = GfxTextureDesc::make2D(2, 2);
	m_defaultWhiteTexture                = Gfx_CreateTexture(textureDesc, whiteTexturePixels);

	{
		m_vs = Gfx_CreateVertexShader(loadShaderFromFile("ModelVS.hlsl.bin"));
		m_ps = Gfx_CreatePixelShader(loadShaderFromFile("ModelPS.hlsl.bin"));
	}

	GfxVertexFormatDesc vfDesc;
	vfDesc.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Position, 0);
	vfDesc.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Normal, 0);
	vfDesc.add(0, GfxVertexFormatDesc::DataType::Float2, GfxVertexFormatDesc::Semantic::Texcoord, 0);

	m_vf = Gfx_CreateVertexFormat(vfDesc);

	GfxShaderBindings bindings;
	bindings.addConstantBuffer("constantBuffer0", 0); // scene consants
	bindings.addConstantBuffer("constantBuffer1", 1); // material constants
	bindings.addSeparateSampler("sampler0", 0);       // linear sampler
	bindings.addTexture("texture0", 0);               // albedo texture
	m_technique = Gfx_CreateTechnique(GfxTechniqueDesc(m_ps, m_vs, m_vf, &bindings));

	GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(Constants));
	m_constantBuffer = Gfx_CreateBuffer(cbDesc);

	if (g_appCfg.argc >= 2)
	{
		const char* modelFilename = g_appCfg.argv[1];
		m_statusString            = std::string("Model: ") + modelFilename;
		m_valid                   = loadModel(modelFilename);

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
		m_statusString = "Usage: ExampleModelViewer <filename.obj>";
	}

	float aspect = m_window->getAspect();
	float fov    = 1.0f;

	m_camera = Camera(aspect, fov, 0.25f, 10000.0f);
	m_camera.lookAt(Vec3(m_boundingBox.m_max) + Vec3(2.0f), m_boundingBox.center());
	m_interpolatedCamera = m_camera;

	m_cameraMan = new CameraManipulator();
}

ExampleModelViewer::~ExampleModelViewer()
{
	m_windowEvents.setOwner(nullptr);

	delete m_cameraMan;

	Gfx_Release(m_defaultWhiteTexture);
	Gfx_Release(m_vertexBuffer);
	Gfx_Release(m_indexBuffer);
	Gfx_Release(m_constantBuffer);
	Gfx_Release(m_vs);
	Gfx_Release(m_ps);
	Gfx_Release(m_vf);
}

void ExampleModelViewer::update()
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
			Log::message("Camera scale: %f", m_cameraScale);
			break;
		default: break;
		}
	}

	float clipNear = 0.25f * m_cameraScale;
	float clipFar  = 10000.0f * m_cameraScale;
	m_camera.setClip(clipNear, clipFar);
	m_camera.setAspect(m_window->getAspect());
	m_cameraMan->setMoveSpeed(20.0f * m_cameraScale);

	m_cameraMan->update(&m_camera, dt, m_window->getKeyboardState(), m_window->getMouseState());

	m_interpolatedCamera.blendTo(m_camera, 0.1f, 0.125f);

	m_windowEvents.clear();
	render();
}

void ExampleModelViewer::render()
{
	const GfxCapability& caps = Gfx_GetCapability();

	Mat4 matView = m_interpolatedCamera.buildViewMatrix();
	Mat4 matProj = m_interpolatedCamera.buildProjMatrix(caps.projectionFlags);

	Constants constants;
	constants.matViewProj = (matView * matProj).transposed();
	constants.matWorld    = m_worldTransform.transposed();

	GfxContext* ctx = Platform_GetGfxContext();

	{
		TimingScope timingScope(m_stats.cpuUpdateConstantBuffer);
		Gfx_UpdateBuffer(ctx, m_constantBuffer, &constants, sizeof(constants));
	}

	GfxPassDesc passDesc;
	passDesc.flags          = GfxPassFlags::ClearAll;
	passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
	Gfx_BeginPass(ctx, passDesc);

	Gfx_SetViewport(ctx, GfxViewport(m_window->getSize()));
	Gfx_SetScissorRect(ctx, m_window->getSize());

	Gfx_SetDepthStencilState(ctx, m_depthStencilStates.writeLessEqual);

	if (m_valid)
	{
		TimingScope timingScope(m_stats.cpuModel);

		Gfx_SetBlendState(ctx, m_blendStates.opaque);

		Gfx_SetTechnique(ctx, m_technique);
		Gfx_SetVertexStream(ctx, 0, m_vertexBuffer);
		Gfx_SetIndexStream(ctx, m_indexBuffer);
		Gfx_SetConstantBuffer(ctx, 0, m_constantBuffer);

		for (const MeshSegment& segment : m_segments)
		{
			GfxTexture texture = m_defaultWhiteTexture;

			const Material& material =
			    (segment.material == 0xFFFFFFFF) ? m_defaultMaterial : m_materials[segment.material];
			if (material.albedoTexture.valid())
			{
				texture = material.albedoTexture.get();
			}
			Gfx_SetConstantBuffer(ctx, 1, material.constantBuffer);

			Gfx_SetSampler(ctx, GfxStage::Pixel, 0, m_samplerStates.anisotropicWrap);
			Gfx_SetTexture(ctx, GfxStage::Pixel, 0, texture);

			Gfx_DrawIndexed(ctx, segment.indexCount, segment.indexOffset, 0, m_vertexCount);
		}
	}

	{
		TimingScope timingScope(m_stats.cpuUI);

		Gfx_SetBlendState(ctx, m_blendStates.lerp);
		Gfx_SetDepthStencilState(ctx, m_depthStencilStates.disable);

		m_prim->begin2D(m_window->getSize());

		m_font->setScale(2.0f);
		m_font->draw(m_prim, Vec2(10.0f), m_statusString.c_str());

		m_font->setScale(1.0f);
		char            timingString[1024];
		const GfxStats& stats = Gfx_Stats();
		sprintf_s(timingString,
		    "Draw calls: %d\n"
		    "Vertices: %d\n"
		    "GPU time: %.2f ms\n"
		    "CPU time: %.2f ms\n"
		    "> Update CB: %.2f ms\n"
		    "> Model: %.2f ms\n"
		    "> UI: %.2f ms",
		    stats.drawCalls,
		    stats.vertices,
		    m_stats.gpuTotal.get() * 1000.0f,
		    m_stats.cpuTotal.get() * 1000.0f,
		    m_stats.cpuUpdateConstantBuffer.get() * 1000.0f,
		    m_stats.cpuModel.get() * 1000.0f,
		    m_stats.cpuUI.get() * 1000.0f);
		m_font->draw(m_prim, Vec2(10.0f, 30.0f), timingString);

		m_prim->end2D();
	}

	Gfx_EndPass(ctx);
}

static std::string directoryFromFilename(const std::string& filename)
{
	size_t pos = filename.find_last_of("/\\");
	if (pos != std::string::npos)
	{
		return filename.substr(0, pos + 1);
	}
	else
	{
		return std::string();
	}
}

GfxRef<GfxTexture> ExampleModelViewer::loadTexture(const std::string& filename)
{
	auto it = m_textures.find(filename);
	if (it == m_textures.end())
	{
		Log::message("Loading texture '%s'", filename.c_str());

		int w, h, comp;
		stbi_set_flip_vertically_on_load(true);
		u8* pixels = stbi_load(filename.c_str(), &w, &h, &comp, 4);

		GfxRef<GfxTexture> texture;

		if (pixels)
		{
			std::vector<std::unique_ptr<u8>> mips;
			mips.reserve(16);

			std::vector<GfxTextureData> textureData;
			textureData.reserve(16);
			textureData.push_back(GfxTextureData(pixels));

			u32 mipWidth  = w;
			u32 mipHeight = h;

			while (mipWidth != 1 && mipHeight != 1)
			{
				u32 nextMipWidth  = max<u32>(1, mipWidth / 2);
				u32 nextMipHeight = max<u32>(1, mipHeight / 2);

				u8* nextMip = new u8[nextMipWidth * nextMipHeight * 4];
				mips.push_back(std::unique_ptr<u8>(nextMip));

				const u32 mipPitch     = mipWidth * 4;
				const u32 nextMipPitch = nextMipWidth * 4;
				int       resizeResult = stbir_resize_uint8((const u8*)textureData.back().pixels, mipWidth, mipHeight,
                    mipPitch, nextMip, nextMipWidth, nextMipHeight, nextMipPitch, 4);
				RUSH_ASSERT(resizeResult);

				textureData.push_back(GfxTextureData(nextMip, (u32)textureData.size()));

				mipWidth  = nextMipWidth;
				mipHeight = nextMipHeight;
			}

			GfxTextureDesc desc = GfxTextureDesc::make2D(w, h);
			desc.mips           = (u32)textureData.size();

			texture.takeover(Gfx_CreateTexture(desc, textureData.data(), (u32)textureData.size()));
			m_textures.insert(std::make_pair(filename, texture));

			stbi_image_free(pixels);
		}
		else
		{
			Log::warning("Failed to load texture '%s'", filename.c_str());
		}

		return texture;
	}
	else
	{
		return it->second;
	}
}

bool ExampleModelViewer::loadModel(const char* filename)
{
	Log::message("Loading model '%s'", filename);

	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      errors;

	std::string directory = directoryFromFilename(filename);

	bool loaded = tinyobj::LoadObj(shapes, materials, errors, filename, directory.c_str());
	if (!loaded)
	{
		Log::error("Could not load model from '%s'\n%s\n", filename, errors.c_str());
		return false;
	}

	const GfxBufferDesc materialCbDesc(GfxBufferFlags::Constant, GfxFormat_Unknown, 1, sizeof(MaterialConstants));
	for (auto& objMaterial : materials)
	{
		MaterialConstants constants;
		constants.baseColor.x = objMaterial.diffuse[0];
		constants.baseColor.y = objMaterial.diffuse[1];
		constants.baseColor.z = objMaterial.diffuse[2];
		constants.baseColor.w = 1.0f;

		Material material;
		if (!objMaterial.diffuse_texname.empty())
		{
			material.albedoTexture = loadTexture(directory + objMaterial.diffuse_texname);
		}

		{
			u64  constantHash = hashFnv1a64(&constants, sizeof(constants));
			auto it           = m_materialConstantBuffers.find(constantHash);
			if (it == m_materialConstantBuffers.end())
			{
				GfxBuffer cb = Gfx_CreateBuffer(materialCbDesc, &constants);
				m_materialConstantBuffers[constantHash].retain(cb);
				material.constantBuffer.retain(cb);
			}
			else
			{
				material.constantBuffer = it->second;
			}
		}

		m_materials.push_back(material);
	}

	{
		MaterialConstants constants;
		constants.baseColor = Vec4(1.0f);
		m_defaultMaterial.constantBuffer.takeover(Gfx_CreateBuffer(materialCbDesc, &constants));
		m_defaultMaterial.albedoTexture.retain(m_defaultWhiteTexture);
	}

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

		for (u32 i = 0; i < vertexCount; ++i)
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

			vertices.push_back(v);
		}

		if (!haveNormals)
		{
			const u32 triangleCount = (u32)mesh.indices.size() / 3;
			for (u32 i = 0; i < triangleCount; ++i)
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

		u32 currentMaterialId = 0xFFFFFFFF;

		const u32 triangleCount = (u32)mesh.indices.size() / 3;
		for (u32 triangleIt = 0; triangleIt < triangleCount; ++triangleIt)
		{
			if (mesh.material_ids[triangleIt] != currentMaterialId || m_segments.empty())
			{
				currentMaterialId = mesh.material_ids[triangleIt];
				m_segments.push_back(MeshSegment());
				m_segments.back().material    = currentMaterialId;
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

	GfxBufferDesc vbDesc(GfxBufferFlags::Vertex, GfxFormat_Unknown, m_vertexCount, sizeof(Vertex));
	m_vertexBuffer = Gfx_CreateBuffer(vbDesc, vertices.data());

	GfxBufferDesc ibDesc(GfxBufferFlags::Index, GfxFormat_R32_Uint, m_indexCount, 4);
	m_indexBuffer = Gfx_CreateBuffer(ibDesc, indices.data());

	return true;
}
