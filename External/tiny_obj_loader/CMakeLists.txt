add_library(tiny_obj_loader tiny_obj_loader.h tiny_obj_loader.cc)
target_include_directories(tiny_obj_loader INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tiny_obj_loader PRIVATE "-w")