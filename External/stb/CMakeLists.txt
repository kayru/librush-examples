add_library(stb stb_image.h stb_image_resize.h stb_impl.c)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(stb PRIVATE _CRT_SECURE_NO_WARNINGS)
