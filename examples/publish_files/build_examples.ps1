param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [ValidateSet("x64")]
    [string]$Architecture = "x64",
    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"

if(-not $Generator) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if(-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere.exe was not found. Install Visual Studio 2017 or newer, or pass -Generator explicitly."
    }
    $vsVersion = & $vswhere -version "[15.0,)" -sort `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationVersion
    if($vsVersion.Length -eq 0) {
        throw "No Visual Studio with the C++ build tools was found. Install Visual Studio 2017 or newer, or pass -Generator explicitly."
    }
    $vsVersion = ($vsVersion | Select-Object -First 1).ToString().Trim()
    $major = $vsVersion.Substring(0, 2)
    $year = ""
    switch($major) {
        15 { $year = "2017" }
        16 { $year = "2019" }
        17 { $year = "2022" }
        18 { $year = "2026" }
        default { throw "Unknown Visual Studio version: $vsVersion. Pass -Generator explicitly." }
    }
    $Generator = "Visual Studio $major $year"
}

$sdkRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$examplesRoot = Join-Path $sdkRoot "examples"
$buildRoot = Join-Path $examplesRoot "build"
$outputRoot = Join-Path $sdkRoot "bin"
$sdkLibRoot = Join-Path $sdkRoot "lib"
$sdkToolRoot = Join-Path $sdkRoot "tools"

if(-not (Test-Path -LiteralPath (Join-Path $examplesRoot "CMakeLists.txt"))) {
    throw "SDK examples CMakeLists.txt was not found: $examplesRoot"
}
if(-not (Test-Path -LiteralPath $sdkLibRoot)) {
    throw "SDK library directory was not found: $sdkLibRoot"
}
if(-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found in PATH. Install CMake and retry."
}

$env:Path = "$sdkToolRoot;$sdkRoot\bin;$env:Path"

Write-Host "SDK root: $sdkRoot"
Write-Host "Examples: $examplesRoot"
Write-Host "Build: $buildRoot"
Write-Host "Output: $outputRoot"
Write-Host "Generator: $Generator"

if(Test-Path -LiteralPath $buildRoot) {
    Remove-Item -LiteralPath $buildRoot -Recurse -Force
}

cmake -S $examplesRoot -B $buildRoot -G $Generator -A $Architecture `
    -DOrbbecSDK_DIR="$sdkLibRoot" `
    -DCMAKE_VS_GLOBALS="TrackFileAccess=false"
if($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

cmake --build $buildRoot --config $Configuration --parallel -- /p:TrackFileAccess=false
if($LASTEXITCODE -ne 0) {
    throw "Example build failed with exit code $LASTEXITCODE."
}

cmake --install $buildRoot --config $Configuration --prefix $sdkRoot
if($LASTEXITCODE -ne 0) {
    throw "Example install failed with exit code $LASTEXITCODE."
}

$extensionRoot = Join-Path $sdkToolRoot "extensions"
if(Test-Path -LiteralPath $extensionRoot) {
    Copy-Item -LiteralPath $extensionRoot -Destination $outputRoot -Recurse -Force
}

Remove-Item -LiteralPath $buildRoot -Recurse -Force

Write-Host "Examples built successfully. Output: $outputRoot"
