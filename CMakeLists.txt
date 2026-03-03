cmake_minimum_required(VERSION 3.23)
project(vpverk LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    SYSTEM ${CMAKE_SOURCE_DIR}/asio/include
)

include_directories(
            ${CMAKE_SOURCE_DIR}/src/shared
            ${CMAKE_SOURCE_DIR}/method-invoke
)

include(cmake/src.cmake)

add_executable(vpverk_server 
    ${SHARED_SOURCES}
    ${SERVER_SOURCES}
    )
add_executable(vpverk_client
    ${SHARED_SOURCES}
    ${CLIENT_SOURCES}
    )

include(cmake/defines.cmake)
include(cmake/compile.cmake)
include(cmake/include.cmake)




