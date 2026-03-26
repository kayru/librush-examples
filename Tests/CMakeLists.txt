set(app Tests)

set(TEST_SOURCES
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

add_executable(${app} ${TEST_SOURCES})
target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "MTL")
	metal_shader_compile_rule(TestRayTracing.metal)
	metal_shader_compile_rule(TestRayTracingPipeline.metal)
	metal_shader_compile_rule(TestRayTracingMaterialIndex.metal)
	metal_shader_compile_rule(TestRayTracingMaterialBinding.metal)
else()
	hlsl_shader_compile_rule(TestRayTracing.hlsl cs_6_0)
	rt_shader_compile_rule(TestRayTracingPipeline.rgen "")
	rt_shader_compile_rule(TestRayTracingPipeline.rmiss "")
	rt_shader_compile_rule(TestRayTracingPipeline.rchit "")
endif()

hlsl_shader_compile_rule(TestTypedBuffer.hlsl cs_6_0)
hlsl_shader_compile_rule(TestIndexBufferOffsetVS.hlsl vs_6_0)
hlsl_shader_compile_rule(TestIndexBufferOffsetPS.hlsl ps_6_0)
