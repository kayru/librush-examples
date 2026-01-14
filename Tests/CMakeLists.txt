set(app Tests)

set(TEST_SOURCES
	TestFramework.h
	TestFramework.cpp
	TestMain.cpp
	TestBuffer.cpp
	TestClear.cpp
	TestArray.cpp
	TestRayTracing.cpp
	TestRayTracing.comp
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
	glsl_shader_compile_rule(TestRayTracing.comp)
	rt_shader_compile_rule(TestRayTracingPipeline.rgen "")
	rt_shader_compile_rule(TestRayTracingPipeline.rmiss "")
	rt_shader_compile_rule(TestRayTracingPipeline.rchit "")
endif()
