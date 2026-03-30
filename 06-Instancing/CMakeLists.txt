rush_add_app(06-Instancing
	SOURCES
		Instancing.cpp
		ModelPS.hlsl
		ModelVS.hlsl
		ModelInstanced.hlsl
		ModelPush.hlsl
		ModelPushOffset.hlsl
	LIBS
		tiny_obj_loader
)

rush_shader_hlsl(ModelPS.hlsl ps_6_0)
rush_shader_hlsl(ModelVS.hlsl vs_6_0)
rush_shader_hlsl(ModelInstanced.hlsl vs_6_0)
rush_shader_hlsl(ModelPush.hlsl vs_6_0)
rush_shader_hlsl(ModelPushOffset.hlsl vs_6_0)
