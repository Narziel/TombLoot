cmake_minimum_required(VERSION 3.25)

project(
	TombLoot
	VERSION 1.2.2
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
	src/Config.h
	src/Config.cpp
	src/Swap.h
	src/Swap.cpp
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
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${_plugins_dir}/TombLoot"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/config/TombLoot.json" "${_plugins_dir}/TombLoot/"
		COMMENT "Deploying ${PROJECT_NAME}.dll to ${_plugins_dir}"
		VERBATIM
	)
endif()

# ---------------------------------------------------------------------------
# Release archive for Nexus: cmake --build --preset release --target package_mod
# Produces <build>/TombLoot-<version>.zip:
#   SKSE/Plugins/TombLoot.dll
#   SKSE/Plugins/TombLoot/TombLoot.json               (commented config)
#   SKSE/Plugins/TombLoot/COPYING.txt                 (GPL-3.0)
#   SKSE/Plugins/TombLoot/THIRD_PARTY_LICENSES.txt
# ---------------------------------------------------------------------------
set(_stage "${CMAKE_BINARY_DIR}/package")
set(_zip "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}.zip")
set(_plugins "${_stage}/SKSE/Plugins")
add_custom_target(package_mod
	COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_stage}"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${_plugins}/${PROJECT_NAME}"
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${_plugins}/"
	COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/config/${PROJECT_NAME}.json" "${_plugins}/${PROJECT_NAME}/"
	COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/COPYING" "${_plugins}/${PROJECT_NAME}/COPYING.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_LICENSES.txt" "${_plugins}/${PROJECT_NAME}/"
	COMMAND "${CMAKE_COMMAND}" -E rm -f "${_zip}"
	COMMAND "${CMAKE_COMMAND}" -E chdir "${_stage}" "${CMAKE_COMMAND}" -E tar cf "${_zip}" --format=zip SKSE
	DEPENDS ${PROJECT_NAME}
	COMMENT "Packaging ${_zip}"
	VERBATIM
)

# ---------------------------------------------------------------------------
# Source archive (GPL Corresponding Source): cmake --build --preset release --target package_source
# Produces <build>/TombLoot-<version>-source.zip from the last git commit (commit first).
# ---------------------------------------------------------------------------
find_package(Git QUIET)
if(GIT_FOUND)
	set(_srczip "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}-source.zip")
	add_custom_target(package_source
		COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}" archive --format=zip
			"--prefix=${PROJECT_NAME}-${PROJECT_VERSION}-source/" -o "${_srczip}" HEAD
		COMMENT "Packaging ${_srczip}"
		VERBATIM
	)
endif()
