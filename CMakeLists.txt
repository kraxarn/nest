cmake_minimum_required(VERSION 4.0)
project(nest LANGUAGES C VERSION 1.1.0)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_C_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
	message(FATAL_ERROR "Only little-endian systems are supported")
endif ()

add_executable(${PROJECT_NAME})

add_subdirectory(src)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	NEST_VERSION=${PROJECT_VERSION_MAJOR}
)

include(deps/tomlc17.cmake)
include(deps/jansson.cmake)