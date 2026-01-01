set(app 06-Instancing)

set(shaders
	ModelPS.frag
	ModelVS.vert
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

target_link_libraries(${app} Common tiny_obj_loader)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

glsl_shader_compile_rule(ModelPS.frag)
glsl_shader_compile_rule(ModelVS.vert)

glsl_shader_compile_rule(ModelInstanced.vert)
glsl_shader_compile_rule(ModelPush.vert)
glsl_shader_compile_rule(ModelPushOffset.vert)
