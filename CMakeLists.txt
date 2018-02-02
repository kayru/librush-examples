cmake_minimum_required(VERSION 3.7)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(RushExamples)

# Dependencies

add_subdirectory("External")

# Example apps

add_subdirectory("01-HelloWorld")
add_subdirectory("02-WindowEvents")
add_subdirectory("03-Primitives")

