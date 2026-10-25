cmake_minimum_required(VERSION 3.12)
project(limesdr_sdr_support)


find_library(LIMESDR_LIBRARY LimeSuite)

# On MSVC the library is normally resolved by the linker, so find_library()
# comes back empty and the plugin used to be built unconditionally. If the FX3
# SDK download fails, LimeSuite is never built and that produces a link error.
# Probe the vcpkg install tree instead, and skip the plugin when it is absent.
if(MSVC AND NOT LIMESDR_LIBRARY)
    find_library(LIMESDR_LIBRARY LimeSuite
                 PATHS
                       "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
                       "$ENV{VCPKG_ROOT}/installed/arm64-windows/lib"
                       "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib"
                       "${CMAKE_SOURCE_DIR}/vcpkg/installed/arm64-windows/lib"
                 NO_DEFAULT_PATH)
endif()

if(LIMESDR_LIBRARY OR ANDROID)
    message("Building with LimeSDR support")
    file(GLOB_RECURSE limesdr_sdr_support_CPPS *.cpp)
    add_library(limesdr_sdr_support SHARED ${limesdr_sdr_support_CPPS})
    target_include_directories(limesdr_sdr_support PUBLIC src)

    if(MSVC)
        target_link_libraries(limesdr_sdr_support PUBLIC satdump_core LimeSuite.dll)
        install(TARGETS limesdr_sdr_support DESTINATION ${CMAKE_INSTALL_LIBDIR}/satdump/plugins RUNTIME_DEPENDENCY_SET satdump_deps)
    elseif(ANDROID)
        target_link_libraries(limesdr_sdr_support PUBLIC satdump_core limesuite usb)
        target_include_directories(limesdr_sdr_support PUBLIC ../../../android/deps/liblimesuite)
        install(TARGETS limesdr_sdr_support DESTINATION ${CMAKE_INSTALL_LIBDIR}/satdump/plugins)
    else()
        target_link_libraries(limesdr_sdr_support PUBLIC satdump_core ${LIMESDR_LIBRARY})
        install(TARGETS limesdr_sdr_support DESTINATION ${CMAKE_INSTALL_LIBDIR}/satdump/plugins)
    endif()
else()
    message("LimeSDR Library could not be found! Not building.")
endif()
