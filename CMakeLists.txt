cmake_minimum_required(VERSION 3.19)

# Project Options
option(IPT_VAMP_PLUGIN_ENABLE_DEBUG "Enables the debug scheme using the Partiels application" ON)
option(IPT_VAMP_PLUGIN_REFRESH_MODEL "Force re-download and re-export of the IPT model during CMake configure" OFF)

set(IPT_VAMP_PLUGIN_TESTER_ENABLE ON CACHE STRING "Enables the tests with the vamp plugin tester")
set(IPT_VAMP_PLUGIN_TESTER_TIMEOUT 600 CACHE STRING "The timeout in seconds of the test with the vamp plugin tester (zero for no timeout)")

### Sccache ###
set(IPT_VAMP_PLUGIN_USE_SCCACHE OFF CACHE BOOL "Uses sccache as the compiler launcher")
if(IPT_VAMP_PLUGIN_USE_SCCACHE)
  if(NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(IPT_VAMP_PLUGIN_SCCACHE_PROGRAM sccache)
    if(IPT_VAMP_PLUGIN_SCCACHE_PROGRAM)
      set(CMAKE_C_COMPILER_LAUNCHER "${IPT_VAMP_PLUGIN_SCCACHE_PROGRAM}" CACHE STRING "C compiler launcher" FORCE)
      set(CMAKE_CXX_COMPILER_LAUNCHER "${IPT_VAMP_PLUGIN_SCCACHE_PROGRAM}" CACHE STRING "C++ compiler launcher" FORCE)
      set(IPT_VAMP_PLUGIN_SCCACHE_LAUNCHER_MANAGED ON CACHE INTERNAL "Whether the Ipt Vamp Plugin enabled the compiler launcher with sccache")
      message(VERBOSE "sccache found, compiler launcher enabled")
    else()
      message(VERBOSE "sccache not found, continuing without compiler cache")
    endif()
  endif()
elseif(IPT_VAMP_PLUGIN_SCCACHE_LAUNCHER_MANAGED)
  unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
  unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
  unset(IPT_VAMP_PLUGIN_SCCACHE_LAUNCHER_MANAGED CACHE)
  message(VERBOSE "sccache compiler launcher disabled")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Default value for MSVC_RUNTIME_LIBRARY of targets" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "Default value for POSITION_INDEPENDENT_CODE of targets" FORCE)

### Project ###
project(IptVampPlugin VERSION 2.1.0 LANGUAGES C CXX)

### Include Vamp (IVE) ###
set(IVE_BUILD_HOST_LIBRARY OFF)
set(IVE_BUILD_PLUGIN_LIBRARY ON)
add_subdirectory(ircam-vamp-extension EXCLUDE_FROM_ALL)

### Include LibTorch ###
include(resources/libtorch.cmake)

### Models
execute_process(COMMAND ${CMAKE_COMMAND} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/resources/download_model.cmake)

### Version ###
execute_process(COMMAND git log -1 --format=%h WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_COMMIT_ID OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_HAS_DIFF OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT GIT_HAS_DIFF STREQUAL "")
  set(GIT_COMMIT_ID "${GIT_COMMIT_ID}-head")
endif()

if(NOT IPT_VAMP_PLUGIN_BUILD_TAG OR IPT_VAMP_PLUGIN_BUILD_TAG STREQUAL "")
  set(IPT_VAMP_PLUGIN_BUILD_TAG ${CMAKE_PROJECT_VERSION})
endif()
message(STATUS "Ipt Vamp Plugin: " ${IPT_VAMP_PLUGIN_BUILD_TAG} " (" ${GIT_COMMIT_ID} ")")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version.txt ${IPT_VAMP_PLUGIN_BUILD_TAG})

### Package ###
set(VPP_NAME "IPT" CACHE PATH "The name of the project" FORCE)
set(VPP_COMPANY "Ircam" CACHE PATH "The name of the company" FORCE)
set(VPP_URL "www.ircam.fr" CACHE PATH "The URL of the project")
set(VPP_APPID "5aca3044-a39f-476d-b16c-ea033dbd1c3e" CACHE PATH "The unique ID of the project")
set(VPP_BUILD_TAG "${IPT_VAMP_PLUGIN_BUILD_TAG}" CACHE STRING "The current tag of the project" FORCE)
set(VPP_ABOUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/About.txt" CACHE PATH "The about file to add to the distribution" FORCE)
set(VPP_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/ChangeLog.txt" CACHE PATH "The changelog file to add to the distribution" FORCE)
set(VPP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/Ircam-logo-noir-RS.png" CACHE PATH "The icon file to add to the distribution" FORCE)
set(VPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/package CACHE PATH "The directory of the package" FORCE)
include(vamp-plugin-packager/vamp-plugin-packager.cmake)

### Source ###
file(GLOB IPT_VAMP_PLUGIN_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/source/ipt_vamp_plugin.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/ipt_vamp_plugin.h
)
list(APPEND IPT_VAMP_PLUGIN_SOURCES ${IPT_VAMP_PLUGIN_LABELS_CPP})
source_group("sources" FILES ${IPT_VAMP_PLUGIN_SOURCES})

### Target ###
add_library(ipt_vamp_plugin MODULE ${IPT_VAMP_PLUGIN_SOURCES})
ive_prepare_plugin_target(ipt_vamp_plugin)
target_include_directories(ipt_vamp_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
if(UNIX AND NOT APPLE)
    target_link_libraries(ipt_vamp_plugin PRIVATE stdc++fs)
endif()

if(IPT_VAMP_PLUGIN_TESTER_ENABLE)
  enable_testing()
endif()

vpp_add_plugin(ipt_vamp_plugin
  OUTPUT_NAME ircamipt
  CATFILE ${CMAKE_CURRENT_SOURCE_DIR}/source/ircamipt.cat
  COPYRIGHT "Copyright 2026 Ircam. MIT License"
  VERSION ${PROJECT_VERSION_MAJOR}
  INSTALL ON
  DEBUG ${IPT_VAMP_PLUGIN_ENABLE_DEBUG}
  DEBUG_DOCUMENT ${CMAKE_CURRENT_SOURCE_DIR}/resources/test.ptldoc
  TESTER ${IPT_VAMP_PLUGIN_TESTER_ENABLE}
  # The test C1 (Normal input) is sufficient to check that the plugin loads (no
  # dependency issues), that the models of the bundle are loaded and that the
  # inference runs, without testing all the configurations that take long.
  TESTER_ARGUMENTS -a -t C1
  TESTER_TIMEOUT ${IPT_VAMP_PLUGIN_TESTER_TIMEOUT}
)

vpp_create_clang_format_targets(ipt_vamp_plugin "${IPT_VAMP_PLUGIN_SOURCES}")

### LibTorch libraries and models ###
# The LibTorch libraries and the models are not compiled into the plugin: they
# are copied into the Contents directory of the Vamp bundle (see the POST_BUILD
# commands). The Vamp bundle itself is generated by the vamp-plugin-packager.
setup_libtorch(ipt_vamp_plugin)

add_custom_command(TARGET ipt_vamp_plugin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_BINARY_DIR}/models/"
    "$<TARGET_FILE_DIR:ipt_vamp_plugin>/../Resources/models"
)

### Licenses ###
# The licenses of the plugin, of the third-party libraries and of the models are
# copied into the Resources/licenses folder of the Vamp bundle (hence they are
# distributed with the bundle and the installers). The ThirdPartyNotices.txt file
# lists the components and refers to the license files. The licenses of LibTorch
# and of its runtime libraries are copied by setup_libtorch (see
# resources/libtorch.cmake). The license of the models (CC BY-NC 4.0) is a copy
# of the license of the libipt project (see resources/licenses/libipt.txt).
set(IPT_VAMP_PLUGIN_LICENSES_DIR "$<TARGET_FILE_DIR:ipt_vamp_plugin>/../Resources/licenses")
add_custom_command(TARGET ipt_vamp_plugin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${IPT_VAMP_PLUGIN_LICENSES_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/ThirdPartyNotices.txt" "${IPT_VAMP_PLUGIN_LICENSES_DIR}/ThirdPartyNotices.txt"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${IPT_VAMP_PLUGIN_LICENSES_DIR}/IPT-Vamp-Plugin.txt"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/licenses/libipt.txt" "${IPT_VAMP_PLUGIN_LICENSES_DIR}/ipt-models.txt"
  COMMAND ${CMAKE_COMMAND} -E copy "${VAMP_SDK_PATH}/COPYING" "${IPT_VAMP_PLUGIN_LICENSES_DIR}/vamp-plugin-sdk.txt"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/ircam-vamp-extension/LICENSE" "${IPT_VAMP_PLUGIN_LICENSES_DIR}/ircam-vamp-extension.txt"
)

### Publishing ###
set(PROJECT_URL "https://github.com/Ircam-Partiels/ipt-vamp-plugin")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/ipt-release.md "## Downloads\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/ipt-release.md "- [IPT-Linux.zip](${PROJECT_URL}/releases/download/${IPT_VAMP_PLUGIN_BUILD_TAG}/IPT-Linux.zip)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/ipt-release.md "- [IPT-MacOS.pkg](${PROJECT_URL}/releases/download/${IPT_VAMP_PLUGIN_BUILD_TAG}/IPT-MacOS.pkg)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/ipt-release.md "- [IPT-Windows.exe](${PROJECT_URL}/releases/download/${IPT_VAMP_PLUGIN_BUILD_TAG}/IPT-Windows.exe)\n")
