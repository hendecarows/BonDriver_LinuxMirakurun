cmake_minimum_required(VERSION 3.16)

# Project name
project(BonDriver_LinuxMirakurun LANGUAGES CXX)

# System Information (for debug)
message(STATUS "System Information")
message(STATUS "System = ${CMAKE_SYSTEM_NAME}")
message(STATUS "C Compiler = ${CMAKE_C_COMPILER_ID}")
message(STATUS "C++ Compiler = ${CMAKE_CXX_COMPILER_ID}")

# Build Type Configuration
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Options
option(READ_COUNT "Output Read Count" OFF)
message(STATUS "READ_COUNT is set to: ${READ_COUNT}")

# Package
include(FetchContent)

FetchContent_Declare(
	plog
	GIT_REPOSITORY https://github.com/SergiusTheBest/plog
	GIT_TAG        1.1.11
	GIT_SHALLOW    true
)
FetchContent_MakeAvailable(plog)

FetchContent_Declare(
	cpr
	GIT_REPOSITORY https://github.com/libcpr/cpr.git
	GIT_TAG        1.14.2
)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(
	json
	GIT_REPOSITORY https://github.com/nlohmann/json
	GIT_TAG        v3.12.0
)
FetchContent_MakeAvailable(json)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


add_library(
	${PROJECT_NAME}
	SHARED
	src/BonDriver_LinuxMirakurun.cpp
)

target_compile_features(
	${PROJECT_NAME}
	PRIVATE
	cxx_std_20
)

set_target_properties(
	${PROJECT_NAME}
	PROPERTIES
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	PREFIX ""
)

target_compile_options(
	${PROJECT_NAME}
	PRIVATE
	-Wall -Wextra -Wpedantic
)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	include
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
	Threads::Threads
	plog::plog
	cpr::cpr
	nlohmann_json::nlohmann_json
)

if(READ_COUNT)
	target_compile_definitions(
		${PROJECT_NAME}
		PRIVATE
		READ_COUNT
	)
endif()
