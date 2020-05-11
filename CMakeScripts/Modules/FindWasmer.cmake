
find_path(WASMER_INCLUDE_DIR
  NAMES
    wasmer.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
    $ENV{DEVLIBS_PATH}/include
    $ENV{WASMER_DIR}/include
)

find_library(WASMER_LIBRARY
  NAMES
    wasmer 
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    $ENV{DEVLIBS_PATH}/lib
    $ENV{WASMER_DIR}/lib
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Wasmer DEFAULT_MSG
	WASMER_LIBRARY WASMER_INCLUDE_DIR)

IF(WASMER_FOUND)
  set(WASMER_INCLUDE_DIRS ${WASMER_INCLUDE_DIR})
  set(WASMER_LIBRARIES ${WASMER_LIBRARY})
  add_definitions(-DWITH_WASMER)
ENDIF(WASMER_FOUND)

mark_as_advanced(WASMER_INCLUDE_DIR WASMER_LIBRARY)

