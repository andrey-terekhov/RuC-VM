cmake_minimum_required(VERSION 3.0.0)

project(interpreter)


file(GLOB SRC "*.c")
add_library(${PROJECT_NAME} SHARED ${SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


if(MSVC)
	target_link_libraries(${PROJECT_NAME} pthread)
	target_compile_options(${PROJECT_NAME} PRIVATE -DHAVE_STRUCT_TIMESPEC)
else()
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} m ${CMAKE_THREAD_LIBS_INIT})
endif()

if(DEFINED TESTING_EXIT_CODE)
	add_compile_definitions(TESTING_EXIT_CODE=${TESTING_EXIT_CODE})
endif()
