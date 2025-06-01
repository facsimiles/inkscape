# SPDX-License-Identifier: GPL-2.0-or-later

set(debug_SRC
	console-output-undo-observer.cpp
	demangle.cpp
	heap.cpp
	logger.cpp
	sysv-heap.cpp
	timestamp.cpp

	# ------
	# Header
	console-output-undo-observer.h
	demangle.h
	event-tracker.h
	event.h
	gc-heap.h
	heap.h
	logger.h
	simple-event.h
	sysv-heap.h
	timestamp.h
)

# add_inkscape_lib(debug_LIB "${debug_SRC}")
add_inkscape_source("${debug_SRC}")
