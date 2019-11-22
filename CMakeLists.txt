cmake_minimum_required(VERSION 3.0.0)

project(ruc-vm)

find_package(Threads REQUIRED)

file(GLOB SRC "*/*.c")
file(GLOB HEADERS "*/*.h")

execute_process(COMMAND dirname ${HEADERS}
	OUTPUT_VARIABLE HEADERS)
string(REPLACE "\n" ";" HEADERS ${HEADERS})
list(REMOVE_DUPLICATES HEADERS)

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} m ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(${PROJECT_NAME} PRIVATE ${HEADERS})
