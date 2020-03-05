# SPDX-License-Identifier: GPL-2.0-or-later
set(xmpp_PART_SRCS xmpp.cpp xmpp.h)
include_directories(${CMAKE_BINARY_DIR}/src)
add_library(xmpp SHARED EXCLUDE_FROM_ALL ${xmpp_PART_SRCS})
