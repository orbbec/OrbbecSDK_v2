# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

# Get current directory for return here later
$current_dir = Get-Location

# Default args
$defaultLibName = "OrbbecSDK"
$platform = "x64"
$targetArch = "x64"
$fullPlatform = "win_x64"
$buildType = "Release"
$sdkLibName = "OrbbecSDK"

# User args
foreach ($arg in $args) {
    if ($arg -match "^--?([^=]+)=(.+)") {
        $key = $matches[1].ToLower()
        $value = $matches[2]
        
        switch ($key) {
            "arch" {
                if ($value -ieq "x64") { 
                    $platform = "x64"
                    $fullPlatform = "win_x64"
                    $targetArch = "x64"
                }
                elseif ($value -ieq "x86") {
                    $platform = "Win32"
                    $fullPlatform = "win_x86"
                    $targetArch = "x86"
                    # x86 build release with debug info
                    $buildType = "RelWithDebInfo"
                }
                else {
                    Write-Output "Invalid arch argument, please use --arch=x64 or --arch=x86"
                    exit 1
                }
            }
            "config" {
                if ($value -ieq "Debug") {
                    $buildType = "Debug"
                }
                elseif ($value -ieq "Release") {
                    $buildType = "Release"
                }
                elseif ($value -ieq "RelWithDebInfo") {
                    $buildType = "RelWithDebInfo"
                }
                else {
                    Write-Output "Invalid build config argument, please use --config=Debug, --config=Release or --config=RelWithDebInfo"
                    exit 1
                }
            }
            "libName" { $sdkLibName = $value }
            default {
                Write-Output "Ignore the current unsupported parameter. key=$key,value=$value"
            }
        }
    } else {
        Write-Output "Invalid parameter format: $arg"
        Write-Output "Please use -key=value format (e.g., --arch=x64 --config=Release --libName=OrbbecSDK)"
        exit 1
    }
}

Write-Output "Build Configuration:"
Write-Output ">>>>> arch: $targetArch"
Write-Output ">>>>> build config: $buildType"
Write-Output ">>>>> SDK library name: $sdkLibName"
Write-Output ""

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
$package_name = "${sdkLibName}_v${version}_${timestamp}_${git_hash}_${fullPlatform}"

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

$build_dir = "build_$fullPlatform"
# create build directory
if (Test-Path $build_dir) {
    Remove-Item -Recurse -Force $build_dir
}
mkdir $build_dir
Set-Location $build_dir

# create install directory
$install_dir = Join-Path (Get-Location).Path "install\${package_name}"
mkdir $install_dir
mkdir $install_dir/bin

# copy opencv dll to install bin directory (here we use opencv 3.4.0 vc15 build)
$opencvPath = [System.Environment]::GetEnvironmentVariable("OpenCV_DIR")
$opencvLibPath  = Join-Path $opencvPath $targetArch
if (-not $opencvPath -or -not (Test-Path $opencvLibPath)) {
    Write-Warning "OpenCV path not found in environment variables. OpenCV DLL copy will be skipped."
} else {
    # get all available opencv vc version
    $availableVersions = Get-ChildItem -Directory $opencvLibPath  | Sort-Object Name -Descending

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

    $opencvLibPath = "$opencvLibPath/${selectedVersion}"
    $opencvDll = Get-ChildItem -Path $opencvLibPath -Filter "opencv_world*.dll" -Recurse
    $opencv_path = $opencvDll.FullName
    if (-not (Test-Path $opencv_path)) {
        Write-Output  "OpenCV dll not found at $opencv_path, please change the opecv dll path in build_win_msvc.ps1"
        exit 1
    }
    Copy-Item $opencv_path $install_dir/bin/
}

# build and install
cmake -G "$cmake_generator" `
    -A "$platform" `
    -T "v141,host=${targetArch}" `
    -DCMAKE_BUILD_TYPE=$buildType `
    -DCMAKE_INSTALL_PREFIX="${install_dir}" `
    -DOPEN_SDK_LIB_NAME="$sdkLibName" `
    ..
cmake --build . --config $buildType --target install
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Build $sdkLibName failed, exiting..."
    Set-Location  $current_dir
    exit $LASTEXITCODE
}

Write-Output "$sdkLibName install directory: ${install_dir}"

# Define a function to process file contents
function Remove-CopyrightAndLicensedLines {
    param (
        [string]$directoryPath  # Directory path to process
    )

    # Get all files in the directory
    $files = Get-ChildItem -Path $directoryPath -Recurse -File

    foreach ($file in $files) {
        # Read the file contents
        $fileContent = Get-Content -Path $file.FullName -Encoding UTF8
        $modifiedContent = @()

        # Iterate through each line and remove comment lines containing 'Copyright' or 'Licensed'
        foreach ($line in $fileContent) {
            if (($line.TrimStart().StartsWith("//") -or $line.TrimStart().StartsWith("#")) -and ($line -match "Copyright" -or $line -match "Licensed")) {
                continue  # Skip comment lines
            }
            $modifiedContent += $line  # Keep non-comment lines
        }

        # If content has been modified, save the file without BOM
        if ($modifiedContent.Count -ne $fileContent.Count) {
            # Use UTF8Encoding without BOM
            $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
            [System.IO.File]::WriteAllLines($file.FullName, $modifiedContent, $utf8NoBom)
            Write-Host "Modified file: $($file.FullName)"
        }
    }
}

if ($sdkLibName -ne ${defaultLibName}) {
    # Call the function to process 'examples' and 'include' directories
    Remove-CopyrightAndLicensedLines "$install_dir\examples"
    Remove-CopyrightAndLicensedLines "$install_dir\include"
}

# create zip file
$zip_file = "${package_name}.zip"
$zip_file_path = Join-Path (Get-Location).Path "install\${zip_file}"
Add-Type -assembly "system.io.compression.filesystem"
[io.compression.zipfile]::CreateFromDirectory("${install_dir}", "${zip_file_path}")
Write-Output  "Package zip file created at ${zip_file_path}"

Write-Output  "$sdkLibName for ${fullPlatform} build and install completed"

# return to current_dir
Set-Location  $current_dir
