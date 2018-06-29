add_library(imgui
	imgui.cpp
	imgui_demo.cpp
	imgui_internal.h
	stb_textedit.h
	imconfig.h
	imgui.h
	imgui_draw.cpp
	stb_rect_pack.h
	stb_truetype.h
)
target_include_directories(imgui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
	target_compile_options(imgui PRIVATE -W0 -wd4996)
else()
	target_compile_options(imgui PRIVATE -w)
endif()

