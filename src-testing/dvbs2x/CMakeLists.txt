cmake_minimum_required(VERSION 3.18)
project(dvbs2x_regression LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(SATTOOL_ROOT ../.. ABSOLUTE)
find_path(VOLK_INCLUDE_DIR volk/volk.h REQUIRED)
find_library(VOLK_LIBRARY NAMES volk REQUIRED)
set(VOLK_EXTRA_INCLUDE_DIR "" CACHE PATH "Additional VOLK source include directory for an uninstalled build")
option(S2X_TEST_SSE41 "Exercise the 16-frame SSE4.1 LDPC decoder" OFF)
find_package(Threads REQUIRED)

set(DVB ${SATTOOL_ROOT}/plugins/dvb_support)
add_executable(dvbs2x_regression
    regression.cpp
    ${DVB}/codings/dvb-s2/bbframe_bch.cpp
    ${DVB}/codings/dvb-s2/bch/bitman.cpp
    ${DVB}/codings/dvb-s2/bbframe_ldpc.cpp
    ${DVB}/codings/dvb-s2/bbframe_descramble.cpp
    ${DVB}/codings/dvb-s2/s2_deinterleaver.cpp
    ${DVB}/codings/dvb-s2/s2_scrambling.cpp
    ${DVB}/dvbs2/dvbs2_pl_sync.cpp
    ${DVB}/dvbs2/dvbs2_pll.cpp
    ${DVB}/dvbs2/dvbs2_bb_to_soft.cpp
    ${SATTOOL_ROOT}/src-core/common/dsp/demod/constellation.cpp
    ${SATTOOL_ROOT}/src-core/common/dsp/buffer.cpp
    ${SATTOOL_ROOT}/src-core/logger.cpp)
target_include_directories(dvbs2x_regression PRIVATE
    ${DVB} ${SATTOOL_ROOT}/src-core ${VOLK_INCLUDE_DIR}
    ${VOLK_EXTRA_INCLUDE_DIR} ${SATTOOL_ROOT}/volk_includes)
target_compile_definitions(dvbs2x_regression PRIVATE SOURCE_PATH_SIZE=0 SATDUMP_DLL_EXPORT)
target_link_libraries(dvbs2x_regression PRIVATE ${VOLK_LIBRARY} Threads::Threads)
if(S2X_TEST_SSE41)
    if(MSVC)
        target_compile_definitions(dvbs2x_regression PRIVATE __SSE4_1__)
    else()
        target_compile_options(dvbs2x_regression PRIVATE -msse4.1)
    endif()
endif()
add_test(NAME dvbs2x_regression COMMAND dvbs2x_regression)
set_tests_properties(dvbs2x_regression PROPERTIES TIMEOUT 180)
