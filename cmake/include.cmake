target_include_directories(vpverk_server PUBLIC
            ${CMAKE_SOURCE_DIR}/src/server
)
target_include_directories(vpverk_client PUBLIC
            ${CMAKE_SOURCE_DIR}/src/client
)

target_link_libraries(vpverk_server OpenSSL::SSL)
target_link_libraries(vpverk_client OpenSSL::SSL)
