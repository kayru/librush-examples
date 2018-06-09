set(app 07-VSyncTest)
add_executable(${app} VSyncTest.cpp)
target_link_libraries(${app} Rush)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)
