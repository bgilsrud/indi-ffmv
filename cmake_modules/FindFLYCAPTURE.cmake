# - Try to find FlyCapture library
# Once done this will define
#
#  FLYCAPTURE_FOUND - system has FlyCapture
#  FLYCAPTURE_INCLUDE_DIR - the FlyCapture include directory
#  FLYCAPTURE_LIBRARIES - Link these to use FlyCapture

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (FLYCAPTURE_INCLUDE_DIR AND FLYCAPTURE_LIBRARIES)

  # in cache already
  set(FLYCAPTURE_FOUND TRUE)
  message(STATUS "Found libflycapture: ${FLYCAPTURE_LIBRARIES}")

else (FLYCAPTURE_INCLUDE_DIR AND FLYCAPTURE_LIBRARIES)

  find_path(FLYCAPTURE_INCLUDE_DIR FlyCapture2.h
    PATH_SUFFIXES flycapture
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(FLYCAPTURE_LIBRARIES NAMES flycapture
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(FLYCAPTURE_INCLUDE_DIR AND FLYCAPTURE_LIBRARIES)
    set(FLYCAPTURE_FOUND TRUE)
  else (FLYCAPTURE_INCLUDE_DIR AND FLYCAPTURE_LIBRARIES)
    set(FLYCAPTURE_FOUND FALSE)
  endif(FLYCAPTURE_INCLUDE_DIR AND FLYCAPTURE_LIBRARIES)


  if (FLYCAPTURE_FOUND)
    if (NOT FLYCAPTURE_FIND_QUIETLY)
      message(STATUS "Found FlyCapture: ${FLYCAPTURE_LIBRARIES}")
    endif (NOT FLYCAPTURE_FIND_QUIETLY)
  else (FLYCAPTURE_FOUND)
    if (FLYCAPTURE_FIND_REQUIRED)
      message(FATAL_ERROR "FlyCapture not found. Please install FlyCapture http://ww2.ptgrey.com/sdk/flycap")
    endif (FLYCAPTURE_FIND_REQUIRED)
  endif (FLYCAPTURE_FOUND)

  mark_as_advanced(FLYCAPTURE_INCLUDE_DIR FLYCAPTURE_LIBRARIES)
  
endif (FLYCAPTURE_INCLUDE_DIR AND FLYCAPTURE_LIBRARIES)
