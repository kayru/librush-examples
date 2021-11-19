set(app 12-PathTracer)

set(shaders
	Blit.vert
	BlitTonemap.frag
	PathTracer.rchit
	PathTracer.rgen
	PathTracer.rmiss
)

set(shaderDependencies
	Common.glsl
	BRDF.glsl
)

add_executable(${app}
	${shaders}
	ExamplePathTracer.h
	ExamplePathTracer.cpp
)

target_link_libraries(${app} Common Rush stb tiny_obj_loader cgltf)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

function(rt_shader_compile_rule shaderName dependencies)
	add_custom_command(
		OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
		COMMAND ${GLSLC} --target-env=vulkan1.2 -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		DEPENDS ${dependencies}
	)
endfunction(rt_shader_compile_rule)

foreach(shader ${shaders})
	rt_shader_compile_rule(${shader} "${shaderDependencies}")
endforeach()
