rush_add_app(04-Shaders
	SOURCES
		Shaders.cpp
		Vertex.hlsl
		Pixel.hlsl
)

rush_shader_hlsl(Pixel.hlsl ps_6_0)
rush_shader_hlsl(Vertex.hlsl vs_6_0)
