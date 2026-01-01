set(app 08-ModelViewer)

set(shaders
	Common.glsl
	ModelPS.frag
	ModelVS.vert
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

glsl_shader_compile_rule("ModelPS.frag" "Common.glsl")
glsl_shader_compile_rule("ModelVS.vert" "Common.glsl")
