set(app 06-Instancing)

set(shaders
	ModelPS.hlsl
	ModelVS.hlsl
)

set(shaders ${shaders}
	ModelInstanced.hlsl
	ModelPush.hlsl
	ModelPushOffset.hlsl
)

add_executable(${app}
	${shaders}
	Instancing.cpp
)

target_link_libraries(${app} Common tiny_obj_loader)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

hlsl_shader_compile_rule(ModelPS.hlsl ps_6_0)
hlsl_shader_compile_rule(ModelVS.hlsl vs_6_0)

hlsl_shader_compile_rule(ModelInstanced.hlsl vs_6_0)
hlsl_shader_compile_rule(ModelPush.hlsl vs_6_0)
hlsl_shader_compile_rule(ModelPushOffset.hlsl vs_6_0)
