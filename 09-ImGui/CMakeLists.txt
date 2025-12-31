set(app 09-ImGui)

add_executable(${app}
	ExampleImGui.cpp
)

target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)
