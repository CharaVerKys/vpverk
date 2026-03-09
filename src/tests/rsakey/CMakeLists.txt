cmake_minimum_required(VERSION 3.22)
project(rsakey_test LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
            ../../shared #what the actual fuck?
        )

add_executable(rsakey_test rsakey_test.cpp cussert.cpp)

find_package(OpenSSL REQUIRED)
target_link_libraries(rsakey_test OpenSSL::SSL)
