cmake_minimum_required(VERSION 3.12)
project(sdrplay_sdr_support)


if(APPLE)
    include_directories("/usr/local/include")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib)
endif()

find_library(SDRPLAY_LIBRARY sdrplay_api)

# The SDRPlay API is a manual download (the ZIP is not in vcpkg). On MSVC the
# plugin used to be built unconditionally, so a missing/failed API download
# turned into a link error. Probe the install tree and skip the plugin instead.
if(MSVC AND NOT SDRPLAY_LIBRARY)
    find_library(SDRPLAY_LIBRARY sdrplay_api
                 PATHS
                       "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
                       "$ENV{VCPKG_ROOT}/installed/arm64-windows/lib"
                       "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib"
                       "${CMAKE_SOURCE_DIR}/vcpkg/installed/arm64-windows/lib"
                 NO_DEFAULT_PATH)
endif()

if(SDRPLAY_LIBRARY)
    message("Building with SDRPlay support")
    file(GLOB_RECURSE sdrplay_sdr_support_CPPS *.cpp)
    add_library(sdrplay_sdr_support SHARED ${sdrplay_sdr_support_CPPS})
    target_include_directories(sdrplay_sdr_support PUBLIC src)

    if(MSVC)
        target_link_libraries(sdrplay_sdr_support PUBLIC satdump_core sdrplay_api.dll)
        install(TARGETS sdrplay_sdr_support DESTINATION ${CMAKE_INSTALL_LIBDIR}/satdump/plugins RUNTIME_DEPENDENCY_SET satdump_deps)
    else()
        target_link_libraries(sdrplay_sdr_support PUBLIC satdump_core ${SDRPLAY_LIBRARY})
        install(TARGETS sdrplay_sdr_support DESTINATION ${CMAKE_INSTALL_LIBDIR}/satdump/plugins)
    endif()
else()
    message("SDRPlay Library could not be found! Not building.")
endif()
