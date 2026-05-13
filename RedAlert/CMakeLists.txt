file(
	GLOB ra_srcs
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.asm
)

add_library(
	redalert
	SHARED
	${ra_srcs}
)

if(WIN32)
    target_compile_definitions(redalert PRIVATE WIN32)
else()
    target_compile_definitions(redalert PRIVATE LINUX)
endif()

target_compile_definitions(redalert PRIVATE RED_ALERT)

target_include_directories(redalert PRIVATE ${CMAKE_CURRENT_LIST_DIR})
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_options(redalert PRIVATE /W4)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_options(redalert PRIVATE -Wall -Wextra)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_compile_options(redalert PRIVATE -Wall -Wextra)
endif()

target_link_libraries(redalert PRIVATE SDL3::SDL3)
# target_include_directories(redalert PRIVATE .)
# target_link_librairies(
# 	redalert
# )
