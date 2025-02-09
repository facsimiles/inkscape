#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "2Geom::2geom" for configuration "Release"
set_property(TARGET 2Geom::2geom APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(2Geom::2geom PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/lib2geom.1.5.0.dylib"
  IMPORTED_SONAME_RELEASE "@rpath/lib2geom.1.5.0.dylib"
  )

list(APPEND _cmake_import_check_targets 2Geom::2geom )
list(APPEND _cmake_import_check_files_for_2Geom::2geom "${_IMPORT_PREFIX}/lib/lib2geom.1.5.0.dylib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
