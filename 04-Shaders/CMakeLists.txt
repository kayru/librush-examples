set(app 04-Shaders)

add_executable(${app}
	Shaders.cpp
	Vertex.vert
	Pixel.frag
)

target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

glsl_shader_compile_rule(Pixel.frag)
glsl_shader_compile_rule(Vertex.vert)
