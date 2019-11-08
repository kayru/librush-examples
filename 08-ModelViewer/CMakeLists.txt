set(app 08-ModelViewer)

set(shaders
	ModelPS.hlsl
	ModelVS.hlsl
)

add_executable(${app}
	${shaders}
	ExampleModelViewer.h
	ExampleModelViewer.cpp
	Model.h
	Model.cpp
)

target_link_libraries(${app} Common Rush stb tiny_obj_loader)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_program(spirv-cross NAMES spirv-cross PATHS
		$ENV{VK_SDK_PATH}/Bin
		$ENV{PATH}
		"~/bin"
	)
	function(hlsl_shader_compile_rule2 shaderName stage)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
			COMMAND ${GLSLC} -x hlsl -fentry-point=main -fshader-stage=${stage} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			COMMAND ${spirv-cross} --msl --msl-version 200000 --msl-argument-buffers ${CMAKE_CFG_INTDIR}/${shaderName}.bin > ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(hlsl_shader_compile_rule2)
else()
	function(hlsl_shader_compile_rule2 shaderName stage)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
			COMMAND ${GLSLC} -x hlsl -fentry-point=main -fshader-stage=${stage} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		)
	endfunction(hlsl_shader_compile_rule2)
endif()

hlsl_shader_compile_rule2("ModelPS.hlsl" fragment)
hlsl_shader_compile_rule2("ModelVS.hlsl" vertex)
