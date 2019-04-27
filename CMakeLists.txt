cmake_minimum_required(VERSION 3.7)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(RushExamples)

# Multithreaded compilation in MSVC
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# Dependencies

add_subdirectory("External")

# Custom build rules

find_program(GLSLC NAMES glslc PATHS
	$ENV{VULKAN_SDK}/Bin
	$ENV{VK_SDK_PATH}/Bin
	$ENV{PATH}
	"~/bin"
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_program(spirv-cross NAMES spirv-cross PATHS
		$ENV{VK_SDK_PATH}/Bin
		$ENV{PATH}
		"~/bin"
	)
	function(hlsl_shader_compile_rule shaderName stage)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
			COMMAND ${GLSLC} -x hlsl -fentry-point=main -fshader-stage=${stage} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			COMMAND ${spirv-cross} --metal ${CMAKE_CFG_INTDIR}/${shaderName}.bin > ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(hlsl_shader_compile_rule)
	function(glsl_shader_compile_rule shaderName)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
			COMMAND ${GLSLC} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			COMMAND ${spirv-cross} --metal ${CMAKE_CFG_INTDIR}/${shaderName}.bin > ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(glsl_shader_compile_rule)
else()
	function(hlsl_shader_compile_rule shaderName stage)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
			COMMAND ${GLSLC} -x hlsl -fentry-point=main -fshader-stage=${stage} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(hlsl_shader_compile_rule)
	function(glsl_shader_compile_rule shaderName)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
			COMMAND ${GLSLC} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(glsl_shader_compile_rule)
endif()

# Example apps

add_subdirectory("Common")

add_subdirectory("01-HelloWorld")
add_subdirectory("02-WindowEvents")
add_subdirectory("03-Primitives")
add_subdirectory("04-Shaders")
add_subdirectory("05-Compute")
add_subdirectory("06-Instancing")
add_subdirectory("07-VSyncTest")
add_subdirectory("08-ModelViewer")
add_subdirectory("09-ImGui")
add_subdirectory("10-MSAA")
