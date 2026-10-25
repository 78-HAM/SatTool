cmake_minimum_required(VERSION 3.12)
project(usrp_sdr_support)


find_library(UHD_LIBRARY uhd)

# On MSVC the library is resolved by the linker, so find_library() is normally
# empty and the plugin used to be built unconditionally. If UHD failed to build
# (transient dependency error, unsupported toolchain, ...) that turns into a
# hard link failure. Probe the install tree and skip the plugin when UHD is not
# actually available.
if(MSVC AND NOT UHD_LIBRARY)
    find_library(UHD_LIBRARY uhd
                 PATHS
                       "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
                       "$ENV{VCPKG_ROOT}/installed/arm64-windows/lib"
                       "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib"
                       "${CMAKE_SOURCE_DIR}/vcpkg/installed/arm64-windows/lib"
                 NO_DEFAULT_PATH)
endif()

if(UHD_LIBRARY) # OR ANDROID
    message("Building with USRP support")
    file(GLOB_RECURSE usrp_sdr_support_CPPS *.cpp)
    add_library(usrp_sdr_support SHARED ${usrp_sdr_support_CPPS})
    target_include_directories(usrp_sdr_support PUBLIC src)

    if(MSVC)
        target_link_libraries(usrp_sdr_support PUBLIC satdump_core uhd.dll)
        install(TARGETS usrp_sdr_support DESTINATION ${CMAKE_INSTALL_LIBDIR}/satdump/plugins RUNTIME_DEPENDENCY_SET satdump_deps)
    else()
        target_link_libraries(usrp_sdr_support PUBLIC satdump_core ${UHD_LIBRARY})
        install(TARGETS usrp_sdr_support DESTINATION ${CMAKE_INSTALL_LIBDIR}/satdump/plugins)
    endif()
else()
    message("UHD Library could not be found! Not building.")
endif()
