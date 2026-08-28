#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8-sig")


def require(relative, needle):
    assert needle in read(relative), f"{relative} is missing {needle!r}"


workflow = ".github/workflows/build.yml"
require(workflow, "-DPLUGINS_ALL=ON")
require(workflow, "-DPLUGIN_SDDC_SDR_SUPPORT=ON")
require(workflow, "shell: pwsh")
require(workflow, "python3 tools/verify_windows_release_config.py")

dependency_script = "windows/Configure-vcpkg.ps1"
require(dependency_script, "hdf5[cpp,hl]")
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

print("Complete Windows decoder, SDR plugin, and runtime packaging configuration verified")
