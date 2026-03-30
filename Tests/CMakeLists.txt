rush_add_app(Tests
	SOURCES
		TestFramework.h
		TestFramework.cpp
		TestMain.cpp
		TestBuffer.cpp
		TestClear.cpp
		TestCopyTextureToBuffer.cpp
		TestArray.cpp
		TestRayTracing.cpp
		TestRayTracing.hlsl
		TestRayTracing.metal
		TestRayTracingPipeline.cpp
		TestRayTracingPipeline.rgen
		TestRayTracingPipeline.rmiss
		TestRayTracingPipeline.rchit
		TestRayTracingPipeline.metal
		TestRayTracingMaterialIndex.cpp
		TestRayTracingMaterialIndex.metal
		TestRayTracingMaterialBinding.cpp
		TestRayTracingMaterialBinding.metal
		TestTypedBuffer.cpp
		TestTypedBuffer.hlsl
		TestIndexBufferOffset.cpp
		TestIndexBufferOffsetVS.hlsl
		TestIndexBufferOffsetPS.hlsl
)

if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "MTL")
	rush_shader_metal(TestRayTracing.metal)
	rush_shader_metal(TestRayTracingPipeline.metal)
	rush_shader_metal(TestRayTracingMaterialIndex.metal)
	rush_shader_metal(TestRayTracingMaterialBinding.metal)
else()
	rush_shader_hlsl(TestRayTracing.hlsl cs_6_0)
	rush_shader_rt(TestRayTracingPipeline.rgen)
	rush_shader_rt(TestRayTracingPipeline.rmiss)
	rush_shader_rt(TestRayTracingPipeline.rchit)
endif()

rush_shader_hlsl(TestTypedBuffer.hlsl cs_6_0)
rush_shader_hlsl(TestIndexBufferOffsetVS.hlsl vs_6_0)
rush_shader_hlsl(TestIndexBufferOffsetPS.hlsl ps_6_0)
