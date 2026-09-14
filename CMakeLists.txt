cmake_minimum_required(VERSION 3.16)
project(OJH VERSION 0.1.0 DESCRIPTION "Objective Judge Horizon" LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(ojh
    src/main.c
    src/platform.c
    src/json.c
    src/sha256.c
    src/machine.c
    src/netmeter.c
    src/procmeter.c
    src/runner.c
    src/tpm.c
)
target_include_directories(ojh PRIVATE src)

if(MSVC)
    target_compile_options(ojh PRIVATE /W4)
    target_compile_definitions(ojh PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(ojh PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_compile_definitions(ojh PRIVATE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
    target_link_libraries(ojh PRIVATE ws2_32 psapi advapi32 user32)
    if(MINGW)
        # One self-contained .exe: no libgcc or winpthread DLLs to ship next to it.
        target_link_options(ojh PRIVATE -static)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(ojh PRIVATE Threads::Threads m)
endif()

enable_testing()
foreach(test sha256 json relay procmeter runner tpm)
    add_test(NAME ${test} COMMAND ojh selftest ${test})
endforeach()
