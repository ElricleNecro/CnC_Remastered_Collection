cmake_minimum_required(VERSION 3.5)

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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(RedAlert)
add_subdirectory(TiberianDawn)
