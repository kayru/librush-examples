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

	void update() override;

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
	};

	std::vector<Vertex> m_vertices;
	std::vector<u32>    m_indices;

	GfxOwn<GfxBuffer> m_indexBuffer;
	GfxOwn<GfxBuffer> m_vertexBuffer;
	GfxOwn<GfxBuffer> m_sceneConstantBuffer;
	GfxOwn<GfxBuffer> m_tonemapConstantBuffer;
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
	};

	Mat4 m_worldTransform = Mat4::identity();
	Box3 m_boundingBox;

	std::string m_statusString;
	bool m_valid = false;

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
		u32 albedoTextureId = 0;
		u32 specularTextureId = 0;
		u32 firstIndex = 0;
		AlphaMode alphaMode = AlphaMode::Opaque;
		float metallicFactor = 0;
		float roughnessFactor = 1;
		float reflectance = 0.08f;
		MaterialMode materialMode = MaterialMode::MetallicRoughness;
	};

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

	static constexpr u32     MaxTextures = 1024;
	GfxOwn<GfxDescriptorSet> m_materialDescriptorSet;

	bool m_loadingThreadShouldExit = false;
	u32 m_frameIndex = 0;
	bool m_showUI = true;

	std::mutex m_loadingMutex;

	GfxOwn<GfxRayTracingPipeline>    m_rtPipeline;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;
	GfxOwn<GfxBuffer>                m_sbtBuffer;
	GfxOwn<GfxTexture>               m_outputImage;
	GfxOwn<GfxTechnique>             m_blitTonemap;
	GfxOwn<GfxTexture>               m_envmap;
	GfxOwn<GfxBuffer>                m_envmapDistribution;

	struct Settings
	{
		bool m_useEnvmap = false;
		bool m_useNeutralBackground = false;
		bool m_useDepthOfField = false;
		float m_gamma = 1.8f;
		float m_exposureEV100 = 2.0f;
		Vec2 m_cameraSensorSizeMM = Vec2(36.0f, 24.0f); // 35mm film
		float m_focalLengthMM = 50.0;
		float m_apertureSizeMM = 0.0;
		float m_focusDistance = 1.0;
		float m_envmapRotationDegrees = 0.0;
	};

	Settings m_settings;

	void loadingThreadFunction();
	void createRayTracingScene(GfxContext* ctx);

	void createGpuScene();
	void saveCamera();
	void loadCamera();
	void resetCamera();
	void loadEnvmap(const char* filename);
};
