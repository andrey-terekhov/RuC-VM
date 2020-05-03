cmake_minimum_required(VERSION 3.0.0)

project(ruc-vm)


set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


file(GLOB SRC "*.c")

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} interpreter)
