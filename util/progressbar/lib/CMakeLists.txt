add_library(progressbar progressbar.c)
add_library(statusbar statusbar.c)

target_link_libraries(progressbar ${CURSES_LIBRARIES})
target_link_libraries(statusbar ${CURSES_LIBRARIES})
