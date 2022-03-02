cmake_minimum_required(VERSION 3.13.5)


# Add interpreter library
add_subdirectory(interpreter)

# Add windows pthread
if(MSVC)
	add_subdirectory(pthread)
endif()
