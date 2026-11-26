set(QUICKPVR_COMPANY "Tatsh")
string(CONCAT QUICKPVR_COPYRIGHT "Copyright (c) 2009-2010 Limbic Software, Inc. "
              "Copyright (c) 2018-2026 Andrew Udvare. BSD 3-Clause License.")

# Attach a version resource to a Windows executable or DLL. Explorer shows its company, product,
# description, version, and copyright on the Details tab of Properties.
function(quickpvr_add_version_resource target description file_type)
  set(QUICKPVR_RESOURCE_DESCRIPTION "${description}")
  set(QUICKPVR_RESOURCE_FILE_TYPE "${file_type}")
  get_target_property(_output_name ${target} OUTPUT_NAME)
  if(NOT _output_name)
    set(_output_name ${target})
  endif()
  set(QUICKPVR_RESOURCE_INTERNAL_NAME "${_output_name}")
  if(file_type STREQUAL "VFT_DLL")
    set(QUICKPVR_RESOURCE_FILENAME "${_output_name}.dll")
  else()
    set(QUICKPVR_RESOURCE_FILENAME "${_output_name}.exe")
  endif()
  set(_resource "${CMAKE_CURRENT_BINARY_DIR}/${target}-version.rc")
  configure_file("${PROJECT_SOURCE_DIR}/cmake/version.rc.in" "${_resource}" @ONLY)
  target_sources(${target} PRIVATE "${_resource}")
endfunction()
