set(COMMON_SRC
	Utils.h
	Utils.cpp
	ExampleApp.h
	ExampleApp.cpp
	ImGuiImpl.h
	ImGuiImpl.cpp
)

add_library(Common STATIC ${COMMON_SRC})
target_include_directories(Common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(Common PUBLIC Rush imgui stb)
