#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/GfxRef.h>
#include <Rush/MathTypes.h>
#include <Rush/Platform.h>
#include <Rush/UtilCamera.h>
#include <Rush/UtilCameraManipulator.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <Common/ExampleApp.h>
#include <Common/Utils.h>

#include <stdio.h>
#include <memory>
#include <string>
#include <unordered_map>

class ExampleModelViewer : public ExampleApp
{
public:

	ExampleModelViewer();
	~ExampleModelViewer();

	void update() override;

private:

	void render();

	bool loadModel(const char* filename);
	bool loadModelObj(const char* filename);
	bool loadModelNative(const char* filename);

	GfxRef<GfxTexture> loadTexture(const std::string& filename);

	Timer m_timer;

	struct Stats
	{
		MovingAverage<double, 60> gpuTotal;
		MovingAverage<double, 60> cpuTotal;
		MovingAverage<double, 60> cpuUI;
		MovingAverage<double, 60> cpuModel;
		MovingAverage<double, 60> cpuUpdateConstantBuffer;
	} m_stats;

	Camera m_camera;
	Camera m_interpolatedCamera;

	CameraManipulator* m_cameraMan;

	GfxVertexShader m_vs;
	GfxPixelShader m_ps;
	GfxVertexFormat m_vf;
	GfxTechnique m_technique;
	GfxTexture m_defaultWhiteTexture;

	GfxBuffer m_vertexBuffer;
	GfxBuffer m_indexBuffer;
	GfxBuffer m_constantBuffer;
	u32 m_indexCount = 0;
	u32 m_vertexCount = 0;

	struct Constants
	{
		Mat4 matViewProj = Mat4::identity();
		Mat4 matWorld = Mat4::identity();
		Vec4 baseColor = Vec4(1.0f);
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

	std::unordered_map<std::string, GfxRef<GfxTexture>> m_textures;
	std::unordered_map<u64, GfxRef<GfxBuffer>> m_materialConstantBuffers;

	struct MaterialConstants
	{
		Vec4 baseColor;
	};

	struct Material
	{
		GfxTextureRef albedoTexture;
		GfxBufferRef constantBuffer;
	};

	std::vector<Material> m_materials;
	Material m_defaultMaterial;

	struct MeshSegment
	{
		u32 material = 0;
		u32 indexOffset = 0;
		u32 indexCount = 0;
	};

	std::vector<MeshSegment> m_segments;

	WindowEventListener m_windowEvents;

	float m_cameraScale = 1.0f;
};