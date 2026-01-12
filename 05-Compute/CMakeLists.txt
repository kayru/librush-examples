set(app 05-Compute)

add_executable(${app}
	Compute.cpp
	ComputeShader.hlsl
)

target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

hlsl_shader_compile_rule(ComputeShader.hlsl cs_6_0)
