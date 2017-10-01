# - Houdini finder module
# This module searches for a valid Houdini installation.

set(_clarisse_hints "${HOUDINI_ROOT}" "$ENV{HOUDINI_ROOT}")

find_path(HOUDINI_INCLUDE_DIR
    HAPI/HAPI.h
    HINTS ${_clarisse_hints}
    PATH_SUFFIXES toolkit/include/
    DOC "Houdini include directories.")

find_path(HOUDINI_LIBRARY_DIR
    libHAPIL.so
    HINTS ${_clarisse_hints}
    PATH_SUFFIXES dsolib/
    DOC "Houdini library directory.")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Houdini DEFAULT_MSG
        REQUIRED_VARS HOUDINI_INCLUDE_DIR HOUDINI_LIBRARY_DIR)