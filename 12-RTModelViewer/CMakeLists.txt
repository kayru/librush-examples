set(app 12-RTModelViewer)

set(shaders

)

add_executable(${app}
	${shaders}
	ExampleRTModelViewer.h
	ExampleRTModelViewer.cpp
)

target_link_libraries(${app} Common Rush stb tiny_obj_loader)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)
