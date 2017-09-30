# - Houdini finder module
# This module searches for a valid Houdini installation.

find_path(HOUDINI_INCLUDE_DIR
    HAPI/HAPI.h
    HINTS "${HOUDINI_ROOT}" "$ENV{HOUDINI_ROOT}"
    PATH_SUFFIXES toolkit/include/
    DOC "Houdini include directories.")

find_path(HOUDINI_LIB_DIR
    libHAPIL.so
    HINTS "${HOUDINI_ROOT}" "$ENV{HOUDINI_ROOT}"
    PATH_SUFFIXES dsolib/
    DOC "Houdini library directory.")