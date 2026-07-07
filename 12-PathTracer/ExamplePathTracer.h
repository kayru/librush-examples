#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/MathTypes.h>
#include <Rush/Platform.h>
#include <Rush/UtilCamera.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <Common/ExampleApp.h>
#include <Common/Utils.h>
#include <Common/VirtualGamepad.h>

#include <memory>
#include <mutex>
#include <stdio.h>
#include <string>
#include <thread>
#include <unordered_map>

#include "Common.glsl"

class ExamplePathTracer : public ExampleApp
{
public:

	ExamplePathTracer();
	~ExamplePathTracer();

	void onUpdate() override;

private:

	void render();

	bool loadModel(const char* filename);
	bool loadModelObj(const char* filename);
	bool loadModelGLTF(const char* filename);

	u32 enqueueLoadTexture(const std::string& filename, GfxFormat format);

	Timer m_timer;

	struct Stats
	{
		MovingAverage<double, 60> gpuTotal;
		MovingAverage<double, 60> cpuTotal;
	} m_stats;

	double m_totalGpuRenderTime = 0;

	Camera m_camera;
	CameraManipulator* m_cameraMan;

	u32 m_defaultWhiteTextureId;

	struct Vertex
	{
		Vec3 position;
		Vec3 normal;
		Vec2 texcoord;
		Vec4 tangent; // bitangent = cross(normal, tangent.xyz) * tangent.w
	};

	std::vector<Vertex> m_vertices;
	std::vector<u32>    m_indices;

	GfxOwn<GfxBuffer> m_indexBuffer;
	GfxOwn<GfxBuffer> m_vertexBuffer;
	GfxOwn<GfxBuffer> m_sceneConstantBuffer;
	GfxOwn<GfxBuffer> m_tonemapConstantBuffer;
	GfxOwn<GfxBuffer> m_materialBuffer;
	GfxOwn<GfxBuffer> m_materialIndexBuffer;
	GfxOwn<GfxBuffer> m_rtInstanceBuffer;
	u32 m_indexCount = 0;
	u32 m_vertexCount = 0;

	struct TonemapConstants
	{
		float exposure = 1;
		float gamma = 1;
	};

	struct SceneConstants
	{
		Mat4 matView = Mat4::identity();
		Mat4 matProj = Mat4::identity();
		Mat4 matViewProj = Mat4::identity();
		Mat4 matViewProjInv = Mat4::identity();
		Mat4 matEnvmapTransform = Mat4::identity();
		Vec4 cameraPosition = Vec4(0.0);

		Tuple2i outputSize = {};
		u32 frameIndex = 0;
		u32 flags = 0;

		Tuple2i envmapSize = {};
		Vec2 cameraSensorSize;

		float focalLength;
		float focusDistance;
		float apertureSize;
		u32 debugVisMode = 0;

		Tuple2i focusPickPixel = {-1, -1}; // cursor pixel; x < 0 = no pick
		float focalPlaneFalloffPx = 4.0f;
		u32 normalMapBounceLimit = 5; // apply normal maps only on bounces <= this (perf toggle)

		// Single rectangular area light (PT_FLAG_USE_AREA_LIGHT): point(u,v) = origin + u*edgeU + v*edgeV.
		Vec4 areaLightOrigin = Vec4(0.0f);
		Vec4 areaLightEdgeU = Vec4(0.0f);
		Vec4 areaLightEdgeV = Vec4(0.0f);
		Vec4 areaLightEmission = Vec4(0.0f);

		u32 samplerMode = 0;
	};

	Mat4 m_worldTransform = Mat4::identity();
	Box3 m_boundingBox;

	std::string m_statusString;
	std::string m_modelFilename;
	bool m_valid = false;

	bool m_haveNormals = false;
	bool m_haveTangents = false;
	bool m_haveTexcoords = false;
	bool m_haveNormalMaps = false;

	enum class AlphaMode : u32
	{
		Opaque,
		Mask,
		Blend
	};

	enum class MaterialMode : u32
	{
		MetallicRoughness  = PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS,
		SpecularGlossiness = PT_MATERIAL_MODE_PBR_SPECULAR_GLOSSINESS
	};

	struct MaterialConstants
	{
		Vec4 albedoFactor = Vec4(1.0f);
		Vec4 specularFactor = Vec4(1.0f);
		Vec4 emissiveFactor = Vec4(0.0f); // xyz = emitted radiance (area-light surfaces)
		u32 albedoTextureId = 0;
		u32 specularTextureId = 0;
		u32 normalTextureId = 0;
		u32 firstIndex = 0;
		AlphaMode alphaMode = AlphaMode::Opaque;
		float metallicFactor = 0;
		float roughnessFactor = 1;
		float reflectance = 0.08f;
		MaterialMode materialMode = MaterialMode::MetallicRoughness;
	};

	static_assert(sizeof(MaterialConstants) == 84, "MaterialConstants must stay tightly packed (scalar layout)");

	std::vector<MaterialConstants> m_materials;
	GfxOwn<GfxBuffer> m_defaultConstantBuffer;

	struct MeshSegment
	{
		u32 material = 0;
		u32 indexOffset = 0;
		u32 indexCount = 0;
	};

	std::vector<MeshSegment> m_segments;

	WindowEventListener m_windowEvents;

	float m_cameraScale = 1.0f;

	struct TextureData
	{
		GfxTextureDesc     desc;
		std::vector<u8>    mips[16];
		u32                descriptorIndex;
		std::string        filename;
	};

	std::vector<GfxOwn<GfxTexture>> m_textureDescriptors;

	std::unordered_map<std::string, TextureData*> m_textures;
	std::vector<TextureData*>                     m_pendingTextures;
	std::vector<TextureData*>                     m_loadedTextures;

	static constexpr u32     MaxTextures = PT_MAX_TEXTURES;
	GfxOwn<GfxDescriptorSet> m_materialDescriptorSet;

	bool m_loadingThreadShouldExit = false;
	u32 m_frameIndex = 0;
	bool m_showUI = true;
	std::string m_startupError;
	bool m_useProceduralScene = false;

	// Scene-owned rectangular area light (self-lit procedural scenes, e.g. Cornell Box).
	bool m_useAreaLight = false;
	Vec3 m_areaLightOrigin = Vec3(0.0f);
	Vec3 m_areaLightEdgeU = Vec3(0.0f);
	Vec3 m_areaLightEdgeV = Vec3(0.0f);
	Vec3 m_areaLightEmission = Vec3(0.0f);

	// Headless render-to-PNG; active when m_headlessOutPath is set (--out).
	std::string m_headlessOutPath;
	u32         m_headlessSpp  = 1024;
	Tuple2i     m_headlessSize = {1920, 1080};

	std::mutex m_loadingMutex;

	// Full RT pipeline (traceRayEXT + SBT) or inline ray-query compute (shared with Metal).
	enum class TracingMode : u32
	{
		RayTracingPipeline = 0,
		RayQuery           = 1,
	};

	GfxOwn<GfxRayTracingPipeline>    m_rtPipeline;
	GfxOwn<GfxComputePipeline>       m_rayQueryPipeline;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;
	GfxOwn<GfxBuffer>                m_sbtBuffer;
	bool                             m_blasIsInline = false;
	GfxOwn<GfxTexture>               m_outputImage;
	GfxOwn<GfxRenderPipeline>        m_blitTonemap;
	GfxOwn<GfxTexture>               m_envmap;
	GfxOwn<GfxBuffer>                m_envmapDistribution;
	GfxOwn<GfxBuffer>                m_sobolBuffer;

	// click-to-focus: shader writes the cursor pixel's depth here, read back same frame
	GfxOwn<GfxBuffer> m_focusFeedbackBuffer;
	Tuple2i           m_focusPickPixel = {};
	bool              m_focusPickRequested = false;

	struct Settings
	{
		bool m_useEnvmap = false;
		bool m_useNeutralBackground = false;
		bool m_useDepthOfField = false;
		bool  m_useNormalMapping = true;
		bool m_debugSimpleShading = false;
		bool m_debugDisableAccumulation = false;
		bool m_debugHitMask = false;
		int m_debugVisMode = 0;
		float m_gamma = 1.8f;
		float m_exposureEV100 = 0.0f;
		int m_sensorPreset = 0; // index into g_sensorPresets
		Vec2 m_cameraSensorSizeMM = Vec2(36.0f, 24.0f);
		int m_focalLengthPreset = 4; // index into g_focalLengthPresets (35 mm)
		float m_focalLengthMM = 35.0;
		float m_apertureFStop = 1.4f;
		float m_focusDistance = 1.0;
		bool m_showFocusAssist = false;
		float m_focusAssistFalloffPx = 4.0f;
		float m_envmapRotationDegrees = 0.0;
		int m_tracingMode = int(TracingMode::RayQuery); // Vulkan-only, clamped to available backends on load
		bool m_useRussianRoulette = true; // perf: terminate low-throughput paths (unbiased)
		int m_normalMapBounceLimit = 5; // perf: apply normal maps only on bounces <= this (5 = all)
		int m_samplerMode = int(PT_SAMPLER_LCG); // PT_SAMPLER_* sample generator

		template <typename Ar> void describe(Ar& ar)
		{
			ar.field("useEnvmap", m_useEnvmap);
			ar.field("useNeutralBackground", m_useNeutralBackground);
			ar.field("useDepthOfField", m_useDepthOfField);
			ar.field("useNormalMapping", m_useNormalMapping);
			ar.field("debugSimpleShading", m_debugSimpleShading);
			ar.field("debugDisableAccumulation", m_debugDisableAccumulation);
			ar.field("debugHitMask", m_debugHitMask);
			ar.field("debugVisMode", m_debugVisMode);
			ar.field("gamma", m_gamma);
			ar.field("exposureEV100", m_exposureEV100);
			ar.field("sensorPreset", m_sensorPreset);
			ar.field("cameraSensorSizeMM", m_cameraSensorSizeMM);
			ar.field("focalLengthPreset", m_focalLengthPreset);
			ar.field("focalLengthMM", m_focalLengthMM);
			ar.field("apertureFStop", m_apertureFStop);
			ar.field("focusDistance", m_focusDistance);
			ar.field("showFocusAssist", m_showFocusAssist);
			ar.field("focusAssistFalloffPx", m_focusAssistFalloffPx);
			ar.field("envmapRotationDegrees", m_envmapRotationDegrees);
			ar.field("tracingMode", m_tracingMode);
			ar.field("useRussianRoulette", m_useRussianRoulette);
			ar.field("normalMapBounceLimit", m_normalMapBounceLimit);
			ar.field("samplerMode", m_samplerMode);
		}
	};

	Settings m_settings;

	void loadingThreadFunction();
	void createRayTracingScene(GfxContext* ctx);

	// Inline single-geometry BLAS + material buffers (always on Metal; Vulkan ray-query mode).
	bool useInlineScene() const;
	void createBottomLevelAccelerationStructure();
	void rebuildAccelerationStructures();

	SceneConstants makeSceneConstants(Tuple2i outputSize, u32 frameIndex) const;

	float computeExposure() const;

	// The window's aspect when present, else the headless render size.
	float outputAspect() const;

	void renderHeadless(GfxContext* ctx);

	void createGpuScene();
	void createSobolBuffer();
	const char* configModelName() const; // model path for the config key, or null when procedural
	void saveConfig();
	void loadConfig();
	void resetCamera();
	void focusOnCursor();
	void loadEnvmap(const char* filename);

	VirtualGamepad m_virtualGamepad;
};
