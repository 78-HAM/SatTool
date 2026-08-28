#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def read_bytes(relative):
    return (ROOT / relative).read_bytes()


def require(relative, needle):
    assert needle in read(relative), f"{relative} is missing {needle!r}"


def forbid(relative, needle):
    assert needle not in read(relative), f"{relative} still contains {needle!r}"


require("CMakeLists.txt", 'project(SatTool VERSION "0.0.1")')
require("CMakeLists.txt", 'set(CPACK_PACKAGE_VENDOR "SatTool")')
require("src-ui/CMakeLists.txt", "OUTPUT_NAME sattool-ui")
require("src-cli/CMakeLists.txt", "OUTPUT_NAME sattool")
require("src-core/CMakeLists.txt", "OUTPUT_NAME sattool_core")
require("src-interface/CMakeLists.txt", "OUTPUT_NAME sattool_interface")
require("src-interface/settings.cpp", '{"zh_CN", "Chinese"}')
require("src-interface/loader/loader.cpp", 'title = "SatTool";')
require("android/app/build.gradle", 'applicationId "com.sattool.app"')
require("android/app/build.gradle", 'versionName "0.0.1"')
require("android/app/src/main/res/values/strings.xml", ">SatTool<")
assert read_bytes("resources/icon.png") == read_bytes("icon.png"), (
    "the welcome screen still uses a different legacy icon asset"
)

for path in (
    "sattool.desktop",
    "sattool.appdata.xml",
    "android/app/src/main/AndroidManifest.xml",
    "android/app/src/main/res/values/strings.xml",
    "src-interface/loader/loader.cpp",
    "src-interface/main_ui.cpp",
    "src-interface/settings.cpp",
    "src-interface/status_logger_sink.cpp",
):
    forbid(path, "SatDump")

for obsolete in (
    "satdump.desktop",
    "satdump.appdata.xml",
    "satdump_cfg.json",
    "resources/i18n/po/satdump.pot",
    "resources/i18n/zh_CN/LC_MESSAGES/satdump.mo",
):
    assert not (ROOT / obsolete).exists(), f"obsolete branded file still exists: {obsolete}"

for required in (
    "icon.png",
    "windows/icon.ico",
    "sattool_cfg.json",
    "resources/i18n/po/sattool.pot",
    "resources/i18n/zh_CN/LC_MESSAGES/sattool.mo",
):
    assert (ROOT / required).is_file(), f"required SatTool file is missing: {required}"

print("SatTool branding and V0.0.1 metadata verified")
