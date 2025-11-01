cmake_minimum_required(VERSION 3.24)
project(
	pico
	VERSION 0.1.0
	LANGUAGES C
)

option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors." ON)
option(ENABLE_SANITIZERS "Enable asan sanitizers" ON)

include("cmake/base.cmake")
include("cmake/warnings.cmake")

add_executable(${PROJECT_NAME})

set_default_warnings(${PROJECT_NAME})
enable_tools(${PROJECT_NAME})

# Enable code sanitizers
if(ASAN_SANITIZER_FLAGS)
	target_compile_options(${PROJECT_NAME} PRIVATE ${ASAN_SANITIZER_FLAGS})
endif()

target_compile_features(
	${PROJECT_NAME}
	PRIVATE
		c_std_23
)

target_sources(
	${PROJECT_NAME}
	PRIVATE
		"src/main.c"
)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		"${CMAKE_SOURCE_DIR}/src"
)
