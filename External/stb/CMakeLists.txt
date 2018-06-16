add_library(stb stb_image.h stb_image_resize.h stb_impl.c)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(stb PRIVATE "-w")