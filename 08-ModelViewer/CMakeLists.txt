set(app 08-ModelViewer)

set(shaders
	Common.hlsl
	ModelPS.hlsl
	ModelVS.hlsl
)

add_executable(${app}
	${shaders}
	ExampleModelViewer.h
	ExampleModelViewer.cpp
	Model.h
	Model.cpp
)

target_link_libraries(${app} Common stb tiny_obj_loader)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

hlsl_shader_compile_rule("ModelPS.hlsl" ps_6_0 "Common.hlsl")
hlsl_shader_compile_rule("ModelVS.hlsl" vs_6_0 "Common.hlsl")
