cmake_minimum_required(VERSION 3.13.5)

project(interpreter)


file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.c")
file(GLOB_RECURSE HDR CONFIGURE_DEPENDS "*.h")

source_group("\\" FILES ${SRC} ${HDR})
add_library(${PROJECT_NAME} SHARED ${SRC} ${HDR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


if(MSVC)
	target_link_libraries(${PROJECT_NAME} pthread)
	target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_STRUCT_TIMESPEC)
else()
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} m ${CMAKE_THREAD_LIBS_INIT})
endif()
