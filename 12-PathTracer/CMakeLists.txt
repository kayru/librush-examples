set(app 12-PathTracer)

set(shaders
	Blit.vert
	BlitTonemap.frag
	PathTracer.rchit
	PathTracer.rgen
	PathTracer.rmiss
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

foreach(shader ${shaders})
	rt_shader_compile_rule(${shader} "${shaderDependencies}")
endforeach()
