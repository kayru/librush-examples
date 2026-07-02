rush_add_app(12-PathTracer
	SOURCES
		ExamplePathTracer.h
		ExamplePathTracer.cpp
		Blit.hlsl
		BlitTonemap.hlsl
		PathTracer.rchit
		PathTracer.rgen
		PathTracer.rmiss
		PathTracer.comp
		PathTracer.metal
		PathTracerContext.glsl
		PathTracerCore.glsl
	LIBS
		stb
		tiny_obj_loader
		cgltf
)

set(shaderDependencies
	Common.glsl
	ShaderShared.glsl
	PathTracerConstants.glsl
	PathTracerContext.glsl
	PathTracerCore.glsl
)

if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "MTL")
	rush_shader_hlsl(Blit.hlsl vs_6_0)
	rush_shader_hlsl(BlitTonemap.hlsl ps_6_0)
	rush_shader_metal(PathTracer.metal DEPENDS ${shaderDependencies})
else()
	rush_shader_hlsl(Blit.hlsl vs_6_0)
	rush_shader_hlsl(BlitTonemap.hlsl ps_6_0)
	rush_shader_glsl(PathTracer.rchit DEPENDS ${shaderDependencies})
	rush_shader_glsl(PathTracer.rgen DEPENDS ${shaderDependencies})
	rush_shader_glsl(PathTracer.rmiss DEPENDS ${shaderDependencies})
	rush_shader_glsl(PathTracer.comp DEPENDS ${shaderDependencies})
endif()
