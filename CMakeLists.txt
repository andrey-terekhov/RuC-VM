cmake_minimum_required(VERSION 3.0.0)

project(ruc-vm)

file(GLOB SRC "src/*.c")

add_executable(${PROJECT_NAME} ${SRC})

add_subdirectory("libs/interpreter")

target_include_directories(${PROJECT_NAME} PRIVATE "libs/interpreter")
target_link_libraries(${PROJECT_NAME} interpreter)
