set(app 04-Shaders)

add_executable(${app}
	Shaders.cpp
	Vertex.hlsl
	Pixel.hlsl
)

target_link_libraries(${app} Common)

hlsl_shader_compile_rule(Pixel.hlsl fragment)
hlsl_shader_compile_rule(Vertex.hlsl vertex)
