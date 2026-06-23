cmake_minimum_required(VERSION 3.19)

#[[
# IPT Vamp Plugin
# ---------------
# A Vamp plugin for audio analysis using LibTorch (PyTorch C++ API).
#
# Dependencies:
# - LibTorch (downloaded automatically via FetchContent)
# - Vamp SDK (included via ircam-vamp-extension)
# - IRCAM Vamp Extension (IVE)
#
# Supported Platforms:
# - macOS (arm64)
# - Linux (x86_64)
# - Windows (x86_64)
#]]

# Project Options
option(IGNORE_PARTIELS_DEBUG "Disables the debug scheme using the Partiels application" OFF)
option(IPT_VAMP_PLUGIN_REFRESH_MODEL "Force re-download and re-export of the IPT model during CMake configure" OFF)

# LibTorch Configuration (Configurable via CMake options)
set(LIBTORCH_VERSION "2.4.1" CACHE STRING "Version of LibTorch to download")
set(LIBTORCH_PYTHON_VERSION "3.12" CACHE STRING "Python version for LibTorch")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Default value for MSVC_RUNTIME_LIBRARY of targets" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "Default value for POSITION_INDEPENDENT_CODE of targets" FORCE)

### Project ###
project(IptVampPlugin VERSION 1.0.0 LANGUAGES C CXX)

### Include Vamp (IVE) ###
set(IVE_BUILD_HOST_LIBRARY OFF)
set(IVE_BUILD_PLUGIN_LIBRARY ON)
add_subdirectory(ircam-vamp-extension EXCLUDE_FROM_ALL)

### Include Pytorch ###
include(FetchContent)

set(LIBTORCH_DIR "${CMAKE_CURRENT_BINARY_DIR}/libtorch")
if(APPLE)
  set(LIBTORCH_URL "https://anaconda.org/pytorch/pytorch/${LIBTORCH_VERSION}/download/osx-arm64/pytorch-${LIBTORCH_VERSION}-py${LIBTORCH_PYTHON_VERSION}_0.tar.bz2")
elseif(UNIX)
  set(LIBTORCH_URL "https://anaconda.org/pytorch/pytorch/${LIBTORCH_VERSION}/download/linux-64/pytorch-${LIBTORCH_VERSION}-py${LIBTORCH_PYTHON_VERSION}_cpu_0.tar.bz2")
elseif(WIN32)
  set(LIBTORCH_URL "https://anaconda.org/pytorch/pytorch/${LIBTORCH_VERSION}/download/win-64/pytorch-${LIBTORCH_VERSION}-py${LIBTORCH_PYTHON_VERSION}_cpu_0.tar.bz2")
endif()

if(NOT EXISTS "${LIBTORCH_DIR}/lib")
  message(STATUS "Downloading libtorch from ${LIBTORCH_URL} to ${LIBTORCH_DIR}")
  FetchContent_Declare(
    libtorch
    URL ${LIBTORCH_URL}
    SOURCE_DIR ${LIBTORCH_DIR}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(libtorch)
endif()
if(WIN32)
  set(TORCH_INSTALL_PREFIX "${LIBTORCH_DIR}/Lib/site-packages/torch")
  find_package(Torch REQUIRED HINTS "${LIBTORCH_DIR}/Lib/site-packages/torch/share/cmake")
else()
  set(TORCH_INSTALL_PREFIX "${LIBTORCH_DIR}/lib/python${LIBTORCH_PYTHON_VERSION}/site-packages/torch")
  find_package(Torch REQUIRED HINTS "${LIBTORCH_DIR}/lib/python${LIBTORCH_PYTHON_VERSION}/site-packages/torch/share/cmake")
endif()


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
target_compile_definitions(ipt_vamp_plugin PRIVATE IPT_VAMP_PLUGIN_PLUGIN_VERSION=${PROJECT_VERSION_MAJOR})
target_include_directories(ipt_vamp_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(ipt_vamp_plugin PRIVATE ${TORCH_LIBRARIES})
if(UNIX AND NOT APPLE)
    target_link_libraries(ipt_vamp_plugin PRIVATE stdc++fs)
endif()
target_compile_options(ipt_vamp_plugin PRIVATE ${TORCH_CXX_FLAGS})
target_include_directories(ipt_vamp_plugin PRIVATE ${TORCH_INCLUDE_DIRS})
set_target_properties(ipt_vamp_plugin PROPERTIES LIBRARY_OUTPUT_NAME ircamipt)

if(APPLE)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  target_link_libraries(ipt_vamp_plugin PRIVATE ${COREFOUNDATION_LIBRARY})
  set_target_properties(ipt_vamp_plugin PROPERTIES
    BUNDLE TRUE
    OUTPUT_NAME "ircamipt"
    PREFIX ""
    BUNDLE_EXTENSION "vamp"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/source/Info.plist"
    BUILD_WITH_INSTALL_RPATH FALSE
    LINK_FLAGS "-Wl,-rpath,@loader_path/../Frameworks"
    XCODE_EMBED_FRAMEWORKS "${TORCH_INSTALL_PREFIX}/lib/libtorch.dylib;${TORCH_INSTALL_PREFIX}/lib/libtorch_cpu.dylib;${TORCH_INSTALL_PREFIX}/lib/libc10.dylib"
    XCODE_EMBED_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/models"
  )
  add_custom_command(TARGET ipt_vamp_plugin POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E echo "BNDL????" > "$<TARGET_BUNDLE_CONTENT_DIR:ipt_vamp_plugin>/PkgInfo"
  )
elseif(UNIX)
    set_target_properties(ipt_vamp_plugin PROPERTIES 
      BUNDLE TRUE
      OUTPUT_NAME "ircamipt"
      PREFIX ""
      BUNDLE_EXTENSION "vamp"
      BUILD_WITH_INSTALL_RPATH FALSE
      BUILD_RPATH "$ORIGIN/libs"
      INSTALL_RPATH "$ORIGIN/libs"
    )
    add_custom_command(TARGET ipt_vamp_plugin POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/x86_64-linux/libs"
      COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:ipt_vamp_plugin>"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/ircamipt.cat"
        "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/x86_64-linux"
      COMMAND ${CMAKE_COMMAND} -E copy
        "${TORCH_INSTALL_PREFIX}/lib/libtorch.so"
        "${TORCH_INSTALL_PREFIX}/lib/libtorch_cpu.so"
        "${TORCH_INSTALL_PREFIX}/lib/libc10.so"
        "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/x86_64-linux/libs"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/models/"
        "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/Resources/models"
    )
elseif(WIN32)

    function(download_and_extract url output_dir)
      get_filename_component(filename "${url}" NAME)
      set(filename "${CMAKE_CURRENT_BINARY_DIR}/${filename}")
      if(NOT EXISTS "${output_dir}")
        message(STATUS "Downloading ${url} to ${filename}")
        file(DOWNLOAD ${url} "${filename}")
        file(MAKE_DIRECTORY "${output_dir}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${filename}" WORKING_DIRECTORY "${output_dir}")
        if(filename MATCHES "\\.conda$")
          file(GLOB inner_tar "${output_dir}/pkg-*.tar.zst")
          execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${inner_tar}" WORKING_DIRECTORY "${output_dir}")
        endif()
      endif()
    endfunction()

    set(MKL_URL "https://repo.anaconda.com/pkgs/main/win-64/mkl-2023.1.0-h6b88ed4_46358.tar.bz2")
    set(MKL_DIR "${CMAKE_CURRENT_BINARY_DIR}/mkl")
    download_and_extract(${MKL_URL} ${MKL_DIR})

    set(INTEL_OPENMP_URL "https://repo.anaconda.com/pkgs/main/win-64/intel-openmp-2023.1.0-h59b6b97_46320.conda")
    set(INTEL_OPENMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/intel-openmp")
    download_and_extract(${INTEL_OPENMP_URL} ${INTEL_OPENMP_DIR})

    set(UV_URL "https://repo.anaconda.com/pkgs/main/win-64/libuv-1.48.0-h827c3e9_0.tar.bz2")
    set(UV_DIR "${CMAKE_CURRENT_BINARY_DIR}/uv")
    download_and_extract(${UV_URL} ${UV_DIR})

    set(PYTORCH_DLLS
      "${TORCH_INSTALL_PREFIX}/lib/torch_cpu.dll"
      "${TORCH_INSTALL_PREFIX}/lib/c10.dll"
      "${TORCH_INSTALL_PREFIX}/lib/fbgemm.dll"
      "${TORCH_INSTALL_PREFIX}/lib/asmjit.dll"
      "${MKL_DIR}/Library/bin/mkl_core.2.dll"
      "${MKL_DIR}/Library/bin/mkl_intel_thread.2.dll"
      "${MKL_DIR}/Library/bin/mkl_avx2.2.dll"
      "${MKL_DIR}/Library/bin/mkl_core.2.dll"
      "${MKL_DIR}/Library/bin/mkl_def.2.dll"
      "${MKL_DIR}/Library/bin/mkl_intel_thread.2.dll"
      "${MKL_DIR}/Library/bin/mkl_vml_avx2.2.dll"
      "${MKL_DIR}/Library/bin/mkl_vml_def.2.dll"
      "${INTEL_OPENMP_DIR}/Library/bin/libiomp5md.dll"
      "${UV_DIR}/Library/bin/uv.dll"
    )

    set_target_properties(ipt_vamp_plugin PROPERTIES 
      BUNDLE TRUE
      OUTPUT_NAME "ircamipt"
      PREFIX ""
      BUNDLE_EXTENSION "vamp"
    )
    add_custom_command(TARGET ipt_vamp_plugin POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/x86_64-win"
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/Resources/models"
      COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:ipt_vamp_plugin>"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/ircamipt.cat"
        "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/x86_64-win"
      COMMAND ${CMAKE_COMMAND} -E copy
        ${PYTORCH_DLLS}
        "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/x86_64-win"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/models/"
        "$<TARGET_FILE_DIR:ipt_vamp_plugin>/ircamipt.vamp/Contents/Resources/models"
    )
endif()

vpp_add_plugin(ipt_vamp_plugin ${CMAKE_CURRENT_SOURCE_DIR}/source/ircamipt.cat)

if(NOT IGNORE_PARTIELS_DEBUG)
  vpp_set_plugin_debug(ipt_vamp_plugin "${CMAKE_CURRENT_SOURCE_DIR}/resources/test.ptldoc")
  get_target_property(VAX_VAMP_PLUGIN_XCODE_SCHEME_ENVIRONMENT ipt_vamp_plugin XCODE_SCHEME_ENVIRONMENT)
  set_target_properties(ipt_vamp_plugin PROPERTIES XCODE_SCHEME_ENVIRONMENT "${VAX_VAMP_PLUGIN_XCODE_SCHEME_ENVIRONMENT}")
endif()
vpp_set_plugin_install(ipt_vamp_plugin)
vpp_create_clang_format_targets(ipt_vamp_plugin "${IPT_VAMP_PLUGIN_SOURCES}")
