if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    target_compile_options(vpverk_server  PRIVATE 
        -Werror
        -Wall
        -Wextra
        -Wpedantic
        # -Wsign-conversion # pretty useless, just silenced all warnings
        -Wconversion # found couple of bugs, keep it
        -Wno-sign-conversion # for clang, against Wconversion
      #  -Weverything

      -fdiagnostics-color=always

#        -fno-omit-frame-pointer
#        -fsanitize=address
#        -fsanitize=undefined
#        -fsanitize=(un)signed-integer-overflow
        )
   target_link_options( vpverk_server  PRIVATE
#        -fsanitize=address
#        -fsanitize=undefined
#        -fsanitize=(un)signed-integer-overflow
       )
####################################################################
    target_compile_options(vpverk_client  PRIVATE 
        -Werror
        -Wall
        -Wextra
        -Wpedantic
        # -Wsign-conversion # pretty useless, just silenced all warnings
        -Wconversion # found couple of bugs, keep it
        -Wno-sign-conversion # for clang, against Wconversion
      #  -Weverything

      -fdiagnostics-color=always

#        -fno-omit-frame-pointer
#        -fsanitize=address
#        -fsanitize=undefined
#        -fsanitize=(un)signed-integer-overflow
        )
   target_link_options( vpverk_client PRIVATE
#        -fsanitize=address
#        -fsanitize=undefined
#        -fsanitize=(un)signed-integer-overflow
       )
endif()
