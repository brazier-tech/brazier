#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "brazier::core" for configuration "Debug"
set_property(TARGET brazier::core APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(brazier::core PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libbrazier_core.a"
  )

list(APPEND _cmake_import_check_targets brazier::core )
list(APPEND _cmake_import_check_files_for_brazier::core "${_IMPORT_PREFIX}/lib/libbrazier_core.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
