# SPDX-License-Identifier: GPL-2.0-or-later
set(grid_PART_SRCS grid.cpp)                                                                      

include_directories( ${CMAKE_BINARY_DIR}/src )

add_library(grid2 SHARED EXCLUDE_FROM_ALL ${grid_PART_SRCS})
install(FILES libgrid2.inx DESTINATION ${INKSCAPE_SHARE_INSTALL}/extensions/)
install(TARGETS grid2 DESTINATION ${INKSCAPE_SHARE_INSTALL}/extensions/)
                                                                                                              
target_link_libraries(grid2 inkscape_base)                                                  
                                                                                                              
