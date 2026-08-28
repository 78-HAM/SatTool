#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8-sig")


def require(relative, needle):
    assert needle in read(relative), f"{relative} is missing {needle!r}"


workflow = ".github/workflows/build.yml"
require(workflow, "runs-on: windows-2022")
require(workflow, "-DPLUGINS_ALL=ON")
require(workflow, "-DPLUGIN_SDDC_SDR_SUPPORT=ON")
require(workflow, "shell: pwsh")
require(workflow, "python3 tools/verify_windows_release_config.py")
require(workflow, '"bin\\sattool-ui.exe") --version')
assert "Start-Process" not in read(workflow), (
    "the Windows release still tries to open an OpenGL window on the headless Actions runner"
)

require("src-ui/main.cpp", 'argv[i] == std::string("--version")')
require("src-ui/main.cpp", "return 0;")

dependency_script = "windows/Configure-vcpkg.ps1"
require(dependency_script, "hdf5[cpp,hl]")
require(dependency_script, " zlib ")
require(dependency_script, '"/p:PlatformToolset=v145"')
require(dependency_script, "/p:Platform=$generator /p:Configuration=Release @libusb_toolset_args")
require(dependency_script, "/p:Platform=$generator /p:Configuration=Debug @libusb_toolset_args")
require(dependency_script, "9a143094a78ec708f8c426de429a8fce9e7b47be")
require(dependency_script, "CyAPI.vcxproj")
require(dependency_script, "/p:Configuration=Release /p:Platform=$generator")
require(dependency_script, "-DENABLE_BACKEND_LIBUSB=ON")
require(dependency_script, "-DENABLE_BACKEND_CYAPI=OFF")
require(dependency_script, "git clone https://github.com/EttusResearch/uhd --depth 1 -b v4.10.0.0")
assert "git clone https://github.com/EttusResearch/uhd #" not in read(dependency_script), (
    "windows dependency setup still follows the mutable UHD default branch"
)
assert "www.satdump.org/FX3-SDK.zip" not in read(dependency_script), (
    "windows dependency setup still uses the dead FX3 SDK archive"
)

sddc_converter = "plugins/sdr_sources/sddc_sdr_support/lib/Core/conv_r2iq.cpp"
assert "#include <unistd.h>" not in read(sddc_converter), (
    "the Windows SDDC/RX888 plugin still includes the POSIX-only unistd.h header"
)
require("plugins/sdr_sources/sddc_sdr_support/CMakeLists.txt", "Setupapi.lib")
require("plugins/inmarsat_support/CMakeLists.txt", "find_package(ZLIB REQUIRED)")
require("plugins/inmarsat_support/CMakeLists.txt", "target_link_libraries(inmarsat_support PUBLIC ws2_32 ZLIB::ZLIB)")
assert "zlib.dll" not in read("plugins/inmarsat_support/CMakeLists.txt"), (
    "Inmarsat still links a DLL filename instead of the zlib import library"
)
require("plugins/elektro_arktika_support/elektro_arktika/ggak/ingestor.h", "sleep_for")
assert "#include <unistd.h>" not in read("plugins/elektro_arktika_support/elektro_arktika/ggak/ingestor.h"), (
    "Elektro Arktika still includes the POSIX-only unistd.h header"
)
require("plugins/elektro_arktika_support/elektro_arktika/ggak/plot.cpp", "point_count")
assert "std::min(" not in read("plugins/elektro_arktika_support/elektro_arktika/ggak/plot.cpp"), (
    "Elektro Arktika plot code still exposes std::min to Windows min macros"
)
require("plugins/proba_support/proba/module_proba_instruments.cpp", "correction_groups")
require("plugins/simd_extensions/simd_avx2/CMakeLists.txt", "if(CXX_AVX2_FOUND)")
require("plugins/simd_extensions/simd_avx2/CMakeLists.txt", "${CXX_AVX2_FLAGS}")
assert "CXX_HAS_AVX_2" not in read("plugins/simd_extensions/simd_avx2/CMakeLists.txt"), (
    "AVX2 plugin still checks the non-existent CXX_HAS_AVX_2 variable"
)
for runtime in (
    "airspy.dll",
    "airspyhf.dll",
    "bladerf.dll",
    "fobos.dll",
    "hackrf.dll",
    "hydrasdr.dll",
    "libad9361.dll",
    "libiio.dll",
    "libusb-1.0.dll",
    "LimeSuite.dll",
    "rtlsdr.dll",
    "sdrplay_api.dll",
    "uhd.dll",
):
    require(dependency_script, f'"{runtime}"')

require("CMakeLists.txt", 'install(FILES "${SDRPLAY_RUNTIME_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})')

release_verifier = "windows/Verify-WindowsRelease.ps1"
require(release_verifier, '"bin\\sdrplay_api.dll"')
require(release_verifier, '"bin\\sattool_sdr_server.exe"')
for plugin in (
    "firstparty_support",
    "bitview_app",
    "xrit_support",
    "meteor_support",
    "noaa_metop_support",
    "rtlsdr_sdr_support",
    "hackrf_sdr_support",
    "airspy_sdr_support",
    "sdrplay_sdr_support",
    "plutosdr_sdr_support",
    "bladerf_sdr_support",
    "usrp_sdr_support",
    "sddc_sdr_support",
    "simd_avx2",
):
    require(release_verifier, f'"{plugin}"')

for plugin in (
    "airspy_sdr_support",
    "airspyhf_sdr_support",
    "hydrasdr_sdr_support",
    "hackrf_sdr_support",
    "limesdr_sdr_support",
    "plutosdr_sdr_support",
    "rtlsdr_sdr_support",
    "mirisdr_sdr_support",
    "net_source_support",
    "remote_sdr_support",
    "rtltcp_support",
    "spyserver_support",
    "sdrpp_server_support",
):
    require(workflow, plugin)

require(workflow, '$ANDROID_SDK_ROOT/build-tools/30.0.3/aapt')
require(workflow, 'unzip -Z1 "$apk"')
for plugin in (
    "analog_support",
    "dvb_support",
    "fengyun3_support",
    "goes_support",
    "meteor_support",
    "noaa_metop_support",
    "xrit_support",
):
    require(workflow, plugin)

print("Complete Windows decoder, SDR plugin, and runtime packaging configuration verified")
