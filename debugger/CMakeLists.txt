include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
                    "${PROJECT_SOURCE_DIR}/thirdparty/capstone/include")

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(debugger OBJECT ${SOURCES})
