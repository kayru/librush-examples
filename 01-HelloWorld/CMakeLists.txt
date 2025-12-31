set(app 01-HelloWorld)
add_executable(${app} HelloWorld.cpp)
target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)
