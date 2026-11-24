cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required. Usage: cmake -DOUTPUT_DIR=<output_directory> -P download_model.cmake")
endif()

set(IPT_MODEL_RELEASE_TAG "v0-models")
set(IPT_MODELS_RELEASE_API_URL "https://api.github.com/repos/Ircam-Partiels/ipt-vamp-plugin/releases/tags/${IPT_MODEL_RELEASE_TAG}")
set(IPT_MODELS_DIR "${OUTPUT_DIR}/models")
set(IPT_MODELS_RELEASE_JSON "${OUTPUT_DIR}/ipt_release.json")
set(IPT_MODELS_ASSETS_JSON "${OUTPUT_DIR}/ipt_release_assets.json")
file(MAKE_DIRECTORY "${IPT_MODELS_DIR}")

if(DEFINED ENV{GITHUB_TOKEN} AND NOT "$ENV{GITHUB_TOKEN}" STREQUAL "")
  set(HEADERS "")
  # file(DOWNLOAD) expects one HTTPHEADER keyword per header
  list(APPEND HEADERS
    HTTPHEADER "Authorization: Bearer $ENV{GITHUB_TOKEN}"
    HTTPHEADER "Accept: application/vnd.github+json"
    HTTPHEADER "User-Agent: ipt-vamp-plugin-ci"
  )
endif()

if(DEFINED HEADERS)
  file(DOWNLOAD "${IPT_MODELS_RELEASE_API_URL}" "${IPT_MODELS_RELEASE_JSON}" STATUS DOWNLOAD_STATUS ${HEADERS})
else()
  file(DOWNLOAD "${IPT_MODELS_RELEASE_API_URL}" "${IPT_MODELS_RELEASE_JSON}" STATUS DOWNLOAD_STATUS)
endif()


list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
if(NOT STATUS_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to fetch release metadata: ${DOWNLOAD_STATUS}")
endif()

file(READ "${IPT_MODELS_RELEASE_JSON}" IPT_MODELS_RELEASE_CONTENT)
string(JSON IPT_MODELS_ASSETS_API_URL GET "${IPT_MODELS_RELEASE_CONTENT}" assets_url)
if(DEFINED HEADERS)
  file(DOWNLOAD "${IPT_MODELS_ASSETS_API_URL}" "${IPT_MODELS_ASSETS_JSON}" STATUS DOWNLOAD_STATUS ${HEADERS})
else()
  file(DOWNLOAD "${IPT_MODELS_ASSETS_API_URL}" "${IPT_MODELS_ASSETS_JSON}" STATUS DOWNLOAD_STATUS)
endif()
list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
if(NOT STATUS_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to fetch release assets: ${DOWNLOAD_STATUS}")
endif()

file(READ "${IPT_MODELS_ASSETS_JSON}" IPT_MODELS_ASSETS_CONTENT)
string(JSON IPT_MODELS_RELEASE_CONTENT SET "${IPT_MODELS_RELEASE_CONTENT}" assets "${IPT_MODELS_ASSETS_CONTENT}")
file(WRITE "${IPT_MODELS_RELEASE_JSON}" "${IPT_MODELS_RELEASE_CONTENT}\n")

# Writes a description of the models that only depends on their content (the
# JSON of the assets contains volatile fields such as the download counts that
# change each time a model is downloaded), used as the key of the CI cache.
string(JSON ASSET_COUNT LENGTH "${IPT_MODELS_ASSETS_CONTENT}")
math(EXPR LAST_INDEX "${ASSET_COUNT}-1")
set(IPT_MODELS_ASSETS_KEY "")
foreach(i RANGE ${LAST_INDEX})
  string(JSON ASSET_NAME GET "${IPT_MODELS_ASSETS_CONTENT}" ${i} name)
  if(ASSET_NAME MATCHES "\\.ts$")
    string(JSON ASSET_SIZE GET "${IPT_MODELS_ASSETS_CONTENT}" ${i} size)
    string(JSON ASSET_DIGEST ERROR_VARIABLE ASSET_DIGEST_ERROR GET "${IPT_MODELS_ASSETS_CONTENT}" ${i} digest)
    if(ASSET_DIGEST_ERROR)
      string(JSON ASSET_DIGEST GET "${IPT_MODELS_ASSETS_CONTENT}" ${i} updated_at)
    endif()
    string(APPEND IPT_MODELS_ASSETS_KEY "${ASSET_NAME} ${ASSET_SIZE} ${ASSET_DIGEST}\n")
  endif()
endforeach()
file(WRITE "${OUTPUT_DIR}/ipt_release_assets.key" "${IPT_MODELS_ASSETS_KEY}")

if(GENERATE_ASSETS_ONLY)
  return()
endif()

foreach(i RANGE ${LAST_INDEX})
  string(JSON ASSET_NAME GET "${IPT_MODELS_ASSETS_CONTENT}" ${i} name)
  if(ASSET_NAME MATCHES "\\.ts$")
    string(JSON ASSET_URL GET "${IPT_MODELS_ASSETS_CONTENT}" ${i} browser_download_url)
    set(DEST "${IPT_MODELS_DIR}/${ASSET_NAME}")
    if(NOT EXISTS "${DEST}")
      message(STATUS "Downloading model ${ASSET_NAME}")
      if(DEFINED HEADERS)
        file(DOWNLOAD "${ASSET_URL}" "${DEST}" STATUS DOWNLOAD_STATUS ${HEADERS})
      else()
        file(DOWNLOAD "${ASSET_URL}" "${DEST}" STATUS DOWNLOAD_STATUS)
      endif()
      list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
      if(NOT STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed downloading ${ASSET_NAME}: ${DOWNLOAD_STATUS}")
      endif()
    else()
      message(STATUS "${ASSET_NAME} already exists")
    endif()
  endif()
endforeach()