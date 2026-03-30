rush_add_app(12-PathTracer
	SOURCES
		ExamplePathTracer.h
		ExamplePathTracer.cpp
		Blit.hlsl
		BlitTonemap.hlsl
		PathTracer.rchit
		PathTracer.rgen
		PathTracer.rmiss
		PathTracer.metal
	LIBS
		stb
		tiny_obj_loader
		cgltf
)

set(shaderDependencies
	Common.glsl
	BRDF.glsl
)

if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "MTL")
	rush_shader_hlsl(Blit.hlsl vs_6_0)
	rush_shader_hlsl(BlitTonemap.hlsl ps_6_0)
	rush_shader_metal(PathTracer.metal)
else()
	rush_shader_hlsl(Blit.hlsl vs_6_0)
	rush_shader_hlsl(BlitTonemap.hlsl ps_6_0)
	rush_shader_rt(PathTracer.rchit DEPENDS ${shaderDependencies})
	rush_shader_rt(PathTracer.rgen DEPENDS ${shaderDependencies})
	rush_shader_rt(PathTracer.rmiss DEPENDS ${shaderDependencies})
endif()
