rush_add_app(08-ModelViewer
	SOURCES
		ExampleModelViewer.h
		ExampleModelViewer.cpp
		Model.h
		Model.cpp
		Common.hlsl
		ModelPS.hlsl
		ModelVS.hlsl
	LIBS
		stb
		tiny_obj_loader
)

rush_shader_hlsl(ModelPS.hlsl ps_6_0 DEPENDS Common.hlsl)
rush_shader_hlsl(ModelVS.hlsl vs_6_0 DEPENDS Common.hlsl)
