file(
	GLOB td_srcs
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.asm
	${CMAKE_CURRENT_SOURCE_DIR}/win32lib/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/win32lib/*.asm
)
add_executable(
	tiberiandawn
	${td_srcs}
)
target_include_directories(tiberiandawn PRIVATE ${CMAKE_CURRENT_LIST_DIR})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_options(tiberiandawn PRIVATE /W4)
	target_include_directories(tiberiandawn PRIVATE ${CMAKE_CURRENT_LIST_DIR}/win32lib)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_options(tiberiandawn PRIVATE -Wall -Wextra)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_compile_options(tiberiandawn PRIVATE -Wall -Wextra)
endif()

# target_include_directories(tiberiandawn PRIVATE .)
# target_link_librairies(
# 	tiberiandawn
# )
