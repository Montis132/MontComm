########################## source dirs ################################
set(MSGPACK_C_ROOT ${CMAKE_CURRENT_LIST_DIR}/msgpack/cpp/src)

############################### src files ###############################
set(SOURCES
    ${MSGPACK_C_ROOT}/unpack.c
    ${MSGPACK_C_ROOT}/zone.c
    ${MSGPACK_C_ROOT}/objectc.c
    ${MSGPACK_C_ROOT}/vrefbuffer.c
    ${MSGPACK_C_ROOT}/version.c
)

############################# target rules ###############################
add_library(msgpack-c STATIC ${SOURCES})

target_include_directories(msgpack-c PRIVATE
    ${MSGPACK_C_ROOT}
    ${CMAKE_CURRENT_LIST_DIR}/msgpack/msgpack
)

############################# cc option ###################################
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(msgpack-c PRIVATE -Wno-unused-parameter -Wno-unused-variable)
endif()
