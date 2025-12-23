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

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Nice warnings (GCC/Clang)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- mozc fetch tool ----
find_package(CURL REQUIRED)

add_executable(mozc_dic_fetch
  src/dictionary_builder/mozc_fetch/mozc_fetch.cpp
)

target_link_libraries(mozc_dic_fetch PRIVATE CURL::libcurl)

# -----------------------------
# LOUDS builder
# -----------------------------

# prefix (UTF-16)
src/dictionary_builder/louds_builder/prefix_tree/prefix_tree_utf16.cpp

# louds (UTF-16)
src/dictionary_builder/louds_builder/louds/louds_utf16_writer.cpp
src/dictionary_builder/louds_builder/louds/louds_converter_utf16.cpp
src/dictionary_builder/louds_builder/louds/louds_utf16_reader.cpp
