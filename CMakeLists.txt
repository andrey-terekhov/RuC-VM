cmake_minimum_required(VERSION 3.0.0)

project(ruc-vm)


# Put all libraries to one folder
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(-Wno-unused-result)
endif()


# Add libraries
add_subdirectory(libs)

# Add frontend
add_subdirectory(src)
