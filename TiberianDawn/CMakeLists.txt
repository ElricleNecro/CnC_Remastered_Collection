file(
	GLOB td_srcs
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.asm
	${CMAKE_CURRENT_SOURCE_DIR}/win32lib/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/win32lib/*.asm
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	file(
		GLOB td_system_srcs
		${CMAKE_CURRENT_SOURCE_DIR}/win32lib/*.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/win32lib/*.asm
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	file(
		GLOB td_system_srcs
		${CMAKE_CURRENT_SOURCE_DIR}/linuxlib/*.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/linuxlib/*.asm
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
endif()

add_library(
	tiberiandawn
	SHARED
	${td_srcs}
	${td_system_srcs}
)
target_include_directories(tiberiandawn PRIVATE ${CMAKE_CURRENT_LIST_DIR})

if(WIN32)
    target_compile_definitions(tiberiandawn PRIVATE WIN32)
else()
    target_compile_definitions(tiberiandawn PRIVATE LINUX)
endif()

target_compile_definitions(tiberiandawn PRIVATE TIBERIAN_DAWN)

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
