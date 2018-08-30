#----------------------------------------------------------------------
#
#  OSM-Wayback CMakeLists.txt
#
#  Uses @mapbox/mason to link necessary libraries (osmium, rocksdb)
#
#  Currently compiles on Ubuntu, MacOS
#----------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8.11)
project(osm-wayback)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Wextra -pedantic -std=c++11")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type specified, defaulting to Release")
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(MASON_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/.mason/mason)
include(${CMAKE_CURRENT_SOURCE_DIR}/.mason/mason.cmake)

mason_use(libosmium VERSION 2.12.2 HEADER_ONLY)
include_directories(SYSTEM ${MASON_PACKAGE_libosmium_INCLUDE_DIRS})

mason_use(utfcpp VERSION 2.3.4 HEADER_ONLY)
include_directories(SYSTEM ${MASON_PACKAGE_utfcpp_INCLUDE_DIRS})

mason_use(protozero VERSION 1.5.2 HEADER_ONLY)
include_directories(SYSTEM ${MASON_PACKAGE_protozero_INCLUDE_DIRS})

mason_use(rapidjson VERSION 1.1.0 HEADER_ONLY)
include_directories(SYSTEM ${MASON_PACKAGE_rapidjson_INCLUDE_DIRS})

mason_use(boost VERSION 1.63.0 HEADER_ONLY)
include_directories(SYSTEM ${MASON_PACKAGE_boost_INCLUDE_DIRS})

mason_use(expat VERSION 2.2.0)
include_directories(SYSTEM ${MASON_PACKAGE_expat_INCLUDE_DIRS})
set(EXPAT_LIBRARIES ${MASON_PACKAGE_expat_STATIC_LIBS})

mason_use(bzip2 VERSION 1.0.6)
include_directories(SYSTEM ${MASON_PACKAGE_bzip2_INCLUDE_DIRS})
set(BZIP2_LIBRARIES ${MASON_PACKAGE_bzip2_STATIC_LIBS})

mason_use(zlib VERSION 1.2.8)
include_directories(SYSTEM ${MASON_PACKAGE_zlib_INCLUDE_DIRS})
set(ZLIB_LIBRARY ${MASON_PACKAGE_zlib_STATIC_LIBS})

mason_use(rocksdb VERSION 5.4.6)
include_directories(SYSTEM ${MASON_PACKAGE_rocksdb_INCLUDE_DIRS})
set(ROCKSDB_LIBRARIES ${MASON_PACKAGE_rocksdb_STATIC_LIBS})

set(ALL_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${EXPAT_LIBRARIES} ${ROCKSDB_LIBRARIES} ${ZLIB_LIBRARY} ${BZIP2_LIBRARIES} )
#----------------------------------------------------------------------
add_executable(build_lookup_index build_lookup_index.cpp)
add_executable(add_history add_history.cpp)
add_executable(add_geometry add_geometry.cpp)

target_link_libraries(build_lookup_index ${ALL_LIBRARIES})
target_link_libraries(add_history ${ALL_LIBRARIES})
target_link_libraries(add_geometry ${ALL_LIBRARIES})

#-----------------------------------------------------------------------------
#
#  Optional "cppcheck" target that checks C++ code
#
#-----------------------------------------------------------------------------
message(STATUS "Looking for cppcheck")
find_program(CPPCHECK cppcheck)

if(CPPCHECK)
    message(STATUS "Looking for cppcheck - found")
    set(CPPCHECK_OPTIONS --enable=all)

    # cpp doesn't find system includes for some reason, suppress that report
    set(CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --suppress=missingIncludeSystem)

    add_custom_target(cppcheck ${CPPCHECK} --std=c++11 ${CPPCHECK_OPTIONS} ${CMAKE_SOURCE_DIR}/*pp)
else()
    message(STATUS "Looking for cppcheck - not found")
    message(STATUS "  Build target 'cppcheck' will not be available")
endif()


#-----------------------------------------------------------------------------
#
#  Optional "iwyu" target to check headers
#  http://include-what-you-use.org/
#
#-----------------------------------------------------------------------------
find_program(IWYU_TOOL iwyu_tool.py)

if(IWYU_TOOL)
    message(STATUS "Looking for iwyu_tool.py - found")
    add_custom_target(iwyu ${IWYU_TOOL} -p ${CMAKE_BINARY_DIR})
else()
    message(STATUS "Looking for iwyu_tool.py - not found")
    message(STATUS "  Make target 'iwyu' will not be available")
endif()
