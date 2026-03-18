add_library(imgui
	imgui.cpp
	imgui_demo.cpp
	imgui_draw.cpp
	imgui_tables.cpp
	imgui_widgets.cpp
	imgui_internal.h
	imstb_textedit.h
	imconfig.h
	imgui.h
	imstb_rectpack.h
	imstb_truetype.h
)
target_include_directories(imgui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
	target_compile_options(imgui PRIVATE -W0 -wd4996)
else()
	target_compile_options(imgui PRIVATE -w)
endif()
