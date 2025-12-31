add_library(stb stb_image.h stb_image_resize.h stb_image_write.h stb_impl.c)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
	target_compile_options(stb PRIVATE -W0)
else()
	target_compile_options(stb PRIVATE -w)
endif()
