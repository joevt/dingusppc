include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
                    "${PROJECT_SOURCE_DIR}/thirdparty/capstone/include")

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

option(DPPC_KGMACROS  "Include debugger kgmacros" OFF)

if (DPPC_KGMACROS)
    ADD_DEFINITIONS(-DINCLUDE_KGMACROS=1)
else()
    ADD_DEFINITIONS(-DINCLUDE_KGMACROS=0)

    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/kgmacros.cpp"
                             "${CMAKE_CURRENT_SOURCE_DIR}/kgmacros.h"
                             "${CMAKE_CURRENT_SOURCE_DIR}/kgmacrostypes.h")
endif()

add_library(debugger OBJECT ${SOURCES})
