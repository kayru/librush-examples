#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>

#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <Common/ExampleApp.h>
#include <Common/Utils.h>

#include <memory>
#include <stdio.h>
#include <vector>

#include <tiny_obj_loader.h>

static AppConfig g_appCfg;

class InstancingApp : public ExampleApp
{
	struct Vertex
	{
		Vec3       position;
		ColorRGBA8 color;
	};

	struct InstanceConstants
	{
		Mat4 world;
	};

public:
	InstancingApp() : ExampleApp()
	{
		Gfx_SetPresentInterval(0);

		const GfxCapability& caps = Gfx_GetCapability();

		// TODO: load a model from obj file

		if (g_appCfg.argc >= 2)
		{
			loadMesh(g_appCfg.argv[1]);
		}

		if (m_meshIndexCount == 0)
		{
			generateMesh();
		}

		GfxVertexFormatDesc vfDesc;
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Position, 0);
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Color, GfxVertexFormatDesc::Semantic::Color, 0);
		m_vertexFormat = Gfx_CreateVertexFormat(vfDesc);

		GfxVertexFormatDesc vfDescInstanceId;
		vfDescInstanceId.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Position, 0);
		vfDescInstanceId.add(0, GfxVertexFormatDesc::DataType::Color, GfxVertexFormatDesc::Semantic::Color, 0);
		m_vertexFormatInstanceId = Gfx_CreateVertexFormat(vfDescInstanceId);

		{
			auto vs = Gfx_CreateVertexShader(loadShaderFromFile(RUSH_SHADER_NAME("ModelVS.hlsl")));
			auto ps = Gfx_CreatePixelShader(loadShaderFromFile(RUSH_SHADER_NAME("ModelPS.hlsl")));

			struct SpecializationData
			{
				int maxBatchCount = MaxBatchSize;
			} specializationData;

			GfxSpecializationConstant specializationConstants[] = {{0, 0, sizeof(int)}};

			{
				GfxShaderBindingDesc bindings;
				bindings.constantBuffers = 2; // Global, Instance
				GfxTechniqueDesc techDesc(ps, vs, m_vertexFormat, bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_technique = Gfx_CreateTechnique(techDesc);
			}

			if (caps.pushConstants)
			{
				auto vsPush = Gfx_CreateVertexShader(loadShaderFromFile(RUSH_SHADER_NAME("ModelPush.vert")));
				GfxShaderBindingDesc bindings;
				bindings.pushConstants          = u8(sizeof(Mat4));
				bindings.pushConstantStageFlags = GfxStageFlags::Vertex;
				bindings.constantBuffers        = 1; // Global
				GfxTechniqueDesc techDesc(ps, vsPush, m_vertexFormat, bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_techniquePush = Gfx_CreateTechnique(techDesc);
			}

			if (caps.pushConstants)
			{
				auto vsPushOffset = Gfx_CreateVertexShader(loadShaderFromFile(RUSH_SHADER_NAME("ModelPushOffset.vert")));
				GfxShaderBindingDesc bindings;
				bindings.pushConstants          = u8(sizeof(u32));
				bindings.pushConstantStageFlags = GfxStageFlags::Vertex;
				bindings.constantBuffers        = 2; // Global, Instance
				GfxTechniqueDesc techDesc(ps, vsPushOffset, m_vertexFormat, bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_techniquePushOffset = Gfx_CreateTechnique(techDesc);
			}

			if (caps.instancing)
			{
				auto vsInstanced = Gfx_CreateVertexShader(loadShaderFromFile(RUSH_SHADER_NAME("ModelInstanced.vert")));
				GfxShaderBindingDesc bindings;
				bindings.constantBuffers = 2; // Global, Instance
				GfxTechniqueDesc techDesc(ps, vsInstanced, m_vertexFormat, bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_techniqueInstanced = Gfx_CreateTechnique(techDesc);
			}

			if (caps.instancing)
			{
				auto vsInstanceId = Gfx_CreateVertexShader(loadShaderFromFile(RUSH_SHADER_NAME("ModelInstanced.vert")));
				GfxShaderBindingDesc bindings;
				bindings.constantBuffers = 2; // Global, Instance
				GfxTechniqueDesc techDesc(ps, vsInstanceId, m_vertexFormatInstanceId, bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_techniqueInstanceId = Gfx_CreateTechnique(techDesc);
			}
		}

		m_globalConstantBuffer = Gfx_CreateBuffer(
		    GfxBufferDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(GlobalConstants)));

		m_instanceConstantBuffer = Gfx_CreateBuffer(GfxBufferDesc(
		    GfxBufferFlags::TransientConstant, GfxFormat_Unknown, MaxInstanceCount, sizeof(InstanceConstants)));

		m_dynamicInstanceConstantBuffer = Gfx_CreateBuffer(
		    GfxBufferDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(InstanceConstants)));

		m_indirectArgs.resize(MaxBatchSize);

		m_indirectArgsBuffer = Gfx_CreateBuffer(
		    GfxBufferDesc(GfxBufferFlags::IndirectArgs | GfxBufferFlags::Transient, GfxFormat_Unknown, MaxBatchSize, sizeof(GfxDrawIndexedArg)));

		m_paddedInstanceConstants.resize(MaxInstanceCount);
		m_instanceConstants.resize(MaxInstanceCount);
	}

	static Vec4 computeApproximateBoundingSphere(Vertex* vertices, u32 count)
	{
		Vec3 avgPosition = Vec3(0.0f);
		for (u32 i = 0; i < count; ++i)
		{
			avgPosition += vertices[i].position;
		}
		avgPosition /= float(count);

		float maxDistanceSqr = 0.0f;
		for (u32 i = 0; i < count; ++i)
		{
			Vec3 delta = vertices[i].position - avgPosition;
			maxDistanceSqr = dot(delta, delta);
		}
		
		return Vec4(avgPosition, sqrtf(maxDistanceSqr));
	}

	bool loadMesh(const char* filename)
	{
		if (!endsWith(filename, ".obj"))
		{
			RUSH_LOG_WARNING("Only .obj models are supported");
			return false;
		}

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

		std::vector<Vertex> vertices;
		std::vector<u32>  indices;

		for (const auto& shape : shapes)
		{
			u32         firstVertex = (u32)vertices.size();
			const auto& mesh = shape.mesh;

			const u32 vertexCount = (u32)mesh.positions.size() / 3;

			const bool haveTexcoords = !mesh.texcoords.empty();
			const bool haveNormals = mesh.positions.size() == mesh.normals.size();

			for (u32 i = 0; i < vertexCount; ++i)
			{
				Vertex v = {};

				v.position.x = mesh.positions[i * 3 + 0];
				v.position.y = mesh.positions[i * 3 + 1];
				v.position.z = mesh.positions[i * 3 + 2];

				vertices.push_back(v);
			}

			const u32 triangleCount = (u32)mesh.indices.size() / 3;
			for (u32 triangleIt = 0; triangleIt < triangleCount; ++triangleIt)
			{
				indices.push_back(mesh.indices[triangleIt * 3 + 0] + firstVertex);
				indices.push_back(mesh.indices[triangleIt * 3 + 2] + firstVertex);
				indices.push_back(mesh.indices[triangleIt * 3 + 1] + firstVertex);
			}

			m_meshVertexCount = (u32)vertices.size();
			m_meshIndexCount = (u32)indices.size();
		}

		Vec4 boundingSphere = computeApproximateBoundingSphere(&vertices[0], m_meshVertexCount);
		for (Vertex& v : vertices)
		{
			v.position -= boundingSphere.xyz();
			v.position /= boundingSphere.w;

			Vec3 n = normalize(v.position);
			v.color = ColorRGBA(n * 0.5f + 0.5f, 1.0f);
		}

		GfxBufferDesc vbDesc(GfxBufferFlags::Vertex, GfxFormat_Unknown, m_meshVertexCount, sizeof(Vertex));
		m_vertexBuffer = Gfx_CreateBuffer(vbDesc, vertices.data());

		GfxBufferDesc ibDesc(GfxBufferFlags::Index, GfxFormat_R32_Uint, m_meshIndexCount, 4);
		m_indexBuffer = Gfx_CreateBuffer(ibDesc, indices.data());

		return true;
	}

	void generateMesh()
	{
#if 0

		// clang-format off
		Vertex meshVertices[8] =
		{
			{Vec3(-1.0, -1.0, +1.0), ColorRGBA8(0x00, 0x00, 0xFF)},
			{Vec3(+1.0, -1.0, +1.0), ColorRGBA8(0xFF, 0x00, 0xFF)},
			{Vec3(+1.0, +1.0, +1.0), ColorRGBA8(0xFF, 0xFF, 0xFF)},
			{Vec3(-1.0, +1.0, +1.0), ColorRGBA8(0x00, 0xFF, 0xFF)},
			{Vec3(-1.0, -1.0, -1.0), ColorRGBA8(0x00, 0x00, 0x00)},
			{Vec3(+1.0, -1.0, -1.0), ColorRGBA8(0xFF, 0x00, 0x00)},
			{Vec3(+1.0, +1.0, -1.0), ColorRGBA8(0xFF, 0xFF, 0x00)},
			{Vec3(-1.0, +1.0, -1.0), ColorRGBA8(0x00, 0xFF, 0x00)},
		};

		u32 meshIndices[36] =
		{
			0, 1, 2, 2, 3, 0,
			1, 5, 6, 6, 2, 1,
			7, 6, 5, 5, 4, 7,
			4, 0, 3, 3, 7, 4,
			4, 5, 1, 1, 0, 4,
			3, 2, 6, 6, 7, 3,
		};
		// clang-format on
#else

		// clang-format off
		Vertex meshVertices[12];
		{
			const float t = (1.0f + sqrtf(5.0f)) / 2.0f;

			Vec2 p = normalize(Vec2(1, t));

			u32 i = 0;

			auto makeVertex = [](float x, float y, float z) {
				Vertex v;
				v.position = Vec3(x, y, z) * 2.0f;
				v.color = ColorRGBA(Vec3(x, y, z) * 0.5f + 0.5f, 1.0f);
				return v;
			};

			meshVertices[i++] = makeVertex(-p.x, p.y, 0);
			meshVertices[i++] = makeVertex(+p.x, p.y, 0);
			meshVertices[i++] = makeVertex(-p.x, -p.y, 0);
			meshVertices[i++] = makeVertex(+p.x, -p.y, 0);
			meshVertices[i++] = makeVertex(0, -p.x, p.y);
			meshVertices[i++] = makeVertex(0, p.x, p.y);
			meshVertices[i++] = makeVertex(0, -p.x, -p.y);
			meshVertices[i++] = makeVertex(0, p.x, -p.y);
			meshVertices[i++] = makeVertex(p.y, 0, -p.x);
			meshVertices[i++] = makeVertex(p.y, 0, p.x);
			meshVertices[i++] = makeVertex(-p.y, 0, -p.x);
			meshVertices[i++] = makeVertex(-p.y, 0, p.x);
		}

		u32 meshIndices[60] = { 0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6,
			7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1 };
		// clang-format on
#endif

		m_meshVertexCount = RUSH_COUNTOF(meshVertices);
		m_meshIndexCount = RUSH_COUNTOF(meshIndices);

		m_vertexBuffer = Gfx_CreateBuffer(
			GfxBufferDesc(GfxBufferFlags::Vertex, RUSH_COUNTOF(meshVertices), sizeof(meshVertices[0])), meshVertices);

		m_indexBuffer = Gfx_CreateBuffer(
			GfxBufferDesc(GfxBufferFlags::Index, GfxFormat_R32_Uint, RUSH_COUNTOF(meshIndices), sizeof(meshIndices[0])),
			meshIndices);
	}

	void buildInstanceConstants(InstanceConstants& instanceConstants, u32 instanceIndex) const
	{
		u32 x = instanceIndex % m_rowCount;
		u32 y = instanceIndex / m_rowCount;

		float px = m_scale + ((float)x / m_colCount) * 2.0f - 1.0f;
		float py = m_scale + ((float)y / m_rowCount) * 2.0f - 1.0f;
		float pz = 0.0f;
		float s = m_scale * 0.5f;

		const Mat4& matRot = m_matrixPalette[instanceIndex % MatrixPaletteSize];

		instanceConstants.world.rows[0] = matRot.rows[0];
		instanceConstants.world.rows[1] = matRot.rows[1];
		instanceConstants.world.rows[2] = matRot.rows[2];
		instanceConstants.world.rows[3] = Vec4(px, -py, pz, 1.0f);
	};

	void update() override
	{
		auto ctx = Platform_GetGfxContext();

		m_gpuDrawTime.add(Gfx_Stats().lastFrameGpuTime);
		Gfx_ResetStats();

		const GfxCapability& caps    = Gfx_GetCapability();
		Mat4                 matView = Mat4::lookAt(Vec3(0.0f, 0.0f, -2.0f), Vec3(0.0f));
		Mat4 matProj = Mat4::perspective(m_window->getAspect(), 1.0f, 0.1f, 100.0f, caps.projectionFlags);

		GlobalConstants globalConstants;
		globalConstants.viewProj = (matView * matProj).transposed();
		Gfx_UpdateBufferT(ctx, m_globalConstantBuffer, globalConstants);

		GfxPassDesc passDesc;
		passDesc.clearColors[0] = ColorRGBA(0.1f, 0.2f, 0.3f);
		passDesc.clearDepth     = caps.deviceFarDepth;
		passDesc.flags          = GfxPassFlags::ClearAll;
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetDepthStencilState(ctx, m_depthStencilStates.writeLessEqual);
		Gfx_SetPrimitive(ctx, GfxPrimitive::TriangleList);
		Gfx_SetIndexStream(ctx, m_indexBuffer);
		Gfx_SetVertexStream(ctx, 0, m_vertexBuffer);
		Gfx_SetConstantBuffer(ctx, 0, m_globalConstantBuffer);

		double drawTime = 0.0;
		double buildTime = 0.0;

		buildTime -= m_timer.time();
		{
			const float time = float(m_timer.time());
			m_rowCount = u32(ceilf(sqrtf((float)m_instanceCount)));
			m_colCount = divUp(m_instanceCount, m_rowCount);
			float scaleX = 1.0f / m_rowCount;
			float scaleY = 1.0f / m_colCount;
			m_scale = min(scaleX, scaleY);

			m_matrixPalette.clear();
			m_matrixPalette.reserve(MatrixPaletteSize);
			for (int i = 0; i < MatrixPaletteSize; ++i)
			{
				Mat4 mat = Mat4::rotationX((float)i + 0.0f + time * 1.1f) *
					Mat4::rotationY((float)i + 1.0f + time * 1.2f) *
					Mat4::rotationZ((float)i + 2.0f + time * 1.3f);

				mat.rows[0] *= m_scale * 0.5f;
				mat.rows[1] *= m_scale * 0.5f;
				mat.rows[2] *= m_scale * 0.5f;

				m_matrixPalette.push(mat);
			}
		}
		buildTime += m_timer.time();

		u64 trianglesPerDraw = m_meshIndexCount / 3;
		u64 trianglesPerFrame = trianglesPerDraw * m_instanceCount;
		if (m_limitTriangleCount && trianglesPerFrame > m_maxTrianglesPerFrame)
		{
			trianglesPerDraw = max<u64>(1, m_maxTrianglesPerFrame / m_instanceCount);
		}
		RUSH_ASSERT(trianglesPerDraw*3 <= ~0u);
		const u32 indicesPerDraw = u32(trianglesPerDraw * 3);

		if (m_method == Method::ConstantBufferOffset)
		{
			Gfx_SetTechnique(ctx, m_technique);

			buildTime -= m_timer.time();
			for (u32 i = 0; i < (u32)m_instanceCount; ++i)
			{
				buildInstanceConstants(m_paddedInstanceConstants[i], i);
			}
			buildTime += m_timer.time();

			drawTime -= m_timer.time();

			Gfx_UpdateBuffer(ctx, m_instanceConstantBuffer, m_paddedInstanceConstants.data(),
			    m_instanceCount * sizeof(PaddedInstanceConstants));

			for (u32 i = 0; i < (u32)m_instanceCount; ++i)
			{
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer, sizeof(PaddedInstanceConstants) * i);
				Gfx_DrawIndexed(ctx, indicesPerDraw, 0, 0, m_meshVertexCount);
			}

			drawTime += m_timer.time();
		}
		else if (m_method == Method::DynamicConstantBuffer)
		{
			Gfx_SetTechnique(ctx, m_technique);

			for (u32 i = 0; i < (u32)m_instanceCount; ++i)
			{
				InstanceConstants constants;

				buildTime -= m_timer.time();
				buildInstanceConstants(constants, i);
				buildTime += m_timer.time();

				drawTime -= m_timer.time();

				Gfx_UpdateBufferT(ctx, m_dynamicInstanceConstantBuffer, constants);
				Gfx_SetConstantBuffer(ctx, 1, m_dynamicInstanceConstantBuffer);
				Gfx_DrawIndexed(ctx, indicesPerDraw, 0, 0, m_meshVertexCount);

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::PushConstants && caps.pushConstants)
		{
			Gfx_SetTechnique(ctx, m_techniquePush);

			for (u32 i = 0; i < (u32)m_instanceCount; ++i)
			{
				InstanceConstants constants;
				buildTime -= m_timer.time();
				buildInstanceConstants(constants, i);
				buildTime += m_timer.time();

				drawTime -= m_timer.time();

				Gfx_DrawIndexed(
				    ctx, indicesPerDraw, 0, 0, m_meshVertexCount, &constants.world, sizeof(constants.world));

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::ConstantBufferPushOffset && caps.pushConstants)
		{
			Gfx_SetTechnique(ctx, m_techniquePushOffset);

			const u32 batchSize  = MaxBatchSize;
			const u32 batchCount = divUp(m_instanceCount, batchSize);

			for (u32 batchIt = 0; batchIt < batchCount; ++batchIt)
			{
				const u32 batchBegin = batchIt * batchSize;
				const u32 batchEnd   = min<u32>(batchBegin + batchSize, m_instanceCount);
				const u32 batchSize  = batchEnd - batchBegin;

				buildTime -= m_timer.time();
				for (u32 i = 0; i < batchSize; ++i)
				{
					buildInstanceConstants(m_instanceConstants[i], batchBegin + i);
				}
				buildTime += m_timer.time();

				drawTime -= m_timer.time();

				Gfx_UpdateBuffer(
				    ctx, m_instanceConstantBuffer, m_instanceConstants.data(), batchSize * sizeof(InstanceConstants));
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer);

				for (u32 i = 0; i < (u32)batchSize; ++i)
				{
					Gfx_DrawIndexed(ctx, indicesPerDraw, 0, 0, m_meshVertexCount, &i, sizeof(i));
				}

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::Instancing && caps.instancing)
		{
			Gfx_SetTechnique(ctx, m_techniqueInstanced);

			const u32 batchSize  = MaxBatchSize;
			const u32 batchCount = divUp(m_instanceCount, batchSize);

			for (u32 batchIt = 0; batchIt < batchCount; ++batchIt)
			{
				const u32 batchBegin = batchIt * batchSize;
				const u32 batchEnd   = min<u32>(batchBegin + batchSize, m_instanceCount);
				const u32 batchSize  = batchEnd - batchBegin;

				buildTime -= m_timer.time();
				for (u32 i = 0; i < batchSize; ++i)
				{
					buildInstanceConstants(m_instanceConstants[i], batchBegin + i);
				}
				buildTime += m_timer.time();

				drawTime -= m_timer.time();

				Gfx_UpdateBuffer(
				    ctx, m_instanceConstantBuffer, m_instanceConstants.data(), batchSize * sizeof(InstanceConstants));
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer);
				Gfx_DrawIndexedInstanced(ctx, indicesPerDraw, 0, 0, m_meshVertexCount, batchSize, 0);

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::InstanceId && caps.instancing)
		{
			Gfx_SetVertexStream(ctx, 1, m_instanceIdBuffer);
			Gfx_SetTechnique(ctx, m_techniqueInstanceId);

			const u32 batchSize  = MaxBatchSize;
			const u32 batchCount = divUp(m_instanceCount, batchSize);

			for (u32 batchIt = 0; batchIt < batchCount; ++batchIt)
			{
				const u32 batchBegin = batchIt * batchSize;
				const u32 batchEnd   = min<u32>(batchBegin + batchSize, m_instanceCount);
				const u32 batchSize  = batchEnd - batchBegin;

				buildTime -= m_timer.time();
				for (u32 i = 0; i < batchSize; ++i)
				{
					buildInstanceConstants(m_instanceConstants[i], batchBegin + i);
				}
				buildTime += m_timer.time();

				drawTime -= m_timer.time();

				Gfx_UpdateBuffer(
				    ctx, m_instanceConstantBuffer, m_instanceConstants.data(), batchSize * sizeof(InstanceConstants));
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer);

				for (u32 i = 0; i < (u32)batchSize; ++i)
				{
					Gfx_DrawIndexedInstanced(ctx, indicesPerDraw, 0, 0, m_meshVertexCount, 1, i);
				}

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::DrawIndirect && caps.drawIndirect)
		{
			Gfx_SetVertexStream(ctx, 1, m_instanceIdBuffer);
			Gfx_SetTechnique(ctx, m_techniqueInstanceId);

			const u32 batchSize  = MaxBatchSize;
			const u32 batchCount = divUp(m_instanceCount, batchSize);

			for (u32 batchIt = 0; batchIt < batchCount; ++batchIt)
			{
				const u32 batchBegin = batchIt * batchSize;
				const u32 batchEnd   = min<u32>(batchBegin + batchSize, m_instanceCount);
				const u32 batchSize  = batchEnd - batchBegin;

				buildTime -= m_timer.time();
				for (u32 i = 0; i < batchSize; ++i)
				{
					buildInstanceConstants(m_instanceConstants[i], batchBegin + i);
				}
				buildTime += m_timer.time();

				drawTime -= m_timer.time();

				for (u32 i = 0; i < batchSize; ++i)
				{
					GfxDrawIndexedArg& arg = m_indirectArgs[i];
					arg.indexCount = indicesPerDraw;
					arg.instanceCount = 1;
					arg.firstIndex = 0;
					arg.vertexOffset = 0;
					arg.firstInstance = i;
				}

				Gfx_UpdateBuffer(
					ctx, m_indirectArgsBuffer, m_indirectArgs.data(), batchSize * sizeof(GfxDrawIndexedArg));

				Gfx_UpdateBuffer(
				    ctx, m_instanceConstantBuffer, m_instanceConstants.data(), batchSize * sizeof(InstanceConstants));

				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer);

				Gfx_DrawIndexedIndirect(ctx, m_indirectArgsBuffer, 0, batchSize);

				drawTime += m_timer.time();
			}
		}

		m_cpuBuildTime.add(buildTime);
		m_cpuDrawTime.add(drawTime);

		m_prim->begin2D(m_window->getSize());
		char statusString[1024];
		HumanFriendlyValue triangleCountHR = getHumanFriendlyValueShort(double(indicesPerDraw) * m_instanceCount / 3);
		HumanFriendlyValue trianglesPerDrawHR = getHumanFriendlyValueShort(double(indicesPerDraw) / 3);
		snprintf(statusString, 1024,
			"Method    : %s\n"
			"Meshes    : %d\n"
			"Triangles : %.2f%s\n"
			"Tris/draw : %.2f%s\n"
			"CPU build : %.2f ms\n"
			"CPU draw  : %.2f ms\n"
			"GPU draw  : %.2f", toString(m_method),
			m_instanceCount, 
			triangleCountHR.value, triangleCountHR.unit,
			trianglesPerDrawHR.value, trianglesPerDrawHR.unit,
			m_cpuBuildTime.get() * 1000.0f,
			m_cpuDrawTime.get() * 1000.0f, m_gpuDrawTime.get() * 1000.0f);

		m_font->draw(m_prim, Vec2(10.0f), statusString);

		snprintf(statusString, 1024, 
			"Key bindings\n"
			"  1..7:        Change draw method\n"
			"  Up/Down:     Increase/decrease number of draws by 1/frame\n"
			"  Left/Right:  Increase/decrease number of draws by 50/frame\n"
			"  PageUp/Down: Increase/decrease number of draws by 1000/frame\n"
			"  Home/End:    Maximum/minimum number of draws\n"
		);

		Vec2 stringSize = m_font->measure(statusString);
		m_font->draw(m_prim, Vec2(10.0f, m_window->getSizeFloat().y - stringSize.y), statusString);

		m_prim->end2D();

		Gfx_EndPass(ctx);

		u32 oldInstanceCount = m_instanceCount;
		Method oldMethod = m_method;

		if (m_window->getKeyboardState().isKeyDown(Key_Up))
		{
			m_instanceCount += 1;
		}

		if (m_window->getKeyboardState().isKeyDown(Key_Down))
		{
			m_instanceCount -= 1;
		}

		if (m_window->getKeyboardState().isKeyDown(Key_Right))
		{
			m_instanceCount += 50;
		}

		if (m_window->getKeyboardState().isKeyDown(Key_Left))
		{
			m_instanceCount -= 50;
		}

		if (m_window->getKeyboardState().isKeyDown(Key_PageUp))
		{
			m_instanceCount += 1000;
		}

		if (m_window->getKeyboardState().isKeyDown(Key_PageDown))
		{
			m_instanceCount -= 1000;
		}

		if (m_window->getKeyboardState().isKeyDown(Key_Home))
		{
			m_instanceCount = MaxInstanceCount;
		}

		if (m_window->getKeyboardState().isKeyDown(Key_End))
		{
			m_instanceCount = 1;
		}

		for (u32 i = 0; i < (u32)Method::count; ++i)
		{
			if (m_window->getKeyboardState().isKeyDown(Key_1 + i))
			{
				m_method = (Method)i;
			}
		}

		m_instanceCount = min<int>(max(m_instanceCount, 1), MaxInstanceCount);

		if (m_instanceCount != oldInstanceCount || m_method != oldMethod)
		{
			m_gpuDrawTime.reset();
			m_cpuDrawTime.reset();
			m_cpuBuildTime.reset();
		}

		m_frameCount++;
	}

private:


	struct GlobalConstants
	{
		Mat4 viewProj;
	};

	// Instance constants padded to 256 bytes
	struct PaddedInstanceConstants : InstanceConstants
	{
		Mat4 padding[3];
	};

	GfxOwn<GfxTechnique>    m_technique;
	GfxOwn<GfxTechnique>    m_techniquePush;
	GfxOwn<GfxTechnique>    m_techniquePushOffset;
	GfxOwn<GfxTechnique>    m_techniqueInstanced;
	GfxOwn<GfxTechnique>    m_techniqueInstanceId;
	GfxOwn<GfxVertexFormat> m_vertexFormat;
	GfxOwn<GfxVertexFormat> m_vertexFormatInstanceId;

	GfxOwn<GfxBuffer> m_vertexBuffer;
	GfxOwn<GfxBuffer> m_instanceIdBuffer;
	GfxOwn<GfxBuffer> m_indexBuffer;
	GfxOwn<GfxBuffer> m_indirectArgsBuffer;

	GfxOwn<GfxBuffer>                    m_globalConstantBuffer;
	GfxOwn<GfxBuffer>                    m_instanceConstantBuffer;
	GfxOwn<GfxBuffer>                    m_dynamicInstanceConstantBuffer;
	std::vector<PaddedInstanceConstants> m_paddedInstanceConstants;
	std::vector<InstanceConstants>       m_instanceConstants;
	std::vector<GfxDrawIndexedArg>       m_indirectArgs;

	enum class Method
	{
		ConstantBufferOffset,
		DynamicConstantBuffer,
		PushConstants,
		ConstantBufferPushOffset,
		Instancing,
		InstanceId,
		DrawIndirect,

		count
	};

	static const char* toString(Method method)
	{
		switch (method)
		{
		default: return "Unknown";
		case Method::ConstantBufferOffset: return "ConstantBufferOffset";
		case Method::DynamicConstantBuffer: return "DynamicConstantBuffer";
		case Method::PushConstants: return "PushConstants";
		case Method::ConstantBufferPushOffset: return "ConstantBufferPushOffset";
		case Method::Instancing: return "Instancing";
		case Method::InstanceId: return "InstanceId";
		case Method::DrawIndirect: return "DrawIndirect";
		}
	}

	Method m_method = Method::Instancing;
	bool m_limitTriangleCount = true;
	const u64 m_maxTrianglesPerFrame = 200'000'000;

	u32 m_meshVertexCount = 0;
	u32 m_meshIndexCount  = 0;

	int m_instanceCount = 10000;
	enum
	{
		MaxInstanceCount = 1'000'000,
		MaxBatchSize     = 1000,
	};

	Timer m_timer;

	MovingAverage<double, 60> m_gpuDrawTime;
	MovingAverage<double, 60> m_cpuDrawTime;
	MovingAverage<double, 60> m_cpuBuildTime;

	u32 m_frameCount = 0;

	u32 m_rowCount = 1;
	u32 m_colCount = 1;
	float m_scale = 1.0f;

	static constexpr u32 MatrixPaletteSize = 1024;
	DynamicArray<Mat4> m_matrixPalette;
};

int main(int argc, char** argv)
{
	g_appCfg.name = "Instancing (" RUSH_RENDER_API_NAME ")";

	g_appCfg.width     = 1280;
	g_appCfg.height    = 720;
	g_appCfg.argc      = argc;
	g_appCfg.argv      = argv;
	g_appCfg.resizable = true;

#ifdef RUSH_DEBUG
	g_appCfg.debug = true;
#endif

	return Platform_Main<InstancingApp>(g_appCfg);
}
