set(app 02-WindowEvents)
add_executable(${app} WindowEvents.cpp)
target_link_libraries(${app} Rush)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)
