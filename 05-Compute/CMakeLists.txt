set(app 05-Compute)

add_executable(${app}
	Compute.cpp
	ComputeShader.comp
)

target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

glsl_shader_compile_rule(ComputeShader.comp)
