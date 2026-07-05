############################## project name #################################
project(Utils)
########################### cmake version need ##############################
cmake_minimum_required(VERSION 3.0)
########################## project directories ##############################
set(UTILS_ROOT .)
set(THIRD_PARTY_ROOT ${UTILS_ROOT}/../third_party)
################################ src files ##################################
set(SOURCES
    UtilsCommonUtil.c
    UtilsModuleCommon.c
    UtilsCmdLine.c
    UtilsModuleHealth.c
    UtilsLogIO.c
    UtilsMem.c
    UtilsMsg.c
    UtilsThreadPool.c
    UtilsTimer.c
    UtilsCrypto.c
)
############################# include dir ###################################
include_directories(
    ${UTILS_ROOT}/include
    ${THIRD_PARTY_ROOT}/libevent/include
	${THIRD_PARTY_ROOT}/libevent/build/include
    ${THIRD_PARTY_ROOT}/GmSSL/include
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

########################### unittest options ###############################
option(BUILD_TESTS "Build unit tests" ON)
if (BUILD_TESTS)
	enable_testing()
    set(TEST_SOURCES
        UnitTest/UnitTestTPool.c
        UnitTest/UnitTestMsg.c
        UnitTest/UnitTestMem.c
        UnitTest/UnitTestHealth.c
        UnitTest/UnitTestTimer.c
        UnitTest/UnitTestCrypto.c
        UnitTest/UnitTestCommonUtil.c
    )
	foreach(UnitTestSrc ${TEST_SOURCES})
		get_filename_component(testName ${UnitTestSrc} NAME_WE)
		add_executable(${testName} ${UnitTestSrc})
		add_test(NAME ${testName} COMMAND ${testName})
		target_link_directories(${testName} PRIVATE
        	${THIRD_PARTY_ROOT}/libevent/build/lib
        	${THIRD_PARTY_ROOT}/GmSSL/build/bin
    	)
		target_link_libraries(${testName} PRIVATE
        	${PROJECT_NAME}
        	event
        	pthread
            gmssl
    	)
	endforeach()
endif ()
