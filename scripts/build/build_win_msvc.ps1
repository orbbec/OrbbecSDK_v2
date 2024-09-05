
# Get current directory for return here later
$current_dir = Get-Location

# Set platform
$platform = "x64"
$targetArch = "x64"
$fullPlatform = "win_x64"

if ($args.Length -eq 1) {
    if ($args[0] -eq "x64") {
        $platform = "x64"
        $fullPlatform = "win_x64"
        $targetArch = "x64"
    }
    elseif ($args[0] -eq "x86") {
        $platform = "Win32"
        $fullPlatform = "win_x86"
        $targetArch = "x86"
    }
    else {
        Write-Output "Invalid argument, please use x64 or x86"
        exit 1
    }
}

$SCRIPT_DIR = Split-Path -Path $MyInvocation.MyCommand.Definition -Parent
$PROJECT_ROOT = "$SCRIPT_DIR/../../"
Set-Location $PROJECT_ROOT

Write-Output  "Building OrbbecSDK for ${fullPlatform}"

# Variables for version from CMakeLists.txt project
$verPattern = 'project\(.*?VERSION\s+([0-9]+(?:\.[0-9]+)+)\s'
$content = Get-Content -Path $PROJECT_ROOT\CMakeLists.txt -Raw
$match = [regex]::Match($content, $verPattern)
$version = $match.Groups[1].Value
$timestamp = Get-Date -Format "yyyyMMddHHmm"
$git_hash = $(git rev-parse --short HEAD)
$package_name = "OrbbecSDK_v${version}_${timestamp}_${git_hash}_${fullPlatform}"

# check visual studio 2017 or 2019 or 2022 is installed
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsversion = & $vswhere -version "[15.0,)" -property installationVersion
if ($vsversion.Length -eq 0) {
    Write-Output  "Visual Studio 2017 or 2019 or 2022 is not installed"
    exit 1
}
$vsversion = ($vsversion | Select-object -First 1).ToString().Trim()
$vsversion = $vsversion.Substring(0, 2)
$year = ""
switch($vsversion){
  15 {$year = "2017"}
  16 {$year = "2019"}
  17 {$year = "2022"}
  default {
      Write-Output "Unknown Visual Studio version: $vsversion"
      exit 1
 }
}
$cmake_generator = "Visual Studio ${vsversion} ${year}"
Write-Output  "Using $cmake_generator as cmake generator"

# create build directory
if (Test-Path build_win_x64) {
    Remove-Item -Recurse -Force build_win_x64
}
mkdir build_win_x64
Set-Location build_win_x64

# create install directory
$install_dir = Join-Path (Get-Location).Path "install\${package_name}"
mkdir $install_dir
mkdir $install_dir/bin

# copy opencv dll to install bin directory (here we use opencv 3.4.0 vc15 build)
$opencvPath = [System.Environment]::GetEnvironmentVariable("OpenCV_DIR")
if (-not $opencvPath) {
    Write-Output "OpenCV path not found in environment variables."
}

$opencvPath = "$opencvPath\x64"
# get all available opencv vc version
$availableVersions = Get-ChildItem -Directory $opencvPath  | Sort-Object Name -Descending

# initialize
$selectedVersion = $null
$exactMatch = $false

# matching vc&vs version
foreach ($version in $availableVersions) {
    $versionNumber = [int]($version.Name -replace "vc", "")

    # Find the correct version and stop searching
    if ($versionNumber -eq $vsversion) {
        $selectedVersion = $version.Name
        $exactMatch = $true
        break
    } elseif ($versionNumber -lt $vsversion) {
        # If the correct version is not found, select the next available smaller version
        $selectedVersion = $version.Name
        break
    } elseif (-not $selectedVersion) {
        # If the correct version is not found, select the next larger version available
        $selectedVersion = $version.Name
    }
}

# Check whether the correct version is found
if (-not $selectedVersion) {
    Write-Output "No compatible OpenCV version found for Visual Studio $cmake_generator"
    exit 1
} else {
    Write-Output "Using OpenCV version: $selectedVersion"

    # No correct version found. Issue a warning
    if (-not $exactMatch) {
        Write-Warning "Warning: Selected OpenCV version ($selectedVersion) may not be fully compatible with Visual Studio toolset $cmake_generator. Proceed with caution."
    }
}

$opencvPath = "$opencvPath/${selectedVersion}"
$opencvDll = Get-ChildItem -Path $opencvPath -Filter "opencv_world*.dll" -Recurse
$opencv_path = $opencvDll.FullName
if (-not (Test-Path $opencv_path)) {
    Write-Output  "OpenCV dll not found at $opencv_path, please change the opecv dll path in build_win_msvc.ps1"
    exit 1
}
Copy-Item $opencv_path $install_dir/bin/

# build and install
cmake -G "$cmake_generator" -A "$platform" -T "v141,host=${targetArch}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${install_dir}" ..
cmake --build . --config Release --target install

# create zip file
$zip_file = "${package_name}.zip"
$zip_file_path = Join-Path (Get-Location).Path "install\${zip_file}"
Add-Type -assembly "system.io.compression.filesystem"
[io.compression.zipfile]::CreateFromDirectory("${install_dir}", "${zip_file_path}")
Write-Output  "Package zip file created at ${zip_file_path}"


Write-Output  "OrbbecSDK for ${fullPlatform} build and install completed"

# return to current_dir
Set-Location  $current_dir