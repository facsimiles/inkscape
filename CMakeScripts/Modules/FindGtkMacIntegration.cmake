# - Try to find gtk-mac-integration
# NOTE: gtk-mac-intergration works with both GTK2 and GTK3
# Once done, this will define
#
#  GTKMACINTEGRATION_FOUND - system has libgtkmacintegration
#  GTKMACINTEGRATION_INCLUDE_DIRS - the libgtkmacintegration include directory
#  GTKMACINTEGRATION_LIBRARIES - The libraries needed to use libgtkmacintegration
#  GTKMACINTEGRATION_CPPFLAGS - The cflags needed to use libgtkmacintegration

set(_GTKMACINTEGRATION_ROOT_PATHS
  ${CMAKE_INSTALL_PREFIX}
)

find_path(GTKMACINTEGRATION_INCLUDE_DIRS
  NAMES
    gtkosxapplication.h
  HINTS
    _GTKMACINTEGRATION_ROOT_PATHS
  PATH_SUFFIXES
    include/gtkmacintegration-gtk3 
    include/gtkmacintegration
)

find_library(GTKMACINTEGRATION_LIBRARIES
  NAMES
    gtkmacintegration-gtk3 gtkmacintegration
  HINTS
    ${_GTKMACINTEGRATION_ROOT_PATHS}
  PATH_SUFFIXES
    bin
    lib
)

set(GTKMACINTEGRATION_CPPFLAGS "-DMAC_INTEGRATION")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GTKMACINTEGRATION
  DEFAULT_MSG
  GTKMACINTEGRATION_INCLUDE_DIRS GTKMACINTEGRATION_LIBRARIES GTKMACINTEGRATION_CPPFLAGS
)

mark_as_advanced(GTKMACINTEGRATION_INCLUDE_DIRS GTKMACINTEGRATION_LIBRARIES GTKMACINTEGRATION_CPPFLAGS)
