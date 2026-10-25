cmake_minimum_required(VERSION 3.12)
project(inmarsat_support)

file(GLOB_RECURSE inmarsat_support_CPPS *.cpp aero/libacars/*.c aero/mbelib/*.c)
add_library(inmarsat_support SHARED ${inmarsat_support_CPPS})
target_link_libraries(inmarsat_support PUBLIC satdump_core)
target_include_directories(inmarsat_support PUBLIC src .)

if(MSVC)
    # "zlib.dll" only resolves when the import library is literally named
    # zlib.lib in a searched directory. vcpkg ships it as zlib.lib/zlibd.lib
    # (and names it differently per triplet), so the ARM64 build failed with
    # "LNK1181: cannot open input file 'zlib.lib'". Prefer the imported target,
    # then find_library, and fall back to the historical name.
    target_link_libraries(inmarsat_support PUBLIC ws2_32)
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        target_link_libraries(inmarsat_support PUBLIC ZLIB::ZLIB)
    else()
        find_library(INMARSAT_ZLIB_LIBRARY NAMES zlib zlibd zlib1)
        if(INMARSAT_ZLIB_LIBRARY)
            target_link_libraries(inmarsat_support PUBLIC ${INMARSAT_ZLIB_LIBRARY})
        else()
            target_link_libraries(inmarsat_support PUBLIC zlib.dll)
        endif()
    endif()
endif()

install(TARGETS inmarsat_support DESTINATION ${CMAKE_INSTALL_LIBDIR}/satdump/plugins)
