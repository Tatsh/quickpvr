# Sign BUNDLE (an app, extension, or generator) and everything nested in it, innermost first.
# Run as cmake -DBUNDLE=path -DIDENTITY=- -DENTITLEMENTS=path -P sign-bundle.cmake.
if(NOT BUNDLE OR NOT EXISTS "${BUNDLE}")
  message(FATAL_ERROR "No bundle to sign at '${BUNDLE}'.")
endif()
if(NOT IDENTITY)
  set(IDENTITY "-")
endif()

function(_sign path)
  set(args --force --sign "${IDENTITY}" --timestamp=none)
  if(path MATCHES "\\.appex$")
    # QuickLook extensions must be sandboxed.
    list(APPEND args --entitlements "${ENTITLEMENTS}")
  endif()
  execute_process(COMMAND codesign ${args} "${path}" RESULT_VARIABLE result
                  ERROR_VARIABLE output)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "codesign failed for ${path}: ${output}")
  endif()
endfunction()

file(GLOB nested LIST_DIRECTORIES true "${BUNDLE}/Contents/PlugIns/*.appex"
     "${BUNDLE}/Contents/Library/QuickLook/*.qlgenerator")
foreach(item IN LISTS nested)
  _sign("${item}")
endforeach()
_sign("${BUNDLE}")
