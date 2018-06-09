set(app 03-Primitives)
add_executable(${app} Primitives.cpp)
target_link_libraries(${app} Rush)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)
