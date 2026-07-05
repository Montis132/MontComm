########################### cmake version need ##############################
cmake_minimum_required(VERSION 3.0)
############################## project name #################################
project(Server VERSION 1.0.0)
############################### c++ standard ################################
set(CMAKE_CXX_STANDARD 17)
########################## project directories ##############################
set(BUILD_ROOT .)
set(UTILS_ROOT ${BUILD_ROOT}/../../utils)
set(THIRD_PARTY_ROOT ${BUILD_ROOT}/../../third_party)
set(SC_SHARE_ROOT ${BUILD_ROOT}/../../SCShare)
set(MS_SHARE_ROOT ${BUILD_ROOT}/../../MSShare)
######################### global set ########################################
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
################################ src files ##################################
set(SOURCES
	ServerMain.cpp
	ServerWorker.cpp
	ServerMsgHandler.cpp
    CommMngrClient.cpp
    ${MS_SHARE_ROOT}/MSMsg.pb.cc
)
add_executable(${PROJECT_NAME} ${SOURCES})
############################# include dir ###################################
include_directories(
    ${BUILD_ROOT}/include
    ${UTILS_ROOT}/include
    ${THIRD_PARTY_ROOT}/libevent/include
    ${THIRD_PARTY_ROOT}/libevent/build/include
	${THIRD_PARTY_ROOT}/json/include
    ${SC_SHARE_ROOT}/include
    ${MS_SHARE_ROOT}/include
	${PROTOBUF_INCLUDE_DIRS}
)
############################# cc option ###################################
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE -g -O0)
set(CMAKE_VERBOSE_MAKEFILE ON)
############################ lib options ####################################
## set link dir
## find static lib first
set(OLD_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
# static link these
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
find_library(EVENT_LIBRARY NAMES event PATHS ${THIRD_PARTY_ROOT}/libevent/build/lib NO_DEFAULT_PATH)
find_library(SCSHARE_LIBRARY NAMES SCShare PATHS ${SC_SHARE_ROOT}/build/lib NO_DEFAULT_PATH)
find_library(UTILS_LIBRARY NAMES Utils PATHS ${UTILS_ROOT}/build/lib NO_DEFAULT_PATH)
find_library(GMSSL_LIBRARY NAMES gmssl PATHS ${THIRD_PARTY_ROOT}/GmSSL/build/bin NO_DEFAULT_PATH)
# dynamic link others
set(CMAKE_FIND_LIBRARY_SUFFIXES ${OLD_CMAKE_FIND_LIBRARY_SUFFIXES})
## link 
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${SCSHARE_LIBRARY} 
	${UTILS_LIBRARY} 
	${EVENT_LIBRARY} 
	${GMSSL_LIBRARY}
	${PROTOBUF_LIBRARIES}
	Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)
