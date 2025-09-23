# SPDX-License-Identifier: GPL-2.0-or-later

# Non-unit testable files
set(colors_SRC
	cms/system.cpp   # Needs preferences
	document-cms.cpp # Links to sp-object tree
	dragndrop.cpp    # Uses xml-color
	color-set.cpp    # Uses unnlinkable sigc+
	xml-color.cpp    # sp-document and document-cms

	cms/profile.h
	document-cms.h
	dragndrop.h
	color-set.h
	xml-color.h
)

include("${CMAKE_CURRENT_LIST_DIR}/CMakeUnit.txt")

add_inkscape_source("${colors_unit_SRC}")
add_inkscape_source("${colors_SRC}")
