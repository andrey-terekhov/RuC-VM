cmake_minimum_required(VERSION 3.0.0)

project(ruc-vm)

find_package(Threads REQUIRED)

file(GLOB SRC "src/*.c")

add_executable(${PROJECT_NAME} ${SRC})

add_subdirectory(libs)

target_include_directories(${PROJECT_NAME} PRIVATE libs)
target_link_libraries(${PROJECT_NAME} interpreter)
