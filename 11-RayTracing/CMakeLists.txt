set(app 11-RayTracing)

set(shaders
	RayQuery.hlsl
)

add_executable(${app}
	${shaders}
	ExampleRayTracing.h
	ExampleRayTracing.cpp
)

target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)

foreach(shader ${shaders})
	hlsl_shader_compile_rule(${shader} cs_6_5)
endforeach()
