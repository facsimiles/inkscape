# SPDX-License-Identifier: GPL-2.0-or-later

include(${CMAKE_CURRENT_LIST_DIR}/../HelperMacros.cmake)

INKSCAPE_PKG_CONFIG_FIND(
    MacIntegration
    gtk-mac-integration-gtk3
    >=2.0.8
    gtkosxapplication.h
    gtkmacintegration
    gtkmacintegration-gtk3)
