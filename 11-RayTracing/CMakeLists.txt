set(app 11-RayTracing)

set(shaders
	Primary.rgen
	Primary.rmiss
)

set(shaderDependencies
	Common.glsl
)

add_executable(${app}
	${shaders}
	ExampleRayTracing.h
	ExampleRayTracing.cpp
)

target_link_libraries(${app} Common Rush stb tiny_obj_loader)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

function(rt_shader_compile_rule shaderName dependencies)
	add_custom_command(
		OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
		COMMAND ${GLSLC} -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		DEPENDS ${dependencies}
	)
endfunction(rt_shader_compile_rule)

foreach(shader ${shaders})
	rt_shader_compile_rule(${shader} "${shaderDependencies}")
endforeach()
