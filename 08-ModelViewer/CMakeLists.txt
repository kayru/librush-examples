set(app 08-ModelViewer)

set(shaders
	ModelPS.hlsl
	ModelVS.hlsl
)

add_executable(${app}
	${shaders}
	ExampleModelViewer.h
	ExampleModelViewer.cpp
)

target_link_libraries(${app} Common Rush stb tiny_obj_loader)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

hlsl_shader_compile_rule("ModelPS.hlsl" fragment)
hlsl_shader_compile_rule("ModelVS.hlsl" vertex)
