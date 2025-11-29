# SPDX-License-Identifier: GPL-2.0-or-later
find_package(PkgConfig)
if(PkgConfig_FOUND)
	pkg_check_modules(Gloox gloox)
	if(Gloox_FOUND)
		set(xmpp_PART_SRCS xmpp.cpp xmpp.h sxe.h sxe.cpp)
		include_directories(${CMAKE_BINARY_DIR}/src)
		add_library(xmpp SHARED EXCLUDE_FROM_ALL ${xmpp_PART_SRCS})
		install(FILES libxmpp.inx DESTINATION ${INKSCAPE_SHARE_INSTALL}/extensions/)
		install(TARGETS xmpp DESTINATION ${INKSCAPE_SHARE_INSTALL}/extensions/)


		target_include_directories(xmpp PRIVATE ${Gloox_INCLUDE_DIRS})
		target_link_libraries(xmpp inkscape_base ${Gloox_LIBRARIES})
	endif()
endif()
