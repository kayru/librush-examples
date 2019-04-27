set(app 10-MSAA)

add_executable(${app}
	ExampleMSAA.cpp
)

target_link_libraries(${app} Common Rush)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)
