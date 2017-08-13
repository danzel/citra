set(SRCS
            scripted_input.cpp
            scripted_buttons.cpp
            script_runner.cpp
            )

set(HEADERS
            scripted_input.h
            scripted_buttons.h
            script_runner.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(scripted_input STATIC ${SRCS} ${HEADERS})
target_link_libraries(scripted_input PUBLIC common core)
target_link_libraries(scripted_input PRIVATE glad SDL2)
