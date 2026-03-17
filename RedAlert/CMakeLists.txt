file(
	GLOB ra_srcs
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.asm
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	file(
		GLOB ra_system_srcs
		${CMAKE_CURRENT_SOURCE_DIR}/win32lib/*.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/win32lib/*.asm
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	file(
		GLOB ra_system_srcs
		${CMAKE_CURRENT_SOURCE_DIR}/linuxlib/*.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/linuxlib/*.asm
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
endif()

add_executable(
	redalert
	${ra_srcs}
	${ra_system_srcs}
)
target_include_directories(redalert PRIVATE ${CMAKE_CURRENT_LIST_DIR})
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_options(redalert PRIVATE /W4)
	target_include_directories(redalert PRIVATE ${CMAKE_CURRENT_LIST_DIR}/win32lib)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_options(redalert PRIVATE -Wall -Wextra)
	target_include_directories(redalert PRIVATE ${CMAKE_CURRENT_LIST_DIR}/linuxlib)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_compile_options(redalert PRIVATE -Wall -Wextra)
endif()

# target_include_directories(redalert PRIVATE .)
# target_link_librairies(
# 	redalert
# )
