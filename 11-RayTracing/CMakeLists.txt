rush_add_app(11-RayTracing
	SOURCES
		ExampleRayTracing.h
		ExampleRayTracing.cpp
		RayQuery.hlsl
)

rush_shader_hlsl(RayQuery.hlsl cs_6_5)
