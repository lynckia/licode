pkg_check_modules(GLIB_PKG glib-2.0)

if (GLIB_PKG_FOUND)
  find_path(GLIB_INCLUDE_DIR  NAMES glib.h PATH_SUFFIXES glib-2.0
    PATHS
    ${GLIB_PKG_INCLUDE_DIRS}
    /usr/include/glib-2.0
    /usr/include
    /usr/local/include
    )
  find_path(GLIB_CONFIG_INCLUDE_DIR NAMES glibconfig.h PATHS ${GLIB_PKG_LIBDIR} PATH_SUFFIXES glib-2.0/include)
  find_library(GLIB_LIBRARIES2 NAMES glib-2.0
    PATHS
    ${GLIB_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
    )

  find_library(GOBJECT_LIBRARIES NAMES gobject-2.0
    PATHS
    ${GLIB_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
    )

  find_library(GTHREAD_LIBRARIES NAMES gthread-2.0
    PATHS
    ${GLIB_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
    )

  set(GLIB_LIBRARIES ${GLIB_LIBRARIES2} ${GOBJECT_LIBRARIES} ${GTHREAD_LIBRARIES}) 

else (GLIB_PKG_FOUND)
  message (FATAL_ERROR "pkg-config is needed")
  return()
endif (GLIB_PKG_FOUND)

if (GLIB_INCLUDE_DIR AND GLIB_CONFIG_INCLUDE_DIR AND GLIB_LIBRARIES)
  set(GLIB_INCLUDE_DIRS ${GLIB_INCLUDE_DIR} ${GLIB_CONFIG_INCLUDE_DIR})
endif (GLIB_INCLUDE_DIR AND GLIB_CONFIG_INCLUDE_DIR AND GLIB_LIBRARIES)

if(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)
  set(GLIB_FOUND TRUE CACHE INTERNAL "glib-2.0 found")
  message(STATUS "Found glib-2.0: ${GLIB_INCLUDE_DIR}, ${GLIB_LIBRARIES}")
else(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)
  set(GLIB_FOUND FALSE CACHE INTERNAL "glib-2.0 found")
  message(STATUS "glib-2.0 not found.")
endif(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)

mark_as_advanced(GLIB_INCLUDE_DIR GLIB_CONFIG_INCLUDE_DIR GLIB_INCLUDE_DIRS GLIB_LIBRARIES)
