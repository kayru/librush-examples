set(app Tests)

set(TEST_SOURCES
	TestFramework.h
	TestFramework.cpp
	TestMain.cpp
	TestBuffer.cpp
	TestClear.cpp
	TestArray.cpp
)

add_executable(${app} ${TEST_SOURCES})
target_link_libraries(${app} Common)
target_compile_definitions(${app} PRIVATE RUSH_USING_NAMESPACE)
