#----------------------------------------------------------------------
#
#  Single example osmium-contrib CMakeLists.txt
#
#----------------------------------------------------------------------
project(osm-tag-history)
cmake_minimum_required(VERSION 2.8.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++11")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(MASON_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/.mason/mason)
include(${CMAKE_CURRENT_SOURCE_DIR}/.mason/mason.cmake)

mason_use(libosmium VERSION 2.12.2 HEADER_ONLY)
include_directories(${MASON_PACKAGE_libosmium_INCLUDE_DIRS})
set(OSMIUM_INCLUDE_DIRS ${MASON_PACKAGE_libosmium_INCLUDE_DIRS})

mason_use(boost VERSION 1.63.0 HEADER_ONLY)
include_directories(${MASON_PACKAGE_boost_INCLUDE_DIRS})
set(BOOST_INCLUDE_DIRS ${MASON_PACKAGE_boost_INCLUDE_DIRS})

mason_use(rocksdb VERSION 4.13)
include_directories(${MASON_PACKAGE_rocksdb_INCLUDE_DIRS})
set(ROCKSDB_INCLUDE_DIRS ${MASON_PACKAGE_rocksdb_INCLUDE_DIRS})

#----------------------------------------------------------------------

include_directories(include)

add_executable(build_tag_lookup build_tag_lookup.cpp)
add_executable(add_tags add_tags.cpp)

target_link_libraries(build_tag_lookup ${Boost_LIBRARIES} ${OSMIUM_LIBRARIES} ${ROCKSDB_LIBRARIES})
target_link_libraries(add_tags ${Boost_LIBRARIES} ${OSMIUM_LIBRARIES} ${ROCKSDB_LIBRARIES})

target_link_libraries(add_tags Threads::Threads)


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


