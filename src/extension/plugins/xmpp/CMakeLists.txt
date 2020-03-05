# SPDX-License-Identifier: GPL-2.0-or-later
set(xmpp_PART_SRCS xmpp.cpp xmpp.h)
include_directories(${CMAKE_BINARY_DIR}/src)
add_library(xmpp SHARED EXCLUDE_FROM_ALL ${xmpp_PART_SRCS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(Gloox REQUIRED gloox)
target_include_directories(xmpp PRIVATE ${Gloox_INCLUDE_DIRS})
target_link_libraries(xmpp inkscape_base ${Gloox_LIBRARIES})
