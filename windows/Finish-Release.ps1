param(
    [string]$BuildPath="$(Split-Path -Parent $MyInvocation.MyCommand.Path)\..\build",
    [string]$SourcePath="$(Split-Path -Parent $MyInvocation.MyCommand.Path)\..",
    [string]$platform="x64-windows" #or x86-windows, arm64-windows
)

function Parse-DumpBin($binary_path)
{
    $return_val = @()
    $dumpbin_result = dumpbin /dependents $binary_path
    $reading = $false
    for($i = 0; $i -lt $dumpbin_result.Count; $i++)
    {
        $this_line = $dumpbin_result[$i].trim()
        if($this_line -eq "Image has the following dependencies:")
        {
            $i++
            $reading = $true
            continue
        }
        if($reading -and [string]::IsNullOrWhiteSpace($this_line))
        {
            break
        }
        if($reading)
        {
            $return_val += $this_line
        }
    }

    return $return_val
}

# Copy all files into a self-contained portable tree.
cd $BuildPath
if(Test-Path Release) { Remove-Item Release -Recurse -Force }
mkdir Release\plugins | Out-Null
cp plugins\Release\*.dll Release\plugins
cp -r $SourcePath\resources Release
cp $SourcePath\sattool_cfg.json Release
cp $SourcePath\icon.png Release
cp $SourcePath\vcpkg\installed\$platform\bin\*.dll Release
cd Release

$input_dlls = Get-ChildItem -Recurse -Filter *.dll
$input_dlls += Get-ChildItem -Recurse -Filter *.exe
$dll_array = @()

# Read dependencies
foreach($input_dll in $input_dlls)
{
    $dll_array += Parse-Dumpbin $input_dll.FullName
}

$dll_array = $dll_array | select -Unique
$available_dlls = Get-ChildItem $SourcePath\vcpkg\installed\$platform\bin -Filter *.dll
$dlls_to_copy = @()
foreach($available_dll in $available_dlls)
{
    if($dll_array.Contains($available_dll.Name))
    {
        $dlls_to_copy += $available_dll
    }
}

# Recursively get remaining dependencies
$last_count = 0
while($last_count -ne $dlls_to_copy.Count)
{
    $last_count = $dlls_to_copy.Count
    foreach($dll_to_copy in $dlls_to_copy)
    {
        $potential_dlls = Parse-DumpBin $dll_to_copy.FullName
        foreach($available_dll in $available_dlls)
        {
            if($potential_dlls.Contains($available_dll.Name) -and -not $dlls_to_copy.Name.Contains($available_dll.Name))
            {
                $dlls_to_copy += $available_dll
            }
        }
    }
}

# Copy determined dependencies
foreach($dll_to_copy in $dlls_to_copy)
{
    cp $dll_to_copy.FullName .
}

# Verify that the portable tree has a closed runtime dependency set.
$system_dlls = @('KERNEL32.dll','USER32.dll','GDI32.dll','ADVAPI32.dll','SHELL32.dll','OLE32.dll','OLEAUT32.dll','WS2_32.dll','COMDLG32.dll','SHLWAPI.dll','IPHLPAPI.DLL','VERSION.dll','IMM32.dll','SETUPAPI.dll','MSVCP140.dll','VCRUNTIME140.dll','VCRUNTIME140_1.dll','ucrtbase.dll')
$packaged_names = (Get-ChildItem -File -Filter *.dll).Name
$missing = @()
foreach($input_dll in $input_dlls)
{
    foreach($dep in (Parse-DumpBin $input_dll.FullName))
    {
        if($dep -match '^[A-Za-z0-9_.-]+\.dll$' -and $system_dlls -notcontains $dep -and $packaged_names -notcontains $dep -and -not (Test-Path (Join-Path $input_dll.DirectoryName $dep)))
        {
            $missing += "$($input_dll.Name) -> $dep"
        }
    }
}
if($missing.Count -gt 0) { $missing | Sort-Object -Unique | ForEach-Object { Write-Error "Unpackaged runtime dependency: $_" } }

cd ..\..
Write-Output "Done!"
