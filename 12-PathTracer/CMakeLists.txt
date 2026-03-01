set(app 12-PathTracer)

set(shaders
	Blit.hlsl
	BlitTonemap.hlsl
	PathTracer.rchit
	PathTracer.rgen
	PathTracer.rmiss
	PathTracer.metal
)

set(shaderDependencies
	Common.glsl
	BRDF.glsl
)

add_executable(${app}
	${shaders}
	ExamplePathTracer.h
	ExamplePathTracer.cpp
)

target_link_libraries(${app} Common stb tiny_obj_loader cgltf)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "MTL")
	hlsl_shader_compile_rule(Blit.hlsl vs_6_0)
	hlsl_shader_compile_rule(BlitTonemap.hlsl ps_6_0)
	metal_shader_compile_rule(PathTracer.metal)
else()
	hlsl_shader_compile_rule(Blit.hlsl vs_6_0)
	hlsl_shader_compile_rule(BlitTonemap.hlsl ps_6_0)
	rt_shader_compile_rule(PathTracer.rchit "${shaderDependencies}")
	rt_shader_compile_rule(PathTracer.rgen "${shaderDependencies}")
	rt_shader_compile_rule(PathTracer.rmiss "${shaderDependencies}")
endif()
