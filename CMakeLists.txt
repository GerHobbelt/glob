cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  Glob
  VERSION 1.0
  LANGUAGES CXX
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

# PackageProject.cmake will be used to make our target installable
CPMAddPackage(
  NAME PackageProject.cmake
  GITHUB_REPOSITORY TheLartians/PackageProject.cmake
  VERSION 1.3
)

# ---- Add source files ----

# ---- Create library ----

# Note: for header-only libraries change all PUBLIC flags to INTERFACE and create an interface
# target: add_library(Glob INTERFACE) set_target_properties(Glob PROPERTIES
# INTERFACE_COMPILE_FEATURES cxx_std_17)

add_library(Glob_header_only INTERFACE )
target_include_directories(
  Glob_header_only INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                 $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)
target_compile_features(Glob_header_only INTERFACE cxx_std_17)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(Glob_header_only INTERFACE "$<$<BOOL:${MSVC}>:/permissive->")


add_library(Glob STATIC ${CMAKE_CURRENT_LIST_DIR}/source/glob.cpp)
target_compile_definitions(Glob PUBLIC -DGLOBLIB_SEPARATE_COMPILATION)
target_link_libraries(Glob PUBLIC Glob_header_only)
SET_TARGET_PROPERTIES(Glob PROPERTIES OUTPUT_NAME glob)

# ---- Create an installable target ----
# this allows users to install and find the library via `find_package()`.

# the location where the project's version header will be placed should match the project's regular
# header paths
string(TOLOWER ${PROJECT_NAME}/version.h VERSION_HEADER_LOCATION)

packageProject(
  NAME Glob_header_only
  VERSION ${PROJECT_VERSION}
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
  VERSION_HEADER "${VERSION_HEADER_LOCATION}"
  DEPENDENCIES ""
)

packageProject(
  NAME ${PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
  VERSION_HEADER "${VERSION_HEADER_LOCATION}"
  DEPENDENCIES ""
)
