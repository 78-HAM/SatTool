param(
    [Parameter(Mandatory=$true)][string]$StagingPath
)

$ErrorActionPreference = "Stop"
if(Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue)
{
    $PSNativeCommandUseErrorActionPreference = $true
}
$bin = Join-Path $StagingPath "bin"
$plugins = Join-Path $StagingPath "lib\sattool\plugins"

$required = @(
    "bin\sattool-ui.exe",
    "bin\sattool.exe",
    "bin\sattool_sdr_server.exe",
    "share\sattool\sattool_cfg.json",
    "share\sattool\resources\i18n\zh_CN\LC_MESSAGES\sattool.mo",
    "share\sattool\resources\fonts\NotoSansSC-Regular.otf",
    "bin\sdrplay_api.dll"
)

$required_plugins = @(
    "aim_support", "analog_support", "aws_support", "bitview_app",
    "bluewalker3_support", "cluster_support", "cubesat_support", "dmsp_support",
    "dscovr_support", "dvb_support", "earthcare_support", "elektro_arktika_support",
    "eos_support", "experimental_devices_support", "fengyun2_support", "fengyun3_support",
    "fengyun4_support", "firstparty_loader_support", "firstparty_support", "gcom_support",
    "geonetcast_support", "gk2a_support", "goes_support", "himawari_support",
    "hinode_support", "inmarsat_support", "insat_support", "jason3_support",
    "jpss_support", "kanopus_support", "landsat_support", "mats_support",
    "meteor_support", "meteosat_support", "metopsg_support", "noaa_metop_support",
    "oceansat_support", "orbcomm_support", "others_support", "portaudio_audio_sink",
    "proba_support", "radiosonde_support", "seawifs_support", "simd_avx2", "simd_sse41",
    "spacex_support", "stereo_support", "tools_app", "tubsat_support",
    "umka_support", "uvsq_support", "webhook_app", "wipsettings_app",
    "wsf_support", "xrit_support",
    "airspy_sdr_support", "airspyhf_sdr_support", "bladerf_sdr_support",
    "fobos_sdr_support", "hackrf_sdr_support", "hydrasdr_sdr_support",
    "limesdr_sdr_support", "mirisdr_sdr_support", "net_source_support",
    "plutosdr_sdr_support", "remote_sdr_support", "rfnm_sdr_support",
    "rtlsdr_sdr_support", "rtltcp_support", "sddc_sdr_support",
    "sdrplay_sdr_support", "sdrpp_server_support", "spyserver_support",
    "usrp_sdr_support"
)
$required += $required_plugins | ForEach-Object { "lib\sattool\plugins\$_.dll" }

foreach($relative in $required)
{
    if(!(Test-Path (Join-Path $StagingPath $relative)))
    {
        throw "Required release file is missing: $relative"
    }
}

$plugin_count = @(Get-ChildItem $plugins -File -Filter *.dll).Count
if($plugin_count -lt $required_plugins.Count)
{
    throw "Only $plugin_count plugins were packaged; this release requires at least $($required_plugins.Count)"
}

function Get-Dependencies([string]$Path)
{
    $lines = & dumpbin /nologo /dependents $Path
    if($LASTEXITCODE -ne 0)
    {
        throw "dumpbin failed for $Path with exit code $LASTEXITCODE"
    }
    $reading = $false
    foreach($line in $lines)
    {
        $trimmed = $line.Trim()
        if($trimmed -eq "Image has the following dependencies:") { $reading = $true; continue }
        if($reading -and $trimmed -match '^[A-Za-z0-9_.+-]+\.dll$') { $trimmed }
    }
}

$binary_files = @(Get-ChildItem $bin,$plugins -File | Where-Object { $_.Extension -in '.exe','.dll' })
$packaged_dlls = @{}
Get-ChildItem $StagingPath -Recurse -File -Filter *.dll | ForEach-Object { $packaged_dlls[$_.Name.ToLowerInvariant()] = $true }
$missing = @()
foreach($binary in $binary_files)
{
    foreach($dependency in (Get-Dependencies $binary.FullName))
    {
        $name = $dependency.ToLowerInvariant()
        if($name -match '^(api-ms-|ext-ms-)' -or $packaged_dlls.ContainsKey($name)) { continue }
        if(Test-Path (Join-Path "$env:WINDIR\System32" $dependency)) { continue }
        $missing += "$($binary.Name) -> $dependency"
    }
}

if($missing.Count -gt 0)
{
    throw "Unpackaged runtime dependencies:`n$($missing | Sort-Object -Unique | Out-String)"
}

Write-Output "Verified complete SatTool release: $plugin_count plugins and no missing non-system DLL dependencies."
