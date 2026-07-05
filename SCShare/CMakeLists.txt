############################## project name #################################
project(SCShare)
########################### cmake version need ##############################
cmake_minimum_required(VERSION 3.0)
########################## project directories ##############################
set(BUILD_ROOT .)
set(THIRD_PARTY_ROOT ${BUILD_ROOT}/../third_party)
set(UTILS_ROOT ${BUILD_ROOT}/../utils)
################################ src files ##################################
set(SOURCES
    SCMsg.pb.cc
    SCShareCommon.cpp
)
find_package(Protobuf REQUIRED)
############################# include dir ###################################
include_directories(
    ${BUILD_ROOT}/include
    ${UTILS_ROOT}/include
	${THIRD_PARTY_ROOT}/json/include
    ${THIRD_PARTY_ROOT}/libevent/include
    ${THIRD_PARTY_ROOT}/libevent/build/include
    ${PROTO_MSG_ROOT}
)
############################ target rules ###################################
# lib options
option(BUILD_SHARED_LIBS "Build shared library" OFF)
if (BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
else ()
    add_library(${PROJECT_NAME} STATIC ${SOURCES})
endif ()
target_compile_options(${PROJECT_NAME} PRIVATE -g -O0)
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
############################# cc option ###################################
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
endif()
