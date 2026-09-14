cmake_minimum_required(VERSION 3.16)
set(OJH_RELEASE_VERSION "0.1.0" CACHE STRING "Version stamped into ojh and its packages")
set(OJH_LICENSE "LicenseRef-OJH-Pending" CACHE STRING "License identifier written into packages")
project(OJH VERSION ${OJH_RELEASE_VERSION} DESCRIPTION "Objective Judge Horizon" LANGUAGES C)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
    src/jsonread.c
    src/report.c
    src/gamespec.c
    src/score.c
    src/stats.c
    src/chart.c
    src/footprint.c
    src/net.c
    src/fps.c
)
target_include_directories(ojh PRIVATE src)
target_compile_definitions(ojh PRIVATE OJH_VERSION="${PROJECT_VERSION}")

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
        target_link_options(ojh PRIVATE -static)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(ojh PRIVATE Threads::Threads m)
endif()

enable_testing()
foreach(test sha256 json jsonread relay procmeter runner tpm report spec score footprint net fps)
    add_test(NAME ${test} COMMAND ojh selftest ${test})
endforeach()

include(GNUInstallDirs)
install(TARGETS ojh RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY drivers/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ojh/drivers)
install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ojh/examples)
install(PROGRAMS tools/run-all.sh DESTINATION ${CMAKE_INSTALL_DATADIR}/ojh/tools)
install(FILES README.md docs/adding-your-game.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/ojh)

set(CPACK_PACKAGE_NAME ojh)
set(CPACK_PACKAGE_VENDOR Pr1nted)
set(CPACK_PACKAGE_CONTACT "111568178+Pr1nted@users.noreply.github.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Objective Judge Horizon: a benchmark for turn-based strategy games")
set(CPACK_PACKAGE_DESCRIPTION "Measures turn-based strategy games the same way on the same machine: turn speed, frame rate, network, footprint and an independent score per game.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Pr1nted/objective-judge-horizon")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY ojh)
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE ${OJH_LICENSE})
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_URL ${CPACK_PACKAGE_HOMEPAGE_URL})
if(NOT CPACK_GENERATOR)
    if(WIN32)
        set(CPACK_GENERATOR ZIP)
    elseif(APPLE)
        set(CPACK_GENERATOR TGZ)
    else()
        set(CPACK_GENERATOR "TGZ;DEB;RPM")
    endif()
endif()
include(CPack)
