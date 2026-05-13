cmake_minimum_required(VERSION 3.28)

# Disable in-source builds to prevent source tree corruption.
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
	message(FATAL_ERROR "
FATAL: In-source builds are not allowed.
       You should create a separate directory for build files.
")
endif()

project(
	CommandAndConquerRemastered
	VERSION 1.0.0
	DESCRIPTION "Command and Conquer Remastered edition."
	LANGUAGES CXX ASM
)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if (APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")    # Disable shared builds on platforms where it does not make sense to use them
    set(SDL_SHARED OFF)
else()
    set(SDL_SHARED ON)
endif()

# This assumes the SDL source is available in vendored/SDL
add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-fpermissive)
add_compile_options(-fvisibility=hidden)

add_subdirectory(RedAlert)
add_subdirectory(TiberianDawn)
