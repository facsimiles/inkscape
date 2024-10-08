set(THEME Dash)
install(FILES "index.theme" DESTINATION ${INKSCAPE_SHARE_INSTALL}/icons/${THEME} COMPONENT themes)
install(FILES "highlights.css" DESTINATION ${INKSCAPE_SHARE_INSTALL}/icons/${THEME} COMPONENT themes)

set(PIXMAP_SIZES "symbolic")
set(CONTENT "actions")

foreach(pixmap_size ${PIXMAP_SIZES})
  foreach(content ${CONTENT})
    FILE(GLOB PIXMAP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${pixmap_size}/${content}/*.png ${CMAKE_CURRENT_SOURCE_DIR}/${pixmap_size}/${content}/*.svg)
    install(FILES ${PIXMAP_FILES} DESTINATION ${INKSCAPE_SHARE_INSTALL}/icons/${THEME}/${pixmap_size}/${content} COMPONENT themes)
  endforeach(content)
endforeach(pixmap_size)

FILE(GLOB CURSOR_FILES ${CMAKE_CURRENT_SOURCE_DIR}/cursors/*.svg)
install(FILES ${CURSOR_FILES} DESTINATION ${INKSCAPE_SHARE_INSTALL}/icons/${THEME}/cursors)

FILE(GLOB CURSOR_CSS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/cursors/*.css)
install(FILES ${CURSOR_CSS_FILES} DESTINATION ${INKSCAPE_SHARE_INSTALL}/icons/${THEME}/cursors)

if(WIN32)
    install(CODE "execute_process(COMMAND gtk-update-icon-cache \${CMAKE_INSTALL_PREFIX}/${INKSCAPE_SHARE_INSTALL}/icons/${THEME})")
endif()
