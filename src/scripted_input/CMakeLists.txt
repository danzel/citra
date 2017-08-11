set(SRCS
            scripted_input.cpp
            scripted_buttons.cpp
            )

set(HEADERS
            scripted_input.h
            scripted_buttons.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(scripted_input STATIC ${SRCS} ${HEADERS})
target_link_libraries(scripted_input PUBLIC common core)
