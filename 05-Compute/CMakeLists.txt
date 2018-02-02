set(app 05-Compute)

add_executable(${app}
	Compute.cpp
	ComputeShader.hlsl
)

target_link_libraries(${app} Common)

hlsl_shader_compile_rule(ComputeShader.hlsl compute)
