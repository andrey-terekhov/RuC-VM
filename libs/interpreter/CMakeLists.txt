cmake_minimum_required(VERSION 3.0.0)

project(interpreter)


if(MSVC)
	add_library(pthread STATIC IMPORTED)

	if(CMAKE_CL_64)
		set_property(TARGET pthread PROPERTY
			IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../pthread/x64/pthreadVC2.lib")
	else()
		set_property(TARGET pthread PROPERTY
			IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../pthread/x86/pthreadVC2.lib")
	endif()

	include_directories("../pthread")
else()
	find_package(Threads REQUIRED)
endif()


file(GLOB SRC "*.c")
add_library(${PROJECT_NAME} SHARED ${SRC})
target_include_directories(${PROJECT_NAME} PRIVATE ".")


if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE -DHAVE_STRUCT_TIMESPEC)
	target_link_libraries(${PROJECT_NAME} pthread)
else()
	target_link_libraries(${PROJECT_NAME} m ${CMAKE_THREAD_LIBS_INIT})
endif()
