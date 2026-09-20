if(NOT EXISTS "C:/Users/STACJO~1/Desktop/ZAKADY~1/build-r42/install_manifest.txt")
  message(FATAL_ERROR "Cannot find install manifest: C:/Users/STACJO~1/Desktop/ZAKADY~1/build-r42/install_manifest.txt")
endif()

file(READ "C:/Users/STACJO~1/Desktop/ZAKADY~1/build-r42/install_manifest.txt" files)
string(REGEX REPLACE "\n" ";" files "${files}")
foreach(file ${files})
  message(STATUS "Uninstalling $ENV{DESTDIR}${file}")
  if(IS_SYMLINK "$ENV{DESTDIR}${file}" OR EXISTS "$ENV{DESTDIR}${file}")
    exec_program(
      "C:/Users/STACJO~1/Desktop/ZAKADY~1/tools/CMAKE-~1.3-W/bin/cmake.exe" ARGS "-E remove \"$ENV{DESTDIR}${file}\""
      OUTPUT_VARIABLE rm_out
      RETURN_VALUE rm_retval
      )
    if(NOT "${rm_retval}" STREQUAL 0)
      message(FATAL_ERROR "Problem when removing $ENV{DESTDIR}${file}")
    endif()
  else(IS_SYMLINK "$ENV{DESTDIR}${file}" OR EXISTS "$ENV{DESTDIR}${file}")
    message(STATUS "File $ENV{DESTDIR}${file} does not exist.")
  endif()
endforeach()
