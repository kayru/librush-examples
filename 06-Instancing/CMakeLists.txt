set(app 06-Instancing)

set(shaders
	ModelPS.hlsl
	ModelVS.hlsl
)

set(shaders ${shaders}
	ModelInstanced.vert
	ModelPush.vert
	ModelPushOffset.vert
)

add_executable(${app}
	${shaders}
	Instancing.cpp
)

target_link_libraries(${app} Common)

hlsl_shader_compile_rule(ModelPS.hlsl fragment)
hlsl_shader_compile_rule(ModelVS.hlsl vertex)

glsl_shader_compile_rule(ModelInstanced.vert)
glsl_shader_compile_rule(ModelPush.vert)
glsl_shader_compile_rule(ModelPushOffset.vert)

