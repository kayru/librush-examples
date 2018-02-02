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

using namespace Rush;

class InstancingApp : public ExampleApp
{
public:
	InstancingApp() : ExampleApp()
	{
		Gfx_SetPresentInterval(0);

		// TODO: load a model from obj file

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
				v.color    = ColorRGBA(Vec3(x, y, z) * 0.5f + 0.5f, 1.0f);
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

		u32 meshIndices[60] = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6,
		    7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
		// clang-format on
#endif

		m_meshVertexCount = RUSH_COUNTOF(meshVertices);
		m_meshIndexCount  = RUSH_COUNTOF(meshIndices);

		m_vertexBuffer.takeover(Gfx_CreateBuffer(GfxBufferDesc(GfxBufferType::Vertex, GfxBufferMode::Static,
		                                             RUSH_COUNTOF(meshVertices), sizeof(meshVertices[0])),
		    meshVertices));

		m_indexBuffer.takeover(
		    Gfx_CreateBuffer(GfxBufferDesc(GfxBufferType::Index, GfxBufferMode::Static, GfxFormat_R32_Uint,
		                         RUSH_COUNTOF(meshIndices), sizeof(meshIndices[0])),
		        meshIndices));

		GfxVertexFormatDesc vfDesc;
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Position, 0);
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Color, GfxVertexFormatDesc::Semantic::Color, 0);
		m_vertexFormat.takeover(Gfx_CreateVertexFormat(vfDesc));

		GfxVertexFormatDesc vfDescInstanceId;
		vfDescInstanceId.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Position, 0);
		vfDescInstanceId.add(0, GfxVertexFormatDesc::DataType::Color, GfxVertexFormatDesc::Semantic::Color, 0);
		m_vertexFormatInstanceId.takeover(Gfx_CreateVertexFormat(vfDescInstanceId));

		{
			GfxVertexShader vs = Gfx_CreateVertexShader(loadShaderFromFile("ModelVS.hlsl.bin"));
			GfxPixelShader  ps = Gfx_CreatePixelShader(loadShaderFromFile("ModelPS.hlsl.bin"));

			struct SpecializationData
			{
				int maxBatchCount = MaxBatchSize;
			} specializationData;

			GfxSpecializationConstant specializationConstants[] = {{0, 0, sizeof(int)}};

			{
				GfxShaderBindings bindings;
				bindings.addConstantBuffer("Global", 0);
				bindings.addConstantBuffer("Instance", 1);
				GfxTechniqueDesc techDesc(ps, vs, m_vertexFormat.get(), &bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_technique.takeover(Gfx_CreateTechnique(techDesc));
			}

			{
				GfxVertexShader   vsPush = Gfx_CreateVertexShader(loadShaderFromFile("ModelPush.vert.bin"));
				GfxShaderBindings bindings;
				bindings.addPushConstants("PushConstants", GfxStageFlags::Vertex, sizeof(Mat4));
				bindings.addConstantBuffer("Global", 0);
				GfxTechniqueDesc techDesc(ps, vsPush, m_vertexFormat.get(), &bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_techniquePush.takeover(Gfx_CreateTechnique(techDesc));
				Gfx_Release(vsPush);
			}

			{
				GfxVertexShader   vsPushOffset = Gfx_CreateVertexShader(loadShaderFromFile("ModelPushOffset.vert.bin"));
				GfxShaderBindings bindings;
				bindings.addPushConstants("PushConstants", GfxStageFlags::Vertex, sizeof(u32));
				bindings.addConstantBuffer("Global", 0);
				bindings.addConstantBuffer("Instance", 1);
				GfxTechniqueDesc techDesc(ps, vsPushOffset, m_vertexFormat.get(), &bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_techniquePushOffset.takeover(Gfx_CreateTechnique(techDesc));
				Gfx_Release(vsPushOffset);
			}

			{
				GfxVertexShader   vsInstanced = Gfx_CreateVertexShader(loadShaderFromFile("ModelInstanced.vert.bin"));
				GfxShaderBindings bindings;
				bindings.addConstantBuffer("Global", 0);
				bindings.addConstantBuffer("Instance", 1);
				GfxTechniqueDesc techDesc(ps, vsInstanced, m_vertexFormat.get(), &bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_techniqueInstanced.takeover(Gfx_CreateTechnique(techDesc));
				Gfx_Release(vsInstanced);
			}

			{
				GfxVertexShader   vsInstanceId = Gfx_CreateVertexShader(loadShaderFromFile("ModelInstanced.vert.bin"));
				GfxShaderBindings bindings;
				bindings.addConstantBuffer("Global", 0);
				bindings.addConstantBuffer("Instance", 1);
				GfxTechniqueDesc techDesc(ps, vsInstanceId, m_vertexFormatInstanceId.get(), &bindings);
				techDesc.specializationConstants     = specializationConstants;
				techDesc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				techDesc.specializationData          = &specializationData;
				techDesc.specializationDataSize      = sizeof(specializationData);
				m_techniqueInstanceId.takeover(Gfx_CreateTechnique(techDesc));
				Gfx_Release(vsInstanceId);
			}

			Gfx_Release(ps);
			Gfx_Release(vs);
		}

		m_globalConstantBuffer.takeover(Gfx_CreateBuffer(GfxBufferDesc(
		    GfxBufferType::Constant, GfxBufferMode::Temporary, GfxFormat_Unknown, 1, sizeof(GlobalConstants))));

		m_instanceConstantBuffer.takeover(Gfx_CreateBuffer(GfxBufferDesc(GfxBufferType::Constant,
		    GfxBufferMode::Temporary,
		    GfxFormat_Unknown,
		    MaxInstanceCount,
		    sizeof(InstanceConstants))));

		m_dynamicInstanceConstantBuffer.takeover(Gfx_CreateBuffer(GfxBufferDesc(
		    GfxBufferType::Constant, GfxBufferMode::Temporary, GfxFormat_Unknown, 1, sizeof(InstanceConstants))));

		m_indirectArgs.resize(MaxInstanceCount);
		for (u32 i = 0; i < MaxInstanceCount; ++i)
		{
			GfxDrawIndexedArg& arg = m_indirectArgs[i];
			arg.indexCount         = m_meshIndexCount;
			arg.instanceCount      = 1;
			arg.firstIndex         = 0;
			arg.vertexOffset       = 0;
			arg.firstInstance      = i;
		}

		m_indirectArgsBuffer.takeover(Gfx_CreateBuffer(GfxBufferDesc(GfxBufferType::IndirectArgs,
		                                                   GfxBufferMode::Static,
		                                                   GfxFormat_Unknown,
		                                                   MaxInstanceCount,
		                                                   sizeof(GfxDrawIndexedArg)),
		    m_indirectArgs.data()));

		m_paddedInstanceConstants.resize(MaxInstanceCount);
		m_instanceConstants.resize(MaxInstanceCount);
	}

	void update() override
	{
		auto ctx = Platform_GetGfxContext();

		m_gpuTime.add(Gfx_Stats().lastFrameGpuTime);
		Gfx_ResetStats();

		const GfxCapability& caps    = Gfx_GetCapability();
		Mat4                 matView = Mat4::lookAt(Vec3(0.0f, 0.0f, -2.0f), Vec3(0.0f));
		Mat4 matProj = Mat4::perspective(m_window->getAspect(), 1.0f, 0.1f, 100.0f, caps.projectionFlags);

		GlobalConstants globalConstants;
		globalConstants.viewProj = (matView * matProj).transposed();
		Gfx_UpdateBuffer(ctx, m_globalConstantBuffer, globalConstants);

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

		u32   rowCount = u32(ceilf(sqrtf((float)m_instanceCount)));
		u32   colCount = divUp(m_instanceCount, rowCount);
		float scaleX   = 1.0f / rowCount;
		float scaleY   = 1.0f / colCount;
		float scale    = min(scaleX, scaleY);

		auto buildInstanceConstants = [&](InstanceConstants& instanceConstants, u32 instanceIndex) {
			u32 x = instanceIndex % rowCount;
			u32 y = instanceIndex / rowCount;

			float px = scale + ((float)x / colCount) * 2.0f - 1.0f;
			float py = scale + ((float)y / rowCount) * 2.0f - 1.0f;
			float pz = 0.0f;
			float s  = scale * 0.5f;

			Mat4& matWorld   = instanceConstants.world;
			matWorld.rows[0] = Vec4(s, 0.0f, 0.0f, 0.0f);
			matWorld.rows[1] = Vec4(0.0f, s, 0.0f, 0.0f);
			matWorld.rows[2] = Vec4(0.0f, 0.0f, s, 0.0f);
			matWorld.rows[3] = Vec4(px, -py, pz, 1.0f);

#if 1
			Mat4 matWorldRot = Mat4::rotationX((float)instanceIndex + 1.0f + (float)m_timer.time() * 1.1f) *
			                   Mat4::rotationY((float)instanceIndex + 2.0f + (float)m_timer.time() * 1.2f) *
			                   Mat4::rotationZ((float)instanceIndex + 3.0f + (float)m_timer.time() * 1.3f);
			instanceConstants.world = matWorldRot * matWorld;
#else
			instanceConstants.world = matWorld;
#endif

			instanceConstants.world.transpose();
		};

		double drawTime = 0.0;

		if (m_method == Method::ConstantBufferOffset)
		{
			Gfx_SetTechnique(ctx, m_technique);

			for (u32 i = 0; i < (u32)m_instanceCount; ++i)
			{
				buildInstanceConstants(m_paddedInstanceConstants[i], i);
			}

			drawTime -= m_timer.time();

			Gfx_UpdateBuffer(ctx, m_instanceConstantBuffer, m_paddedInstanceConstants.data(), 0,
			    m_instanceCount * sizeof(PaddedInstanceConstants), true);

			for (u32 i = 0; i < (u32)m_instanceCount; ++i)
			{
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer, sizeof(PaddedInstanceConstants) * i);
				Gfx_DrawIndexed(ctx, m_meshIndexCount, 0, 0, m_meshVertexCount);
			}

			drawTime += m_timer.time();
		}
		else if (m_method == Method::DynamicConstantBuffer)
		{
			Gfx_SetTechnique(ctx, m_technique);

			for (u32 i = 0; i < (u32)m_instanceCount; ++i)
			{
				InstanceConstants constants;
				buildInstanceConstants(constants, i);

				drawTime -= m_timer.time();

				Gfx_UpdateBuffer(ctx, m_dynamicInstanceConstantBuffer, constants);
				Gfx_SetConstantBuffer(ctx, 1, m_dynamicInstanceConstantBuffer);
				Gfx_DrawIndexed(ctx, m_meshIndexCount, 0, 0, m_meshVertexCount);

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::PushConstants)
		{
			Gfx_SetTechnique(ctx, m_techniquePush);

			for (u32 i = 0; i < (u32)m_instanceCount; ++i)
			{
				InstanceConstants constants;
				buildInstanceConstants(constants, i);

				drawTime -= m_timer.time();

				Gfx_DrawIndexed(
				    ctx, m_meshIndexCount, 0, 0, m_meshVertexCount, &constants.world, sizeof(constants.world));

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::ConstantBufferPushOffset)
		{
			Gfx_SetTechnique(ctx, m_techniquePushOffset);

			const u32 batchSize  = MaxBatchSize;
			const u32 batchCount = divUp(m_instanceCount, batchSize);

			for (u32 batchIt = 0; batchIt < batchCount; ++batchIt)
			{
				const u32 batchBegin = batchIt * batchSize;
				const u32 batchEnd   = min<u32>(batchBegin + batchSize, m_instanceCount);
				const u32 batchSize  = batchEnd - batchBegin;

				for (u32 i = 0; i < batchSize; ++i)
				{
					buildInstanceConstants(m_instanceConstants[i], batchBegin + i);
				}

				drawTime -= m_timer.time();

				Gfx_UpdateBuffer(ctx, m_instanceConstantBuffer, m_instanceConstants.data(), 0,
				    batchSize * sizeof(InstanceConstants), true);
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer);

				for (u32 i = 0; i < (u32)batchSize; ++i)
				{
					Gfx_DrawIndexed(ctx, m_meshIndexCount, 0, 0, m_meshVertexCount, &i, sizeof(i));
				}

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::Instancing)
		{
			Gfx_SetTechnique(ctx, m_techniqueInstanced);

			const u32 batchSize  = MaxBatchSize;
			const u32 batchCount = divUp(m_instanceCount, batchSize);

			for (u32 batchIt = 0; batchIt < batchCount; ++batchIt)
			{
				const u32 batchBegin = batchIt * batchSize;
				const u32 batchEnd   = min<u32>(batchBegin + batchSize, m_instanceCount);
				const u32 batchSize  = batchEnd - batchBegin;

				for (u32 i = 0; i < batchSize; ++i)
				{
					buildInstanceConstants(m_instanceConstants[i], batchBegin + i);
				}

				drawTime -= m_timer.time();

				Gfx_UpdateBuffer(ctx, m_instanceConstantBuffer, m_instanceConstants.data(), 0,
				    batchSize * sizeof(InstanceConstants), true);
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer);
				Gfx_DrawIndexedInstanced(ctx, m_meshIndexCount, 0, 0, m_meshVertexCount, batchSize, 0);

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::InstanceId)
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

				for (u32 i = 0; i < batchSize; ++i)
				{
					buildInstanceConstants(m_instanceConstants[i], batchBegin + i);
				}

				drawTime -= m_timer.time();

				Gfx_UpdateBuffer(ctx, m_instanceConstantBuffer, m_instanceConstants.data(), 0,
				    batchSize * sizeof(InstanceConstants), true);
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer);

				for (u32 i = 0; i < (u32)batchSize; ++i)
				{
					Gfx_DrawIndexedInstanced(ctx, m_meshIndexCount, 0, 0, m_meshVertexCount, 1, i);
				}

				drawTime += m_timer.time();
			}
		}
		else if (m_method == Method::DrawIndirect)
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

				for (u32 i = 0; i < batchSize; ++i)
				{
					buildInstanceConstants(m_instanceConstants[i], batchBegin + i);
				}

				drawTime -= m_timer.time();

				Gfx_UpdateBuffer(ctx, m_instanceConstantBuffer, m_instanceConstants.data(), 0,
				    batchSize * sizeof(InstanceConstants), true);
				Gfx_SetConstantBuffer(ctx, 1, m_instanceConstantBuffer);

				Gfx_DrawIndexedIndirect(ctx, m_indirectArgsBuffer.get(), 0, batchSize);

				drawTime += m_timer.time();
			}
		}

		m_cpuTime.add(drawTime);

		m_prim->begin2D(m_window->getSize());
		char statusString[1024];
		snprintf(statusString, 1024, "Method: %s\nCubes: %d\nCPU: %.2f ms\nGPU: %.2f", toString(m_method),
		    m_instanceCount, m_cpuTime.get() * 1000.0f, m_gpuTime.get() * 1000.0f);

		m_font->draw(m_prim, Vec2(10.0f), statusString);
		m_prim->end2D();

		Gfx_EndPass(ctx);

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

		for (u32 i = 0; i < (u32)Method::count; ++i)
		{
			if (m_window->getKeyboardState().isKeyDown(Key_1 + i))
			{
				m_method = (Method)i;
			}
		}

		m_instanceCount = min<int>(max(m_instanceCount, 1), MaxInstanceCount);
	}

private:
	struct Vertex
	{
		Vec3       position;
		ColorRGBA8 color;
	};

	struct GlobalConstants
	{
		Mat4 viewProj;
	};

	struct InstanceConstants
	{
		Mat4 world;
	};

	// Instance constants padded to 256 bytes
	struct PaddedInstanceConstants : InstanceConstants
	{
		Mat4 padding[3];
	};

	GfxTechniqueRef    m_technique;
	GfxTechniqueRef    m_techniquePush;
	GfxTechniqueRef    m_techniquePushOffset;
	GfxTechniqueRef    m_techniqueInstanced;
	GfxTechniqueRef    m_techniqueInstanceId;
	GfxVertexFormatRef m_vertexFormat;
	GfxVertexFormatRef m_vertexFormatInstanceId;

	GfxBufferRef m_vertexBuffer;
	GfxBufferRef m_instanceIdBuffer;
	GfxBufferRef m_indexBuffer;
	GfxBufferRef m_indirectArgsBuffer;

	GfxBufferRef                         m_globalConstantBuffer;
	GfxBufferRef                         m_instanceConstantBuffer;
	GfxBufferRef                         m_dynamicInstanceConstantBuffer;
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

	u32 m_meshVertexCount = 0;
	u32 m_meshIndexCount  = 0;

	int m_instanceCount = 10000;
	enum
	{
		MaxInstanceCount = 50000,
		MaxBatchSize     = 1000, // making this larger requires updating vertex shaders
	};

	Timer m_timer;

	MovingAverage<double, 60> m_gpuTime;
	MovingAverage<double, 60> m_cpuTime;
};

int main()
{
	AppConfig cfg;

	cfg.name = "Instancing (" RUSH_RENDER_API_NAME ")";

	cfg.width     = 1280;
	cfg.height    = 720;
	cfg.resizable = true;

	return Platform_Main<InstancingApp>(cfg);
}
