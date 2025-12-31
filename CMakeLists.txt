cmake_minimum_required(VERSION 3.20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(RushExamples)

set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO" CACHE STRING "Disable code signing for Xcode builds")

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
endif(APPLE)

# Multithreaded compilation in MSVC
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# Dependencies

add_subdirectory("External")

if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "VK")
	# Collect Vulkan loader and MoltenVK dylibs so they can be staged next to the built executables.
	set(_RUSH_VK_RUNTIME_CANDIDATES
		"$ENV{VULKAN_SDK}/lib/libvulkan.1.dylib"
		"$ENV{VK_SDK_PATH}/lib/libvulkan.1.dylib"
		"$ENV{VULKAN_SDK}/MoltenVK/dylib/macOS/libMoltenVK.dylib"
		"$ENV{VK_SDK_PATH}/MoltenVK/dylib/macOS/libMoltenVK.dylib"
	)

	set(RUSH_VK_RUNTIME_LIBS "")
	foreach(candidate ${_RUSH_VK_RUNTIME_CANDIDATES})
		if(candidate AND EXISTS "${candidate}")
			list(APPEND RUSH_VK_RUNTIME_LIBS "${candidate}")
		endif()
	endforeach()
	if(NOT RUSH_VK_RUNTIME_LIBS)
		message(FATAL_ERROR "Vulkan SDK libraries were not found. Set VULKAN_SDK or VK_SDK_PATH so libvulkan.1.dylib and libMoltenVK.dylib are accessible.")
	endif()

	set(_RUSH_MOLTENVK_ICD_CANDIDATES
		"$ENV{VULKAN_SDK}/share/vulkan/icd.d/MoltenVK_icd.json"
		"$ENV{VK_SDK_PATH}/share/vulkan/icd.d/MoltenVK_icd.json"
		"$ENV{VULKAN_SDK}/MoltenVK/icd/MoltenVK_icd.json"
		"$ENV{VK_SDK_PATH}/MoltenVK/icd/MoltenVK_icd.json"
	)

	set(RUSH_VK_ICD_JSON "")
	foreach(candidate ${_RUSH_MOLTENVK_ICD_CANDIDATES})
		if(candidate AND EXISTS "${candidate}")
			set(RUSH_VK_ICD_JSON "${candidate}")
			break()
		endif()
	endforeach()
	if(NOT RUSH_VK_ICD_JSON)
		message(FATAL_ERROR "MoltenVK_icd.json was not found. Ensure VULKAN_SDK or VK_SDK_PATH points to a valid Vulkan SDK with MoltenVK.")
	endif()

	if(RUSH_VK_ICD_JSON)
		set(RUSH_VK_ICD_RUNTIME "${CMAKE_BINARY_DIR}/MoltenVK_icd_runtime.json")
		file(READ "${RUSH_VK_ICD_JSON}" RUSH_VK_ICD_CONTENT)
		if(NOT RUSH_VK_ICD_CONTENT OR NOT RUSH_VK_ICD_CONTENT MATCHES "\"library_path\"")
			message(FATAL_ERROR "MoltenVK_icd.json is invalid or missing the library_path field: ${RUSH_VK_ICD_JSON}")
		endif()
		string(REGEX REPLACE "\"library_path\"[ \t]*:[ \t]*\"[^\"]+\"" "\"library_path\": \"libMoltenVK.dylib\"" RUSH_VK_ICD_CONTENT "${RUSH_VK_ICD_CONTENT}")
		file(WRITE "${RUSH_VK_ICD_RUNTIME}" "${RUSH_VK_ICD_CONTENT}")
	endif()
endif()

# Custom build rules

find_program(GLSLC NAMES glslc PATHS
	$ENV{VULKAN_SDK}/Bin
	$ENV{VK_SDK_PATH}/Bin
	$ENV{PATH}
	"~/bin"
)

find_program(glslangValidator NAMES glslangValidator PATHS
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
			COMMAND ${GLSLC} -O -x hlsl -fentry-point=main -fshader-stage=${stage} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			COMMAND ${spirv-cross} --msl --msl-version 200000 --msl-argument-buffers --msl-decoration-binding ${extra_args} ${CMAKE_CFG_INTDIR}/${shaderName}.bin > ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(hlsl_shader_compile_rule)
	function(glsl_shader_compile_rule shaderName)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
			COMMAND ${GLSLC} -O -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			COMMAND ${spirv-cross} --msl --msl-version 200000 --msl-argument-buffers --msl-decoration-binding ${spirv-cross-args} ${CMAKE_CFG_INTDIR}/${shaderName}.bin > ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(glsl_shader_compile_rule)
else()
	function(hlsl_shader_compile_rule shaderName stage)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
			COMMAND ${GLSLC} -O -x hlsl -fentry-point=main -fshader-stage=${stage} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(hlsl_shader_compile_rule)
	function(glsl_shader_compile_rule shaderName)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
			COMMAND ${GLSLC} -O -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(glsl_shader_compile_rule)
endif()

# Common code

add_subdirectory("Common")

# Example apps

set(RUSH_EXAMPLE_TARGETS
	01-HelloWorld
	02-WindowEvents
	03-Primitives
	04-Shaders
	05-Compute
	06-Instancing
	07-VSyncTest
	08-ModelViewer
	09-ImGui
	10-MSAA
	11-RayTracing
	12-PathTracer
	Tests
)

foreach(example_target IN LISTS RUSH_EXAMPLE_TARGETS)
	add_subdirectory("${example_target}")
endforeach()

if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "VK" AND RUSH_VK_RUNTIME_LIBS)
	set(RUSH_VK_RUNTIME_FILES ${RUSH_VK_RUNTIME_LIBS})
	if(RUSH_VK_ICD_RUNTIME)
		list(APPEND RUSH_VK_RUNTIME_FILES "${RUSH_VK_ICD_RUNTIME}")
	endif()

	add_custom_target(rush_copy_vulkan_runtime ALL
		COMMENT "Staging Vulkan/MoltenVK runtime libraries for example targets"
	)
	foreach(example_target IN LISTS RUSH_EXAMPLE_TARGETS)
		if(TARGET ${example_target})
			set_target_properties(${example_target} PROPERTIES BUILD_RPATH "@executable_path")
			add_dependencies(${example_target} rush_copy_vulkan_runtime)
			add_custom_command(TARGET rush_copy_vulkan_runtime POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${example_target}>"
				COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUSH_VK_RUNTIME_FILES} "$<TARGET_FILE_DIR:${example_target}>"
				COMMENT "Copying Vulkan/MoltenVK runtime libraries to $<TARGET_FILE_DIR:${example_target}>"
				VERBATIM
			)
			if(CMAKE_GENERATOR MATCHES "Xcode" AND RUSH_VK_ICD_RUNTIME)
				# Ensure Xcode launches set the ICD path so the loader finds MoltenVK.
				set_property(TARGET ${example_target} PROPERTY XCODE_SCHEME_ENVIRONMENT
					"VK_ICD_FILENAMES=$<TARGET_FILE_DIR:${example_target}>/MoltenVK_icd_runtime.json")
			endif()
		endif()
	endforeach()
endif()
