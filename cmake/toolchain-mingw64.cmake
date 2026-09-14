# Cross-compile the Windows build from macOS or Linux with MinGW-w64:
#   cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
# With Wine installed, ctest runs the Windows tests through it.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
find_program(OJH_WINE NAMES wine wine64)
if(OJH_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${OJH_WINE})
endif()
