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

class ExampleRTModelViewer : public ExampleApp
{
public:

	ExampleRTModelViewer();
	~ExampleRTModelViewer();

	void update() override;

private:

	void render();

	bool loadModel(const char* filename);
	bool loadModelObj(const char* filename);

	void enqueueLoadTexture(const std::string& filename, u32 materialId);

	Timer m_timer;

	struct Stats
	{
		MovingAverage<double, 60> gpuTotal;
		MovingAverage<double, 60> cpuTotal;
	} m_stats;

	Camera m_camera;
	Camera m_interpolatedCamera;

	CameraManipulator* m_cameraMan;

	GfxOwn<GfxTexture> m_defaultWhiteTexture;

	GfxOwn<GfxBuffer> m_vertexBuffer;
	GfxOwn<GfxBuffer> m_indexBuffer;
	GfxOwn<GfxBuffer> m_constantBuffer;
	u32 m_indexCount = 0;
	u32 m_vertexCount = 0;

	struct Constants
	{
		Mat4 matViewProj = Mat4::identity();
		Mat4 matWorld = Mat4::identity();
	};

	Mat4 m_worldTransform = Mat4::identity();

	Box3 m_boundingBox;

	struct Vertex
	{
		Vec3 position;
		Vec3 normal;
		Vec2 texcoord;
	};

	std::string m_statusString;
	bool m_valid = false;

	std::unordered_map<u64, GfxOwn<GfxBuffer>> m_materialConstantBuffers;

	struct MaterialConstants
	{
		Vec4 baseColor = Vec4(1.0f);
		u32 albedoTextureId = ~0u;
	};

	struct Material
	{
		GfxTexture albedoTexture;
		GfxBuffer constantBuffer;

		GfxOwn<GfxDescriptorSet> descriptorSet;
	};

	GfxDescriptorSetDesc m_materialDescriptorSetDesc;

	std::vector<Material> m_materials;
	Material m_defaultMaterial;
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
		std::vector<u32>   patchList;
		std::string        filename;
		GfxOwn<GfxTexture> albedoTexture;
	};

	std::unordered_map<std::string, TextureData*> m_textures;
	std::vector<TextureData*>                     m_pendingTextures;
	std::vector<TextureData*>                     m_loadedTextures;

	std::vector<std::thread> m_loadingThreads;
	bool        m_loadingThreadShouldExit = false;

	std::mutex m_loadingMutex;

	GfxOwn<GfxRayTracingPipeline>    m_rtPipeline;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;
	GfxOwn<GfxBuffer>                m_sbtBuffer;

	void loadingThreadFunction();
};
