cmake_minimum_required(VERSION 3.13.5)

project(ruc-vm)


file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.c")
file(GLOB_RECURSE HDR CONFIGURE_DEPENDS "*.h")

source_group("\\" FILES ${SRC} ${HDR})
add_executable(${PROJECT_NAME} ${SRC} ${HDR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


target_link_libraries(${PROJECT_NAME} interpreter)
