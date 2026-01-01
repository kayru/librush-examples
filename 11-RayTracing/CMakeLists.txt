set(app 11-RayTracing)

set(shaders
	Primary.rgen
	Primary.rmiss
	Primary.rchit
)

set(shaderDependencies
	Common.glsl
)

add_executable(${app}
	${shaders}
	ExampleRayTracing.h
	ExampleRayTracing.cpp
)

target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

foreach(shader ${shaders})
	rt_shader_compile_rule(${shader} "${shaderDependencies}")
endforeach()
