cmake_minimum_required(VERSION 3.13.5)

project(ruc-vm)


# Put all libraries to one folder
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Set RPATH for searching dynamic libraries in the same folder
set(CMAKE_INSTALL_RPATH $ORIGIN @loader_path)
set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


if(MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(-Wno-unused-result)
endif()

# Disable asserts for Release build
add_compile_definitions("$<$<NOT:$<CONFIG:Debug>>:NDEBUG>")


# Import testing exit code
if(DEFINED TESTING_EXIT_CODE)
	add_compile_definitions(TESTING_EXIT_CODE=${TESTING_EXIT_CODE})
endif()


# Add libraries
add_subdirectory(libs)

# Add frontend
add_subdirectory(src)


function(get_all_targets _targets _dir)
	get_property(_subdirs DIRECTORY ${_dir} PROPERTY SUBDIRECTORIES)

	foreach(_subdir IN LISTS _subdirs)
		get_all_targets(${_targets} ${_subdir})
	endforeach()

	get_property(_sub_targets DIRECTORY ${_dir} PROPERTY BUILDSYSTEM_TARGETS)
	set(${_targets} ${${_targets}} ${_sub_targets} PARENT_SCOPE)
endfunction()


# Set install parameters
get_all_targets(targets .)
install(TARGETS ${targets}
		RUNTIME DESTINATION ${PROJECT_NAME}
		LIBRARY DESTINATION ${PROJECT_NAME})
