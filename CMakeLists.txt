cmake_minimum_required(VERSION 3.25)

project(
	TombLoot
	VERSION 0.1.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "Building inside the source folder is not allowed.")
endif()

# ---------------------------------------------------------------------------
# Optional deployment to an MO2 mod folder after each build. Off by default.
# ---------------------------------------------------------------------------
set(TOMBLOOT_DEPLOY_DIR "G:/Skyrim/Instance MO2/mods/TombLoot_dev" CACHE PATH
	"Root of the MO2 development mod (the DLL goes to SKSE/Plugins)")
option(TOMBLOOT_DEPLOY "Copy the DLL to the MO2 mod after building" OFF)

# ---------------------------------------------------------------------------
# CommonLibSSE-NG (maintained fork alandtse/CommonLibVR, ng branch), same pinned version as
# Every Crumb Counts. SE + AE only: no VR.
# ---------------------------------------------------------------------------
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(COMMONLIB_ENABLE_IPO OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
	CommonLibSSE
	GIT_REPOSITORY https://github.com/alandtse/CommonLibVR.git
	GIT_TAG bf27825961c932c940304317156c412cda8300a7 # v13.0.0
	GIT_SUBMODULES ""
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(CommonLibSSE)
include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

find_package(nlohmann_json CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
set(SOURCES
	src/PCH.h
	src/main.cpp
	src/Forms.h
	src/Forms.cpp
	src/Survey.h
	src/Survey.cpp
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

add_commonlibsse_plugin(
	${PROJECT_NAME}
	AUTHOR "Narziels"
	USE_ADDRESS_LIBRARY
	SOURCES ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8 /Zi /permissive-)
	target_link_options(${PROJECT_NAME} PRIVATE /DEBUG:FULL /OPT:REF /OPT:ICF)
endif()

if(TOMBLOOT_DEPLOY)
	set(_plugins_dir "${TOMBLOOT_DEPLOY_DIR}/SKSE/Plugins")
	add_custom_command(
		TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${_plugins_dir}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${_plugins_dir}/"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${_plugins_dir}/"
		COMMENT "Deploying ${PROJECT_NAME}.dll to ${_plugins_dir}"
		VERBATIM
	)
endif()
