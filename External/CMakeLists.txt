add_subdirectory("librush")
add_subdirectory("stb")
add_subdirectory("tiny_obj_loader")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
add_library(imgui
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui INTERFACE ${IMGUI_DIR})
target_include_directories(imgui PRIVATE ${IMGUI_DIR})
set_property(TARGET imgui PROPERTY CXX_STANDARD 20)
set_property(TARGET imgui PROPERTY CXX_STANDARD_REQUIRED ON)
if (MSVC)
	target_compile_options(imgui PRIVATE -W0 -wd4996)
else()
	target_compile_options(imgui PRIVATE -w)
endif()
add_subdirectory("cgltf") # 89904c0dfb16246abe05be2c572466a2ba317f47
