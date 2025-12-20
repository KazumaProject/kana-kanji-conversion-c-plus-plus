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

# -----------------------------
# Library: core sources
# -----------------------------
add_library(core STATIC
  # prefix
  src/prefix/prefix_tree.cpp

  # louds
  src/louds/converter.cpp
  src/louds/louds.cpp

  # termId prefix
  src/prefix_with_term_id/prefix_tree_with_term_id.cpp

  # termId louds
  src/louds_with_term_id/converter_with_term_id.cpp
  src/louds_with_term_id/louds_with_term_id.cpp
)

target_include_directories(core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(core PUBLIC cxx_std_20)

# -----------------------------
# Tests
# -----------------------------
enable_testing()

add_executable(test_louds
  tests/test_louds.cpp
)
target_link_libraries(test_louds PRIVATE core)
add_test(NAME test_louds COMMAND test_louds)

add_executable(test_louds_with_term_id
  tests/test_louds_with_term_id.cpp
)
target_link_libraries(test_louds_with_term_id PRIVATE core)
add_test(NAME test_louds_with_term_id COMMAND test_louds_with_term_id)
