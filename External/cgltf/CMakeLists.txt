add_library(cgltf cgltf.h cgltf_write.h cgltf_impl.c)
target_include_directories(cgltf INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
	target_compile_options(cgltf PRIVATE -W0)
else()
	target_compile_options(cgltf PRIVATE -w)
endif()
