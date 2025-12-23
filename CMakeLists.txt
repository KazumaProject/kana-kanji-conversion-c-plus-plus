cmake_minimum_required(VERSION 3.20)

project(jawiki_louds_cpp
  VERSION 0.1.0
  LANGUAGES CXX
)

# -----------------------------
# Global settings
# -----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- mozc fetch tool ----
# NOTE: In some environments CMake may resolve CURL::libcurl to a static libcurl
# that requires additional transitive system libraries. Make the fetch tool
# opt-in so the rest of the project can build cleanly.
option(BUILD_MOZC_FETCH "Build mozc_dic_fetch (requires a fully linkable libcurl)" OFF)
if(BUILD_MOZC_FETCH)
  find_package(CURL REQUIRED)
  add_executable(mozc_dic_fetch
    src/dictionary_builder/mozc_fetch/mozc_fetch.cpp
  )
  target_link_libraries(mozc_dic_fetch PRIVATE CURL::libcurl)
endif()

# -----------------------------
# Dictionary builder libs
# -----------------------------

add_library(louds_utf16 STATIC
  src/dictionary_builder/louds_builder/prefix_tree/prefix_tree_utf16.cpp
  src/dictionary_builder/louds_builder/prefix_tree_with_term_id/prefix_tree_with_term_id_utf16.cpp
  src/dictionary_builder/louds_builder/louds/louds_utf16_writer.cpp
  src/dictionary_builder/louds_builder/louds/louds_converter_utf16.cpp
  src/dictionary_builder/louds_builder/louds/louds_utf16_reader.cpp
  src/dictionary_builder/louds_builder/louds_with_term_id/louds_with_term_id_utf16.cpp
  src/dictionary_builder/louds_builder/louds_with_term_id/louds_converter_with_term_id_utf16.cpp
)

target_include_directories(louds_utf16 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dictionary_builder/louds_builder
)

# ConnectionIdBuilder (new)
add_library(connection_id STATIC
  src/dictionary_builder/connection_id/connection_id_builder.cpp
)
target_include_directories(connection_id PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dictionary_builder
)

# TokenArray (posting lists keyed by yomi termId)
add_library(token_array STATIC
  src/dictionary_builder/token_array/token_array.cpp
)
target_include_directories(token_array PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dictionary_builder
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dictionary_builder/louds_builder
)
target_link_libraries(token_array PUBLIC louds_utf16)

# -----------------------------
# Dictionary builder executable
# -----------------------------

add_executable(dictionary_builder
  src/dictionary_builder/dictionary_builder.cpp
)

target_link_libraries(dictionary_builder PRIVATE
  louds_utf16
  connection_id
)

# -----------------------------
# Tries + TokenArray builder (Kotlin buildTriesAndTokenArray equivalent)
# -----------------------------

add_executable(tries_token_builder
  src/dictionary_builder/tries_token_builder.cpp
)

target_link_libraries(tries_token_builder PRIVATE
  louds_utf16
  token_array
)

# -----------------------------
# CLI
# -----------------------------

add_executable(cps_cli cli/louds/cps_cli.cpp)
target_link_libraries(cps_cli PRIVATE louds_utf16)
